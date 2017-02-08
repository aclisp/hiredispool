#include <pthread.h>
#include <time.h>

#include "hiredispool.h"
#include "log.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    LOG_CONFIG c = {
        9,
        LOG_DEST_FILES,
        "log/test_hiredispool.log",
        "test_hiredispool",
        0,
        1
    };
    log_set_config(&c);

    REDIS_ENDPOINT endpoints[2] = {
        { "127.0.0.1", 6379 },
        { "127.0.0.1", 6380 }
    };

    REDIS_CONFIG conf = {
        (REDIS_ENDPOINT*)&endpoints,
        2,
        10000,
        5000,
        20,
        2,
    };

    REDIS_INSTANCE* inst;
    if (redis_pool_create(&conf, &inst) < 0)
        return -1;

    redis_pool_destroy(inst);
    return 0;
}
