#include "log.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    LOG_CONFIG c = {
        9,
        LOG_DEST_FILES,
        "log/test_log.log",
        "test_log",
        0,
        1
    };
    log_set_config(&c);

    DEBUG("debug %s!", "me");
    log_(L_DEBUG, "debug %s!", "me2");
    log_(L_INFO|L_CONS, "hello %s!", "me");
    log_(L_WARN, "only in file");

    return 0;
}
