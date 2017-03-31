#ifndef REDISCLIENTYAF_H
#define REDISCLIENTYAF_H


#include "RedisClient.h"
#include "logger.h"

#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

#include <stdarg.h>
#include <stdint.h>
#include <time.h>


namespace _internal {
	inline int _atom_inc(int& v) { return __sync_add_and_fetch(&v, 1); }
	inline int _atom_dec(int& v) { return __sync_sub_and_fetch(&v, 1); }
	inline int _atom_finc(int& v) { return __sync_fetch_and_add(&v, 1); }
	inline int _atom_fdec(int& v) { return __sync_fetch_and_sub(&v, 1); }
	inline int _atom_get(int& v) { return __sync_fetch_and_or(&v, 0); }

	class SharedCount
	{
	public:
		SharedCount() : c(1) {}
		void addRef() { _atom_inc(c); }
		int release() { return _atom_dec(c); }
		int get() { return _atom_get(c); }
	private:
		int c;
	};

	// RedisClientPtr is a smart pointer encapsulate RedisClient*
	class RedisClientPtr
	{
	public:
		explicit RedisClientPtr(RedisClient* _p = 0)
			: p(_p), c(new SharedCount) {
			//printf("RedisClientPtr create %p, %d\n", (void*)p, c->get());
			logth(Info, "RedisClientPtr create %p, %d\n", (void*)p, c->get());
		}
		~RedisClientPtr() {
			//printf("RedisClientPtr releas %p, %d\n", (void*)p, c->get());
			//logth(Info, "RedisClientPtr releas %p, %d\n", (void*)p, c->get());
			if (c->release() == 0) {
				//printf("RedisClientPtr destro %p, %d\n", (void*)p, c->get());
				logth(Info, "RedisClientPtr destro %p, %d\n", (void*)p, c->get());
				delete p;
				delete c;
			}
		}

		RedisClientPtr(const RedisClientPtr& other)
			: p(other.p), c(other.c) {
			c->addRef();
			//printf("RedisClientPtr copied %p, %d\n", (void*)p, c->get());
			//logth(Info, "RedisClientPtr copied %p, %d\n", (void*)p, c->get());
		}
		RedisClientPtr& operator=(RedisClientPtr other) {
			this->swap(other);
			return *this;
		}
		void swap(RedisClientPtr& other) {
			std::swap(p, other.p);
			std::swap(c, other.c);
		}

		bool notNull() const { return (p != 0); }
		bool isNull() const { return (p == 0); }
		RedisClient* get() const { return p; }
		RedisClient* operator->() const { return p; }
		RedisClient& operator*() const { return *p; }

		int getrc() const { return c->get(); }
	private:
		RedisClient* p;
		SharedCount* c;
	};
} // namespace _internal


namespace anka {

	class IConfigObserver;

	namespace ramcloud {

		void init(IConfigObserver *, void * fwk = 0);
		void fini();

		// 命令执行状态
		class Status
		{
		public:
			// 命令成功执行，或者KEY没有找到，都认为本次操作是成功的
			bool ok() const { return code_ == "ok" || code_ == "not_found"; }
			bool not_found() const { return code_ == "not_found"; }
			std::string code() const { return code_; }
			void code(const std::string & c) { code_ = c; }
			Status() :code_("") {}
			Status(const std::string &code) { code_ = code; }
			Status(const Status & s) { code_ = s.code_;	}
		private:
			std::string code_;
		};

		int64_t microsec(); // 当前时间戳，微秒

		class Command
		{
		private:
			int64_t start_;
			std::string command_;
		public:
			explicit Command(const std::string & command) { start_ = microsec(); command_ = command; }
			int64_t duration() const { return microsec() - start_; }
			const char * c_str() const { return command_.c_str(); }
			std::string::size_type length() const { return command_.length(); }
			const std::string & str() const { return command_; }
		};

		class Client
		{  // 线程安全
		public:
			Client(const std::string& dbname = "default");
			virtual ~Client();

			// 命令
			// 生命期控制
			virtual Status type(const std::string & key, std::string & type);
			virtual Status delStringKey(const std::string & key, int * affected = 0);
			virtual Status delStringKey(const std::set<std::string> & keys, int * affected = 0);
			virtual Status delOtherKeyUnsafe(const std::string & key, int * affected = 0);
			virtual Status expire(const std::string & key, unsigned ttl, int * affected = 0);
			virtual Status ttl(const std::string &key, int & ttl);
			virtual Status persist(const std::string & key, int *affected = 0);

			// key-string
			virtual Status set(const std::string & key, const std::string & val);
			virtual Status setex(const std::string & key, const std::string & val, int ttl);
			virtual Status mset(const std::map<std::string, std::string> & kvs); // 不是原子操作
			virtual Status get(const std::string & key, std::string & val);
			virtual Status mget(const std::set<std::string> & keys, std::map<std::string, std::string> & vals);
			virtual Status incrby(const std::string & key, int64_t increment, int64_t * newval = 0);
			// add for redisHelper
			virtual Status setMapValue(const std::string & key, const std::map<uint32_t, std::string> & val, int ttl = -1);
			virtual Status getMapValue(const std::string & key, std::map<uint32_t, std::string>& value, bool& keyExist);

			// set
			virtual Status scard(const std::string & key, int & count);
			virtual Status sadd(const std::string & key, const std::string & member);
			virtual Status sadd(const std::string & key, const std::set<std::string> & members);
			virtual Status srem(const std::string & key, const std::string & member, int * delcount = 0);
			virtual Status srem(const std::string & key, const std::set<std::string> & members, int * delcount = 0);
			virtual Status smembers(const std::string & key, std::set<std::string> & members);
			virtual Status sismember(const std::string & key, const std::string & member, bool & ismember);

			// hash
			virtual Status hdel(const std::string & key, const std::string &field, int * delcount = 0);
			virtual Status hdel(const std::string & key, const std::set<std::string> &field, int * delcount = 0);
			virtual Status hset(const std::string & key, const std::string & field, const std::string & val);
			virtual Status hmset(const std::string & key, const std::map<std::string, std::string> & field2val);
			virtual Status hget(const std::string & key, const std::string & field, std::string & val);
			virtual Status hmget(const std::string & key, const std::set<std::string> & fields, std::map<std::string, std::string> & field2val);
			virtual Status hgetall(const std::string & key, std::map<std::string, std::string> & allfield);
			virtual Status hlen(const std::string & key, int & fieldcount);
			virtual Status hincrby(const std::string & key, const std::string & field, int64_t increment, int64_t * newval = 0);

			virtual Status zcard(const std::string & key, int & count);
			virtual Status zadd(const std::string & key, double score, const std::string & member);
			virtual Status zadd(const std::string & key, const std::map<std::string, double> & member2score);
			virtual Status zrem(const std::string & key, const std::string & member, int * delcount = 0);
			virtual Status zrem(const std::string & key, const std::set<std::string> & members, int * delcount = 0);
			virtual Status zscore(const std::string & key, const std::string & member, double & score);
			virtual Status zrembyscore(const std::string& key, const int min, const int max);
			virtual Status zcount(const std::string & key, double minScore, double maxScore, int & count);
			virtual Status zrevrank(const std::string& key, const std::string& member, int* rank);
			virtual Status zrank(const std::string& key, const std::string& member, int& rank);
			virtual Status zrange(const std::string & key, int startIndex, int stopIndex, std::vector<std::string> & members);
			virtual Status zrangeWithScores(const std::string & key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members);
			virtual Status zrevrange(const std::string & key, int startIndex, int stopIndex, std::vector<std::string> & members);
			virtual Status zrevrangeWithScores(const std::string & key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members);
			virtual Status zincrby(const std::string& key, const std::string& field, int64_t increment, int64_t& newval);
			virtual Status zrangebyscore(const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::string> & members);
			virtual Status zrangebyscoreWithScores(const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::pair<std::string, double> > & members);
			virtual Status zrevrangebyscore(const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::string> & members);
			virtual Status zrevrangebyscoreWithScores(const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::pair<std::string, double> > & members);

		protected:
			Status zrangbyscore_impl(const std::string & cmd, const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::string> & members);
			Status zrangbyscoreWithScores_impl(const std::string & cmd, const std::string & key, const std::string & score1, const std::string & score2, std::vector<std::pair<std::string, double> > & members);
			Status zrange_impl(const std::string & command, const std::string & key, int startIndex, int stopIndex, std::vector<std::string> & members);
			Status zrangeWithScores_impl(const std::string & command, const std::string & key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members);

		protected:
			std::string dbname;  // 当前使用的实例名，默认为 default
			_internal::RedisClientPtr client;  // 当前使用哪个实例，延迟初始化
			virtual void _init();

			virtual RedisReplyPtr myRedisCommand(const char *format, ...);
			virtual RedisReplyPtr myRedisvCommand(const char *format, va_list ap);
			virtual RedisReplyPtr myRedisCommandArgv(int argc, const char **argv, const size_t *argvlen);

			static Status reply2status(const RedisReplyPack* p);
			static std::string reply2string(const redisReply* reply);
			static int64_t reply2integer(const redisReply* reply);
			static void reportStatus(const RedisReplyPack* p, const Command& command, const Status& status);
			static void appendArgs(const std::set<std::string> & coll, std::vector<const char *> &args, std::vector<size_t> &lens);
		};

	} // namespace ramcloud

	namespace redis {

		void init(IConfigObserver *, void * fwk = 0);
		void fini();

		using ramcloud::Status;
		using ramcloud::Command;

		class Client : public ramcloud::Client
		{  // 线程安全
		public:
			Client(const std::string& dbname = "default");
		protected:
			virtual void _init();
		};

	} // namespace redis

} // namespace anka


#endif//REDISCLIENTYAF_H
