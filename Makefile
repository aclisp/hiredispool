OBJ = log.o hiredispool.o
LIBNAME = libhiredispool

CC = gcc
CXX = g++
OPTIMAZATION ?= -O0
WARNINGS = -Wall -Wpedantic -W -Wstrict-prototypes -Wwrite-strings
DEBUG_FLAGS ?= -g -ggdb

REAL_CFLAGS = $(OPTIMAZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG_FLAGS) $(ARCH)
REAL_CXXFLAGS = $(REAL_CFLAGS)
REAL_LDFLAGS = $(LDFLAGS) $(ARCH)

STLIBSUFFIX = a
STLIBNAME = $(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

all: $(STLIBNAME)

# Deps (use make dep to generate this)


$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

static: $(STLIBNAME)

# Binaries
test_log.exe: test_log.c $(STLIBNAME)
	$(CC) -o $@ $(REAL_CFLAGS) $(REAL_LDFLAGS) -I. $< $(STLIBNAME)

test_hiredispool.exe: test_hiredispool.cpp $(STLIBNAME)
	$(CXX) -o $@ $(REAL_CXXFLAGS) $(REAL_LDFLAGS) -I. $< $(STLIBNAME)

.c.o:
	$(CC) -std=c99 -c $(REAL_CFLAGS) $<

.cpp.o:
	$(CXX) -std=c++11 -c $(REAL_CXXFLAGS) $<

clean:
	ls $(STLIBNAME) *.o *.out *.exe

dep:
	$(CC) -MM *.c

.PHONY: all static clean dep
