#include "RedisClient.h"

#include <stdarg.h>


using namespace std;


RedisReplyPtr RedisClient::redisCommand(const char *format, ...)
{
    va_list ap;
    RedisReplyPtr reply;

    va_start(ap, format);
    reply = redisvCommand(format, ap);
    va_end(ap);

    return reply;
}

RedisReplyPtr RedisClient::redisvCommand(const char *format, va_list ap)
{
    void* reply;
    PooledSocket socket(inst);

    reply = redis_vcommand(socket, inst, format, ap);

    return RedisReplyPtr(reply);
}

string RedisClient::set(const string& key, const string& value)
{
    RedisReplyPtr reply = redisCommand("SET %s %s", key.c_str(), value.c_str());
    if (reply.notNull()) {
        return string(reply->str, reply->len);
    }
    return "";
}

string RedisClient::get(const string& key)
{
    RedisReplyPtr reply = redisCommand("GET %s", key.c_str());
    if (reply.notNull()) {
        return string(reply->str, reply->len);
    }
    return "";
}

long long RedisClient::incr(const string& key)
{
    RedisReplyPtr reply = redisCommand("INCR %s", key.c_str());
    if (reply.notNull()) {
        return reply->integer;
    }
    return 0;
}
