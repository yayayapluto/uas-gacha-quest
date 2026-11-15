#ifndef TERM_H
#define TERM_H

typedef enum {
    KEY_NONE = 0,   /* timeout / nothing to read */
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_ENTER, KEY_ESC, KEY_RESIZE, KEY_EOF,
    KEY_CHAR
} TermKey;

void term_init(void);          /* after caps_detect; raw mode + alt screen if interactive */
void term_shutdown(void);      /* idempotent: restore cooked, cursor, colors, screen */
TermKey term_getkey(char *ch); /* blocks for one key; KEY_NONE on poll timeout */
#ifndef _WIN32
struct termios;
int term_saved_tio(struct termios *out); /* 1 when a cooked state was saved */
#endif
void term_raw(void);           /* force raw input (POSIX) */
void term_cooked(void);        /* force cooked input (POSIX), for line prompts */
void term_cursor(int on);
void term_alt(int on);         /* alternate screen */
int term_size_query(void);     /* refresh caps size; returns 1 when it changed */
void term_poll_size(void);     /* update caps size, set on resize */

#endif
