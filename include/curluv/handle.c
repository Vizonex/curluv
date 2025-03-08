#include <uv.h>
#include <curl/curl.h>
#include "handle.h"


/* Some code is based off multi-uv.c */

void* __curl_uv_default_malloc(void* ctx, size_t size){
    return malloc(size);
};

void* __curl_uv_default_realloc(void* ctx, void* ptr, size_t new_size){
    return realloc(ptr, new_size);
};

void __curl_uv_free(void* ctx, void* ptr){
    return free(ptr);
}

int curl_uv_memory_init(
    curl_uv_memory_t* handle,
    void* (*curl_uv_malloc)(void* ctx, size_t size),
    void* (*curl_uv_realloc)(void* ctx, void* ptr, size_t new_size),
    void (*curl_uv_free)(void* ctx, void* ptr)
){
    handle->malloc = curl_uv_malloc;
    handle->realloc = curl_uv_realloc;
    handle->free = curl_uv_free;
    return 0;
};

int curl_uv_memory_default(
    curl_uv_memory_t* handle
){
    handle->malloc = __curl_uv_default_malloc;
    handle->realloc = __curl_uv_default_realloc;
    handle->free = __curl_uv_free;
    return 0;
}


int curl_uv_init(
    curl_uv_t* curl_uv, 
    uv_loop_t* loop,
    CURLM* multi
){

    curl_uv->loop = loop;
    curl_uv->multi = multi;
    curl_uv_memory_default(&curl_uv->alloc);
    return uv_timer_init(
        curl_uv->loop, &curl_uv->timeout
    );
}

// CURLMcode curl_uv_cleanup(curl_uv_t* curl_uv){
//     return curl_multi_cleanup(curl_uv->multi);
// }


int curl_uv_init_custom_memory(
    curl_uv_t* curl_uv, 
    uv_loop_t* loop,
    CURLM* multi,
    void *(*curl_uv_malloc)(void* ctx, size_t size),
    void *(*curl_uv_realloc)(void* ctx, void* ptr, size_t new_size),
    void (*curl_uv_free)(void* ctx, void* ptr)
){
    curl_uv->loop = loop;
    curl_uv->multi = multi;
    curl_uv_memory_init(
        &curl_uv->alloc,
        curl_uv_malloc,
        curl_uv_realloc,
        curl_uv_free
    );
    curl_uv->on_socket_created = NULL;
    curl_uv->on_response = NULL;
    return uv_timer_init(
        curl_uv->loop, &curl_uv->timeout
    );
}


/* ==== CURL_UV_SOCKET ==== */

curl_uv_socket_t *curl_uv_socket_create(curl_uv_t *ctx, curl_socket_t sockfd)
{
    curl_uv_socket_t* handle = (curl_uv_socket_t*)ctx->alloc.malloc((void*)ctx ,sizeof(curl_uv_socket_t));
    if (handle == NULL){
        return NULL;
    }
    handle->socket_fd = sockfd;
    handle->ctx = ctx;
    uv_poll_init_socket(ctx->loop, &handle->poll_handle, (uv_os_sock_t)sockfd);
    handle->poll_handle.data = (void*)ctx;
    
    // Handle Socket Creation by User
    if (ctx->on_socket_created != NULL){
        ctx->on_socket_created(handle);
    }

    return handle;
}

static void curl_uv_socket_close_cb(uv_handle_t* handle){
    curl_uv_socket_t* ctx  = (curl_uv_socket_t*)handle->data;
    curl_uv_t* main_ctx = ctx->ctx;
    // Free Using custom alloc.
    main_ctx->alloc.free(main_ctx, ctx);
};

void curl_uv_socket_close(curl_uv_socket_t* context){
    uv_close((uv_handle_t*)&context->poll_handle, curl_uv_socket_close_cb);
}


static void __curl_uv_check_multi_info(curl_uv_socket_t *context)
{
    CURLMsg *message;
    int pending;
    CURL *easy_handle;
    curl_uv_t* ctx = context->ctx;

    while (message = curl_multi_info_read(ctx->multi, &pending)){
        switch (message->msg){
            case CURLMSG_DONE: {
                
                // See if there were callbacks placed on either the main or socket levels...

                easy_handle = message->easy_handle;
                if (ctx->on_response != NULL){
                    ctx->on_response(context, easy_handle);
                }

                if (context->on_response != NULL){
                    context->on_response(context, easy_handle);
                }
            }
            default:
                break;
        }
    }


}

static void __cb_curl_uv_on_socket_activity(uv_poll_t *req, int status, int events)
{
  int running_handles;
  int flags = 0;
  curl_uv_socket_t *context = (curl_uv_socket_t *) req->data;
  (void)status;
  if(events & UV_READABLE)
    flags |= CURL_CSELECT_IN;
  if(events & UV_WRITABLE)
    flags |= CURL_CSELECT_OUT;

    curl_multi_socket_action(context->ctx->multi, context->socket_fd, flags,
                           &running_handles);
 
}


/* callback from libcurl to update socket activity to wait for */
static int __cb_curl_uv_socket(
    CURL *easy, 
    curl_socket_t s, 
    int action,
    curl_uv_t *uv,
    void *socketp
){
    curl_uv_socket_t* curl_socket;
    int events = 0;
    (void)easy;


    #define __CURLUV_CREATE_SOCKET \
        curl_socket = socketp ? (curl_socket_t *) socketp : curl_uv_socket_create(s, uv); \
        curl_multi_assign(uv->multi, s, (void*)curl_socket);
    

    switch (action) {
        case CURL_POLL_IN: {
            __CURLUV_CREATE_SOCKET
            events |= UV_READABLE;
            uv_poll_start(&curl_socket->poll_handle, events, __cb_curl_uv_on_socket_activity);
        }
        case CURL_POLL_OUT: {
            __CURLUV_CREATE_SOCKET
            events |= UV_WRITABLE;
            uv_poll_start(&curl_socket->poll_handle, events, __cb_curl_uv_on_socket_activity);
        }
        case CURL_POLL_INOUT: {
            __CURLUV_CREATE_SOCKET
            uv_poll_start(&curl_socket->poll_handle, events, __cb_curl_uv_on_socket_activity);
            break;
        }
        case CURL_POLL_REMOVE: {
            if(socketp) {
                uv_poll_stop(&((curl_uv_socket_t*)socketp)->poll_handle);
                ((curl_uv_socket_t*) socketp);
                curl_multi_assign(uv->multi, s, NULL);
            }
        }

    }


    return 0;
}

