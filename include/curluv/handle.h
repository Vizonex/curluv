#ifndef __CURLUV_HANDLE_H__
#define __CURLUV_HANDLE_H__

#include <uv.h>
#include <curl/curl.h>

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* === HANDLES AND CALLBACKS === */

typedef struct curl_uv_socket_s curl_uv_socket_t;

/* Defines callbacks related to curl_uv_sockets */
typedef void (*curl_uv_socket_cb)(curl_uv_socket_t*);
typedef void (*curl_uv_response_cb)(curl_uv_socket_t*, CURL*);


/* Inspired by yyjson */

typedef struct curl_uv_memory_s {
    void* (*malloc)(void* ctx, size_t size);
    void* (*realloc)(void* ctx, void* ptr, size_t new_size);
    void (*free)(void* ctx, void* ptr);
} curl_uv_memory_t;


typedef struct curl_uv_s {
    uv_timer_t timeout;
    uv_loop_t* loop;
    CURLM* multi;
    curl_uv_memory_t alloc;
    curl_uv_socket_cb on_socket_created;
    curl_uv_response_cb on_response;

    /**
     * @brief Unused but should be used for binding with other contexts for example
     * If you were to bind curl_uv_t to a Cython Class
     */
    void* data;
} curl_uv_t;


/** 
 * @brief Libuv's eventloop 
 * And initializes it's internal timer.
 * @param curl_uv the main context to be initalized
 * @param loop the event loop
 * @param multi the context belonging to CURLM
 * @returns This function always returns `0`
 * */
int curl_uv_init(
    curl_uv_t* curl_uv, 
    uv_loop_t* loop,
    CURLM* multi
);

/** If I gave a signature for this next function it would be this 
 * CURLMcode curl_uv_setopt(curl_uv_t* curl_uv, CURLMoption option, ...)
 * 
 * For now, a macro is more optimized
 */


#ifndef curl_uv_setopt 
/**
 * @brief Sets options for the multi handle.
 * @param curl_uv `curl_uv_t*` our main multi-handle to be manipulated
 * @param option `CURLMoption` the option to be set
 * @returns `CURLMcode` to determine weather or not there was an error
 */
#define curl_uv_setopt(curl_uv, option, ...) curl_multi_setopt(curl_uv->multi, option, __VA_ARGS__)
#endif /* curl_uv_setopt */


#ifndef curl_uv_cleanup
/**
 * @brief From Curl Docs: Cleans up and removes a whole multi stack. It does not free or
 * touch any individual easy handles in any way. We need to define
 * in what state those handles will be if this function is called
 * in the middle of a transfer.
 * @returns CURLMcode type, general multi error code.
 */
#define curl_uv_cleanup(curl_uv) curl_multi_cleanup(curl_uv->multi)
#endif /* curl_uv_cleanup */


/**
 * @brief Adds custom allocator callbacks to allocate handles with. 
 * This could be used for binding with other languages such as CPython, Node-js or Rust
 */
int curl_uv_init_custom_memory(
    curl_uv_t* curl_uv, 
    uv_loop_t* loop,
    CURLM* multi,
    void (*curl_uv_malloc)(void* ctx, size_t size),
    void (*curl_uv_realloc)(void* ctx, void* ptr, size_t new_size),
    void (*curl_uv_free)(void* ctx, void* ptr)
);


void curl_uv_set_socket_create_cb(curl_uv_t* ctx, curl_uv_socket_cb* socket_cb){
    ctx->on_socket_created = socket_cb;
}

void curl_uv_set_response_cb(curl_uv_t* ctx, curl_uv_response_cb* resp_cb){
    ctx->on_response = resp_cb;
}



struct curl_uv_socket_s{
    uv_poll_t poll_handle;
    curl_socket_t socket_fd;
    curl_uv_t* ctx;
    
    curl_uv_response_cb on_response;

    
    /**
     * @brief Unused but should be used for binding with other contexts for example
     * If you were to bind curl_uv_socket_t to a Cython Class
     */
    void* data;
}; 




/**
 * @brief Allocates memory to a new handle and initalizes the socket 
 * @param ctx the main context variable
 * @param sockfd the socket file descriptor
 */
curl_uv_socket_t* curl_uv_socket_create(curl_uv_t* ctx, curl_socket_t sockfd);




/**
 * @brief Closes the socket handle created
 * @param context the socket handle to free up and close
 */
void curl_uv_socket_close(curl_uv_socket_t* context);





#ifdef __cplusplus
};
#endif




#endif // __CURLUV_HANDLE_H__