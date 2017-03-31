#include "RedisClientYaf.h"
#include "IConfig.h"

#include <iostream>
#include <string>
#include <map>

#include <assert.h>
#include <unistd.h>
#include <pthread.h>


using namespace std;
using namespace anka;
using namespace anka::ramcloud;

#define __const_iterator_def(Iterator, Initializer) __typeof(Initializer) Iterator=(Initializer)
#define __foreach(Iterator,Container) for(__const_iterator_def(Iterator,(Container).begin()); Iterator!=(Container).end(); ++Iterator)


const int THR_NUM = 1;


class MockConfig : public anka::IConfigObserver
{
public:
	MockConfig() {}
	void RegisterWatcher(const string& key, anka::IConfigWatcher* watcher) {

		map<string, map<string, string> > cfg;
		map<string, string> attrs;

		if (key == "ramcloudsources") {
			attrs["host"] = "106.38.198.210";
			attrs["port"] = "9403";
			attrs["backup_host"] = "106.120.184.86";
			attrs["backup_port"] = "9403";

			attrs["connect_timeout"] = "200";
			attrs["net_readwrite_timeout"] = "200";
			attrs["num_redis_socks"] = "10";
			attrs["connect_failure_retry_delay"] = "1";

			cfg["default"] = attrs;
		}
		else if (key == "redisources") {
			attrs["num_redis_socks"] = "10";
			attrs["host"] = "127.0.0.1";

			int port = 6379;
			char buf[32];
			for (int i = 0; i < THR_NUM; ++i) {
				sprintf(buf, "%d", port + i);
				attrs["port"] = buf;
				sprintf(buf, "redis%d", i);
				cfg[buf] = attrs;
			}
		}

		watcher->MapValuesConfigUpdate(key, cfg);
	}
};

void test_del(Client& c)
{
	Status status;
	int affected = 0;

	affected = 0;
	status = c.set(__func__, "1");
	cout << "set " << status.code() << endl;
	assert(status.ok());

	affected = 0;
	status = c.delStringKey(__func__, &affected);
	cout << "del " << status.code() << " affected " << affected << endl;;
	assert(status.ok());

	affected = 0;
	status = c.set("test_del_2", "1");
	cout << "set test_del_2 " << status.code() << endl;

	std::set<std::string> keys;
	keys.insert("test_del");
	keys.insert("test_del_2");
	affected = 0;
	status = c.delStringKey(keys, &affected);
	cout << "del multi " << status.code() << " affected " << affected << endl;
	assert(status.ok() && affected == 1);
}

void test_wrong_type(Client& c)
{
	Status status;

	status = c.hset(__func__, "1", "1");
	cout << "hset " << status.code() << endl;
	assert(status.ok());

	status = c.incrby(__func__, 1);
	cout << "incrby " << status.code() << endl;
	assert(!status.ok());
}

void test_getset(Client& c)
{
	Status status;
	int affected = 0;
	int ttl = 0;
	string val;

	affected = 0; val.clear();
	status = c.setex(__func__, "abc", 3);
	cout << "setex " << status.code() << " ttl " << 3 << endl;
	assert(status.ok());

	sleep(2);
	affected = 0; val.clear();
	status = c.ttl(__func__, ttl);
	cout << "ttl " << status.code() << " ttl " << ttl << endl;
	assert(status.ok() && ttl > 0);

	sleep(2);
	affected = 0; val.clear();
	status = c.ttl(__func__, ttl);
	cout << "ttl " << status.code() << " ttl " << ttl << endl;
	assert(status.not_found());

	affected = 0; val.clear();
	status = c.setex(__func__, "abc", 10);
	cout << "setex " << status.code() << " ttl " << 10 << endl;

	sleep(2);
	affected = 0; val.clear();
	status = c.ttl(__func__, ttl);
	cout << "ttl " << status.code() << " ttl " << ttl << endl;

	status = c.persist(__func__);
	cout << "persist " << status.code() << endl;

	sleep(2);
	affected = 0; val.clear();
	status = c.ttl(__func__, ttl);
	cout << "ttl " << status.code() << " ttl " << ttl << endl;
	assert(status.ok() && ttl == -1);

	affected = 0; val.clear();
	status = c.set(__func__, "1");
	cout << "set " << status.code() << endl;
	assert(status.ok());

	affected = 0; val.clear();
	status = c.get(__func__, val);
	cout << "get " << status.code() << " val " << val << endl;
	assert(status.ok() && val == "1");

	std::map<string, string> kvs;
	kvs["tttt1"] = "1111";
	kvs["tttt2"] = "2222";
	affected = 0; val.clear();
	status = c.mset(kvs);
	cout << "mset " << status.code() << endl;
	assert(status.ok());


	kvs.clear();
	std::set<string> keys;
	keys.insert("tttt1");
	keys.insert("tttt2");
	affected = 0; val.clear();
	status = c.mget(keys, kvs);
	cout << "get " << status.code() << " kvs.size() " << kvs.size() << endl;
	assert(status.ok() && kvs.size() == 2 && kvs.begin()->second == "1111");

	affected = 0; val.clear();
	status = c.delStringKey(__func__, &affected);
	cout << "del " << status.code() << " affected " << affected << endl;
	assert(status.ok() && affected == 1);

	affected = 0; val.clear();
	status = c.get(__func__, val);
	cout << "get " << status.code() << " val " << val << endl;
	assert(status.ok() && status.not_found());

	std::map<uint32_t, string> kvs1;
	kvs1[1] = "1111";
	kvs1[2] = "2222";
	affected = 0; val.clear();
	status = c.setMapValue(__func__, kvs1, 10);
	cout << "setMap " << status.code() << endl;
	assert(status.ok());

	std::map<uint32_t, string> val1;
	bool keyExist;
	status = c.getMapValue(__func__, val1, keyExist);
	cout << "getMap " << status.code() << endl;
	assert(val1 == kvs1);
	assert(keyExist);
}

void test_incrby(Client& c)
{
	Status status;
	int affected = 0;
	int64_t val = 0;

	affected = 0; val = 0;
	status = c.set(__func__, "10");
	cout << "set 10" << status.code() << endl;
	assert(status.ok());

	affected = 0; val = 0;
	status = c.incrby(__func__, 1, &val);
	cout << "incrby 1 " << status.code() << " newval " << val << endl;
	assert(status.ok() && val == 11);

	affected = 0; val = 0;
	status = c.incrby(__func__, -2, &val);
	cout << "incrby -2 " << status.code() << " newval " << val << endl;
	assert(status.ok() && val == 9);

	affected = 0; val = 0;
	status = c.delStringKey(__func__, &affected);
	cout << "del " << status.code() << " affected " << affected << endl;
	assert(status.ok() && affected == 1);

	affected = 0; val = 0;
	status = c.set(__func__, "abc");
	cout << "set 'abc'" << status.code() << endl;
	assert(status.ok());

	affected = 0; val = 0;
	status = c.incrby(__func__, 1, &val);
	cout << "incrby 1 " << status.code() << " newval " << val << endl;
	assert(!status.ok());

}

void test_set(Client& c)
{
	Status status;
	std::string type;

	status = c.sadd(__func__, "1");
	cout << "sadd 1 " << status.code() << endl;
	assert(status.ok());

	std::set<std::string> x; x.insert("2"); x.insert("3"); x.insert("4"); x.insert("5");
	status = c.sadd(__func__, x);
	cout << "sadd 2 3 4 5 " << status.code() << endl;
	assert(status.ok());

	status = c.type(__func__, type);
	cout << "type " << status.code() << " type: " << type << endl;
	assert(status.ok() && type == "set");


	int count = 0;
	status = c.scard(__func__, count);
	cout << "scard " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 5);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	count = 0;
	status = c.srem(__func__, "1", &count);
	cout << "srem 1 " << status.code() << " delcount " << count << endl;
	assert(status.ok() && count == 1);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	count = 0;
	status = c.scard(__func__, count);
	cout << "scard " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 4);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	x.clear();
	status = c.smembers(__func__, x);
	cout << "smembers " << status.code() << " count " << x.size() << endl;
	assert(status.ok() && x.size() == 4);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	bool ismember = false;
	status = c.sismember(__func__, "1", ismember);
	cout << "sismember 1 " << status.code() << " ismember: " << (ismember ? "true" : "false") << endl;
	assert(status.ok() && !ismember);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	status = c.sismember(__func__, "2", ismember);
	cout << "sismember 2 " << status.code() << " ismember: " << (ismember ? "true" : "false") << endl;
	assert(status.ok() && ismember);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	count = 0;
	status = c.srem(__func__, x, &count);
	cout << "srem multi " << status.code() << " delcount " << count << endl;
	assert(status.ok() && count == 4);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	count = 0;
	status = c.scard(__func__, count);
	cout << "scard " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 0);

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;

	count = 0;
	status = c.delOtherKeyUnsafe(__func__, &count);
	cout << "delOtherKeyUnsafe 0 " << status.code() << " affected " << count << endl;
	assert(status.ok());

	status = c.type(__func__, type);
	cout << " *** type " << status.code() << " type: " << type << endl;
	assert(status.ok() && (type == "zset" || type == "none")); // redis is none; ramcloud is zset

}

void test_hash(Client& c)
{
	Status status;
	std::string type;

	status = c.hset(__func__, "1", "1");
	cout << "hset 1->1 " << status.code() << endl;
	assert(status.ok());

	std::map<std::string, std::string> x;
	x["2"] = "2"; x["3"] = "3"; x["4"] = "4"; x["5"] = "5";
	status = c.hmset(__func__, x);
	cout << "hmset 2 3 4 5 " << status.code() << endl;
	assert(status.ok());

	status = c.type(__func__, type);
	cout << "type " << status.code() << " type: " << type << endl;
	assert(status.ok() && type == "hash");


	int count = 0;
	status = c.hlen(__func__, count);
	cout << "hlen " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 5);

	count = 0;
	status = c.hdel(__func__, "1", &count);
	cout << "hdel 1 " << status.code() << " delcount " << count << endl;
	assert(status.ok() && count == 1);

	count = 0;
	status = c.hlen(__func__, count);
	cout << "hlen " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 4);

	x.clear();
	status = c.hgetall(__func__, x);
	cout << "hgetall " << status.code() << " count " << x.size() << endl;
	assert(status.ok() && x.size() == 4);
	__foreach(I, x)
	{
		cout << "        FIELD " << I->first << " " << I->second << endl;
	}

	std::string value;
	status = c.hget(__func__, "1", value);
	cout << "hget 1 " << status.code() << " " << value << endl;
	assert(status.ok() && status.not_found());

	value.clear();
	status = c.hget(__func__, "2", value);
	cout << "hget 2 " << status.code() << " " << value << endl;
	assert(status.ok() && value == "2");

	value.clear();
	status = c.hset(__func__, "2", "xxxx2");
	cout << "hset 2 " << status.code() << endl;
	assert(status.ok());

	value.clear();
	status = c.hget(__func__, "2", value);
	cout << "hget 2 " << status.code() << " " << value << endl;
	assert(status.ok() && value == "xxxx2");


	x.clear();
	x["2"] = "22"; x["3"] = "33"; x["4"] = "44"; x["5"] = "55";
	status = c.hmset(__func__, x);
	cout << "hmset " << status.code() << endl;
	assert(status.ok());

	value.clear();
	status = c.hget(__func__, "2", value);
	cout << "hget 2 " << status.code() << " " << value << endl;
	assert(status.ok() && value == "22");

	count = 0;
	std::set<std::string> toget;
	toget.insert("1"); toget.insert("2"); toget.insert("3"); x.clear();
	status = c.hmget(__func__, toget, x);
	assert(status.ok() && x.size() == 2);
	cout << "hmget " << status.code() << " count " << x.size() << endl;
	__foreach(I, x)
	{
		cout << "        FIELD " << I->first << " " << I->second << endl;
	}


	{
		int64_t newval = 0;
		status = c.hincrby(__func__, "3", 3, &newval);
		cout << "hincrby 3 3 " << status.code() << " newval " << newval << endl;
		assert(status.ok() && newval == 36);

		status = c.hincrby(__func__, "3", -300, &newval);
		cout << "hincrby 3 -300 " << status.code() << " newval " << newval << endl;
		assert(status.ok() && newval == -264);
	}

	std::set<string> fields; count = 0;
	fields.insert("1");
	fields.insert("2");
	fields.insert("3");
	fields.insert("4");
	fields.insert("5");
	status = c.hdel(__func__, fields, &count);
	assert(status.ok() && (count == 5 || count == 4)); // redis is 4; ramcloud is 5
	cout << "hdel " << status.code() << " affected " << count << endl;
	__foreach(I, fields)
	{
		cout << "        TODEL " << *I << endl;
	}

	count = 0;
	status = c.delOtherKeyUnsafe(__func__, &count);
	cout << "delOtherKeyUnsafe 0 " << status.code() << " affected " << count << endl;
	assert(status.ok() && (count == 1 || count == 0)); // redis is 0; ramcloud is 1

	status = c.type(__func__, type);
	cout << "type " << status.code() << " type: " << type << endl;
	assert(status.ok() && type == "none");
}

void test_zset(Client& c)
{
	Status status;
	std::string type;

	status = c.zadd(__func__, 1, "a");
	cout << "zadd a->1 " << status.code() << endl;
	assert(status.ok());

	std::map<std::string, double> x;
	x["b"] = 2; x["c"] = 3; x["d"] = 4; x["e"] = 5.6;
	status = c.zadd(__func__, x);
	cout << "zadd b c d e " << status.code() << endl;
	assert(status.ok());

	status = c.type(__func__, type);
	cout << "type " << status.code() << " type: " << type << endl;
	assert(status.ok() && type == "zset");

	status = c.type(__func__, type);
	cout << "type " << status.code() << " type: " << type << endl;

	int count = 0;
	status = c.zcount(__func__, 2, 4, count);
	cout << "zcount 2 4 " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 3);

	count = 0;
	status = c.zrem(__func__, "a", &count);
	cout << "zrem a " << status.code() << " delcount " << count << endl;
	assert(status.ok() && count == 1);

	std::set<string> membs;
	membs.insert("a");
	membs.insert("aa");
	membs.insert("aaa");
	count = 0;
	status = c.zrem(__func__, membs, &count);
	cout << "zrem (a,aa,aaa) " << status.code() << " delcount " << count << endl;
	assert(status.ok() && count == 0);

	count = 0;
	status = c.zcard(__func__, count);
	cout << "zcard " << status.code() << " count " << count << endl;
	assert(status.ok() && count == 4);

	x.clear();
	std::vector<std::string> members;
	std::vector<std::pair<std::string, double> > member_scores;
	members.clear();
	status = c.zrange(__func__, 0, -1, members);
	cout << "zrange 0 -1 " << status.code() << " count " << members.size() << endl;
	assert(status.ok() && members.size() == 4 && *members.begin() == "b");
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrange(__func__, 0, 1, members);
	assert(status.ok() && members.size() == 2);
	cout << "zrange 0 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrange(__func__, 1, 2, members);
	assert(status.ok() && members.size() == 2 && *members.begin() == "c");
	cout << "zrange 1 2 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrange(__func__, -1, -2, members);
	assert(status.ok() && members.empty());
	cout << "zrange -1 -2 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	member_scores.clear();
	status = c.zrangeWithScores(__func__, 0, -1, member_scores);
	assert(status.ok() && member_scores.size() == 4);
	cout << "zrangeWithScores 0 -1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangeWithScores(__func__, 0, 1, member_scores);
	assert(status.ok() && member_scores.size() == 2);
	cout << "zrangeWithScores 0 1 " << status.code() << " count " << member_scores.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangeWithScores(__func__, 1, 2, member_scores);
	assert(status.ok() && member_scores.size() == 2 && member_scores.begin()->second == 3);
	cout << "zrangeWithScores 1 2 " << status.code() << " count " << member_scores.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangeWithScores(__func__, -2, -1, member_scores);
	cout << "zrangeWithScores -2 -1 " << status.code() << " count " << members.size() << endl;
	assert(status.ok() && member_scores.size() == 2);
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	//////////
	members.clear();
	status = c.zrevrange(__func__, 0, -1, members);
	assert(status.ok() && members.size() == 4);
	cout << "zrevrange 0 -1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrange(__func__, 0, 1, members);
	assert(status.ok() && members.size() == 2);
	cout << "zrevrange 0 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrange(__func__, 1, 2, members);
	assert(status.ok() && members.size() == 2);
	cout << "zrevrange 1 2 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrange(__func__, -2, -1, members);
	cout << "zrevrange -2, -1 " << status.code() << " count " << members.size() << endl;
	assert(status.ok() && members.size() == 2);
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	member_scores.clear();
	status = c.zrevrangeWithScores(__func__, 0, -1, member_scores);
	assert(status.ok() && member_scores.size() == 4);
	cout << "zrevrangeWithScores 0 -1 " << status.code() << " count " << member_scores.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangeWithScores(__func__, 0, 1, member_scores);
	assert(status.ok() && member_scores.size() == 2);
	cout << "zrevrangeWithScores 0 1 " << status.code() << " count " << member_scores.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangeWithScores(__func__, 1, 2, member_scores);
	assert(status.ok() && member_scores.size() == 2);
	cout << "zrevrangeWithScores 1 2 " << status.code() << " count " << member_scores.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangeWithScores(__func__, -2, -1, member_scores);
	cout << "zrevrangeWithScores -2, -1 " << status.code() << " count " << member_scores.size() << endl;
	assert(status.ok() && member_scores.size() == 2);
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	///////////***************
	members.clear();
	status = c.zrangebyscore(__func__, "1", "4", members);
	assert(status.ok() && members.size() == 3);
	cout << "zrangebyscore 1 4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrangebyscore(__func__, "(2", "4", members);
	cout << "zrangebyscore (2 4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}
	assert(status.ok() && members.size() == 2 && *members.begin() == "c");

	members.clear();
	status = c.zrangebyscore(__func__, "1", "(4", members);
	cout << "zrangebyscore 1 (4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}
	assert(status.ok() && members.size() == 2 && *members.begin() == "b");

	members.clear();
	status = c.zrangebyscore(__func__, "(2", "(4", members);
	cout << "zrangebyscore (2 (4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}
	assert(status.ok() && members.size() == 1 && *members.begin() == "c");

	member_scores.clear();
	status = c.zrangebyscoreWithScores(__func__, "1", "4", member_scores);
	cout << "zrangebyscoreWithScores 1 4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangebyscoreWithScores(__func__, "(1", "4", member_scores);
	cout << "zrangebyscoreWithScores  1 4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangebyscoreWithScores(__func__, "1", "(4", member_scores);
	cout << "zrangebyscoreWithScores 1 (4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrangebyscoreWithScores(__func__, "(1", "(4", member_scores);
	cout << "zrangebyscoreWithScores (1 (4 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	//////////
	members.clear();
	status = c.zrevrangebyscore(__func__, "4", "1", members);
	cout << "zrevrangebyscore 4 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrangebyscore(__func__, "(4", "1", members);
	cout << "zrevrangebyscore (4 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrangebyscore(__func__, "4", "(1", members);
	cout << "zrevrangebyscore 4 (1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	members.clear();
	status = c.zrevrangebyscore(__func__, "(4", "(1", members);
	cout << "zrevrangebyscore (4 (1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, members)
	{
		cout << "        FIELD " << *I << endl;
	}

	member_scores.clear();
	status = c.zrevrangebyscoreWithScores(__func__, "4", "1", member_scores);
	cout << "zrevrangebyscoreWithScores 4 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangebyscoreWithScores(__func__, "(4", "1", member_scores);
	cout << "zrevrangebyscoreWithScores 4 1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangebyscoreWithScores(__func__, "4", "(1", member_scores);
	cout << "zrevrangebyscoreWithScores 4 (1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	member_scores.clear();
	status = c.zrevrangebyscoreWithScores(__func__, "(4", "(1", member_scores);
	cout << "zrevrangebyscoreWithScores (4 (1 " << status.code() << " count " << members.size() << endl;
	__foreach(I, member_scores)
	{
		cout << "        FIELD " << I->second << " " << I->first << endl;
	}

	/////////***********
	double score = 0;
	status = c.zscore(__func__, "a", score);
	cout << "zscore a  " << status.code() << " score " << score << endl;
	assert(status.ok() && status.not_found());

	score = 0;
	status = c.zscore(__func__, "b", score);
	cout << "zscore b  " << status.code() << " score " << score << endl;
	assert(status.ok() && score == 2);

	score = 0;
	status = c.zadd(__func__, 12345678.5678, "b");
	cout << "zscore zadd  " << status.code() << endl;

	score = 0;
	status = c.zscore(__func__, "b", score);
	cout << "zscore b  " << status.code() << " score " << score << endl;
}

// 测试直连 redis
void redis_test(anka::IConfigObserver *ob)
{
	anka::redis::init(ob);
	anka::redis::Client cache("redis0");

	test_del(cache);
	test_wrong_type(cache);
	test_getset(cache);
	test_incrby(cache);
	test_set(cache);
	test_hash(cache);
	test_zset(cache);

	anka::redis::fini();
}

void ramcloud_test(anka::IConfigObserver * ob)
{
	anka::ramcloud::init(ob);
	anka::ramcloud::Client cache;

	test_del(cache);
	test_wrong_type(cache);
	test_getset(cache);
	test_incrby(cache);
	test_set(cache);
	test_hash(cache);
	test_zset(cache);

	anka::ramcloud::fini();
}

// 测试多线程
void * thread_func(void *)
{
	return 0;
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	MockConfig conf;

	redis_test(&conf);
	ramcloud_test(&conf);

	return 0;
}
