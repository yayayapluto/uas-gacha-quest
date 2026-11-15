#ifndef CRASH_H
#define CRASH_H

/* Fatal-signal handlers (POSIX): restore the terminal from static data,
   append data/crash.log, then re-raise with the default disposition.
   Windows has no sigaltstack; recovery there relies on the session lock. */
void crash_install(void);

#endif