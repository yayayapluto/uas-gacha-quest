#ifndef UI_H
#define UI_H

#include "config.h"
#include "style.h"

/* Draw callback for ui_menu_pick: draws the whole screen for the given selection. */
typedef void (*UiDrawFn)(void *ctx, int sel);

void ui_init(void);
void ui_shutdown(void);
int ui_interactive(void);
void set_message(const char *fmt, ...);
const char *last_message(void);
void ui_wait_enter(const char *text);

/* Scrolling action log: shown in panels; printed immediately in fallback mode. */
void ui_log(const char *fmt, ...);
void ui_log_style(Style st, const char *fmt, ...);
void ui_log_clear(void);
int ui_log_count(void);
const char *ui_log_text(int i);
Style ui_log_style_at(int i);

void ui_frame(const char *screen);       /* start a frame: wipe grid, draw title row */
void ui_flush(void);                     /* send the frame to the terminal */

void ui_panel(int row, int col, int w, int h, const char *title);
void ui_hp_bar(int row, int col, int w, int hp, int maxHp);
void ui_bar(int row, int col, int w, int cur, int max, Style fill);
void ui_menu_v(int row, int col, const char *const *items, const int *nums, int n, int sel);
void ui_menu_h(int row, int col, const char *const *items, const int *nums, int n, int sel);
void ui_footer(const char *extra);
void ui_message(int row);                /* shows last_message when interactive */
void ui_status_panel_at(const Player *p, int row, int col, int w);

/* Menu loop shared by every screen. Returns the chosen number (not the index). */
int ui_menu_pick(const char *const *items, const int *nums, int n,
                 int minNum, int maxNum, UiDrawFn draw, void *ctx);

/* Line prompts: cooked input with cursor visible in interactive mode. */
void ui_line_begin(void);
void ui_line_end(void);
void ui_line_prompt(const char *fmt, ...);
void ui_line_error(const char *fmt, ...);

/* Row picker for tables (inventory equip / shop sell). Returns index or -1. */
int ui_pick_row(int count, UiDrawFn draw, void *ctx);

/* Legacy print-style API, kept until every screen is migrated. */
#define ANSI_RESET "\033[0m"
#define ANSI_CYAN "\033[36m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_DIM "\033[2m"

int ui_utf8_enabled(void);
void clear_screen(void);
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
