#include <stdio.h>
#include <string.h>
#include "config.h"
#include "player.h"
#include "util.h"
#include "ui.h"
#include "render.h"

/* Fills a new player with the starting values from rule 6.1. */
void initPlayer(Player *p, const char *name)
{
    memset(p, 0, sizeof *p);
    strncpy(p->name, name, MAX_NAME - 1);
    p->name[MAX_NAME - 1] = '\0';
    p->hp = 100;
    p->maxHp = 100;
    p->atk = 10;
    p->def = 3;
    p->level = 1;
    p->exp = 0;
    p->gold = 200;
    p->potion = 2;
    p->pity = 0;
    p->totalPull = 0;
    p->score = 0;
    p->seed = 0;
    p->itemCount = 0;
    p->weaponIndex = -1;
    p->armorIndex = -1;
}

/* Adds EXP and resolves level-ups (possibly several at once). */
void addExp(Player *p, int exp)
{
    int needed;
    p->exp += exp;
    while (p->level < MAX_LEVEL) {
        needed = p->level * 50;
        if (p->exp < needed) break;
        p->exp -= needed;
        p->level++;
        p->maxHp += 15;
        p->atk += 2;
        p->def += 1;
        p->hp = p->maxHp;
        set_message("Naik level! Sekarang level %d (maxHp %d, ATK %d, DEF %d).",
                    p->level, p->maxHp, p->atk, p->def);
    }
    if (p->level >= MAX_LEVEL) p->exp = 0;
}

/* Total ATK = base ATK + equipped weapon power. */
int totalAtk(const Player *p)
{
    if (p->weaponIndex >= 0) return p->atk + p->inventory[p->weaponIndex].power;
    return p->atk;
}

/* Total DEF = base DEF + equipped armor power. */
int totalDef(const Player *p)
{
    if (p->armorIndex >= 0) return p->def + p->inventory[p->armorIndex].power;
    return p->def;
}

/* Shows the full player status: statistics and equipment side by side. */
void showStatus(const Player *p)
{
    char buf[64];
    int expMax = p->level >= MAX_LEVEL ? 0 : p->level * 50;

    ui_frame("Status");
    ui_status_panel_at(p, 2, 0, UI_COLS);

    ui_panel(9, 0, 50, 11, "STATISTIK");
    snprintf(buf, sizeof buf, "Level      : %d (EXP %d/%d)", p->level, p->exp, expMax);
    render_text(10, 2, ST_TEXT, "%s", buf);
    ui_bar(11, 2, 44, p->exp, expMax > 0 ? expMax : 1, ST_ACCENT);
    snprintf(buf, sizeof buf, "ATK        : %d", totalAtk(p));
    render_text(12, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "DEF        : %d", totalDef(p));
    render_text(13, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Potion     : %d", p->potion);
    render_text(14, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Pity       : %d/%d", p->pity, PITY_THRESHOLD);
    render_text(15, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Total pull : %d", p->totalPull);
    render_text(16, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Seed dunia : %u", p->seed);
    render_text(17, 2, ST_TEXT, "%s", buf);
    snprintf(buf, sizeof buf, "Item       : %d/%d", p->itemCount, MAX_ITEM);
    render_text(18, 2, ST_TEXT, "%s", buf);

    ui_panel(21, 0, 50, 4, "PERLENGKAPAN");
    if (p->weaponIndex >= 0) {
        const Item *w = &p->inventory[p->weaponIndex];
        snprintf(buf, sizeof buf, "Senjata: %s (power %d)", w->name, w->power);
        render_text(22, 2, style_rarity(w->rarity), "%s", buf);
    } else {
        render_text(22, 2, ST_DIM, "Senjata: -");
    }
    if (p->armorIndex >= 0) {
        const Item *a = &p->inventory[p->armorIndex];
        snprintf(buf, sizeof buf, "Armor  : %s (power %d)", a->name, a->power);
        render_text(23, 2, style_rarity(a->rarity), "%s", buf);
    } else {
        render_text(23, 2, ST_DIM, "Armor  : -");
    }

    ui_footer(NULL);
    ui_flush();
    ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
}
