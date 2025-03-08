# CurlUV
A Helpful C Library meant to help bind Curl and Libuv together. 

# Reasons
There were a few reasons for why I decided to start developing this library
1. Binding Winloop & Uvloop python libraries to `curl_ffi` or `cycurl` leading to safer asynchronous code for these third party sources
2. Safer Visitation of As many curl-multi-handles as you want.  
3. Wanted to add custom allocators if I decide to develop Cython bindings.
4. Other Code Language developers might want to bind these to other languages so making this completely seperate from Cython was a must.


## TODOS
Library is incomplete and is not ready 
- [ ] Finish Listeners and `curl_uv_socket_t` handles
- [ ] Set Default CurlM options
- [ ] CMakeLists
- [ ] tools for helping with Python Setuptools
