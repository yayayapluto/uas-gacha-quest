#ifndef AUTOSAVE_H
#define AUTOSAVE_H

#include "config.h"

enum {
    AS_SCREEN_MENU = 0,
    AS_SCREEN_HUB,
    AS_SCREEN_BATTLE,
    AS_SCREEN_DUNGEON
};

typedef struct {
    Player player;
    int screen;
    int battleActive;
    Monster battleEnemy;
    int battleEnemyHp;
    int dungeonActive;
    DungeonState dungeon;
    int dungeonSteps;
    int dungeonHints;
    unsigned long long rng;
} AutoSnapshot;

/* Background thread: sleeps on a condvar, writes the newest published
   snapshot with debounce, skips unchanged CRCs, retries failed writes with
   1/2/4 s backoff capped at 30 s. The UI thread never touches the file. */
void autosave_start(void);
void autosave_stop(void);          /* flush pending snapshot, join thread */

/* Copy-on-publish: call at safe points (after an action, before redraw).
   important=1 bypasses the debounce (battle end, gacha result, buy/sell,
   equip, treasure). */
void autosave_publish(const Player *p, int screen,
                      const Monster *enemy, int enemyHp,
                      const DungeonState *dst, int steps, int hints,
                      int important);

/* Loads data/autosave.dat (falls back to autosave.bak); validates magic,
   version, length and CRC32. Returns 1 and fills *out on success. */
int autosave_load(AutoSnapshot *out);

/* Non-blocking error drain for the message panel; returns 1 when a failed
   write was reported since the previous call. */
int autosave_poll_error(char *buf, int len);

#endif