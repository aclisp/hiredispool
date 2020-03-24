#include "RedisClient.h"

#include <string>
#include <iostream>
#include <vector>

#include <pthread.h>


using namespace std;


struct thread_info
{
    pthread_t thread;
    RedisClient* predis;
    long long count;
    int id;
};

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
    if (count != c) {
        cerr << " *** something error" << endl;
    }
}

void *thread_routine (void * arg)
{
    thread_info* pinfo = (thread_info*) arg;
    foo(*pinfo->predis, pinfo->count, pinfo->id);
    return NULL;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    const int num_endpoints = 4;

    REDIS_ENDPOINT endpoints[num_endpoints] = {
        { "127.0.0.1", 6379 },
        { "127.0.0.1", 6379 },
        { "127.0.0.1", 6379 },
        { "127.0.0.1", 6379 },
    };

    REDIS_CONFIG conf = {
        (REDIS_ENDPOINT*)&endpoints,
        num_endpoints,
        80,
        80,
        25,
        1,
    };

    RedisClient client(conf);
    vector<thread_info*> threads;

    for (int i=0; i<20; ++i) {
        thread_info* pinfo = new thread_info;

        pinfo->predis = &client;
        pinfo->count = 5000;
        pinfo->id = i;

        if (pthread_create(&pinfo->thread, NULL, thread_routine, pinfo) != 0)
        {
            cerr << "Can not create thread #" << pinfo->id << ", quit" << endl;
            delete pinfo;
            break;
        }

        threads.push_back(pinfo);
    }

    for(vector<thread_info*>::const_iterator i=threads.begin(); i!=threads.end(); ++i) {
        pthread_t thread = (*i)->thread;
        pthread_join(thread, NULL);
        delete (*i);
    }

    return 0;
}
