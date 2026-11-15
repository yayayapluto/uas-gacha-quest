#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "gacha.h"
#include "util.h"
#include "ui.h"
#include "render.h"
#include "rng.h"
#include "autosave.h"

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

int findItemInInventory(const Player *p, int id)
{
    int i;
    for (i = 0; i < p->itemCount; i++)
        if (p->inventory[i].id == id) return i;
    return -1;
}

int rollRarity(int *pity)
{
    int roll = plat_rand() % 100 + 1;
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
    picked = plat_rand() % candidateCount;
    index = findItemInInventory(p, candidates[picked].id);
    if (index >= 0) {
        int price = duplicatePrice(candidates[picked].rarity);
        p->gold += price;
        ui_log_style(style_rarity(rarity),
                     "  [%s] %s -> DUPLIKAT, jadi +%d gold",
                     rarityName(rarity), candidates[picked].name, price);
    } else if (p->itemCount < MAX_ITEM) {
        p->inventory[p->itemCount] = candidates[picked];
        p->itemCount++;
        ui_log_style(style_rarity(rarity),
                     "  [%s] %s (power %d) -> baru!",
                     rarityName(rarity), candidates[picked].name, candidates[picked].power);
    }
}

static void gachaDraw(void *ctx, int sel)
{
    static const char *items[] = { "Pull 1x (100 gold)", "Pull 10x (900 gold)", "Kembali" };
    static const int nums[] = { 1, 2, 0 };
    Player *p = ctx;
    char buf[64];
    int i, start, logRow;

    ui_frame("Gacha");
    ui_status_panel_at(p, 2, 0, UI_COLS);

    ui_panel(8, 0, UI_COLS, 5, "PULL");
    snprintf(buf, sizeof buf, "Pity      : %d/%d", p->pity, PITY_THRESHOLD);
    render_text(9, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Total pull: %d", p->totalPull);
    render_text(10, 2, ST_TEXT, "%s", buf);
    ui_bar(11, 2, 44, p->pity, PITY_THRESHOLD, ST_GOLD);

    ui_panel(14, 0, UI_COLS, 6, "Hasil");
    start = ui_log_count() > 4 ? ui_log_count() - 4 : 0;
    logRow = 15;
    for (i = start; i < ui_log_count(); i++, logRow++)
        render_text_clipped(logRow, 2, UI_COLS - 4, ui_log_style_at(i), "%s", ui_log_text(i));

    ui_menu_v(21, 0, items, nums, 3, sel);
    ui_footer(NULL);
}

void gachaMenu(Player *p, const Item catalog[], int count)
{
    static const char *items[] = { "Pull 1x (100 gold)", "Pull 10x (900 gold)", "Kembali" };
    static const int nums[] = { 1, 2, 0 };

    for (;;) {
        int choice;
        choice = ui_menu_pick(items, nums, 3, 0, 2, gachaDraw, p);
        if (choice == 0) return;
        /* clear only when a new pull is about to start, so the previous
           result stays visible on the frame drawn before it */
        ui_log_clear();
        if (choice == 1) {
            if (p->gold < PULL_COST) {
                set_message("Gold kurang! Pull 1x butuh %d gold.", PULL_COST);
                continue;
            }
            p->gold -= PULL_COST;
            p->totalPull++;
            pullOnce(p, catalog, count, rollRarity(&p->pity));
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
        } else {
            int i;
            if (p->gold < PULL10_COST) {
                set_message("Gold kurang! Pull 10x butuh %d gold.", PULL10_COST);
                continue;
            }
            p->gold -= PULL10_COST;
            ui_log("Hasil pull 10x:");
            for (i = 0; i < 10; i++) {
                p->totalPull++;
                pullOnce(p, catalog, count, rollRarity(&p->pity));
            }
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
        }
        gachaDraw(p, -1);
        ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
    }
}