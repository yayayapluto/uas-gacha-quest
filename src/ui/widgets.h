#ifndef WIDGETS_H
#define WIDGETS_H

#include "style.h"

void widget_panel(int row, int col, int w, int h, const char *title);
void widget_hp_bar(int row, int col, int w, int hp, int maxHp);
void widget_bar(int row, int col, int w, int cur, int max, Style fill);
void widget_menu_v(int row, int col, const char *const *items, const int *nums,
                   int n, int sel);
void widget_menu_h(int row, int col, const char *const *items, const int *nums,
                   int n, int sel);
void widget_footer(const char *extra);
void widget_msg(int row, int col, int w, const char *msg);

#endif
