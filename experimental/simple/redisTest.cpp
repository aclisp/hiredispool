#include "framework/anka.h"


inline int _atom_inc(int& v) { return __sync_add_and_fetch(&v, 1); }
inline int _atom_dec(int& v) { return __sync_sub_and_fetch(&v, 1); }
inline int _atom_get(int& v) { return __sync_fetch_and_or(&v, 0); }

namespace anka {
    namespace ramcloud {
        void _injectAddServer();
        void _injectRemoveServer();
    }
}

using namespace anka;
using namespace std;

ramcloud::Client ramCli;
redis::Client redCli;

class Counter
{
public:
    Counter() : count(1) {}
    void inc() { _atom_inc(count); }
    int dec() { return _atom_dec(count); }
    int get() { return _atom_get(count); }
private:
    int count;
};

class Jungle
{
public:
    bool testRedis();
    bool testRamcloud();
    bool injectAddServer();
    bool injectRemoveServer();
private:
    Counter ramCnt;
    Counter redCnt;
};

bool Jungle::testRamcloud()
{
    ramCnt.inc();

    char buf[256];
    sprintf(buf, "%s: ramcloud counter - %d\n", __func__, ramCnt.get());
    //printf(buf);
    logth(Info, buf);

    ramCli.set(__func__, buf);
    string val;
    ramCli.get(__func__, val);
    if (val != buf) {
        printf("%s: fail ! get=\"%s\" set=\"%s\"\n", __func__, val.c_str(), buf);
        logth(Error, "%s: fail ! get=\"%s\" set=\"%s\"\n", __func__, val.c_str(), buf);
    }

    return true;
}

bool Jungle::testRedis()
{
    redCnt.inc();

    char buf[256];
    sprintf(buf, "%s: redis counter - %d\n", __func__, redCnt.get());
    //printf(buf);
    //logth(Info, buf);

    redCli.set(__func__, buf);
    string val;
    redCli.get(__func__, val);
    //assert(val == buf);

    return true;
}

bool Jungle::injectAddServer()
{
    ramcloud::_injectAddServer();
    return true;
}

bool Jungle::injectRemoveServer()
{
    ramcloud::_injectRemoveServer();
    return true;
}

int main()
{
    Framework& fwk = Framework::ins("ankaredis");
    Jungle jun;

    fwk.createTimer(&Jungle::testRamcloud, &jun, 1, 1);
    //fwk.createTimer(&Jungle::testRedis, &jun, 1, 1);

    fwk.createTimer(&Jungle::injectAddServer, &jun, 2020, 1);
    fwk.createTimer(&Jungle::injectRemoveServer, &jun, 5250, 1);

    fwk.initS2S();
    fwk.initRamcloud();
    fwk.initRedis();

    fwk.start();
    return 0;
}
