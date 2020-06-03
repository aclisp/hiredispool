#include <hiredispool/RedisClient.h>

#include <string>
#include <iostream>
#include <thread>
#include <vector>


using namespace std;


void foo(RedisClient& client, long long c, int id)
{
    client.redisCommand("SET ID%d 0", id);
    long long count = -1;
    for (long long i=0; i<c; ++i) {
        RedisReplyPtr reply = client.redisCommand("INCR ID%d", id);
        if (reply.notNull()) {
            count = reply->integer;
        }
    }
    cout << "INCR ID" << id << " to " << count << endl;
}

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
        500,
        500,
        20,
        1,
    };

    RedisClient client(conf);
    vector<thread> threads;

    for (int i=0; i<20; ++i) {
        threads.emplace_back(thread(foo, ref(client), 1000, i));
    }

    for(auto i=threads.begin(); i!=threads.end(); ++i) {
        i->join();
    }

    return 0;
}

