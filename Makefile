LDFLAGS = hiredis/libhiredis.a

OBJ = log.o hiredispool.o RedisClient.o
LIBNAME = libhiredispool

CC = gcc
CXX = g++
OPTIMAZATION ?= -O0
WARNINGS = -Wall -Wpedantic -W -Wstrict-prototypes -Wwrite-strings
DEBUG_FLAGS ?= -g -ggdb

REAL_CFLAGS = $(OPTIMAZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG_FLAGS) $(ARCH)
REAL_CXXFLAGS = $(REAL_CFLAGS) -Wno-c99-extensions
REAL_LDFLAGS = $(LDFLAGS) $(ARCH)

STLIBSUFFIX = a
STLIBNAME = $(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

all: $(STLIBNAME)

# Deps (use make dep to generate this)
hiredispool.o: hiredispool.c hiredispool.h log.h hiredis/hiredis.h \
  hiredis/read.h hiredis/sds.h
log.o: log.c log.h
RedisClient.o: RedisClient.cpp RedisClient.h hiredispool.h \
  hiredis/hiredis.h hiredis/read.h hiredis/sds.h

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

static: $(STLIBNAME)

# Binaries
test_log.exe: test_log.c log.h $(STLIBNAME)
	$(CC) -o $@ $(REAL_CFLAGS) $(REAL_LDFLAGS) -I. $< $(STLIBNAME)

test_hiredispool.exe: test_hiredispool.cpp hiredispool.h log.h $(STLIBNAME)
	$(CXX) -std=c++11 -o $@ $(REAL_CXXFLAGS) $(REAL_LDFLAGS) -I. $< $(STLIBNAME)

test_RedisClient.exe: test_RedisClient.cpp RedisClient.h hiredispool.h log.h $(STLIBNAME)
	$(CXX) -std=c++11 -o $@ $(REAL_CXXFLAGS) $(REAL_LDFLAGS) -I. $< $(STLIBNAME)

.c.o:
	$(CC) -std=c99 -c $(REAL_CFLAGS) $<

.cpp.o:
	$(CXX) -std=c++11 -c $(REAL_CXXFLAGS) $<

clean:
	rm -rf $(STLIBNAME) *.o *.out *.exe

dep:
	$(CC) -MM *.c
	$(CXX) -MM *.cpp

.PHONY: all static clean dep
