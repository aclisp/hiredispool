#include "RedisClient.h"


using namespace std;


string RedisClient::set(const string& key, const string& value)
{
    PooledSocket socket(inst);
    RedisReply reply(redis_command(socket, inst, "SET %s %s", key.c_str(), value.c_str()));
    if (reply.valid()) {
        return string(reply->str, reply->len);
    }
    return "";
}

string RedisClient::get(const string& key)
{
    PooledSocket socket(inst);
    RedisReply reply(redis_command(socket, inst, "GET %s", key.c_str()));
    if (reply.valid()) {
        return string(reply->str, reply->len);
    }
    return "";
}
