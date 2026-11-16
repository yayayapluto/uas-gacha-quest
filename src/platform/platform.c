#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "platform.h"
#include "sig.h"
#include "crash.h"
#include "lock.h"
#include "atomicfile.h"
#include "autosave.h"

static int inited = 0;
static int previousRunning = 0;

/* Resolve the install root (exe dir's parent) and make it the working
   directory, so the relative "data/" paths in lock, storage, autosave and
   crash all resolve to the real data directory regardless of where the
   user launched the binary from. No-op if the data directory is not found
   there, so a broken install keeps its original (misleading) behaviour. */
static void anchor_data_root(void)
{
#ifdef _WIN32
    char exe[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return;
    {
        char *p = strrchr(exe, '\\');
        if (p) *p = '\0';
    }
    {
        char *q = strrchr(exe, '\\');
        if (q) *q = '\0';
    }
    {
        char test[MAX_PATH + 32];
        snprintf(test, sizeof test, "%s\\data\\item.txt", exe);
        if (GetFileAttributesA(test) == INVALID_FILE_ATTRIBUTES) return;
        SetCurrentDirectoryA(exe);
    }
#else
    {
        char exe_path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (len > 0 && len < (ssize_t)sizeof exe_path) {
            char *p, *q;
            char test[PATH_MAX + 32];
            exe_path[len] = '\0';
            p = strrchr(exe_path, '/');
            if (p) *p = '\0';
            q = strrchr(exe_path, '/');
            if (q) *q = '\0';
            snprintf(test, sizeof test, "%s/data/item.txt", exe_path);
            if (access(test, F_OK) == 0) chdir(exe_path);
        }
    }
#endif
}

static void platformAtExit(void)
{
    platform_shutdown();
}

void platform_init(void)
{
    if (inited) return;
    inited = 1;
    anchor_data_root();
    file_cleanup_tmp("data");
    if (!lock_acquire()) {
        if (lock_is_contention())
            printf("Game sudah berjalan di terminal lain.\n");
        else
            printf("Tidak dapat membuka data/session.lock (folder data tidak ditemukan atau tidak bisa ditulis).\n");
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