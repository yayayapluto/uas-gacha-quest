#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "gacha.h"
#include "util.h"
#include "ui.h"

const char *rarityName(int rarity)
{
    switch (rarity) {
    case RARITY_COMMON: return "Common";
    case RARITY_RARE: return "Rare";
    case RARITY_EPIC: return "Epic";
    case RARITY_LEGENDARY: return "Legendary";
    }
    return "?";
}

int duplicatePrice(int rarity)
{
    switch (rarity) {
    case RARITY_COMMON: return DUPLICATE_PRICE_COMMON;
    case RARITY_RARE: return DUPLICATE_PRICE_RARE;
    case RARITY_EPIC: return DUPLICATE_PRICE_EPIC;
    case RARITY_LEGENDARY: return DUPLICATE_PRICE_LEGENDARY;
    }
    return 0;
}

/* Sequential search by item ID in the inventory; -1 when absent. */
int findItemInInventory(const Player *p, int id)
{
    int i;
    for (i = 0; i < p->itemCount; i++)
        if (p->inventory[i].id == id) return i;
    return -1;
}

/* Rolls rarity from rand() % 100 + 1 and applies pity; updates *pity. */
int rollRarity(int *pity)
{
    int roll = rand() % 100 + 1;
    int r;

    if (roll >= 98) r = RARITY_LEGENDARY;
    else if (roll >= 86) r = RARITY_EPIC;
    else if (roll >= 61) r = RARITY_RARE;
    else r = RARITY_COMMON;

    if (r >= RARITY_EPIC) {
        *pity = 0;
    } else {
        (*pity)++;
        if (*pity >= PITY_THRESHOLD) {
            r = RARITY_EPIC;
            *pity = 0;
        }
    }
    return r;
}

/* One pull: pick a random item of the given rarity, convert duplicates to gold, print the result. */
void pullOnce(Player *p, const Item catalog[], int count, int rarity)
{
    Item candidates[MAX_ITEM];
    int candidateCount = 0, i, index, picked;

    for (i = 0; i < count; i++)
        if (catalog[i].rarity == rarity) {
            candidates[candidateCount] = catalog[i];
            candidateCount++;
        }
    if (candidateCount == 0) return;
    picked = rand() % candidateCount;
    index = findItemInInventory(p, candidates[picked].id);
    if (index >= 0) {
        int price = duplicatePrice(candidates[picked].rarity);
        p->gold += price;
        printf("  [%s%s%s] %s -> DUPLIKAT, jadi +%d gold\n",
               rarity_color(rarity), rarityName(rarity), ANSI_RESET,
               candidates[picked].name, price);
    } else if (p->itemCount < MAX_ITEM) {
        p->inventory[p->itemCount] = candidates[picked];
        p->itemCount++;
        printf("  [%s%s%s] %s (power %d) -> baru!\n",
               rarity_color(rarity), rarityName(rarity), ANSI_RESET,
               candidates[picked].name, candidates[picked].power);
    }
}

/* Gacha menu: one pull, ten pulls, or back. */
void gachaMenu(Player *p, const Item catalog[], int count)
{
    int choice;

    for (;;) {
        clear_screen();
        draw_header("GACHA", p);
        draw_box_top("PULL");
        draw_box_line("Pity    : %d/%d", p->pity, PITY_THRESHOLD);
        draw_box_line("Total pull: %d", p->totalPull);
        draw_box_bottom();
        draw_menu(1, "Pull 1x (100 gold)");
        draw_menu(2, "Pull 10x (900 gold)");
        draw_menu(0, "Kembali");
        if (last_message()[0] != '\0')
            printf("%s%s%s\n", ANSI_CYAN, last_message(), ANSI_RESET);
        printf("Pilih: ");
        choice = readInt("", 0, 2);
        set_message("");
        if (choice == 0) return;
        if (choice == 1) {
            if (p->gold < PULL_COST) {
                set_message("Gold kurang! Pull 1x butuh %d gold.", PULL_COST);
                continue;
            }
            p->gold -= PULL_COST;
            p->totalPull++;
            pullOnce(p, catalog, count, rollRarity(&p->pity));
        } else {
            int i;
            if (p->gold < PULL10_COST) {
                set_message("Gold kurang! Pull 10x butuh %d gold.", PULL10_COST);
                continue;
            }
            p->gold -= PULL10_COST;
            printf("Hasil pull 10x:\n");
            for (i = 0; i < 10; i++) {
                p->totalPull++;
                pullOnce(p, catalog, count, rollRarity(&p->pity));
            }
        }
        ui_wait_enter("Tekan Enter untuk lanjut...");
    }
}
