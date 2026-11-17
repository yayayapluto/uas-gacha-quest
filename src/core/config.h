#ifndef CONFIG_H
#define CONFIG_H

#define MAX_ITEM 36
#define MAX_MONSTER 12
#define MAX_NAME 30

#define CHUNK_SIZE 16
#define CACHED_CHUNKS 5
#define AREA_MAX (CHUNK_SIZE * CACHED_CHUNKS)
#define MAX_PATH_CELLS (AREA_MAX * AREA_MAX)
#define MAX_TAKEN_CELLS 128

#define MAX_LEVEL 10
#define MAX_POTION 20
#define MAX_LEADERBOARD 10
#define TOP_DISPLAY 5

#define PULL_COST 100
#define PULL10_COST 900
#define POTION_COST 40
#define HINT_COST 30

#define CRIT_CHANCE 10      /* percent chance a hit is critical */
#define CRIT_MULTIPLIER 2
#define POTION_HEAL 50

#define MAX_HINTS 3
#define PITY_THRESHOLD 10

#define TREASURE_GOLD 30
#define DUNGEON_GOLD_MIN 10
#define DUNGEON_GOLD_MAX 40
#define CHEST_GOLD_MIN 40
#define CHEST_GOLD_MAX 120
#define CHEST_POTION_COUNT 2

/* Chunk content rolls, in percent out of 100 per 16x16 chunk. The first roll
   hits the chunk, the second finds the cell inside it. */
#define CHUNK_MONSTER_PCT 45
#define CHUNK_CHEST_PCT 35
#define CHUNK_GOLD_PCT 55
#define CHEST_EQUIPMENT_PCT 55
#define CHEST_POTION_PCT 25

#define RARITY_COMMON 1
#define RARITY_RARE 2
#define RARITY_EPIC 3
#define RARITY_LEGENDARY 4

#define DUPLICATE_PRICE_COMMON 20
#define DUPLICATE_PRICE_RARE 50
#define DUPLICATE_PRICE_EPIC 150
#define DUPLICATE_PRICE_LEGENDARY 500

typedef struct {
    int id;
    char name[MAX_NAME];
    char type;      /* 'S' weapon, 'A' armor */
    int rarity;     /* 1 Common, 2 Rare, 3 Epic, 4 Legendary */
    int power;
} Item;

typedef struct {
    int id;
    char name[MAX_NAME];
    int hp, atk, def;
    int exp, gold, minLevel;
} Monster;

typedef struct {
    char name[MAX_NAME];
    int hp, maxHp, atk, def;
    int level, exp, gold, potion;
    int pity, totalPull, score;
    unsigned int seed;          /* world seed for the procedural dungeon */
    Item inventory[MAX_ITEM];
    int itemCount;
    int weaponIndex, armorIndex;   /* -1 when empty */
} Player;

typedef struct { int row, col; } Point;

/* Procedural dungeon: every cell is derived from the seed, so the world is infinite */
typedef struct {
    unsigned int seed;
    int posX, posY;             /* player position in world coordinates */
    Point taken[MAX_TAKEN_CELLS];   /* content cells already used up */
    int takenCount;
    int originX, originY;       /* top-left corner of the cached area (world coordinates) */
} DungeonState;

#endif
