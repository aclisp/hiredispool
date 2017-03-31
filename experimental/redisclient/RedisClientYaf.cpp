#include "RedisClientYaf.h"
#include "IConfig.h"
#include "IS2SWatcher.h"
#include "framework/anka.h"
#include "logger.h"

#include <string>
#include <map>
#include <exception>
#include <vector>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>

#include "common/packet.h"


#if !defined(__foreach) && !defined(__const_iterator_def)
#define __const_iterator_def(Iterator, Initializer) __typeof(Initializer) Iterator=(Initializer)
#define __foreach(Iterator, Container) for(__const_iterator_def(Iterator, (Container).begin()); Iterator!=(Container).end(); ++Iterator)
#endif


using namespace std;
using _internal::RedisClientPtr;


namespace {
	// Return an IP address in standard dot notation
	char *ip_ntoa(char *buffer, uint32_t ipaddr)
	{
		ipaddr = ntohl(ipaddr);

		sprintf(buffer, "%d.%d.%d.%d",
			(ipaddr >> 24) & 0xff,
			(ipaddr >> 16) & 0xff,
			(ipaddr >>  8) & 0xff,
			(ipaddr      ) & 0xff);
		return buffer;
	}

	// Return an IP address from one supplied in standard dot notation.
	uint32_t ip_addr(const char *ip_str)
	{
		struct in_addr  in;

		if (inet_aton(ip_str, &in) == 0)
			return htonl(INADDR_NONE);
		return in.s_addr;
	}
}

namespace anka {

	namespace ramcloud {

		class Config : public anka::IConfigWatcher, public anka::IS2SWatcher
		{
		private:
			// non construct copyable and non copyable
			Config(const Config&);
			Config& operator=(const Config&);
		protected:
			class ScopedLock {
			public:
				ScopedLock(pthread_mutex_t& _mutex) : mutex(_mutex) {
					pthread_mutex_lock(&mutex);
				}
				~ScopedLock() {
					pthread_mutex_unlock(&mutex);
				}
			private:
				pthread_mutex_t& mutex;
			};
			class Meta
			{
			public:
				Meta() {
					memset(&conf, 0, sizeof(REDIS_CONFIG));
				}
				Meta(const string& _key, const string& _dbname)
					: key(_key), dbname(_dbname) {
					memset(&conf, 0, sizeof(REDIS_CONFIG));
				}
				const REDIS_CONFIG& toConf() {
					if (!endpoints.empty()) {
						conf.num_endpoints = endpoints.size();
						conf.endpoints = &endpoints[0];
					} else {
						conf.num_endpoints = 0;
						conf.endpoints = NULL;
						logth(Error, "%s.%s: no endpoint", key.c_str(), dbname.c_str());
					}
					return conf;
				}
				void addEndpoint(const REDIS_ENDPOINT& ep) {
					for (vector<REDIS_ENDPOINT>::iterator it = endpoints.begin(); it != endpoints.end(); ++it) {
						const REDIS_ENDPOINT& eq = *it;
						if (strcmp(eq.host, ep.host) == 0 && eq.port == ep.port)
							return;
					}
					endpoints.push_back(ep);
				}
				void removeEndpoint(const REDIS_ENDPOINT& ep) {
					for (vector<REDIS_ENDPOINT>::iterator it = endpoints.begin(); it != endpoints.end(); ) {
						const REDIS_ENDPOINT& eq = *it;
						if (strcmp(eq.host, ep.host) == 0 && eq.port == ep.port)
							it = endpoints.erase(it);
						else
							++it;
					}
				}
				void setConf(const REDIS_CONFIG& cf) {
					conf = cf;
				}
			private:
				string key;
				string dbname;
				REDIS_CONFIG conf;
				vector<REDIS_ENDPOINT> endpoints;
			public:
				string s2sname;
				uint32_t groupId;
			};
		public:
			Config(const string& _key) : key(_key) {
				pthread_mutex_init(&metaMutex, NULL);
				pthread_mutex_init(&instMutex, NULL);
			}
			virtual ~Config() {
				{ ScopedLock l(instMutex); instMap.clear(); }
				{ ScopedLock l(metaMutex); metaMap.clear(); }
				pthread_mutex_destroy(&instMutex);
				pthread_mutex_destroy(&metaMutex);
			}
			RedisClientPtr getRedisClient(const std::string& dbname);
			virtual void KeyValueConfigUpdate(const string& _key, const string& value) {}
			virtual void MapValuesConfigUpdate(const string& _key, const map<string, map<string, string> > &key_values);
			virtual void onAddServer(const PServer &server);
			virtual void onRemoveServer(const PServer &server);
			virtual void onUpdateServer(const PServer &server) {}
			virtual void onRegistered(const PServer& server) {}
			virtual void onPollFinished() {}
		protected:
			virtual void convert(const map<string, string>& attrs, Meta& meta);
			RedisClientPtr findInst(const std::string& dbname) {
				ScopedLock l(instMutex);
				map<string, RedisClientPtr>::const_iterator it = instMap.find(dbname);
				if (it != instMap.end()) {
					return it->second;
				}
				return RedisClientPtr(NULL);
			}
			bool findMeta(const std::string& dbname, Meta& meta) {
				ScopedLock l(metaMutex);
				map<string, Meta>::const_iterator it = metaMap.find(dbname);
				if (it != metaMap.end()) {
					meta = it->second;
					return true;
				}
				logth(Error, "%s.%s: not found", key.c_str(), dbname.c_str());
				return false;
			}
			RedisClientPtr createInst(const std::string& dbname, Meta meta) {
				ScopedLock l(instMutex);
				map<string, RedisClientPtr>::const_iterator it = instMap.find(dbname);
				if (it != instMap.end()) {
					return it->second;
				}
				try	{
					RedisClientPtr c(new RedisClient(meta.toConf()));
					instMap[dbname] = c;
					logth(Info, "%s.%s: create", key.c_str(), dbname.c_str());
					return c;
				} catch (const std::exception& e) {
					logth(Error, "%s.%s: can not create, check if your config is good?", key.c_str(), dbname.c_str());
					return RedisClientPtr(NULL);
				}
			}
			void deleteInst(const std::string& dbname) {
				ScopedLock l(instMutex);
				instMap.erase(dbname);
			}
			void addMeta(const std::string& dbname, const Meta& meta) {
				ScopedLock l(metaMutex);
				if (metaMap.find(dbname) != metaMap.end()) {
					logth(Error, "%s.%s: can not update, already exist", key.c_str(), dbname.c_str());
					return;
				}
				metaMap[dbname] = meta;
				logth(Info, "%s.%s: add", key.c_str(), dbname.c_str());
			}
		public:
			string key;  // could be "ramcloudsources" or "redisources"
		protected:
			map<string, Meta> metaMap;  // dbname -> Meta
			pthread_mutex_t metaMutex;
			map<string, RedisClientPtr> instMap;  // dbname -> ConnectionPool
			pthread_mutex_t instMutex;
		};

		void Config::MapValuesConfigUpdate(const string& _key, const map<string, map<string, string> > &key_values)
		{
			map<string, map<string, string> >::const_iterator it = key_values.begin();
			for (; it != key_values.end(); ++it) {
				const string& dbname = it->first;
				const map<string, string>& attrs = it->second;
				Meta meta(key, dbname);
				convert(attrs, meta);
				addMeta(dbname, meta);
			}
		}

		void Config::onAddServer(const PServer &server)
		{
			ScopedLock l(metaMutex);
			for (map<string, Meta>::iterator it = metaMap.begin(); it != metaMap.end(); ++it) {
				const string& dbname = it->first;
				Meta& meta = it->second;
				if (meta.s2sname != server.name)
					continue;
				if (meta.groupId != server.groupId)
					continue;
				uint32_t dip = server.getIp(anka::CTL);
				REDIS_ENDPOINT ep;
				ip_ntoa(ep.host, dip);
				ep.port = server.listenPort;
				meta.addEndpoint(ep);
				deleteInst(dbname);
				logth(Info, "%s.%s: %s: %s(%d) %s:%d", key.c_str(), dbname.c_str(), __func__, server.name.c_str(), server.groupId, ep.host, ep.port);
			}
		}

		void Config::onRemoveServer(const PServer &server)
		{
			ScopedLock l(metaMutex);
			for (map<string, Meta>::iterator it = metaMap.begin(); it != metaMap.end(); ++it) {
				const string& dbname = it->first;
				Meta& meta = it->second;
				if (meta.s2sname != server.name)
					continue;
				if (meta.groupId != server.groupId)
					continue;
				uint32_t dip = server.getIp(anka::CTL);
				REDIS_ENDPOINT ep;
				ip_ntoa(ep.host, dip);
				ep.port = server.listenPort;
				meta.removeEndpoint(ep);
				deleteInst(dbname);
				logth(Info, "%s.%s: %s: %s(%d) %s:%d", key.c_str(), dbname.c_str(), __func__, server.name.c_str(), server.groupId, ep.host, ep.port);
			}
		}

		void Config::convert(const map<string, string>& attrs, Meta& meta)
		{
			REDIS_ENDPOINT ep;
			REDIS_CONFIG cf;

			// setup defaults
			cf.connect_timeout = 500;
			cf.net_readwrite_timeout = 500;
			cf.num_redis_socks = 100;
			cf.connect_failure_retry_delay = 1;

			// override with attrs
			if (attrs.find("host") != attrs.end() && attrs.find("port") != attrs.end()) {
				strcpy(ep.host, attrs.find("host")->second.c_str());
				ep.port = atoi(attrs.find("port")->second.c_str());
				meta.addEndpoint(ep);
			}
			if (attrs.find("backup_host") != attrs.end() && attrs.find("backup_port") != attrs.end()) {
				strcpy(ep.host, attrs.find("backup_host")->second.c_str());
				ep.port = atoi(attrs.find("backup_port")->second.c_str());
				meta.addEndpoint(ep);
			}
			if (attrs.find("connect_timeout") != attrs.end()) {
				cf.connect_timeout = atoi(attrs.find("connect_timeout")->second.c_str());
			}
			if (attrs.find("net_readwrite_timeout") != attrs.end()) {
				cf.net_readwrite_timeout = atoi(attrs.find("net_readwrite_timeout")->second.c_str());
			}
			if (attrs.find("num_redis_socks") != attrs.end()) {
				cf.num_redis_socks = atoi(attrs.find("num_redis_socks")->second.c_str());
			}
			if (attrs.find("connect_failure_retry_delay") != attrs.end()) {
				cf.connect_failure_retry_delay = atoi(attrs.find("connect_failure_retry_delay")->second.c_str());
			}

			meta.setConf(cf);

			// setup S2S defaults
			meta.s2sname = "";
			meta.groupId = 0;

			// override S2S with attrs
			if (attrs.find("s2sname") != attrs.end()) {
				meta.s2sname = attrs.find("s2sname")->second;
			}
			if (attrs.find("groupId") != attrs.end()) {
				meta.groupId = atoi(attrs.find("groupId")->second.c_str());
			}
		}

		RedisClientPtr Config::getRedisClient(const std::string& dbname)
		{
			RedisClientPtr c = findInst(dbname);
			if (c.notNull())
				return c;

			Meta meta;
			if (!findMeta(dbname, meta))
				return c;

			return createInst(dbname, meta);
		}

		static Config CONFIG("ramcloudsources");

		// For mock test only
		void _injectAddServer()
		{
			PServer server;
			server.name = "mock";
			server.groupId = 100;
			server.listenPort = 9403;

			server.isp2Ip[anka::CTL] = ip_addr("106.38.198.210");
			CONFIG.onAddServer(server);
			server.isp2Ip[anka::CTL] = ip_addr("106.120.184.86");
			CONFIG.onAddServer(server);
		}
		void _injectRemoveServer()
		{
			PServer server;
			server.name = "mock";
			server.groupId = 100;
			server.listenPort = 9403;

			server.isp2Ip[anka::CTL] = ip_addr("106.38.198.210");
			CONFIG.onRemoveServer(server);
			//server.isp2Ip[anka::CTL] = ip_addr("106.120.184.86");
			//CONFIG.onRemoveServer(server);
		}

		void init(anka::IConfigObserver *ob, void * fwk)
		{
			ob->RegisterWatcher(CONFIG.key, &CONFIG);
			if (fwk) {
				((anka::Framework*) fwk)->watchS2S(&CONFIG);
				//printf(" *** watchS2S at %s init\n", CONFIG.key.c_str());
			}
		}

		void fini()
		{
		}

		inline int64_t microsec() // 当前时间戳，微秒
		{
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			gettimeofday(&tv, 0);
			return (int64_t)tv.tv_sec * 1000 * 1000 + tv.tv_usec;
		}

		Client::Client(const std::string& _dbname)
			: dbname(_dbname) , client(NULL)
		{
		}

		Client::~Client()
		{
		}

		void Client::_init()
		{
			client = CONFIG.getRedisClient(dbname);
			//printf("%s.%s: _init done. %p, %d\n", CONFIG.key.c_str(), dbname.c_str(), (void*)client.get(), client.getrc());
			logth(Info, "%s.%s: _init done. %p, %d", CONFIG.key.c_str(), dbname.c_str(), (void*)client.get(), client.getrc());
		}

		RedisReplyPtr Client::myRedisCommand(const char *format, ...)
		{
			_init(); if (client.isNull()) return RedisReplyPtr();
			va_list ap;
			RedisReplyPtr reply;
			va_start(ap, format);
			reply = client->redisvCommand(format, ap);
			va_end(ap);
			return reply;
		}

		RedisReplyPtr Client::myRedisvCommand(const char *format, va_list ap)
		{
			_init(); if (client.isNull()) return RedisReplyPtr();
			return client->redisvCommand(format, ap);
		}

		RedisReplyPtr Client::myRedisCommandArgv(int argc, const char **argv, const size_t *argvlen)
		{
			_init(); if (client.isNull()) return RedisReplyPtr();
			return client->redisCommandArgv(argc, argv, argvlen);
		}

		Status Client::reply2status(const RedisReplyPack* p)
		{
			if (!p)
				return Status("CONFIG_ERR");
			const redisReply* reply = p->reply;
			if (reply == 0) {
				switch (p->info.err) {
				case REDIS_ERR_IO:
					return Status("REDIS_ERR_IO " + p->info.errstr);
				case REDIS_ERR_EOF:
					return Status("REDIS_ERR_EOF " + p->info.errstr);
				case REDIS_ERR_PROTOCOL:
					return Status("REDIS_ERR_PROTOCOL " + p->info.errstr);
				case REDIS_ERR_OOM:
					return Status("REDIS_ERR_OOM " + p->info.errstr);
				case REDIS_ERR_OTHER:
					return Status("REDIS_ERR_OTHER " + p->info.errstr);
				case 0:
					return Status("NO_ERR but reply is null?!");
				default:
					return Status("ERR " + p->info.errstr);
				}
			}
			else {
				switch (reply->type) {
				case REDIS_REPLY_ERROR:
					return Status(reply->str);
				case REDIS_REPLY_NIL:
					return Status("not_found");
				default:
					return Status("ok");
				}
			}
		}

		std::string Client::reply2string(const redisReply* reply)
		{
			if (!reply) return "";
			if (reply->type == REDIS_REPLY_STRING && reply->str)
				return std::string(reply->str, reply->len);
			if (reply->type == REDIS_REPLY_STATUS && reply->str)
				return std::string(reply->str, reply->len);
			return "";
		}

		int64_t Client::reply2integer(const redisReply* reply)
		{
			if (!reply) return 0;
			if (reply->type == REDIS_REPLY_INTEGER)
				return reply->integer;
			return 0;
		}

		void Client::reportStatus(const RedisReplyPack* p, const Command& command, const Status& status)
		{
			// TODO 上报metircs
			logth(Info, "%-16s/%-15s_%-5d %-10s, cost(us) %10lld",
				command.c_str(),
				p ? p->info.target.host.c_str() : "",
				p ? p->info.target.port : 0,
				status.code().c_str(),
				command.duration());
		}

		void Client::appendArgs(const std::set<std::string>& coll, std::vector<const char*>& args, std::vector<size_t>& lens)
		{
			__foreach(it, coll)
			{
				args.push_back((*it).c_str());
				lens.push_back((*it).length());
			}
		}

		// 读取key的类型： none, string, set, zset, hash
		Status Client::type(const std::string& key, std::string &type)
		{
			Command command("TYPE");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			type = reply2string(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 删除string类型的KEY，其他类型无效（zset set hash）
		Status Client::delStringKey(const std::string& key, int *affected)
		{

			Command command("DEL");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (affected) *affected = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 删除string类型的KEY，其他类型无效（zset set hash）
		Status Client::delStringKey(const std::set<std::string>& keys, int* affected)
		{
			Command command("DEL");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + keys.size());
			lens.reserve(1 + keys.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());

			appendArgs(keys, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (affected) *affected = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 删除set、hash、zset，注意风险，在并发写的时候，可能删不掉！！！
		Status Client::delOtherKeyUnsafe(const std::string & key, int * affected)
		{
			return expire(key, 1, affected);
		}

		// 设置key超时时间
		Status Client::expire(const std::string& key, unsigned ttl, int *affected)
		{
			Command command("EXPIRE");

			RedisReplyPtr reply = myRedisCommand("%s %s %u", command.c_str(), key.c_str(), ttl);
			Status status = reply2status(reply);
			if (affected) *affected = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 获取key的超时时间
		// ttl=-1: 没有设置超时
		// ttl=-2: key不存在，status.code() == "not_found"
		Status Client::ttl(const std::string& key, int& ttl)
		{
			Command command("TTL");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (status.ok()) {
				ttl = reply2integer(reply);
				if (ttl == -2) { // key不存在
					status.code("not_found");
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 移除key的超时时间
		Status Client::persist(const std::string & key, int * affected)
		{
			Command command("PERSIST");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (affected) *affected = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 写入string
		Status Client::set(const std::string& key, const std::string& val)
		{
			Command command("SET");

			RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), val.data(), val.size());
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::setMapValue(const std::string & key, const std::map<uint32_t, std::string> & val, int ttl)
		{
			try {
				if (ttl < 0) {
					Command command("SET");

					sox::PackBuffer buffer;
					sox::Pack p(buffer);
					sox::marshal_container(p, val);

					RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), p.data(), p.size());
					Status status = reply2status(reply);
					reportStatus(reply, command, status);
					return status;
				}
				else {
					Command command("SETEX");

					sox::PackBuffer buffer;
					sox::Pack p(buffer);
					sox::marshal_container(p, val);

					RedisReplyPtr reply = myRedisCommand("%s %s %d %b", command.c_str(), key.c_str(), ttl, p.data(), p.size());
					Status status = reply2status(reply);
					reportStatus(reply, command, status);
					return status;
				}
			}
			catch (const std::exception& e) {
				logth(Error, "[Client::setMapValue] %s: got exception %s", dbname.c_str(), e.what());
				return Status(e.what());
			}
		}

		Status Client::getMapValue(const std::string & key, std::map<uint32_t, std::string>& value, bool& keyExist)
		{
			try {
				Command command("GET");

				RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
				Status status = reply2status(reply);
				keyExist = !status.not_found();

				if (reply.notNull()) {
					if (reply->type == REDIS_REPLY_STRING) {
						try {
							sox::Unpack up(reply->str, reply->len);
							sox::unmarshal_container(up, inserter(value, value.begin()));
							keyExist = true;
						}
						catch (sox::UnpackError&) {  // 必须捕捉解包异常，并当成key不存在处理
							logth(Error, "[Client::getMapValue] %s: ill-formed cache data in key '%s' found.", dbname.c_str(), key.c_str());
							keyExist = false;
						}
					}
				}

				reportStatus(reply, command, status);
				return status;
			}
			catch (const std::exception& e) {
				logth(Error, "[Client::getMapValue] %s: got exception %s", dbname.c_str(), e.what());
				return Status(e.what());
			}
		}

		// 写入string并设置超时时间
		Status Client::setex(const std::string& key, const std::string& val, int ttl)
		{
			Command command("SETEX");

			RedisReplyPtr reply = myRedisCommand("%s %s %d %b", command.c_str(), key.c_str(), ttl, val.data(), val.size());
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量写入string
		Status Client::mset(const std::map<std::string, std::string>& kvs)
		{
			Command command("MSET");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + kvs.size() * 2);
			lens.reserve(1 + kvs.size() * 2);
			args.push_back(command.c_str());
			lens.push_back(command.length());

			__foreach(I, kvs)
			{
				args.push_back(I->first.c_str());
				lens.push_back(I->first.size());

				args.push_back(I->second.c_str());
				lens.push_back(I->second.length());
			}

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取string
		Status Client::get(const std::string& key, std::string& val)
		{
			Command command("GET");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			val = reply2string(reply);
			reportStatus(reply, command, status);
			return status;
		}

		//批量读取string
		Status Client::mget(const std::set<std::string>& keys, std::map<std::string, std::string>& vals)
		{
			Command command("MGET");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + keys.size());
			lens.reserve(1 + keys.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());

			appendArgs(keys, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				uint32_t i = 0;
				__foreach(I, keys)
				{
					if (reply->element[i]->type == REDIS_REPLY_STRING)
					{
						vals[*I].assign(reply->element[i]->str, reply->element[i]->len);
					}
					++i;
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 原子增加string的数值
		Status Client::incrby(const std::string& key, int64_t increment, int64_t * newval)
		{
			Command command("INCRBY");

			RedisReplyPtr reply = myRedisCommand("%s %s %lld", command.c_str(), key.c_str(), increment);
			Status status = reply2status(reply);
			if (newval) *newval = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取set元素个数
		Status Client::scard(const std::string& key, int& count)
		{
			Command command("SCARD");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (status.ok()) count = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 给set添加成员
		Status Client::sadd(const std::string& key, const std::string& member)
		{
			Command command("SADD");

			RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), member.data(), member.size());
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量给set添加成员
		Status Client::sadd(const std::string& key, const std::set<std::string>& members)
		{
			Command command("SADD");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + 1 + members.size());
			lens.reserve(1 + 1 + members.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			appendArgs(members, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 从set删除元素
		Status Client::srem(const std::string& key, const std::string& member, int* delcount)
		{
			Command command("SREM");

			RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), member.data(), member.size());
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 从set批量删除元素
		Status Client::srem(const std::string& key, const std::set<std::string>& members, int* delcount)
		{
			Command command("SREM");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + 1 + members.size());
			lens.reserve(1 + 1 + members.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			appendArgs(members, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取set所有成员
		Status Client::smembers(const std::string& key, std::set<std::string>& members)
		{
			Command command("SMEMBERS");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (status.ok())
			{
				for (unsigned i = 0; i < reply->elements; ++i)
				{
					members.insert(std::string(reply->element[i]->str, reply->element[i]->len));
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 判断member是否为key的成员
		Status Client::sismember(const std::string& key, const std::string& member, bool& ismember)
		{
			Command command("SISMEMBER");

			RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), member.data(), member.size());
			Status status = reply2status(reply);
			if (status.ok())
			{
				ismember = (reply2integer(reply) == 1 ? true : false);
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 删除hash的字段
		Status Client::hdel(const std::string& key, const std::string& field, int* delcount)
		{
			Command command("HDEL");

			RedisReplyPtr reply = myRedisCommand("%s %s %s", command.c_str(), key.c_str(), field.c_str());
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量删除hash的字段
		Status Client::hdel(const std::string& key, const std::set<std::string>& field, int* delcount)
		{
			Command command("HDEL");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(2 + field.size());
			lens.reserve(2 + field.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			appendArgs(field, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		//设置hash字段属性
		Status Client::hset(const std::string& key, const std::string& field, const std::string& val)
		{
			Command command("HSET");

			RedisReplyPtr reply = myRedisCommand("%s %s %s %b", command.c_str(), key.c_str(), field.c_str(), val.data(), val.size());
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量设置hash字段属性
		Status Client::hmset(const std::string& key, const std::map<std::string, std::string>& field2val)
		{
			Command command("HMSET");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(2 + 2 * field2val.size());
			lens.reserve(2 + 2 * field2val.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			__foreach(I, field2val)
			{
				args.push_back(I->first.c_str());
				lens.push_back(I->first.size());

				args.push_back(I->second.c_str());
				lens.push_back(I->second.size());
			}

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取hash字段数值
		Status Client::hget(const std::string& key, const std::string & field, std::string& val)
		{
			Command command("HGET");

			RedisReplyPtr reply = myRedisCommand("%s %s %s", command.c_str(), key.c_str(), field.c_str());
			Status status = reply2status(reply);
			val = reply2string(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量读取hash字段数值
		Status Client::hmget(const std::string& key, const std::set<std::string>& fields, std::map<std::string, std::string>& field2val)
		{
			Command command("HMGET");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(2 + fields.size());
			lens.reserve(2 + fields.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			appendArgs(fields, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				uint32_t i = 0;
				__foreach(I, fields)
				{
					if (reply->element[i]->type == REDIS_REPLY_STRING)
					{
						field2val[*I].assign(reply->element[i]->str, reply->element[i]->len);
					}
					++i;
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 读取hash左右字段
		Status Client::hgetall(const std::string& key, std::map<std::string, std::string>& allfield)
		{
			Command command("HGETALL");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				for (unsigned i = 0; i < reply->elements / 2; ++i)
				{
					std::string field = reply2string(reply->element[i * 2]);
					std::string val = reply2string(reply->element[i * 2 + 1]);
					allfield[field] = val;
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 返回hash中元素个数
		Status Client::hlen(const std::string& key, int& fieldcount)
		{
			Command command("HLEN");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			fieldcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 原子递增hash某个字段的数值
		Status Client::hincrby(const std::string & key, const std::string & field, int64_t increment, int64_t * newval)
		{
			Command command("HINCRBY");

			RedisReplyPtr reply = myRedisCommand("%s %s %s %lld", command.c_str(), key.c_str(), field.c_str(), increment);
			Status status = reply2status(reply);
			if (newval) *newval = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取sorted-set 元素个数
		Status Client::zcard(const std::string& key, int& count)
		{
			Command command("ZCARD");

			RedisReplyPtr reply = myRedisCommand("%s %s", command.c_str(), key.c_str());
			Status status = reply2status(reply);
			if (status.ok()) count = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 向sorted-set添加元素
		Status Client::zadd(const std::string& key, double score, const std::string& member)
		{
			Command command("ZADD");

			RedisReplyPtr reply = myRedisCommand("%s %s %f %s", command.c_str(), key.c_str(), score, member.c_str());
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 批量向sorted-set添加元素
		Status Client::zadd(const std::string& key, const std::map<std::string, double>& member2score)
		{
			Command command("ZADD");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			std::vector<std::string> scores;
			args.reserve(2 + 2 * member2score.size());
			lens.reserve(2 + 2 * member2score.size());
			scores.reserve(member2score.size());

			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			__foreach(I, member2score) // ZADD KEY SCORE MEMBER SCORE MEMBER
			{
				char buf[100] = { 0 };
				sprintf(buf, "%f", I->second);
				scores.push_back(buf);
				args.push_back(scores.back().c_str());
				lens.push_back(scores.back().size());

				args.push_back(I->first.c_str());
				lens.push_back(I->first.size());
			}

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 从sorted-set删除元素
		Status Client::zrem(const std::string& key, const std::string& member, int* delcount)
		{
			Command command("ZREM");

			RedisReplyPtr reply = myRedisCommand("%s %s %b", command.c_str(), key.c_str(), member.data(), member.size());
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 从sorted-set批量删除元素
		Status Client::zrem(const std::string& key, const std::set<std::string>& members, int* delcount)
		{
			Command command("ZREM");
			std::vector<const char *> args;
			std::vector<size_t> lens;
			args.reserve(1 + 1 + members.size());
			lens.reserve(1 + 1 + members.size());
			args.push_back(command.c_str());
			lens.push_back(command.length());
			args.push_back(key.c_str());
			lens.push_back(key.length());

			appendArgs(members, args, lens);

			RedisReplyPtr reply = myRedisCommandArgv(args.size(), &(args[0]), &(lens[0]));
			Status status = reply2status(reply);
			if (delcount) *delcount = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::zrembyscore(const std::string& key, const int min, const int max)
		{
			Command command("ZREMRANGEBYSCORE");

			RedisReplyPtr reply = myRedisCommand("%s %s %u %u", command.c_str(), key.c_str(), min, max);
			Status status = reply2status(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 读取sorted-set某个元素的分数
		Status Client::zscore(const std::string& key, const std::string& member, double& score)
		{
			Command command("ZSCORE");

			RedisReplyPtr reply = myRedisCommand("%s %s %s", command.c_str(), key.c_str(), member.c_str());
			Status status = reply2status(reply);
			score = atof(reply2string(reply).c_str());
			reportStatus(reply, command, status);
			return status;
		}

		// 读取分数区间的元素个数
		Status Client::zcount(const std::string& key, double minScore, double maxScore, int& count)
		{
			Command command("ZCOUNT");

			RedisReplyPtr reply = myRedisCommand("%s %s %f %f", command.c_str(), key.c_str(), minScore, maxScore);
			Status status = reply2status(reply);
			count = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::zrevrank(const std::string& key, const std::string& member, int* rank)
		{
			Command command("ZREVRANK");

			RedisReplyPtr reply = myRedisCommand("%s %s %s", command.c_str(), key.c_str(), member.c_str());
			Status status = reply2status(reply);
			if (rank) *rank = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::zrank(const std::string& key, const std::string& member, int& rank)
		{
			Command command("ZRANK");

			RedisReplyPtr reply = myRedisCommand("%s %s %s", command.c_str(), key.c_str(), member.c_str());
			Status status = reply2status(reply);
			rank = reply2integer(reply);
			reportStatus(reply, command, status);
			return status;
		}

		// 正向读取sorted-set某个排序序号区间的元素，返回的元素按照score小的在前面
		Status Client::zrange(const std::string& key, int startIndex, int stopIndex, std::vector<std::string>& members)
		{
			return zrange_impl("ZRANGE", key, startIndex, stopIndex, members);
		}

		// 正向读取sorted-set某个排序区间的元素，返回的元素按照score从小到大排序。同时把元素得的分数也返回。
		Status Client::zrangeWithScores(const std::string& key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members)
		{
			return zrangeWithScores_impl("ZRANGE", key, startIndex, stopIndex, members);
		}

		// 反向读取sorted-set某个排序区间的元素，score大的在前面
		Status Client::zrevrange(const std::string& key, int startIndex, int stopIndex, std::vector<std::string>& members)
		{
			return zrange_impl("ZREVRANGE", key, startIndex, stopIndex, members);
		}

		// 反向读取sorted-set某个排序区间的元素，score大的在线面。同时把元素得分也返回。
		Status Client::zrevrangeWithScores(const std::string& key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members)
		{
			return zrangeWithScores_impl("ZREVRANGE", key, startIndex, stopIndex, members);
		}

		Status Client::zrange_impl(const std::string & strCmd, const std::string& key, int startIndex, int stopIndex, std::vector<std::string>& members)
		{
			Command command(strCmd);

			RedisReplyPtr reply = myRedisCommand("%s %s %d %d", command.c_str(), key.c_str(), startIndex, stopIndex);
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				members.reserve((unsigned)(reply->elements));
				for (unsigned i = 0; i < reply->elements; ++i)
				{
					members.push_back(reply2string(reply->element[i]));
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::zrangeWithScores_impl(const std::string & strCmd, const std::string& key, int startIndex, int stopIndex, std::vector<std::pair<std::string, double> > & members)
		{
			Command command(strCmd);

			RedisReplyPtr reply = myRedisCommand("%s %s %d %d WITHSCORES", command.c_str(), key.c_str(), startIndex, stopIndex);
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				members.reserve((unsigned)(reply->elements / 2 + 1));
				for (unsigned i = 0; i < reply->elements / 2; ++i)
				{
					std::string mem = reply2string(reply->element[i * 2]);
					double score = atof(reply2string(reply->element[i * 2 + 1]).c_str());
					members.push_back(std::make_pair(mem, score));
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		// 根据分数区间读取元素，分数小的在前面
		Status Client::zrangebyscore(const std::string& key, const std::string & score1, const std::string & score2, std::vector<std::string>& members)
		{
			return zrangbyscore_impl("ZRANGEBYSCORE", key, score1, score2, members);
		}

		Status Client::zincrby(const std::string& key, const std::string& field, int64_t increment, int64_t& newval)
		{
			Command command("ZINCRBY");

			RedisReplyPtr reply = myRedisCommand("%s %s %lld %s", command.c_str(), key.c_str(), increment, field.c_str());
			Status status = reply2status(reply);
			newval = atol(reply2string(reply).c_str());
			reportStatus(reply, command, status);
			return status;
		}

		// 根据分数区间反向读取元素，分数大的在前面
		Status Client::zrevrangebyscore(const std::string& key, const std::string & score1, const std::string & score2, std::vector<std::string>& members)
		{
			return zrangbyscore_impl("ZREVRANGEBYSCORE", key, score1, score2, members);
		}

		// 根据分数区间读取元素，分数小的在前面
		Status Client::zrangebyscoreWithScores(const std::string& key, const std::string & score1, const std::string & score2, std::vector<std::pair<std::string, double> > & members)
		{
			return zrangbyscoreWithScores_impl("ZRANGEBYSCORE", key, score1, score2, members);
		}

		// 根据分数区间反向读取元素，分数大的在前面
		Status Client::zrevrangebyscoreWithScores(const std::string& key, const std::string & score1, const std::string & score2, std::vector<std::pair<std::string, double> > & members)
		{
			return zrangbyscoreWithScores_impl("ZREVRANGEBYSCORE", key, score1, score2, members);
		}

		Status Client::zrangbyscore_impl(
			const std::string& strCmd,
			const std::string& key,
			const std::string& score1,
			const std::string& score2,
			std::vector<std::string>& members)
		{
			Command command(strCmd);

			RedisReplyPtr reply = myRedisCommand("%s %s %s %s", command.c_str(), key.c_str(), score1.c_str(), score2.c_str());
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				members.reserve((unsigned)reply->elements + 1);
				for (unsigned i = 0; i < reply->elements; ++i)
				{
					members.push_back(reply2string(reply->element[i]));
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

		Status Client::zrangbyscoreWithScores_impl(
			const std::string& strCmd,
			const std::string& key,
			const std::string& score1,
			const std::string& score2,
			std::vector<std::pair<std::string, double> > & members)
		{
			Command command(strCmd);

			RedisReplyPtr reply = myRedisCommand("%s %s %s %s WITHSCORES", command.c_str(), key.c_str(), score1.c_str(), score2.c_str());
			Status status = reply2status(reply);
			if (status.ok() && reply->type == REDIS_REPLY_ARRAY)
			{
				members.reserve((unsigned)reply->elements);
				for (unsigned i = 0; i < reply->elements / 2; ++i)
				{
					std::string mem = reply2string(reply->element[i * 2]);
					double score = atof(reply2string(reply->element[i * 2 + 1]).c_str());
					members.push_back(std::make_pair(mem, score));
				}
			}
			reportStatus(reply, command, status);
			return status;
		}

	} // namespace ramcloud

	namespace redis {

		using ramcloud::Config;

		static Config CONFIG("redisources");

		void init(anka::IConfigObserver *ob, void * fwk)
		{
			ob->RegisterWatcher(CONFIG.key, &CONFIG);
			if (fwk) {
				//((anka::Framework*) fwk)->watchS2S(&CONFIG);
				//printf(" *** watchS2S at %s init\n", CONFIG.key.c_str());
			}
		}

		void fini()
		{
		}

		Client::Client(const std::string& _dbname)
			: ramcloud::Client(_dbname)
		{
		}

		void Client::_init()
		{
			client = CONFIG.getRedisClient(dbname);
			//printf("%s.%s: _init done. %p, %d\n", CONFIG.key.c_str(), dbname.c_str(), (void*)client.get(), client.getrc());
		}

	} // namespace redis

} // namespace anka
