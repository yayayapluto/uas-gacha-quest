#include "crash.h"

#ifdef _WIN32

void crash_install(void)
{
}

#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include "term.h"

static char crashStack[64 * 1024];
static struct sigaction fatalSa;

static const char restoreSeq[] = "\033[?1049l\033[?25h\033[0m";

static void writeAll(int fd, const char *p, size_t n)
{
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return;
        p += w;
        n -= (size_t)w;
    }
}

static void writeDec(int fd, long v)
{
    char buf[24];
    int i = (int)sizeof buf;
    unsigned long u = v < 0 ? (unsigned long)-(v + 1) + 1UL : (unsigned long)v;
    if (u == 0) buf[--i] = '0';
    while (u > 0) {
        buf[--i] = (char)('0' + u % 10);
        u /= 10;
    }
    writeAll(fd, buf + i, (size_t)(sizeof buf - (size_t)i));
}

static void fatalHandler(int sig)
{
    struct termios tio;
    int fd;
    writeAll(STDOUT_FILENO, restoreSeq, sizeof restoreSeq - 1);
    if (term_saved_tio(&tio))
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    fd = open("data/crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        writeAll(fd, "crash sig=", 10);
        writeDec(fd, sig);
        writeAll(fd, " pid=", 5);
        writeDec(fd, (long)getpid());
        writeAll(fd, "\n", 1);
        close(fd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void crash_install(void)
{
    stack_t ss;
    ss.ss_sp = crashStack;
    ss.ss_size = sizeof crashStack;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&fatalSa, 0, sizeof fatalSa);
    fatalSa.sa_handler = fatalHandler;
    sigemptyset(&fatalSa.sa_mask);
    fatalSa.sa_flags = SA_ONSTACK | SA_RESETHAND | SA_NODEFER;
    sigaction(SIGSEGV, &fatalSa, NULL);
    sigaction(SIGBUS, &fatalSa, NULL);
    sigaction(SIGFPE, &fatalSa, NULL);
    sigaction(SIGILL, &fatalSa, NULL);
    sigaction(SIGABRT, &fatalSa, NULL);
}
#endif