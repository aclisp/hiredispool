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

    cout << "Press <ENTER> to continue..." << endl;
    cin.get();

    string res = client.set("key0", "value0");
    cout << "SET: " << res << endl;

    cout << "Press <ENTER> to continue..." << endl;
    cin.get();

    res = client.get("key0");
    cout << "GET: " << res << endl;

    cout << "Press <ENTER> to continue..." << endl;
    cin.get();

    client.set("count0", "0");
    long long count0;
    for (long long i = 0; i < 1000; i++) {
        count0 = client.incr("count0");
    }
    cout << "INCR count0 to " << count0 << endl;

    cout << "Press <ENTER> to continue..." << endl;
    cin.get();

    {
        RedisReplyPtr reply = client.redisCommand("PING");
        cout << "PING: " << reply->str << endl;
        // reply will be freed out of scope.
    }

    return 0;
}

