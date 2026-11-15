#ifndef SIG_H
#define SIG_H

/* Shutdown signalling: handlers only set a flag and poke the self-pipe so a
   poll() on stdin wakes immediately. A watchdog force-exits 3 s later if the
   cleanup path hangs. A second signal is ignored. */
void sig_install(void);            /* handlers + self-pipe + watchdog */
int sig_requested(void);           /* 1 once a shutdown signal arrived */
void sig_cleanup_done(void);       /* watchdog stands down after CLEAN */
int sig_wake_fd(void);             /* fd to poll besides stdin; -1 on Windows */

#endif