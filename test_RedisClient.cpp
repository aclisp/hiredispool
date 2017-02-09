#include "RedisClient.h"
#include "log.h"

#include <string>

using namespace std;


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    LOG_CONFIG c = {
        -9,
        LOG_DEST_FILES,
        "log/test_RedisClient.log",
        "test_RedisClient",
        0,
        1
    };
    log_set_config(&c);

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

    RedisClient client(conf);

    string res = client.set("key0", "value0");
    DEBUG("SET: %s", res.c_str());
    res = client.get("key0");
    DEBUG("GET: %s", res.c_str());

    return 0;
}
