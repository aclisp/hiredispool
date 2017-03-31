LDFLAGS = hiredis/libhiredis.a ../lib/bamSDK.a ../utility/logger.o ../utility/syslog-nb.o ../utility/exception_errno.o ../lib/libanka.so ../lib/libs2sclient.so ../lib/config.a ../lib/mapi4anka.a -luuid ../lib/libcrypto.a -lrt

OBJ = hiredispool.o RedisClient.o RedisClientYaf.o
LIBNAME = libhiredispool

CC = gcc
CXX = g++
OPTIMAZATION ?= -O0
WARNINGS = -Wall -W -Wwrite-strings -Wno-unused-parameter -Wno-unused-local-typedefs
DEBUG_FLAGS ?= -g -ggdb
MACROS = -D_GNU_SOURCE
INCLUDES = -I../include -I../bamSDK -I../config -I../nameserver -I../utility -I.. -I../.. -I../core/net -I../core/thread -I../plugin -I../dbclient/mysql -I../dbclient/mysql/mysqlclient/include -I../redisclient -I../../third_party -I../../protocol

REAL_CFLAGS = $(OPTIMAZATION) -pthread -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG_FLAGS) $(ARCH) $(MACROS) $(INCLUDES)
REAL_CXXFLAGS = $(REAL_CFLAGS)
REAL_LDFLAGS = $(LDFLAGS) $(ARCH)

STLIBSUFFIX = a
STLIBNAME = $(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

all: $(STLIBNAME)

# Deps (use make dep to generate this)
hiredispool.o: hiredispool.c hiredispool.h hiredis/hiredis.h \
  hiredis/read.h hiredis/sds.h
RedisClient.o: RedisClient.cpp RedisClient.h hiredispool.h \
  hiredis/hiredis.h hiredis/read.h hiredis/sds.h
RedisClientYaf.o: RedisClientYaf.cpp RedisClientYaf.h

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)
	cp -f $(STLIBNAME) ../lib
	cp -f hiredis/libhiredis.a ../lib

static: $(STLIBNAME)

# Binaries
test_hiredispool.exe: test_hiredispool.cpp hiredispool.h $(STLIBNAME)
	$(CXX) -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_RedisClient.exe: test_RedisClient.cpp RedisClient.h hiredispool.h $(STLIBNAME)
	$(CXX) -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_RedisClient2.exe: test_RedisClient2.cpp RedisClient.h hiredispool.h $(STLIBNAME)
	$(CXX) -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_RedisClientYaf.exe: test_RedisClientYaf.cpp RedisClientYaf.h RedisClient.h hiredispool.h $(STLIBNAME)
	$(CXX) -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

.c.o:
	$(CXX) -c $(REAL_CFLAGS) $<

.cpp.o:
	$(CXX) -c $(REAL_CXXFLAGS) $<

clean:
	rm -rf $(STLIBNAME) *.o *.out *.exe

dep:
	$(CC) -MM *.c $(REAL_CFLAGS) -I.
	$(CXX) -MM *.cpp $(REAL_CFLAGS) -I.

.PHONY: all static clean dep
