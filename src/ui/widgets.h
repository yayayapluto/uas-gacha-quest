#ifndef WIDGETS_H
#define WIDGETS_H

#include "style.h"

void widget_panel(int row, int col, int w, int h, const char *title);
void widget_hp_bar(int row, int col, int w, int hp, int maxHp);
/* hp_bar(nilai, maks, lebar): exactly w cells, threshold colour, dark empty. */
void widget_hp_bar_n(int row, int col, int w, int hp, int maxHp);
void widget_bar(int row, int col, int w, int cur, int max, Style fill);
void widget_menu_v(int row, int col, const char *const *items, const int *nums,
                   int n, int sel);
void widget_menu_h(int row, int col, const char *const *items, const int *nums,
                   int n, int sel);
void widget_footer(const char *extra);
void widget_footer_line(const char *text); /* custom footer, no menu keys */
void widget_footer_reset(void);   /* new frame: footer may pick a new row */
void widget_msg(int row, int col, int w, const char *msg);

#endif
