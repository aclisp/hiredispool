LDFLAGS = hiredis/libhiredis.a

OBJ = log.o hiredispool.o RedisClient.o
LIBNAME = libhiredispool

CC = gcc
CXX = g++
OPTIMAZATION ?= -O3
WARNINGS = -Wall -Wpedantic -W -Wstrict-prototypes -Wwrite-strings
DEBUG_FLAGS ?= -g -ggdb
MACROS = -D_GNU_SOURCE

REAL_CFLAGS = $(OPTIMAZATION) -pthread -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG_FLAGS) $(ARCH) $(MACROS)
REAL_CXXFLAGS = $(REAL_CFLAGS) -Wno-c99-extensions
REAL_LDFLAGS = $(LDFLAGS) $(ARCH)

STLIBSUFFIX = a
STLIBNAME = $(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

HIREDISPOOL_SONAME=0.1
DYLIBSUFFIX=so
DYLIBNAME=$(LIBNAME).$(DYLIBSUFFIX)
DYLIB_MINOR_NAME=$(LIBNAME).$(DYLIBSUFFIX).$(HIREDISPOOL_SONAME)
DYLIB_MAKE_CMD=$(CC) -shared -Wl,-soname,$(DYLIB_MINOR_NAME) -o $(DYLIBNAME) $(LDFLAGS)
INSTALL?= cp -a
PREFIX?=/usr/local
INCLUDE_PATH?=include/hiredispool
LIBRARY_PATH?=lib
INSTALL_INCLUDE_PATH= $(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH= $(PREFIX)/$(LIBRARY_PATH)

all: $(STLIBNAME)

# Deps (use make dep to generate this)
hiredispool.o: hiredispool.c hiredispool.h log.h hiredis/hiredis.h \
  hiredis/read.h hiredis/sds.h
log.o: log.c log.h
RedisClient.o: RedisClient.cpp RedisClient.h hiredispool.h \
  hiredis/hiredis.h hiredis/read.h hiredis/sds.h log.h

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

static: $(STLIBNAME)

# Binaries
test_log.exe: test_log.c log.h $(STLIBNAME)
	$(CC) -o $@ $(REAL_CFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_hiredispool.exe: test_hiredispool.cpp hiredispool.h log.h $(STLIBNAME)
	$(CXX) -std=c++11 -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_RedisClient.exe: test_RedisClient.cpp RedisClient.h hiredispool.h log.h $(STLIBNAME)
	$(CXX) -std=c++11 -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

test_RedisClient2.exe: test_RedisClient2.cpp RedisClient.h hiredispool.h log.h $(STLIBNAME)
	$(CXX) -std=c++11 -o $@ $(REAL_CXXFLAGS) -I. $< $(STLIBNAME) $(REAL_LDFLAGS)

.c.o:
	$(CC) -std=c99 -c $(REAL_CFLAGS) $<

.cpp.o:
	$(CXX) -std=c++11 -c $(REAL_CXXFLAGS) $<

clean:
	rm -rf $(STLIBNAME) *.o *.out *.exe

dep:
	$(CC) -MM *.c
	$(CXX) -MM *.cpp

$(DYLIBNAME): $(OBJ)
	$(DYLIB_MAKE_CMD) $(OBJ)

hiredis-install:
	cd hiredis && make install

install: hiredis-install $(DYLIBNAME) $(STLIBNAME)
	mkdir -p $(INSTALL_INCLUDE_PATH) $(INSTALL_LIBRARY_PATH)
	$(INSTALL) hiredispool.h RedisClient.h $(INSTALL_INCLUDE_PATH)
	$(INSTALL) $(DYLIBNAME) $(INSTALL_LIBRARY_PATH)
	$(INSTALL) $(STLIBNAME) $(INSTALL_LIBRARY_PATH)

docker-image:
	docker build -t hiredispool .

.PHONY: all static clean dep install hiredis-install docker-image
