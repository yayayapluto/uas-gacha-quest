#ifndef DUNGEON_H
#define DUNGEON_H

#include "config.h"

unsigned int cellHash(unsigned int seed, int x, int y);
char dungeonCell(unsigned int seed, int x, int y);
unsigned int randomWorldSeed(void);
unsigned int promptWorldSeed(void);
int promptWorldSeedTry(unsigned int *out);   /* 0 when the player cancels with Esc */
int dfsReachable(char area[][AREA_MAX], Point from, Point to);
int bfsShortestPath(char area[][AREA_MAX], Point from, Point to, Point path[]);
void runDungeon(Player *p, const Item catalog[], int catalogCount,
                const Monster monsters[], int monsterCount);
void resumeDungeon(Player *p, const DungeonState *saved, int steps, int hints,
                   const Item catalog[], int catalogCount,
                   const Monster monsters[], int monsterCount);

#endif
