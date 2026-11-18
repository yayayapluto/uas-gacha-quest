#ifndef LOG_H
#define LOG_H

/* Lightweight app log to data/game.log. Every line is flushed, so a crash
   leaves the last event before the crash on disk. */
void log_init(void);
void log_close(void);
void log_event(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);
/* Usable from crash handlers; appends even when the normal log is closed. */
void log_crash(const char *fmt, ...);

#endif
