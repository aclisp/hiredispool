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
