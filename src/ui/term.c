#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "term.h"
#include "caps.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <signal.h>
#endif

static int inited = 0;
static int rawOn = 0;
static int altOn = 0;
static int lastCols = -1, lastRows = -1;
#ifndef _WIN32
static struct termios savedTio;
static int haveSaved = 0;
#endif

/* Returns 1 when the terminal size changed since the previous call. */
static int querySize(void)
{
    int changed = 0;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(out, &info)) {
        int c = info.srWindow.Right - info.srWindow.Left + 1;
        int r = info.srWindow.Bottom - info.srWindow.Top + 1;
        if (lastCols >= 0 && (c != lastCols || r != lastRows)) changed = 1;
        lastCols = c;
        lastRows = r;
        caps_set_size(c, r);
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        if (lastCols >= 0 && ((int)ws.ws_col != lastCols || (int)ws.ws_row != lastRows)) changed = 1;
        lastCols = ws.ws_col;
        lastRows = ws.ws_row;
        caps_set_size(ws.ws_col, ws.ws_row);
    }
#endif
    return changed;
}

int term_size_query(void) { return caps_interactive() ? querySize() : 0; }

#ifndef _WIN32
static volatile sig_atomic_t winchFlag = 0;
static void onWinch(int sig) { (void)sig; winchFlag = 1; }

/* Handlers restore the terminal before dying; SIGTSTP/SIGCONT swap modes. */
static void onFatal(int sig)
{
    term_shutdown();
    signal(sig, SIG_DFL);
    raise(sig);
}
static void onStop(int sig)
{
    (void)sig;
    term_cooked();
    term_cursor(1);
    term_alt(0);
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}
static void onCont(int sig)
{
    (void)sig;
    signal(SIGCONT, onCont);
    if (rawOn) term_raw();
    term_alt(1);
    term_cursor(0);
}
#endif

static void enterRaw(void)
{
#ifdef _WIN32
    /* _getch performs no echo and no line editing; console mode untouched */
    rawOn = 1;
#else
    struct termios t;
    if (!haveSaved) return;
    t = savedTio;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    rawOn = 1;
#endif
}

void term_raw(void) { if (caps_interactive() && !rawOn) enterRaw(); }

void term_cooked(void)
{
#ifdef _WIN32
    rawOn = 0;
#else
    if (haveSaved) tcsetattr(STDIN_FILENO, TCSANOW, &savedTio);
    rawOn = 0;
#endif
}

void term_cursor(int on)
{
    if (!caps_interactive()) return;
    fputs(on ? "\033[?25h" : "\033[?25l", stdout);
}

void term_alt(int on)
{
    if (!caps_interactive()) return;
    if (on && !altOn) { fputs("\033[?1049h", stdout); altOn = 1; }
    else if (!on && altOn) { fputs("\033[?1049l", stdout); altOn = 0; }
    fflush(stdout);
}

void term_init(void)
{
    if (!caps_interactive()) return;
#ifdef _WIN32
    /* no terminal state to save; VT already enabled by caps_detect */
#else
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        savedTio = t;
        haveSaved = 1;
    }
    signal(SIGWINCH, onWinch);
    signal(SIGINT, onFatal);
    signal(SIGTERM, onFatal);
    signal(SIGHUP, onFatal);
    signal(SIGTSTP, onStop);
    signal(SIGCONT, onCont);
#endif
    querySize();
    term_alt(1);
    term_cursor(0);
    enterRaw();
    inited = 1;
}

void term_shutdown(void)
{
    if (!inited) return;
    inited = 0;
    if (caps_interactive()) {
        fputs("\033[0m", stdout);
        term_cursor(1);
        term_alt(0);
        fflush(stdout);
    }
    term_cooked();
}

TermKey term_getkey(char *ch)
{
    if (!caps_interactive()) return KEY_EOF;
#ifdef _WIN32
    {
        int c = _getch();
        if (c == 0xE0 || c == 0) {
            int c2 = _getch();
            querySize();
            switch (c2) {
            case 'H': return KEY_UP;
            case 'P': return KEY_DOWN;
            case 'K': return KEY_LEFT;
            case 'M': return KEY_RIGHT;
            default: return KEY_NONE;
            }
        }
        querySize();
        if (c == '\r' || c == '\n') return KEY_ENTER;
        if (c == 27) return KEY_ESC;
        if (c == 3 || c == 26) return KEY_EOF;
        *ch = (char)c;
        return KEY_CHAR;
    }
#else
    {
        unsigned char c;
        struct pollfd pfd;
        int ready;
        if (winchFlag) {
            winchFlag = 0;
            querySize();
            return KEY_RESIZE;
        }
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        ready = poll(&pfd, 1, 30);
        if (ready <= 0) {
            querySize();
            return winchFlag ? KEY_RESIZE : KEY_NONE;
        }
        if (read(STDIN_FILENO, &c, 1) != 1) return KEY_EOF;
        if (c == 27) {
            /* lone ESC vs arrow prefix: short poll decides */
            pfd.events = POLLIN;
            ready = poll(&pfd, 1, 30);
            if (ready <= 0) return KEY_ESC;
            if (read(STDIN_FILENO, &c, 1) != 1) return KEY_ESC;
            if (c == '[' || c == 'O') {
                unsigned char c2;
                if (read(STDIN_FILENO, &c2, 1) != 1) return KEY_ESC;
                switch (c2) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                default: return KEY_NONE;
                }
            }
            return KEY_NONE;
        }
        if (c == '\r' || c == '\n') return KEY_ENTER;
        *ch = (char)c;
        return KEY_CHAR;
    }
#endif
}
