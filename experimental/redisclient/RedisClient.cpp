#include "RedisClient.h"
#include "logger.h"

#include <stdarg.h>


using namespace std;


void RedisClient::_setTarget(RedisReplyPack* p, REDIS_SOCKET* s)
{
    p->info.err = 0;
    p->info.errstr = "";
    p->info.target.host = ((redisContext*)(s->conn))->tcp.host;
    p->info.target.port = ((redisContext*)(s->conn))->tcp.port;
}

void RedisClient::_setError(RedisReplyPack* p, REDIS_SOCKET* s)
{
    p->info.err = s->err;
    p->info.errstr = s->errstr;
    p->info.target.host = s->errhost;
    p->info.target.port = s->errport;
}

void RedisClient::_setError(RedisReplyPack* p, const char* errstr)
{
    p->info.err = -1;
    p->info.errstr = errstr;
    p->info.target.host = "N/A";
    p->info.target.port = -1;
}

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
    RedisReplyPack* p(new RedisReplyPack);
    PooledSocket socket(inst);

    if (socket.notNull()) {
        p->reply = (redisReply*) redis_vcommand(socket.get(), inst, format, ap);
        if (p->reply) {
            _setTarget(p, socket.get());
        } else {
            _setError(p, socket.get());
        }
    } else {
        logth(Error, "Can not get socket from redis connection pool, server down? or not enough connection?");
        _setError(p, "No available connection");
    }

    return RedisReplyPtr(p);
}

RedisReplyPtr RedisClient::redisCommandArgv(int argc, const char **argv, const size_t *argvlen)
{
    RedisReplyPack* p(new RedisReplyPack);
    PooledSocket socket(inst);

    if (socket.notNull()) {
        p->reply = (redisReply*) redis_command_argv(socket.get(), inst, argc, argv, argvlen);
        if (p->reply) {
            _setTarget(p, socket.get());
        } else {
            _setError(p, socket.get());
        }
    } else {
        logth(Error, "Can not get socket from redis connection pool, server down? or not enough connection?");
        _setError(p, "No available connection");
    }

    return RedisReplyPtr(p);
}
