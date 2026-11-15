#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "inventory.h"
#include "util.h"
#include "ui.h"

/* Prints the inventory table with equipped markers. */
static void showInventory(const Player *p)
{
    const char *rarityNames[] = { "-", "Common", "Rare", "Epic", "Legendary" };
    int i;

    printf("Inventory (%d/%d)\n", p->itemCount, MAX_ITEM);
    printf("%-4s %-20s %-4s %-10s %-5s %s\n", "Idx", "Nama", "Tipe", "Rarity", "Power", "Terpasang");
    for (i = 0; i < p->itemCount; i++) {
        printf("%s%-4d %-20s %-4c %-10s %-5d%s %s\n",
               rarity_color(p->inventory[i].rarity),
               i, p->inventory[i].name, p->inventory[i].type,
               rarityNames[p->inventory[i].rarity], p->inventory[i].power,
               ANSI_RESET,
               i == p->weaponIndex ? "[SENJATA]" : (i == p->armorIndex ? "[ARMOR]" : ""));
    }
}

/* Re-locates equipped indexes after item positions change (sort or sell). */
void syncEquipped(Player *p, int oldWeaponId, int oldArmorId)
{
    int i;
    p->weaponIndex = -1;
    p->armorIndex = -1;
    for (i = 0; i < p->itemCount; i++) {
        if (oldWeaponId >= 0 && p->inventory[i].id == oldWeaponId) p->weaponIndex = i;
        if (oldArmorId >= 0 && p->inventory[i].id == oldArmorId) p->armorIndex = i;
    }
}

/* Inventory menu: view, sort, search, equip. */
void inventoryMenu(Player *p)
{
    int choice;

    for (;;) {
        clear_screen();
        draw_header("INVENTORY", p);
        showInventory(p);
        draw_menu(1, "Urutkan");
        draw_menu(2, "Cari item");
        draw_menu(3, "Equip item");
        draw_menu(0, "Kembali");
        if (last_message()[0] != '\0')
            printf("%s%s%s\n", ANSI_CYAN, last_message(), ANSI_RESET);
        printf("Pilih: ");
        choice = readInt("", 0, 3);
        set_message("");
        if (choice == 0) return;
        switch (choice) {
        case 1: {
            int mode;
            printf("Urutkan berdasarkan (1 rarity, 2 power, 3 nama): ");
            mode = readInt("", 1, 3);
            sortInventory(p, mode);
            set_message("Inventory terurut.");
            break;
        }
        case 2: {
            char word[MAX_NAME];
            int found;
            readString("Cari nama item: ", word, MAX_NAME);
            found = searchItemByName(p, word);
            if (found == 0) set_message("Tidak ada item yang cocok.");
            else set_message("Ditemukan %d item.", found);
            break;
        }
        case 3: {
            int index;
            if (p->itemCount == 0) {
                set_message("Inventory kosong.");
                break;
            }
            printf("Nomor indeks item yang dipasang (999 batal): ");
            index = readInt("", 0, 999);
            if (index == 999) break;
            if (index >= p->itemCount) {
                set_message("Indeks tidak ada di inventory.");
                break;
            }
            equipItem(p, index);
            set_message("%s dipasang.", p->inventory[index].name);
            break;
        }
        }
    }
}

/* Sorts the inventory: 1 rarity, 2 power descending, 3 name A-Z (case-insensitive). */
void sortInventory(Player *p, int mode)
{
    int oldWeaponId = -1, oldArmorId = -1;
    int i, j;

    if (p->weaponIndex >= 0) oldWeaponId = p->inventory[p->weaponIndex].id;
    if (p->armorIndex >= 0) oldArmorId = p->inventory[p->armorIndex].id;
    for (i = 0; i < p->itemCount - 1; i++) {
        for (j = 0; j < p->itemCount - 1 - i; j++) {
            int swap = 0;
            char a[MAX_NAME], b[MAX_NAME];
            switch (mode) {
            case 1: swap = p->inventory[j].rarity > p->inventory[j + 1].rarity; break;
            case 2: swap = p->inventory[j].power < p->inventory[j + 1].power; break;
            case 3:
                strncpy(a, p->inventory[j].name, MAX_NAME);
                strncpy(b, p->inventory[j + 1].name, MAX_NAME);
                a[MAX_NAME - 1] = '\0'; b[MAX_NAME - 1] = '\0';
                {
                    int k;
                    for (k = 0; a[k] != '\0'; k++) a[k] = (char)tolower((unsigned char)a[k]);
                    for (k = 0; b[k] != '\0'; k++) b[k] = (char)tolower((unsigned char)b[k]);
                }
                swap = strcmp(a, b) > 0;
                break;
            }
            if (swap) {
                Item tmp = p->inventory[j];
                p->inventory[j] = p->inventory[j + 1];
                p->inventory[j + 1] = tmp;
            }
        }
    }
    syncEquipped(p, oldWeaponId, oldArmorId);
}

/* Finds items whose name contains 'word' (case-insensitive); prints them and returns the count. */
int searchItemByName(const Player *p, const char *word)
{
    char lowerWord[MAX_NAME];
    int i, k, found = 0;

    strncpy(lowerWord, word, MAX_NAME - 1);
    lowerWord[MAX_NAME - 1] = '\0';
    for (k = 0; lowerWord[k] != '\0'; k++) lowerWord[k] = (char)tolower((unsigned char)lowerWord[k]);
    for (i = 0; i < p->itemCount; i++) {
        char lowerName[MAX_NAME];
        strncpy(lowerName, p->inventory[i].name, MAX_NAME - 1);
        lowerName[MAX_NAME - 1] = '\0';
        for (k = 0; lowerName[k] != '\0'; k++) lowerName[k] = (char)tolower((unsigned char)lowerName[k]);
        if (strstr(lowerName, lowerWord) != NULL) {
            printf("%sDitemukan: idx %d %s (tipe %c, power %d)%s\n",
                   rarity_color(p->inventory[i].rarity),
                   i, p->inventory[i].name, p->inventory[i].type, p->inventory[i].power,
                   ANSI_RESET);
            found++;
        }
    }
    return found;
}

/* Equips the item into the weapon or armor slot, replacing the previous one. */
void equipItem(Player *p, int index)
{
    if (p->inventory[index].type == 'S') p->weaponIndex = index;
    else if (p->inventory[index].type == 'A') p->armorIndex = index;
}
