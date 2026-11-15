#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

int loadCatalog(const char *file, Item items[]);
int loadMonsters(const char *file, Monster monsters[]);
#define SAVE_SLOTS 5

int saveGame(const Player *p);
int saveGameSlot(const Player *p, int slot);
int loadGame(Player *p, const Item items[], int count);
int loadGameSlot(Player *p, const Item items[], int count, int slot);
void saveSlotPath(int slot, char *out, int size);
int saveSlotExists(int slot);
int saveSlotName(int slot, char *out, int size);
int saveSlotFirstFree(void);
void recordScore(const Player *p);
void showLeaderboard(void);

#endif
