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

    /* the message panel is always drawn, so a refusal does not shift the list */
    ui_panel(22, 0, UI_COLS, 3, "Info");
    if (last_message()[0] != '\0')
        render_text_clipped(23, 2, UI_COLS - 4, ST_TEXT, "%s", last_message());
    ui_footer(NULL);
}

/* Sell sub-screen: the inventory as a pickable list. Items that are equipped
   are listed but marked, so the player sees why they cannot be sold. */
typedef struct {
    const Player *p;
    int *rows;   /* original inventory index for each drawn row */
    int count;
} SellCtx;

static void sellDraw(void *ctx, int sel)
{
    const SellCtx *c = ctx;
    int i;

    ui_frame("Jual Item");
    ui_panel(2, 0, UI_COLS, 14, "Inventory");
    for (i = 0; i < c->count && i < 12; i++) {
        int idx = c->rows[i];
        const Item *it = &c->p->inventory[idx];
        int locked = idx == c->p->weaponIndex || idx == c->p->armorIndex;
        Style st = (i == sel) ? ST_SELECTED : style_rarity(it->rarity);
        char nameBuf[MAX_NAME];
        char stats[40];
        /* names are padded by bytes, so a long one would shift the columns;
           copying into a fixed field keeps every row aligned */
        snprintf(nameBuf, sizeof nameBuf, "%s", it->name);
        nameBuf[17] = '\0';   /* column is 17 cells wide */
        snprintf(stats, sizeof stats, "%s %-9s +%-5d", locked ? "!" : " ",
                 rarityName(it->rarity), it->power);
        render_text(3 + i, 2, st, "%-2d", idx);
        render_text(3 + i, 5, st, "%-17.17s", nameBuf);
        render_text_clipped(3 + i, 23, 17, st, "%s", stats);
        render_text(3 + i, 41, st, "%4d g", duplicatePrice(it->rarity));
    }
    if (c->count == 0)
        render_text_clipped(3, 2, UI_COLS - 4, ST_DIM, "Inventory kosong.");
    ui_message(16);
    ui_footer(NULL);
}

static int pickSellItem(Player *p)
{
    int rows[MAX_ITEM];
    SellCtx c;
    int i, sel;

    for (i = 0; i < p->itemCount; i++) rows[i] = i;
    c.p = p;
    c.rows = rows;
    c.count = p->itemCount;
    if (p->itemCount <= 0) return -1;
    sel = ui_pick_row(p->itemCount, sellDraw, &c);
    if (sel < 0) return -1;
    i = rows[sel];
    if (i == p->weaponIndex || i == p->armorIndex) {
        /* refusal is reported on the product screen, which owns the message
           panel; looping here would swallow the message unseen */
        set_message("Item sedang terpasang, tidak bisa dijual: %s.", p->inventory[i].name);
        return -1;
    }
    return i;
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
            index = pickSellItem(p);
            if (index < 0) break;
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