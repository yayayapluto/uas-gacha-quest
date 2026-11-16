#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lock.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#endif

#ifdef _WIN32
static HANDLE lockHandle = INVALID_HANDLE_VALUE;
#else
static int lockFd = -1;
#endif
static int previousRunning = 0;
static int contentionFlag = 0;

#ifdef _WIN32
#define LOCK_MARK_OFFSET 0x7FFFFFF0u
#endif

#ifdef _WIN32
static void lockWrite(const char *text)
{
    DWORD written = 0;
    if (lockHandle == INVALID_HANDLE_VALUE) return;
    SetFilePointer(lockHandle, 0, NULL, FILE_BEGIN);
    WriteFile(lockHandle, text, (DWORD)strlen(text), &written, NULL);
    SetEndOfFile(lockHandle);
    FlushFileBuffers(lockHandle);
}
#else
static void lockWrite(const char *text)
{
    if (lockFd < 0) return;
    if (lseek(lockFd, 0, SEEK_SET) == (off_t)-1) return;
    if (ftruncate(lockFd, 0) != 0) return;
    if (write(lockFd, text, strlen(text)) < 0) return;
    fsync(lockFd);
}
#endif

int lock_acquire(void)
{
#ifdef _WIN32
    OVERLAPPED ov;
    DWORD read = 0;
    char buf[128] = {0};
    lockHandle = CreateFileA("data\\session.lock", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (lockHandle == INVALID_HANDLE_VALUE) return 0;
    memset(&ov, 0, sizeof ov);
    /* Lock a byte far past the status text so readers of the content are
       not blocked by the lock range itself. */
    ov.Offset = LOCK_MARK_OFFSET;
    if (!LockFileEx(lockHandle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, 1, 0, &ov)) {
        CloseHandle(lockHandle);
        lockHandle = INVALID_HANDLE_VALUE;
        contentionFlag = 1;
        return 0;
    }
    SetFilePointer(lockHandle, 0, NULL, FILE_BEGIN);
    ReadFile(lockHandle, buf, sizeof buf - 1, &read, NULL);
    buf[read < sizeof buf ? read : sizeof buf - 1] = '\0';
    previousRunning = strncmp(buf, "RUNNING", 7) == 0;
    {
        char line[96];
        snprintf(line, sizeof line, "RUNNING pid=%lu start=%lu\n",
                 (unsigned long)GetCurrentProcessId(), (unsigned long)time(NULL));
        lockWrite(line);
    }
    return 1;
#else
    char buf[128] = {0};
    char line[96];
    ssize_t n;
    lockFd = open("data/session.lock", O_RDWR | O_CREAT, 0644);
    if (lockFd < 0) return 0;
    if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) {
        contentionFlag = 1;
        close(lockFd);
        lockFd = -1;
        return 0;
    }
    n = read(lockFd, buf, sizeof buf - 1);
    if (n > 0) buf[n] = '\0';
    previousRunning = strncmp(buf, "RUNNING", 7) == 0;
    snprintf(line, sizeof line, "RUNNING pid=%ld start=%ld\n",
             (long)getpid(), (long)time(NULL));
    lockWrite(line);
    return 1;
#endif
}

int lock_is_contention(void) { return contentionFlag; }

int lock_previous_running(void) { return previousRunning; }

void lock_mark_clean(void)
{
    lockWrite("CLEAN\n");
#ifdef _WIN32
    if (lockHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(lockHandle);
        lockHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (lockFd >= 0) {
        flock(lockFd, LOCK_UN);
        close(lockFd);
        lockFd = -1;
    }
#endif
}