#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "widgets.h"
#include "render.h"
#include "caps.h"

void widget_panel(int row, int col, int w, int h, const char *title)
{
    const Glyphs *g = caps_glyphs();
    int i, tlen = ui_visible_len(title);
    Style bs = ST_BORDER;

    render_text(row, col, bs, "%s", g->tl);
    render_text(row, col + 1, ST_ACCENT, "%s", title);
    for (i = 1 + tlen; i < w - 1; i++) render_text(row, col + i, bs, "%s", g->hz);
    render_text(row, col + w - 1, bs, "%s", g->tr);
    for (i = 1; i < h - 1; i++) {
        render_text(row + i, col, bs, "%s", g->vr);
        render_text(row + i, col + w - 1, bs, "%s", g->vr);
    }
    render_text(row + h - 1, col, bs, "%s", g->bl);
    for (i = 1; i < w - 1; i++) render_text(row + h - 1, col + i, bs, "%s", g->hz);
    render_text(row + h - 1, col + w - 1, bs, "%s", g->br);
}

/* HP bar: gradient per cell in truecolor, threshold color in 256/16, mono relies on chars */
void widget_hp_bar(int row, int col, int w, int hp, int maxHp)
{
    const Glyphs *g = caps_glyphs();
    int filled = maxHp > 0 ? hp * (w - 2) / maxHp : 0;
    int i;

    if (filled < 0) filled = 0;
    if (filled > w - 2) filled = w - 2;
    for (i = 0; i < w - 2; i++) {
        Style s;
        if (caps_color() == COLOR_TRUE && filled > 0 && i < filled)
            s = ST_HP_GRAD((i * 10) / (filled > 1 ? filled - 1 : 1));
        else
            s = style_hp(hp, maxHp);
        if (i < filled) render_text(row, col + 1 + i, s, "%s", g->blockFull);
        else render_text(row, col + 1 + i, ST_DIM, "%s", g->blockEmpty);
    }
}

void widget_bar(int row, int col, int w, int cur, int max, Style fill)
{
    const Glyphs *g = caps_glyphs();
    int filled = max > 0 ? cur * (w - 2) / max : 0;
    int i;

    if (filled < 0) filled = 0;
    if (filled > w - 2) filled = w - 2;
    for (i = 0; i < w - 2; i++) {
        if (i < filled) render_text(row, col + 1 + i, fill, "%s", g->blockFull);
        else render_text(row, col + 1 + i, ST_DIM, "%s", g->blockEmpty);
    }
}

static void menu_item(int row, int col, int num, const char *label, int sel)
{
    if (sel) {
        render_text(row, col, ST_SELECTED, "%s%d. %s", caps_glyphs()->marker, num, label);
    } else {
        render_text(row, col, ST_TEXT, "  %d. %s", num, label);
    }
}

void widget_menu_v(int row, int col, const char *const *items, const int *nums,
                   int n, int sel)
{
    int i;
    for (i = 0; i < n; i++)
        menu_item(row + i, col, nums[i], items[i], i == sel);
}

void widget_menu_h(int row, int col, const char *const *items, const int *nums,
                   int n, int sel)
{
    int i, x = col;
    for (i = 0; i < n; i++) {
        char text[128];
        snprintf(text, sizeof text, "%s%d. %s", i == sel ? caps_glyphs()->marker : "  ",
                 nums[i], items[i]);
        render_text(row, x, i == sel ? ST_SELECTED : ST_TEXT, "%s", text);
        x += ui_visible_len(text) + 2;
    }
}

void widget_footer(const char *extra)
{
    char buf[192];
    snprintf(buf, sizeof buf, "%s pilih  Enter konfirmasi  Esc kembali%s%s",
             caps_glyphs()->arrowUD, extra != NULL ? "  " : "", extra != NULL ? extra : "");
    render_text(render_max_row() + 2, 0, ST_DIM, "%s", buf);
}

void widget_msg(int row, int col, int w, const char *msg)
{
    if (msg == NULL || msg[0] == '\0') return;
    render_text(row, col, ST_ACCENT, "%s", caps_glyphs()->marker);
    render_text(row, col + 2, ST_TEXT, "%.*s", w - 4, msg);
}
