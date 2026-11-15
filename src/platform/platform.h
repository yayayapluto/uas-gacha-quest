#ifndef PLATFORM_H
#define PLATFORM_H

/* One-time setup: orphan tmp cleanup, session lock, crash + shutdown signal
   handlers, autosave thread. Registers platform_shutdown via atexit so every
   exit path (normal or signal-triggered) flushes the autosave, stops the
   thread and marks the session CLEAN. */
void platform_init(void);              /* exits with a message if locked */

int platform_previous_running(void);   /* last session was RUNNING (crash/kill) */
int platform_shutdown_requested(void); /* shutdown signal arrived */
void platform_shutdown(void);          /* idempotent; CLEAN is written last */

#endif