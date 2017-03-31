CC = gcc
CXX = g++
CXX32 = g++ -m32
CC32 = gcc -m32
AR = ar -ru

BASE_DIR := ..
ROOT_DIR := ..

LDPATH = -Wl,-R,/usr/local/lib -Wl,-rpath,../lib -L../lib  -L../lib -lanka  -ls2sclient ../lib/libcrypto.a -lrt -lpthread -luuid
LDPATH += ../lib/config.a ../lib/bamSDK.a ../lib/mapi4anka.a
LDPATH += ../lib/libmysqltemplate.a ../lib/libdbsafe.so.0.0.1 ../lib/libmysqlclient.a
LDPATH += ../lib/libhiredispool.a ../lib/libhiredis.a
LD_CXXFLAGS = $(CXXFLAGS) $(LDPATH)


DEFS := -DHAVE_EPOLL -DTIXML_USE_STL -DXML_NULL -D_REENTRANT

CXXFLAGS := -D__STDC_LIMIT_MACROS -Wall -Wno-format -Wno-unused-local-typedefs -fno-omit-frame-pointer -DDEBUG  -ggdb $(DEFS)


INCLUDE := -I$(BASE_DIR)/ -I$(BASE_DIR)/include -I$(BASE_DIR)/core/net/ -I$(BASE_DIR)/core/thread -I$(BASE_DIR)/nameserver -I$(BASE_DIR)/config -I$(BASE_DIR)/plugin -I$(BASE_DIR)/utility -I$(BASE_DIR)/mapi4anka
INCLUDE += -I../../ -I../../protocol -I../../musicCommon/inner_clients -I../bamSDK -I./
INCLUDE += -I../dbclient/mysql/mysqlclient/include -I../dbclient/mysql -I../../third_party/
INCLUDE += -I../redisclient -I../../third_party/ -I../../ext/webdb/

BASE := ../core/net/Connection.cpp
BASE += ../core/net/ConnManager.cpp
BASE += ../core/net/countdown.cpp
BASE += ../core/net/selector.cpp
BASE += ../core/net/Selector_epoll.cpp
BASE += ../core/net/snox.cpp
BASE += ../core/net/sockethelper.cpp
BASE += ../core/net/tcpsock.cpp
BASE += ../core/thread/Coroutine.cpp
BASE += ../utility/varint.cpp
BASE += ../utility/exception_errno.cpp
BASE += ../utility/logger.cpp
BASE += ../utility/syslog-nb.cpp
BASE += ../framework/queue.cpp

BASE += ../framework/anka.cpp
BASE += ../framework/cmd.cpp
BASE += ../framework/threadpool.cpp
BASE += ../framework/scheduler.cpp
BASE += ../framework/service.cpp
BASE += ../framework/router.cpp
BASE += ../framework/future.cpp
BASE += ../nameserver/S2SNodeManager.cpp
BASE += ../nameserver/s2sClient.cpp
BASE += ../utility/ConfigLocalHost.cpp
BASE += ../framework/base.cpp
BASE += ../framework/message.cpp
BASE += ../nameserver/daemonInfo.cpp

WEBDB = ../../ext/webdb/new_webDBif.cpp

CLIENT := testClient.cpp
SERVER := testServer.cpp

# MAIN1 := main1.cpp
# MAIN2 := main2.cpp
# MAIN3 := main3.cpp


MAIN1 := main1Async.cpp
MAIN2 := main2Async.cpp
MAIN3 := main3Async.cpp


OBJS_BASE := $(patsubst %.cpp, %.o, $(BASE:.cpp=.o))
OBJS_BASE := $(patsubst %.cpp, %.o, $(WEBDB:.cpp=.o))
OBJS_CLIENT := $(patsubst %.cpp, %.o, $(CLIENT:.cpp=.o))
OBJS_SERVER := $(patsubst %.cpp, %.o, $(SERVER:.cpp=.o))

MAIN1_OBJ  :=$(patsubst %.cpp, %.o, $(MAIN1:.cpp=.o))
MAIN2_OBJ  :=$(patsubst %.cpp, %.o, $(MAIN2:.cpp=.o))
MAIN3_OBJ  :=$(patsubst %.cpp, %.o, $(MAIN3:.cpp=.o))


all: redis_test_m

redis_test_m: redisTest.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDE) $< $(LD_CXXFLAGS)

clean:
	rm -f $(OBJS_BASE) $(OBJS_CLIENT) $(OBJS_SERVER)
	rm -f $(MAIN1_OBJ)
	rm -f $(MAIN2_OBJ)
	rm -f $(MAIN3_OBJ)
	#rm -f music_server_m music_client_m music_main1_m music_main2_m music_main3_m
	rm -f music_server_m music_client_m redis_test_m

