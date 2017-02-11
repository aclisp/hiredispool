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
