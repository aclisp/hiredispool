#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "hiredispool.h"
#include "logger.h"
#include "hiredis/hiredis.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
/*
    LOG_CONFIG c = {
        -9,
        LOG_DEST_FILES,
        "log/test_hiredispool.log",
        "test_hiredispool",
        0,
        1
    };
    log_set_config(&c);
*/
    REDIS_ENDPOINT endpoints[2] = {
        { "127.0.0.1", 6379 },
        { "127.0.0.1", 6380 },
        //{ "127.0.0.1", 6381 },
    };

    REDIS_CONFIG conf = {
        (REDIS_ENDPOINT*)&endpoints,
        2,
        10000,
        5000,
        20,
        1,
    };

    REDIS_INSTANCE* inst;
    if (redis_pool_create(&conf, &inst) < 0)
        return -1;

    //sleep(5);

    REDIS_SOCKET* sock;
    if ((sock = redis_get_socket(inst)) == NULL) {
        redis_pool_destroy(inst);
        return -1;
    }

    //sleep(5);

    redisReply* reply;
    if ((reply = (redisReply*)redis_command(sock, inst, "PING")) == NULL) {
        redis_release_socket(inst, sock);
        redis_pool_destroy(inst);
        return -1;
    }
    logth(Info, "PING: %s", reply->str);
    freeReplyObject(reply);

    redis_release_socket(inst, sock);
    redis_pool_destroy(inst);
    return 0;
}
