/* Author:   Huanghao
 * Date:     2017-2
 * Revision: 0.1
 * Function: Thread-safe redis client
 * Usage:    see test_RedisClient.cpp
 */

#ifndef REDISCLIENT_H
#define REDISCLIENT_H


#include <pthread.h>

#include "hiredispool.h"

#include "hiredis/hiredis.h"

#include <string>
#include <stdexcept>


// PooledSocket is a pooled socket wrapper that provides a convenient
// RAII-style mechanism for owning a socket for the duration of a
// scoped block.
class PooledSocket
{
private:
    // non construct copyable and non copyable
    PooledSocket(const PooledSocket&);
    PooledSocket& operator=(const PooledSocket&);
public:
    // Get a pooled socket from a redis instance.
    // throw runtime_error if something wrong.
    PooledSocket(REDIS_INSTANCE* _inst) : inst(_inst) {
        sock = redis_get_socket(inst);
    }
    // Release the socket to pool
    ~PooledSocket() {
        redis_release_socket(inst, sock);
    }
    // Implicit convert to REDIS_SOCKET*
    operator REDIS_SOCKET*() const { return sock; }
    bool notNull() const { return (sock != NULL); }
    bool isNull() const { return (sock == NULL); }
private:
    REDIS_INSTANCE* inst;
    REDIS_SOCKET* sock;
};

// Helper
struct RedisReplyRef
{
    redisReply* p;
    explicit RedisReplyRef(redisReply* _p): p(_p) {}
};

// RedisReplyPtr is a smart pointer encapsulate redisReply*
class RedisReplyPtr
{
public:
    explicit RedisReplyPtr(void* _p = 0) : p((redisReply*)_p) {}
    ~RedisReplyPtr() {
        //printf("freeReplyObject %p\n", (void*)p);
        freeReplyObject(p);
    }

    // release ownership of the managed object
    redisReply* release() {
        redisReply* temp = p;
        p = NULL;
        return temp;
    }

    // transfer ownership
    RedisReplyPtr(RedisReplyPtr& other) {
        p = other.release();
    }
    RedisReplyPtr& operator=(RedisReplyPtr& other) {
        if (this == &other)
            return *this;
        RedisReplyPtr temp(release());
        p = other.release();
        return *this;
    }

    // automatic conversions
    RedisReplyPtr(RedisReplyRef _ref) {
        p = _ref.p;
    }
    RedisReplyPtr& operator=(RedisReplyRef _ref) {
        if (p == _ref.p)
            return *this;
        RedisReplyPtr temp(release());
        p = _ref.p;
        return *this;
    }
    operator RedisReplyRef() { return RedisReplyRef(release()); }

    bool notNull() const { return (p != 0); }
    bool isNull() const { return (p == 0); }
    redisReply* get() const { return p; }
    redisReply* operator->() const { return p; }
    redisReply& operator*() const { return *p; }

private:
    redisReply* p;
};

// RedisClient provides a threadsafe redis client
class RedisClient
{
private:
    // non construct copyable and non copyable
    RedisClient(const RedisClient&);
    RedisClient& operator=(const RedisClient&);
public:
    RedisClient(const REDIS_CONFIG& conf) {
        if (redis_pool_create(&conf, &inst) < 0)
            throw std::runtime_error("Can't create pool");
    }
    ~RedisClient() {
        redis_pool_destroy(inst);
    }

    // ----------------------------------------------------
    // Thread-safe command
    // ----------------------------------------------------

    // redisCommand is a thread-safe wrapper of that function in hiredis
    // It first get a connection from pool, execute the command on that
    // connection and then release the connection to pool.
    // the command's reply is returned as a smart pointer,
    // which can be used just like raw redisReply pointer.
    RedisReplyPtr redisCommand(const char *format, ...);
    RedisReplyPtr redisvCommand(const char *format, va_list ap);

private:
    REDIS_INSTANCE* inst;
};

#endif // REDISCLIENT_H
