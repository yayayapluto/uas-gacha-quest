#ifndef LOCK_H
#define LOCK_H

/* Exclusive non-blocking lock on data/session.lock held for the process
   lifetime. Content is "RUNNING pid=<pid> start=<epoch>" while playing and
   "CLEAN" after a graceful exit; a RUNNING status found at startup means the
   previous process crashed or was force-killed. */
int lock_acquire(void);            /* 1 acquired, 0 another instance holds it */
int lock_previous_running(void);   /* 1 when the pre-acquire status was RUNNING */
void lock_mark_clean(void);        /* final step of a graceful exit */

#endif