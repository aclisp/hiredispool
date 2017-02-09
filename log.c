#include <sys/stat.h>
#include <sys/time.h> /* for struct timeval */
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>

#include "log.h"


typedef struct log_config_internal {
    int verbose;
    log_dest_t dest;
    char dir[1024];
    char file[1024];
    char progname[256];
    int level_hold;
    int print_millisec;
} LOG_CONFIG_INTERNAL;

static LOG_CONFIG_INTERNAL C = {
    0,
    LOG_DEST_STDERR,
    "",
    "noname.log",
    "noname",
    L_WARN,
    0
};

static _NAME_NUMBER L[] = {
    { " TRACE ",            L_TRACE },
    { " DEBUG ",            L_DEBUG },
    { " INFO  ",            L_INFO  },
    { " WARN  ",            L_WARN  },
    { " ERROR ",            L_ERROR },
    { " FATAL ",            L_FATAL },
    { NULL, 0 }
};

const char * _int2str (
    const _NAME_NUMBER *table,
    int                number,
    const char *def
    )
{
       const _NAME_NUMBER *this;

    for (this = table; this->name != NULL; this++) {
        if (this->number == number) {
            return this->name;
        }
    }

    return def;
}

/*
 * Control log filename, create directory as needed.
 */
static void _set_logfile(const char* filename)
{
    char namebuf[1024];
    char* dirsep;
    char* dir = namebuf;

    if(filename == NULL)
        return;

    /* Create directory and set log_dir */
    strncpy(namebuf, filename, sizeof(namebuf));
    namebuf[sizeof(namebuf) - 1] = '\0';

    while((dirsep = strchr(dir, '/')) != NULL) {
        *dirsep = '\0';
        mkdir(namebuf, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
        *dirsep = '/';
        dir = dirsep + 1;
    }

    dirsep = strrchr(namebuf, '/');
    if(dirsep != NULL) {
        *dirsep = '\0';
        strncpy(C.dir, namebuf, sizeof(C.dir));
        C.dir[sizeof(C.dir) - 1] = '\0';
        *dirsep = '/';
    }

    /* Do set log_file */
    strncpy(C.file, filename, sizeof(C.file));
    C.file[sizeof(C.file) - 1] = '\0';
}

static void convert_logfilename(const char* origin, char* converted, size_t namelen)
{
    char* p;
    int len;
    struct tm m;
    time_t now;

    len = strlen(origin);
    if(namelen < (size_t)len+10)
        goto cannot_convert;

    now = time(NULL);
    if(now == (time_t)-1)
        goto cannot_convert;

    localtime_r(&now, &m);

    p = strrchr(origin, '.');
    if(p == NULL) {
        sprintf(converted, "%s.%04d%02d%02d",
                origin, m.tm_year+1900, m.tm_mon+1, m.tm_mday);
    } else {
        len = p - origin;
        memcpy(converted, origin, len);
        sprintf(converted+len, ".%04d%02d%02d",
                m.tm_year+1900, m.tm_mon+1, m.tm_mday);
        strcat(converted, p);
    }

    return;

cannot_convert:
    strncpy(converted, origin, namelen);
    converted[namelen-1] = '\0';
}

/*
 *  Log the message to the logfile. Include the severity and
 *  a time stamp.
 */
int vlog(int lvl, const char *fmt, va_list ap)
{
    FILE *msgfd = NULL;
    char *p;
    char buffer[8192];
    char namebuf[1024];
    int len, len0;

    /*
     *  Filter out by level_hold
     */
    if(C.level_hold > (lvl & ~L_CONS)) {
        return 0;
    }

    /*
     *  NOT debugging, and trying to log debug messages.
     *
     *  Throw the message away.
     */
    if ((C.verbose == 0) && ((lvl & ~L_CONS) <= L_DEBUG)) {
        return 0;
    }

    /*
     *  If we don't want any messages, then
     *  throw them away.
     */
    if (C.dest == LOG_DEST_NULL) {
        return 0;
    }

    if (C.dest == LOG_DEST_STDOUT) {
            msgfd = stdout;

    } else if (C.dest == LOG_DEST_STDERR) {
            msgfd = stderr;

    } else if (C.dest != LOG_DEST_SYSLOG) {
        /*
         *  No log file set.  It must go to stdout.
         */
        if (C.file[0] == '\0') {
            msgfd = stdout;

            /*
             *  Else try to open the file.
             */
        } else {
            convert_logfilename(C.file, namebuf, sizeof(namebuf));

            if ((msgfd = fopen(namebuf, "a+")) == NULL) {
                fprintf(stderr, "%s: Couldn't open %s for logging: %s\n",
                        C.progname, namebuf, strerror(errno));

                fprintf(stderr, "  (");
                vfprintf(stderr, fmt, ap);  /* the message that caused the log */
                fprintf(stderr, ")\n");
                return -1;
            }
        }
    }

    if (C.dest == LOG_DEST_SYSLOG) {
        *buffer = '\0';
        len0 = 0;
        len = 0;
    } else

    if(C.print_millisec)
    {
        const char* s;
        struct timeval tv;
        struct tm local;
        char deflvl[8];

        gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &local);

        sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d,%03d",
                local.tm_year+1900, local.tm_mon+1, local.tm_mday,
                local.tm_hour, local.tm_min, local.tm_sec,
                (int)tv.tv_usec/1000);

        sprintf(deflvl, " L%04X ", (lvl & ~L_CONS));
        s = _int2str(L, (lvl & ~L_CONS), deflvl);

        len0 = strlen(buffer);
        strcat(buffer, s);
        len = strlen(buffer);
    }
    else
    {
        const char *s;
        time_t tv;
        struct tm local;
        char deflvl[8];

        tv = time(NULL);
        localtime_r(&tv, &local);
        sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d",
                local.tm_year+1900, local.tm_mon+1, local.tm_mday,
                local.tm_hour, local.tm_min, local.tm_sec);

        sprintf(deflvl, " L%04X ", (lvl & ~L_CONS));
        s = _int2str(L, (lvl & ~L_CONS), deflvl);

        len0 = strlen(buffer);
        strcat(buffer, s);
        len = strlen(buffer);
    }

    vsnprintf(buffer + len, sizeof(buffer) - len - 1, fmt, ap);
    buffer[sizeof(buffer) - 2] = '\0';

    /*
     *  Filter out characters not in Latin-1.
     */
    for (p = buffer; *p != '\0'; p++) {
        if (*p == '\r' || *p == '\n')
            *p = ' ';
        else if ((*p >=0 && *p < 32) || (/**p >= -128 &&*/ *p < 0))
            *p = '?';
    }
    strcat(buffer, "\n");

    /*
     *   If we're debugging, for small values of debug, then
     *   we don't do timestamps.
     */
    if (C.verbose == 1) {
        p = buffer + len0;

    } else {
        /*
         *  No debugging, or lots of debugging.  Print
         *  the time stamps.
         */
        p = buffer;
    }

    if (C.dest != LOG_DEST_SYSLOG)
    {
        fputs(p, msgfd);
        if (msgfd == stdout) {
            fflush(stdout);
        } else if (msgfd == stderr) {
            fflush(stderr);
        } else {
            fclose(msgfd);

            if(lvl & L_CONS) {
                if((lvl & ~L_CONS) < L_WARN) {
                    fputs(p, stdout);
                    fflush(stdout);
                } else {
                    fputs(p, stderr);
                    fflush(stderr);
                }

                /*
                 * IF debugging, also to console but no flush
                 */
            } else if(C.verbose) {
                if((lvl & ~L_CONS) < L_WARN) {
                    fputs(p, stdout);
                } else {
                    fputs(p, stderr);
                }
            }
        }
    }
    else {          /* it was syslog */
        lvl = (lvl & ~L_CONS);

        if(lvl > 0 && lvl <= L_DEBUG) {
            lvl = LOG_DEBUG;

        } else if(lvl > L_DEBUG && lvl <= L_INFO) {
            lvl = LOG_NOTICE;

        } else if(lvl > L_INFO && lvl <= L_WARN) {
            lvl = LOG_WARNING;

        } else if(lvl > L_WARN && lvl <= L_ERROR) {
            lvl = LOG_ERR;

        } else if(lvl > L_ERROR && lvl <= L_FATAL) {
            lvl = LOG_CRIT;

        } else if(lvl > L_FATAL) {
            lvl = LOG_ALERT;

        } else {
            lvl = LOG_EMERG;

        }

        syslog(lvl, "%s", buffer + len0); /* don't print timestamp */
    }

    return 0;
}

int log_debug(const char *msg, ...)
{
	va_list ap;
	int r;

	va_start(ap, msg);
	r = vlog(L_DEBUG, msg, ap);
	va_end(ap);

	return r;
}

int log_trace(const char *msg, ...)
{
	va_list ap;
	int r;

	va_start(ap, msg);
	r = vlog(L_TRACE, msg, ap);
	va_end(ap);

	return r;
}

int log_(int lvl, const char *msg, ...)
{
	va_list ap;
	int r;

	va_start(ap, msg);
	r = vlog(lvl, msg, ap);
	va_end(ap);

	return r;
}

void log_set_config(const LOG_CONFIG* config)
{
    C.verbose = config->verbose;
    C.dest = config->dest;
    _set_logfile(config->file);
    C.level_hold = config->level_hold;
    C.print_millisec = config->print_millisec;

    strncpy(C.progname, config->progname, sizeof(C.progname));
    C.progname[sizeof(C.progname) - 1] = '\0';
}

int log_get_verbose()
{
    return C.verbose;
}
