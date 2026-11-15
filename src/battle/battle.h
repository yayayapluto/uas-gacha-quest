#ifndef BATTLE_H
#define BATTLE_H

#include "config.h"

void startBattle(Player *p, const Monster m[], int count);
/* Battle against one specific monster, used when the dungeon decides the
   encounter from the seed instead of rolling a level-appropriate one. */
void startBattleAgainst(Player *p, const Monster *enemy);
void resumeBattle(Player *p, Monster enemy, int enemyHp);
int calculateDamage(int atk, int def);

#endif
