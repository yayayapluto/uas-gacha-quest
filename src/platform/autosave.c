#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "autosave.h"
#include "atomicfile.h"
#include "rng.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#endif

#define AS_MAGIC "GQAS"
#define AS_VERSION 1u
#define AS_DATA "data/autosave.dat"
#define AS_BAK "data/autosave.bak"

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int len;
    unsigned int crc;
} AsHeader;

#ifdef _WIN32
static CRITICAL_SECTION mtx;
static CONDITION_VARIABLE cv;
static HANDLE threadHandle = NULL;
#else
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static pthread_t threadHandle;
static int threadValid = 0;
#endif

static AutoSnapshot pending;
static int pendingFlag = 0;
static int importantFlag = 0;
static int stopFlag = 0;
static unsigned int lastCrc = 0;
static int haveLastCrc = 0;
static char errorText[160];
static int errorFlag = 0;
static int debounceMs = 1500;

static unsigned int crc32Compute(const unsigned char *p, size_t n)
{
    unsigned int crc = 0xFFFFFFFFu;
    size_t i;
    int b;
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

static void sleepMs(long ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static void setError(const char *msg)
{
    snprintf(errorText, sizeof errorText, "%s", msg);
    errorFlag = 1;
}

/* Serializes header + payload and rotates the previous file to .bak. */
static int writeSnapshotFile(const AutoSnapshot *snap)
{
    static char buf[sizeof(AsHeader) + sizeof(AutoSnapshot)];
    AsHeader *h = (AsHeader *)buf;
    unsigned int crc;
    size_t total;

    crc = crc32Compute((const unsigned char *)snap, sizeof *snap);
    if (haveLastCrc && crc == lastCrc) return 1;
    memcpy(h->magic, AS_MAGIC, 4);
    h->version = AS_VERSION;
    h->len = (unsigned int)sizeof *snap;
    h->crc = crc;
    memcpy(buf + sizeof *h, snap, sizeof *snap);
    total = sizeof *h + sizeof *snap;

    remove(AS_BAK);
    if (rename(AS_DATA, AS_BAK) != 0) {
#ifdef _WIN32
        /* no previous file: rename failure is expected and harmless */
#endif
    }
    if (!file_write_atomic(AS_DATA, buf, total)) {
        setError("Autosave gagal: tidak bisa menulis data/autosave.dat.");
        return 0;
    }
    lastCrc = crc;
    haveLastCrc = 1;
    return 1;
}

#ifdef _WIN32
static DWORD WINAPI autosaveThread(LPVOID arg)
{
    (void)arg;
    for (;;) {
        AutoSnapshot cur;
        int imp;
        EnterCriticalSection(&mtx);
        while (!pendingFlag && !stopFlag)
            SleepConditionVariableCS(&cv, &mtx, INFINITE);
        if (stopFlag && !pendingFlag) {
            LeaveCriticalSection(&mtx);
            break;
        }
        cur = pending;
        pendingFlag = 0;
        imp = importantFlag;
        importantFlag = 0;
        LeaveCriticalSection(&mtx);
        if (!imp) {
            sleepMs(debounceMs);
            EnterCriticalSection(&mtx);
            if (pendingFlag) {
                LeaveCriticalSection(&mtx);
                continue;
            }
            LeaveCriticalSection(&mtx);
        }
        if (!writeSnapshotFile(&cur)) {
            long delay = 1000;
            int gaveUp = 0;
            while (!gaveUp) {
                sleepMs(delay);
                EnterCriticalSection(&mtx);
                if (stopFlag) gaveUp = 1;
                LeaveCriticalSection(&mtx);
                if (gaveUp) break;
                if (writeSnapshotFile(&cur)) gaveUp = 1;
                else if (delay < 30000) delay *= 2;
            }
        }
    }
    return 0;
}
#else
static void *autosaveThread(void *arg)
{
    (void)arg;
    for (;;) {
        AutoSnapshot cur;
        int imp;
        pthread_mutex_lock(&mtx);
        while (!pendingFlag && !stopFlag)
            pthread_cond_wait(&cv, &mtx);
        if (stopFlag && !pendingFlag) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        cur = pending;
        pendingFlag = 0;
        imp = importantFlag;
        importantFlag = 0;
        pthread_mutex_unlock(&mtx);
        if (!imp) {
            sleepMs(debounceMs);
            pthread_mutex_lock(&mtx);
            if (pendingFlag) {
                pthread_mutex_unlock(&mtx);
                continue;
            }
            pthread_mutex_unlock(&mtx);
        }
        if (!writeSnapshotFile(&cur)) {
            long delay = 1000;
            int gaveUp = 0;
            while (!gaveUp) {
                sleepMs(delay);
                pthread_mutex_lock(&mtx);
                if (stopFlag) gaveUp = 1;
                pthread_mutex_unlock(&mtx);
                if (gaveUp) break;
                if (writeSnapshotFile(&cur)) gaveUp = 1;
                else if (delay < 30000) delay *= 2;
            }
        }
    }
    return NULL;
}
#endif

static int started = 0;
static int syncReady = 0;

/* Locks created lazily so publish/stop are safe before autosave_start(). */
static void syncInitOnce(void)
{
    if (syncReady) return;
    syncReady = 1;
#ifdef _WIN32
    InitializeCriticalSection(&mtx);
    InitializeConditionVariable(&cv);
#endif
}


static long envDebounce(void)
{
    const char *v = getenv("GACHA_AUTOSAVE_MS");
    long ms;
    if (v == NULL || v[0] == '\0') return 1500;
    ms = atol(v);
    if (ms < 200) ms = 200;
    return ms;
}

void autosave_start(void)
{
    started = 1;
    syncInitOnce();
    debounceMs = (int)envDebounce();
#ifdef _WIN32
    threadHandle = CreateThread(NULL, 0, autosaveThread, NULL, 0, NULL);
#else
    threadValid = pthread_create(&threadHandle, NULL, autosaveThread, NULL) == 0;
#endif
}

void autosave_stop(void)
{
    syncInitOnce();
    if (!started) return;
    started = 0;
#ifdef _WIN32
    if (threadHandle == NULL) return;
    EnterCriticalSection(&mtx);
    stopFlag = 1;
    WakeConditionVariable(&cv);
    LeaveCriticalSection(&mtx);
    WaitForSingleObject(threadHandle, INFINITE);
    threadHandle = NULL;
#else
    if (!threadValid) return;
    pthread_mutex_lock(&mtx);
    stopFlag = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
    pthread_join(threadHandle, NULL);
    threadValid = 0;
#endif
}

void autosave_publish(const Player *p, int screen,
                      const Monster *enemy, int enemyHp,
                      const DungeonState *dst, int steps, int hints,
                      int important)
{
    AutoSnapshot snap;
    memset(&snap, 0, sizeof snap);
    snap.player = *p;
    snap.screen = screen;
    if (enemy != NULL) {
        snap.battleActive = 1;
        snap.battleEnemy = *enemy;
        snap.battleEnemyHp = enemyHp;
    }
    if (dst != NULL) {
        snap.dungeonActive = 1;
        snap.dungeon = *dst;
        snap.dungeonSteps = steps;
        snap.dungeonHints = hints;
    }
    snap.rng = plat_rng_state();
    syncInitOnce();
    if (!started) {
        writeSnapshotFile(&snap);
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&mtx);
    pending = snap;
    pendingFlag = 1;
    if (important) importantFlag = 1;
    WakeConditionVariable(&cv);
    LeaveCriticalSection(&mtx);
#else
    pthread_mutex_lock(&mtx);
    pending = snap;
    pendingFlag = 1;
    if (important) importantFlag = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
#endif
}

static int loadFile(const char *path, AutoSnapshot *out)
{
    static unsigned char buf[sizeof(AsHeader) + sizeof(AutoSnapshot)];
    FILE *fp = fopen(path, "rb");
    size_t n;
    const AsHeader *h;
    if (fp == NULL) return 0;
    n = fread(buf, 1, sizeof buf, fp);
    fclose(fp);
    if (n < sizeof(AsHeader)) return 0;
    h = (const AsHeader *)buf;
    if (memcmp(h->magic, AS_MAGIC, 4) != 0) return 0;
    if (h->version != AS_VERSION) return 0;
    if (h->len != sizeof(AutoSnapshot)) return 0;
    if ((size_t)n != sizeof(AsHeader) + sizeof(AutoSnapshot)) return 0;
    if (crc32Compute(buf + sizeof *h, h->len) != h->crc) return 0;
    memcpy(out, buf + sizeof *h, sizeof *out);
    return 1;
}

int autosave_load(AutoSnapshot *out)
{
    if (loadFile(AS_DATA, out)) return 1;
    return loadFile(AS_BAK, out);
}

int autosave_poll_error(char *buf, int len)
{
    int had = 0;
    syncInitOnce();
#ifdef _WIN32
    EnterCriticalSection(&mtx);
    if (errorFlag) {
        had = 1;
        snprintf(buf, (size_t)len, "%s", errorText);
        errorFlag = 0;
    }
    LeaveCriticalSection(&mtx);
#else
    pthread_mutex_lock(&mtx);
    if (errorFlag) {
        had = 1;
        snprintf(buf, (size_t)len, "%s", errorText);
        errorFlag = 0;
    }
    pthread_mutex_unlock(&mtx);
#endif
    return had;
}