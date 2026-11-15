#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "inventory.h"
#include "util.h"
#include "ui.h"
#include "render.h"
#include "autosave.h"

static void inventoryDraw(void *ctx, int sel)
{
    static const char *items[] = { "Urutkan", "Cari item", "Equip item", "Kembali" };
    static const int nums[] = { 1, 2, 3, 0 };
    Player *p = ctx;
    char buf[64];
    int i, row;

    ui_frame("Inventory");
    ui_status_panel_at(p, 2, 0, UI_COLS);

    snprintf(buf, sizeof buf, "Inventory (%d/%d)", p->itemCount, MAX_ITEM);
    render_text(9, 0, ST_TITLE, "%s", buf);
    render_text(10, 0, ST_DIM, "%-4s %-20s %-4s %-10s %-5s %s",
                "Idx", "Nama", "Tipe", "Rarity", "Power", "Terpasang");
    for (i = 0, row = 11; i < p->itemCount && i < 6; i++, row++) {
        const char *eq = i == p->weaponIndex ? "[SENJATA]"
                       : (i == p->armorIndex ? "[ARMOR]" : "");
        render_text(row, 0, style_rarity(p->inventory[i].rarity),
                    "%-4d %-20s %-4c %-10s %-5d %s",
                    i, p->inventory[i].name, p->inventory[i].type,
                    p->inventory[i].rarity == RARITY_COMMON ? "Common"
                    : p->inventory[i].rarity == RARITY_RARE ? "Rare"
                    : p->inventory[i].rarity == RARITY_EPIC ? "Epic"
                    : "Legendary",
                    p->inventory[i].power, eq);
    }

    ui_menu_v(18, 0, items, nums, 4, sel);
    ui_footer(NULL);
}

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

void inventoryMenu(Player *p)
{
    static const char *items[] = { "Urutkan", "Cari item", "Equip item", "Kembali" };
    static const int nums[] = { 1, 2, 3, 0 };

    ui_input_screen("Inventory");
    for (;;) {
        int choice = ui_menu_pick(items, nums, 4, 0, 3, inventoryDraw, p);
        if (choice == 0) return;
        switch (choice) {
        case 1: {
            int mode;
            mode = readInt("Urutkan berdasarkan (1 rarity, 2 power, 3 nama): ", 1, 3);
            sortInventory(p, mode);
            set_message("Inventory terurut.");
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
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
            index = readInt("Nomor indeks item yang dipasang (999 batal): ", 0, 999);
            if (index == 999) break;
            if (index >= p->itemCount) {
                set_message("Indeks tidak ada di inventory.");
                break;
            }
            equipItem(p, index);
            set_message("%s dipasang.", p->inventory[index].name);
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
            break;
        }
        }
    }
}

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
            ui_log_style(style_rarity(p->inventory[i].rarity),
                         "Ditemukan: idx %d %s (tipe %c, power %d)",
                         i, p->inventory[i].name, p->inventory[i].type,
                         p->inventory[i].power);
            found++;
        }
    }
    return found;
}

void equipItem(Player *p, int index)
{
    if (p->inventory[index].type == 'S') p->weaponIndex = index;
    else if (p->inventory[index].type == 'A') p->armorIndex = index;
}