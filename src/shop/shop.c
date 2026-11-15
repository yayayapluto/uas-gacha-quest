#include <stdio.h>
#include <string.h>
#include "config.h"
#include "shop.h"
#include "util.h"
#include "inventory.h"
#include "gacha.h"
#include "ui.h"

/* Shop menu: buy potions or sell items. */
void shopMenu(Player *p)
{
    int choice;

    for (;;) {
        clear_screen();
        draw_header("SHOP", p);
        draw_menu(1, "Beli potion (40 gold)");
        draw_menu(2, "Jual item");
        draw_menu(0, "Kembali");
        if (last_message()[0] != '\0')
            printf("%s%s%s\n", ANSI_CYAN, last_message(), ANSI_RESET);
        printf("Pilih: ");
        choice = readInt("", 0, 2);
        set_message("");
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
            printf("Jumlah potion yang dibeli (1-%d): ", maxBuy);
            amount = readInt("", 1, maxBuy);
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
            printf("%-4s %-20s %-10s %-5s %s\n", "Idx", "Nama", "Rarity", "Power", "Harga jual");
            for (i = 0; i < p->itemCount; i++) {
                printf("%-4d %-20s %-10s %-5d %d\n",
                       i, p->inventory[i].name, rarityName(p->inventory[i].rarity),
                       p->inventory[i].power, duplicatePrice(p->inventory[i].rarity));
            }
            printf("Nomor indeks item yang dijual (999 batal): ");
            index = readInt("", 0, 999);
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
