#include <stdio.h>
#include <string.h>
#include "config.h"
#include "player.h"
#include "util.h"
#include "ui.h"

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

/* Shows the full player status. */
void showStatus(const Player *p)
{
    clear_screen();
    draw_header("STATUS", p);
    draw_box_top("DETAIL");
    draw_box_line("Level      : %d (EXP %d/%d)", p->level, p->exp, p->level >= MAX_LEVEL ? 0 : p->level * 50);
    draw_box_line("ATK        : %d", totalAtk(p));
    draw_box_line("DEF        : %d", totalDef(p));
    draw_box_line("Potion     : %d", p->potion);
    draw_box_line("Pity       : %d/%d", p->pity, PITY_THRESHOLD);
    draw_box_line("Total pull : %d", p->totalPull);
    draw_box_line("Seed dunia : %u", p->seed);
    draw_box_line("Item       : %d/%d", p->itemCount, MAX_ITEM);
    draw_box_bottom();
    if (p->weaponIndex >= 0)
        printf("Senjata: %s%s%s (power %d)\n",
               rarity_color(p->inventory[p->weaponIndex].rarity),
               p->inventory[p->weaponIndex].name, ANSI_RESET,
               p->inventory[p->weaponIndex].power);
    if (p->armorIndex >= 0)
        printf("Armor  : %s%s%s (power %d)\n",
               rarity_color(p->inventory[p->armorIndex].rarity),
               p->inventory[p->armorIndex].name, ANSI_RESET,
               p->inventory[p->armorIndex].power);
    ui_wait_enter("Tekan Enter untuk lanjut...");
}
