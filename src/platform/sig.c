#include "sig.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#endif

static volatile sig_atomic_t shutdownFlag = 0;
static volatile int cleanupDone = 0;
#ifdef _WIN32
static HANDLE wakeEvent = NULL;
#else
static int wakePipe[2] = {-1, -1};
#endif

#ifdef _WIN32
static BOOL WINAPI ctrlHandler(DWORD type)
{
    (void)type;
    if (shutdownFlag) return TRUE;
    shutdownFlag = 1;
    if (wakeEvent != NULL) SetEvent(wakeEvent);
    Sleep(3000);
    if (!cleanupDone) _exit(130);
    return TRUE;
}
#else
static void sigHandler(int sig)
{
    (void)sig;
    if (shutdownFlag) return;
    shutdownFlag = 1;
    if (wakePipe[1] >= 0) {
        ssize_t r = write(wakePipe[1], "x", 1);
        (void)r;
    }
}

static void *watchdog(void *arg)
{
    (void)arg;
    while (!shutdownFlag) {
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    sleep(3);
    if (!cleanupDone) _exit(130);
    return NULL;
}
#endif

void sig_install(void)
{
#ifdef _WIN32
    wakeEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#else
    struct sigaction sa;
    pthread_t wd;
    if (pipe(wakePipe) != 0) {
        wakePipe[0] = wakePipe[1] = -1;
    }
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* no SA_RESTART: interrupted poll/read wakes the loop */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    if (pthread_create(&wd, NULL, watchdog, NULL) == 0)
        pthread_detach(wd);
#endif
}

int sig_requested(void) { return shutdownFlag != 0; }

void sig_cleanup_done(void) { cleanupDone = 1; }

int sig_wake_fd(void)
{
#ifdef _WIN32
    (void)wakeEvent;
    return -1;
#else
    return wakePipe[0];
#endif
}