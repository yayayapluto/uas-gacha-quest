#include <stdio.h>
#include <string.h>
#include "config.h"
#include "shop.h"
#include "util.h"
#include "inventory.h"
#include "gacha.h"
#include "ui.h"
#include "render.h"
#include "autosave.h"

/* Shop as a product listing: each row is a product with its price and stock,
   plus a detail panel for the highlighted row (typical shop layout). */
typedef struct {
    Player *p;
    const char *name;
    const char *desc;
    int price;
    int stock;      /* remaining potion slots; -1 when not limited */
    char stat[64];
} ShopRow;

static void shopDraw(void *ctx, int sel)
{
    ShopRow *rows = ctx;
    int i;
    int cur = sel < 0 ? 0 : sel;  /* non-interactive draw passes -1 */

    ui_frame("Shop");
    ui_status_panel_at(rows[0].p, 2, 0, UI_COLS);

    ui_panel(9, 0, UI_COLS, 8, "Produk");
    render_text_clipped(10, 2, UI_COLS - 4, ST_DIM, "%-20s %8s %10s", "Nama", "Harga", "Stok");
    for (i = 0; i < 2; i++) {
        int row = 11 + i * 2;
        Style st = (i == cur) ? ST_SELECTED : ST_TEXT;
        char priceText[16], stockText[16];
        snprintf(priceText, sizeof priceText, "%d gold", rows[i].price);
        if (rows[i].stock < 0) snprintf(stockText, sizeof stockText, "banyak");
        else snprintf(stockText, sizeof stockText, "%d", rows[i].stock);
        render_text_clipped(row, 2, UI_COLS - 4, st, "%s%-19s%9s%11s",
                            i == cur ? "> " : "  ", rows[i].name, priceText, stockText);
    }
    render_text_clipped(15, 2, UI_COLS - 4, ST_TEXT, "0  Kembali");

    ui_panel(17, 0, UI_COLS, 5, "Detail");
    render_text_clipped(18, 2, UI_COLS - 4, ST_ACCENT, "%s", rows[cur].name);
    render_text_clipped(19, 2, UI_COLS - 4, ST_TEXT, "%s", rows[cur].desc);
    render_text_clipped(20, 2, UI_COLS - 4, ST_GOLD, "%s", rows[cur].stat);
    ui_footer(NULL);
}

void shopMenu(Player *p)
{
    static const char *items[] = { "Potion", "Jual item", "Kembali" };
    static const int nums[] = { 1, 2, 0 };
    ShopRow rows[2];

    ui_input_screen("Shop");
    for (;;) {
        int choice;

        rows[0].p = p;
        rows[0].name = "Potion";
        rows[0].desc = "Memulihkan HP saat battle";
        rows[0].price = POTION_COST;
        rows[0].stock = MAX_POTION - p->potion;
        snprintf(rows[0].stat, sizeof rows[0].stat, "Punya %d / maks %d", p->potion, MAX_POTION);

        rows[1].p = p;
        rows[1].name = "Jual item";
        rows[1].desc = "Jual item dari inventory";
        rows[1].price = 0;
        rows[1].stock = -1;
        snprintf(rows[1].stat, sizeof rows[1].stat, "%d item di inventory", p->itemCount);

        choice = ui_menu_pick(items, nums, 3, 0, 2, shopDraw, rows);
        if (choice == 0) return;
        switch (choice) {
        case 1: {
            int maxBuy, amount;
            if (p->potion >= MAX_POTION) {
                set_message("Stok potion sudah penuh (maks %d).", MAX_POTION);
                break;
            }
            maxBuy = (MAX_POTION - p->potion) < (p->gold / POTION_COST)
                     ? (MAX_POTION - p->potion) : (p->gold / POTION_COST);
            if (maxBuy == 0) {
                set_message("Gold tidak cukup untuk membeli potion.");
                break;
            }
            amount = readInt("Jumlah potion yang dibeli (1-5): ", 1, maxBuy);
            p->gold -= amount * POTION_COST;
            p->potion += amount;
            set_message("Beli %d potion. Gold: %d, Potion: %d.", amount, p->gold, p->potion);
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
            break;
        }
        case 2: {
            int oldWeaponId = -1, oldArmorId = -1;
            int index, i;
            if (p->itemCount == 0) {
                set_message("Inventory kosong, tidak ada yang bisa dijual.");
                break;
            }
            for (i = 0; i < p->itemCount; i++)
                ui_log_style(style_rarity(p->inventory[i].rarity),
                             "%-4d %-20s %-10s %-5d %d",
                             i, p->inventory[i].name, rarityName(p->inventory[i].rarity),
                             p->inventory[i].power, duplicatePrice(p->inventory[i].rarity));
            index = readInt("Nomor indeks item yang dijual (999 batal): ", 0, 999);
            if (index == 999) break;
            if (index >= p->itemCount) {
                set_message("Indeks tidak ada di inventory.");
                break;
            }
            if (index == p->weaponIndex || index == p->armorIndex) {
                set_message("Item sedang terpasang, tidak bisa dijual.");
                break;
            }
            {
                int price = duplicatePrice(p->inventory[index].rarity);
                if (p->weaponIndex >= 0) oldWeaponId = p->inventory[p->weaponIndex].id;
                if (p->armorIndex >= 0) oldArmorId = p->inventory[p->armorIndex].id;
                p->gold += price;
                set_message("Terjual: %s +%d gold.", p->inventory[index].name, price);
                for (i = index; i < p->itemCount - 1; i++)
                    p->inventory[i] = p->inventory[i + 1];
                p->itemCount--;
                syncEquipped(p, oldWeaponId, oldArmorId);
            }
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
            break;
        }
        }
    }
}