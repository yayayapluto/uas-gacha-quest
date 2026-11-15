#include <stdio.h>
#include <string.h>
#include "atomicfile.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#ifdef _WIN32
static unsigned thisPid(void) { return (unsigned)GetCurrentProcessId(); }
#else
static unsigned thisPid(void) { return (unsigned)getpid(); }
#endif

int file_write_atomic(const char *path, const void *data, size_t len)
{
    char tmp[512];
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
    snprintf(tmp, sizeof tmp, "%s.tmp.%u", path, thisPid());

#ifdef _WIN32
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    {
        DWORD written = 0;
        if (!WriteFile(h, data, (DWORD)len, &written, NULL) || written != (DWORD)len) {
            CloseHandle(h);
            DeleteFileA(tmp);
            return 0;
        }
        if (!FlushFileBuffers(h)) {
            CloseHandle(h);
            DeleteFileA(tmp);
            return 0;
        }
    }
    CloseHandle(h);
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(tmp);
        return 0;
    }
    return 1;
#else
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    {
        const char *p = data;
        size_t left = len;
        while (left > 0) {
            ssize_t n = write(fd, p, left);
            if (n <= 0) {
                close(fd);
                unlink(tmp);
                return 0;
            }
            p += n;
            left -= (size_t)n;
        }
    }
    if (fsync(fd) != 0) {
        close(fd);
        unlink(tmp);
        return 0;
    }
    close(fd);
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return 0;
    }
    {
        char dir[512];
        char *slash;
        int dfd;
        snprintf(dir, sizeof dir, "%s", path);
        slash = strrchr(dir, '/');
#ifdef _WIN32
        if (slash == NULL) slash = strrchr(dir, '\\');
#endif
        if (slash != NULL) {
            *slash = '\0';
            dfd = open(dir, O_RDONLY);
        } else {
            dfd = open(".", O_RDONLY);
        }
        if (dfd >= 0) {
            fsync(dfd);
            close(dfd);
        }
    }
    return 1;
#endif
}

void file_cleanup_tmp(const char *dir)
{
#ifdef _WIN32
    char pattern[512];
    WIN32_FIND_DATAA find;
    HANDLE h;
    snprintf(pattern, sizeof pattern, "%s\\*.tmp.*", dir);
    h = FindFirstFileA(pattern, &find);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char full[512];
        snprintf(full, sizeof full, "%s\\%s", dir, find.cFileName);
        DeleteFileA(full);
    } while (FindNextFileA(h, &find));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    if (d == NULL) return;
    while ((e = readdir(d)) != NULL) {
        if (strstr(e->d_name, ".tmp.") != NULL) {
            char full[512];
            snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
            unlink(full);
        }
    }
    closedir(d);
#endif
}