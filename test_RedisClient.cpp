#include "RedisClient.h"

#include <string>
#include <iostream>

using namespace std;


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    REDIS_ENDPOINT endpoints[1] = {
        { "127.0.0.1", 6379 },
        //{ "127.0.0.1", 6380 },
        //{ "127.0.0.1", 6381 },
    };

    REDIS_CONFIG conf = {
        (REDIS_ENDPOINT*)&endpoints,
        1,
        10000,
        5000,
        20,
        1,
    };

    RedisClient client(conf);

    cin.get();

    string res = client.set("key0", "value0");
    cout << "SET: " << res << endl;

    cin.get();

    res = client.get("key0");
    cout << "GET: " << res << endl;

    return 0;
}
