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

    {
        cout << "Press <ENTER> to continue..." << endl;
        cin.get();

        RedisReplyPtr reply = client.redisCommand("SET %s %s", "key0", "value0");
        if (reply.notNull())
            cout << "SET: " << reply->str << endl;
        else
            cout << "SET: " << "Something wrong." << endl;
    }

    {
        cout << "Press <ENTER> to continue..." << endl;
        cin.get();

        RedisReplyPtr reply = client.redisCommand("GET %s", "key0");
        if (reply.notNull())
            if (reply->type == REDIS_REPLY_NIL)
                cout << "GET: Key does not exist." << endl;
            else
                cout << "GET: " << reply->str << endl;
        else
            cout << "GET: " << "Something wrong." << endl;
    }

    {
        cout << "Press <ENTER> to continue..." << endl;
        cin.get();

        client.redisCommand("SET count0 0");
        long long count0;
        for (long long i = 0; i < 1000; i++) {
            count0 = client.redisCommand("INCR count0")->integer;
        }
        cout << "INCR count0 to " << count0 << endl;
    }

    {
        cout << "Press <ENTER> to continue..." << endl;
        cin.get();

        RedisReplyPtr reply = client.redisCommand("PING");
        cout << "PING: " << reply->str << endl;
        // reply will be freed out of scope.
    }

    return 0;
}
