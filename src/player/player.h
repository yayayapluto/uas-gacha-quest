#ifndef PLAYER_H
#define PLAYER_H

#include "config.h"

void initPlayer(Player *p, const char *name);
/* Back to the starting run values; keeps the player name and the world seed. */
void resetPlayer(Player *p);
void addExp(Player *p, int exp);
int totalAtk(const Player *p);
int totalDef(const Player *p);
void showStatus(const Player *p);

#endif
