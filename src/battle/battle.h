#ifndef BATTLE_H
#define BATTLE_H

#include "config.h"

void startBattle(Player *p, const Monster m[], int count);
void resumeBattle(Player *p, Monster enemy, int enemyHp);
int calculateDamage(int atk, int def);

#endif
