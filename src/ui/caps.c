#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "caps.h"
#include "style.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define IS_TTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define IS_TTY(fd) isatty(fd)
#endif

static ColorMode mode = COLOR_MONO;
static int unicode = 0;
static int interactive = 0;
static int cols = 80, rows = 24;

static const Glyphs glyphsUnicode = {
    "\xE2\x94\x8C", "\xE2\x94\x90", "\xE2\x94\x94", "\xE2\x94\x98", /* ┌ ┐ └ ┘ */
    "\xE2\x94\x80", "\xE2\x94\x82",                                 /* ─ │ */
    "\xE2\x96\x88", "\xE2\x96\x91",                                 /* █ ░ */
    "\xE2\x96\xB8 ",                                                /* ▸+space */
    "\xE2\x86\x91\xE2\x86\x93"                                      /* ↑↓ */
};
static const Glyphs glyphsAscii = {
    "+", "+", "+", "+", "-", "|",
    "#", "-",
    "> ",
    "Up/Down"
};

static const char *envNonEmpty(const char *name)
{
    const char *v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : NULL;
}

static int envFlag(const char *name)
{
    const char *v = envNonEmpty(name);
    return v != NULL && (v[0] == '1' || v[0] == 'y' || v[0] == 'Y');
}

static int strContains(const char *hay, const char *needle)
{
    return hay != NULL && strstr(hay, needle) != NULL;
}

static int colorFromEnv(void)
{
    const char *v = envNonEmpty("GACHA_COLOR");
    if (v == NULL) return -2;
    if (strContains(v, "mono")) return COLOR_MONO;
    if (strContains(v, "truecolor") || strContains(v, "24bit")) return COLOR_TRUE;
    if (strContains(v, "256")) return COLOR_256;
    if (strContains(v, "16")) return COLOR_16;
    return -2;
}

#ifdef _WIN32
static int enableWindowsVt(void)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode)) return 0;
    if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return 0;
    SetConsoleOutputCP(CP_UTF8);
    return 1;
}
#endif

void caps_detect(void)
{
    const char *term = getenv("TERM");
    const char *colorterm = getenv("COLORTERM");
    const char *loc[3];
    int i, utf8Loc = 0, forced;
    int base;

    interactive = IS_TTY(0) && IS_TTY(1) ? 1 : 0;
    if (envFlag("GACHA_TTY")) interactive = 1; /* test hook: pretend stdout is a terminal */

    if (envNonEmpty("NO_COLOR") != NULL) base = COLOR_MONO;
    else if (term != NULL && strcmp(term, "dumb") == 0) base = COLOR_MONO;
    else if (colorterm != NULL && (strContains(colorterm, "truecolor") || strContains(colorterm, "24bit")))
        base = COLOR_TRUE;
    else if (term != NULL && strContains(term, "256color")) base = COLOR_256;
    else if (term != NULL && term[0] != '\0' && strcmp(term, "dumb") != 0) base = COLOR_16;
    else base = COLOR_MONO;

#ifdef _WIN32
    if (interactive && !envFlag("GACHA_TTY")) {
        if (!enableWindowsVt()) {
            base = COLOR_MONO;
        } else if (base == COLOR_MONO && envNonEmpty("NO_COLOR") == NULL
                   && (term == NULL || term[0] == '\0')) {
            base = COLOR_TRUE; /* Windows console with VT: TERM is usually unset */
        }
    }
#endif

    if (!interactive) base = COLOR_MONO;
    mode = (ColorMode)base;

    forced = colorFromEnv();
    if (forced != -2) mode = (ColorMode)forced;

    if (term != NULL && (strcmp(term, "linux") == 0 || strcmp(term, "dumb") == 0)) {
        unicode = 0;
    } else {
        loc[0] = getenv("LC_ALL");
        loc[1] = getenv("LC_CTYPE");
        loc[2] = getenv("LANG");
        for (i = 0; i < 3; i++)
            if (strContains(loc[i], "UTF-8") || strContains(loc[i], "utf8")) utf8Loc = 1;
        unicode = utf8Loc;
#ifdef _WIN32
        if (interactive && mode != COLOR_MONO) unicode = 1;
#endif
    }
    if (envFlag("GACHA_ASCII")) unicode = 0;
    if (envFlag("GACHA_UNICODE")) unicode = 1;

    style_init();
}

ColorMode caps_color(void) { return mode; }
int caps_unicode(void) { return unicode; }
int caps_interactive(void) { return interactive; }
int caps_cols(void) { return cols; }
int caps_rows(void) { return rows; }
const Glyphs *caps_glyphs(void) { return unicode ? &glyphsUnicode : &glyphsAscii; }

void caps_set_size(int c, int r)
{
    if (c > 0) cols = c;
    if (r > 0) rows = r;
}
