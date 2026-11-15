#include <stdio.h>
#include "ui.h"
#include "render.h"
#include "caps.h"
#include "term.h"

/* Mini test screen: shows every style, panels, menus, bars in the active mode. */
static void drawAll(void)
{
    static const char *names[] = { "text", "title", "accent", "border", "dim", "gold",
                                   "danger", "success", "common", "rare", "epic", "legendary" };
    static const Style toks[] = { ST_TEXT, ST_TITLE, ST_ACCENT, ST_BORDER, ST_DIM, ST_GOLD,
                                  ST_DANGER, ST_SUCCESS, ST_RARITY_COMMON, ST_RARITY_RARE,
                                  ST_RARITY_EPIC, ST_RARITY_LEGENDARY };
    int i;

    ui_frame("Preview");
    ui_panel(2, 0, 50, 8, "Panel");
    for (i = 0; i < 12; i++)
        render_text(3 + i / 4, 2 + (i % 4) * 12, toks[i], "%s", names[i]);
    ui_hp_bar(6, 2, 14, 65, 100);
    ui_hp_bar(6, 18, 14, 45, 100);
    ui_hp_bar(6, 34, 14, 15, 100);
    ui_bar(7, 2, 22, 40, 100, ST_SUCCESS);
    ui_bar(7, 26, 22, 70, 100, ST_GOLD);
    {
        static const char *items[] = { "Pilihan satu", "Pilihan dua", "Pilihan tiga" };
        static const int nums[] = { 1, 2, 3 };
        ui_menu_v(10, 0, items, nums, 3, 1);
        ui_menu_h(14, 0, items, nums, 3, 0);
    }
    set_message("Contoh pesan sistem.");
    ui_message(16);
    ui_footer(NULL);
}

int main(void)
{
    ui_init();
    term_alt(0); /* keep the preview visible after exit */
    term_cursor(1);
    drawAll();
    ui_flush();
    ui_shutdown();
    return 0;
}
