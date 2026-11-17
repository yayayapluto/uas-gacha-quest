#ifndef GACHA_H
#define GACHA_H

#include "config.h"

void gachaMenu(Player *p, const Item catalog[], int count);
int rollRarity(int *pity);
int findItemInInventory(const Player *p, int id);
int duplicatePrice(int rarity);
const char *rarityName(int rarity);
int pullOnce(Player *p, const Item catalog[], int count, int rarity);   /* 0 when full: caller refunds */

#endif
