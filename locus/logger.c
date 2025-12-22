#define _POSIX_C_SOURCE 200809L

#include "logger.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* get_log_lvl(log_level_t lvl) {
    switch (lvl) {
        case LOG_TRACE:
            return "TRACE";
        case LOG_DEBUG:
            return "DEBUG";
        case LOG_INFO:
            return "INFO";
        case LOG_WARN:
            return "WARN";
        case LOG_ERROR:
            return "ERROR";
        case LOG_CRIT:
            return "CRIT";
        default:  // This should never happen
            return "UNKN";
    }
}

void log_msg(log_level_t lvl,
             const char* file,
             int line,
             const char* func,
             const char* fmt,
             ...) {
    const char* short_file = strrchr(file, '/');
    struct timespec ts;
    va_list msg;

    // Function gaurd
    if ((int)lvl < (int)LOG_LEVEL) {
        return;
    }

    // Grab precision time
    if (0 != clock_gettime(CLOCK_REALTIME, &ts)) {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }

    intmax_t ms = (intmax_t)(ts.tv_nsec / 1000000);  // milliseconds

    // Grab only the file name, not full path
    short_file = short_file ? short_file + 1 : file;

    // Log meta data header:
    // [seconds.milliseconds] [LEVEL] (file:line) func :
    fprintf(stderr, "[%" PRIdMAX ".%03" PRIdMAX "] [%-5s] (%s:%4d) %s : ",
            (intmax_t)ts.tv_sec, ms, get_log_lvl(lvl), short_file, line, func);

    // Actual passed in fmt msg
    va_start(msg, fmt);
    vfprintf(stderr, fmt, msg);
    va_end(msg);

    fputc('\n', stderr);  // Add newline

    // Ensures logs print even if the program crashes
    fflush(stderr);
}
