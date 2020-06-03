# hiredispool

This library provides connection pooling and auto-reconnect for hiredis. It is also minimalistic and easy to do customization. The code has been thoroughly tested in a multi-threaded application, and used in production environments for a long time. It is proven to be thread-safe and no memory leak.

## Features 

* Connection pooling implemented in pure C
* Auto-reconnect and retry instantly at the first failure
* Comprehensive logging for diagnostic 
* No third-party dependency except hiredis
* Thread safe C++ wrapper with automatic memory management
* [Advanced] Multiple server endpoints can be provisioned, with automatic load-balancing and fail-over, to support scenarios such as high availablity redis-cluster and redis-proxy middleware.
* [Experimental] Jedis-like C++ wrapper 
* [Experimental] On-the-fly re-provisioning

## Limitations

* To support auto-reconnect and retry, you must use a wrapper of hiredis synchronous API

## How to use

To build it, clone the repo and `make`. Check out the `test_*.cpp` for examples.

## docker-hiredispool

Dockerfile to build the [hiredispool](https://github.com/aclisp/hiredispool) library by [aclisp](https://github.com/aclisp).

Use this image as part of a multistage build to get the library into your own containers without having to build it and clean up dependencies afterwards.
```
FROM alanvieyra333/hiredispool:latest as HIREDISPOOL

FROM alpine:3.9

COPY --from=HIREDISPOOL /usr/local/lib/libhiredis.so.0.13 /usr/local/lib/libhiredis.so
COPY --from=HIREDISPOOL /usr/local/lib/libhiredispool.so.0.1 /usr/local/lib/libhiredispool.so
```