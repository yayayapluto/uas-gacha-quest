#ifndef BATTLE_H
#define BATTLE_H

#include "config.h"

void startBattle(Player *p, const Monster m[], int count);
/* Battle against one specific monster, used when the dungeon decides the
   encounter from the seed instead of rolling a level-appropriate one. */
void startBattleAgainst(Player *p, const Monster *enemy);
void resumeBattle(Player *p, Monster enemy, int enemyHp);
int calculateDamage(int atk, int def);
/* 1 when the player left the game-over screen towards the main menu. */
int battleRunAborted(void);
void battleClearRunAbort(void);
/* 1 when the last battle ended the run (defeat), so callers can stop the run. */
int battleRunEnded(void);
void battleClearRunEnded(void);
/* Game-over choice for encounters that resolve inside another screen.
   Returns 1 when the player chose a fresh run, 0 when leaving to the menu. */
int battleGameOverScreen(Player *p);

#endif
