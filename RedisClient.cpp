#include "RedisClient.h"

#include <stdarg.h>


using namespace std;


RedisReply RedisClient::redisCommand(const char *format, ...)
{
    va_list ap;
    void* reply;
    PooledSocket socket(inst);

    va_start(ap, format);
    reply = redis_vcommand(socket, inst, format, ap);
    va_end(ap);

    return RedisReply(reply);
}

string RedisClient::set(const string& key, const string& value)
{
    RedisReply reply = redisCommand("SET %s %s", key.c_str(), value.c_str());
    if (reply.notNull()) {
        return string(reply->str, reply->len);
    }
    return "";
}

string RedisClient::get(const string& key)
{
    RedisReply reply = redisCommand("GET %s", key.c_str());
    if (reply.notNull()) {
        return string(reply->str, reply->len);
    }
    return "";
}
