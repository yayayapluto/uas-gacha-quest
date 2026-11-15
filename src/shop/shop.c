#include <stdio.h>
#include <string.h>
#include "config.h"
#include "shop.h"
#include "util.h"
#include "inventory.h"
#include "gacha.h"
#include "ui.h"
#include "render.h"

static void shopDraw(void *ctx, int sel)
{
    static const char *items[] = { "Beli potion (40 gold)", "Jual item", "Kembali" };
    static const int nums[] = { 1, 2, 0 };
    Player *p = ctx;

    ui_frame("Shop");
    ui_status_panel_at(p, 2, 0, UI_COLS);
    ui_menu_v(9, 0, items, nums, 3, sel);
    ui_message(13);
    ui_footer(NULL);
}

void shopMenu(Player *p)
{
    static const char *items[] = { "Beli potion (40 gold)", "Jual item", "Kembali" };
    static const int nums[] = { 1, 2, 0 };

    for (;;) {
        int choice = ui_menu_pick(items, nums, 3, 0, 2, shopDraw, p);
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
            break;
        }
        }
    }
}