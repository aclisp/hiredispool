/* Author:   Huanghao
 * Date:     2017-2
 * Revision: 0.1
 * Function: Thread-safe redis client that mimic the Jedis interface
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
        if (sock == NULL)
            throw std::runtime_error("Can't get socket from pool");
    }
    // Release the socket to pool
    ~PooledSocket() {
        redis_release_socket(inst, sock);
    }
    // Implicit convert to REDIS_SOCKET*
    operator REDIS_SOCKET*() {
        return sock;
    }
private:
    REDIS_INSTANCE* inst;
    REDIS_SOCKET* sock;
};

class RedisReply
{
private:
    // non construct copyable and non copyable
    RedisReply(const RedisReply&);
    RedisReply& operator=(const RedisReply&);
public:
    RedisReply(void* _reply) : reply((redisReply*)_reply) {}
    RedisReply(redisReply* _reply) : reply(_reply) {}
    ~RedisReply() { freeReplyObject(reply); }
    bool valid() { return (reply != NULL); }
    operator redisReply*() { return reply; }
    redisReply* operator->() { return reply; }
private:
    redisReply* reply;
};

// RedisClient provides a Jedis-like interface, but it is threadsafe!
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
    // Set the string value as value of the key.
    // return status code reply
    std::string set(const std::string& key, const std::string& value);
    // Get the value of the specified key. if the key does not exist,
    // empty string ("") is returned.
    std::string get(const std::string& key);
private:
    REDIS_INSTANCE* inst;
};

#endif // REDISCLIENT_H
