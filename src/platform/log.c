#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.h"

#define LOG_PATH "data/game.log"
#define LOG_MAX_BYTES (1024L * 1024L)

static FILE *logFile = NULL;
static int logClosing = 0;

static void timestamp(char *out, size_t n)
{
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm);
}

static void writeLine(const char *level, const char *fmt, va_list ap)
{
    char ts[32];
    char text[1024];

    if (logFile == NULL) return;
    timestamp(ts, sizeof ts);
    vsnprintf(text, sizeof text, fmt, ap);
    fprintf(logFile, "[%s] %-5s %s\n", ts, level, text);
    fflush(logFile);
}

static void rotateLog(void)
{
    FILE *f = fopen(LOG_PATH, "rb");
    long size;
    if (f == NULL) return;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    if (size < LOG_MAX_BYTES) return;
    remove(LOG_PATH ".1");
    rename(LOG_PATH, LOG_PATH ".1");
}

void log_init(void)
{
    if (logFile != NULL) return;
    rotateLog();
    logFile = fopen(LOG_PATH, "a");
    if (logFile == NULL) return;
    setvbuf(logFile, NULL, _IOLBF, 0);
    atexit(log_close);
    log_event("===== sesi mulai =====");
}

void log_close(void)
{
    if (logFile == NULL || logClosing) return;
    logClosing = 1;
    log_event("===== sesi selesai =====");
    fclose(logFile);
    logFile = NULL;
}

void log_event(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    writeLine("INFO", fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    writeLine("WARN", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    writeLine("ERROR", fmt, ap);
    va_end(ap);
}

void log_crash(const char *fmt, ...)
{
    va_list ap;
    char text[1024];

    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);

    if (logFile != NULL) {
        char ts[32];
        timestamp(ts, sizeof ts);
        fprintf(logFile, "[%s] FATAL %s\n", ts, text);
        fflush(logFile);
        return;
    }
    {
        FILE *f = fopen(LOG_PATH, "a");
        if (f != NULL) {
            fprintf(f, "FATAL %s\n", text);
            fclose(f);
        }
    }
}
