#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "platform.h"
#include "sig.h"
#include "crash.h"
#include "lock.h"
#include "atomicfile.h"
#include "autosave.h"

static int inited = 0;
static int previousRunning = 0;

static void platformAtExit(void)
{
    platform_shutdown();
}

void platform_init(void)
{
    if (inited) return;
    inited = 1;
    file_cleanup_tmp("data");
    if (!lock_acquire()) {
        printf("Game sudah berjalan di terminal lain.\n");
        fflush(stdout);
        exit(1);
    }
    previousRunning = lock_previous_running();
    sig_install();
    crash_install();
    autosave_start();
    atexit(platformAtExit);
}

int platform_previous_running(void) { return previousRunning; }

int platform_shutdown_requested(void) { return sig_requested(); }

void platform_shutdown(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    autosave_stop();
    sig_cleanup_done();
    lock_mark_clean();
}

void platform_sleep_ms(int ms)
{
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (long)(ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
#endif
}
