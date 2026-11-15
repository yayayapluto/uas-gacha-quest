#ifndef UI_H
#define UI_H

#include "config.h"

#define UI_WIDTH 50

#define ANSI_RESET "\033[0m"
#define ANSI_CYAN "\033[36m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_DIM "\033[2m"

void ui_init(void);
int ui_utf8_enabled(void);
void clear_screen(void);
void set_message(const char *fmt, ...);
const char *last_message(void);
void ui_wait_enter(const char *text);

void draw_box_top(const char *title);
void draw_box_bottom(void);
void draw_box_line(const char *fmt, ...);
void format_hp_bar(char *buf, int bufSize, int hp, int maxHp);

void draw_header(const char *title, const Player *p);
void draw_hp_bar(int hp, int maxHp);
void draw_menu(int number, const char *label);

const char *rarity_color(int rarity);
const char *hp_color(int hp, int maxHp);

#endif
