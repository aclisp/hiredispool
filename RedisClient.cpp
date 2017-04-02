#include "RedisClient.h"
#include "log.h"

#include <stdarg.h>


using namespace std;


RedisReplyPtr RedisClient::redisCommand(const char *format, ...)
{
    RedisReplyPtr reply;
    va_list ap;

    va_start(ap, format);
    reply = redisvCommand(format, ap);
    va_end(ap);

    return reply;
}

RedisReplyPtr RedisClient::redisvCommand(const char *format, va_list ap)
{
    void* reply = 0;
    PooledSocket socket(inst);

    if (socket.notNull()) {
        reply = redis_vcommand(socket, inst, format, ap);
    } else {
        log_(L_ERROR|L_CONS, "Can not get socket from redis connection pool, server down? or not enough connection?");
    }

    return RedisReplyPtr(reply);
}
