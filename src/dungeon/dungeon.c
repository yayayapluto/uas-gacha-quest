#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "dungeon.h"
#include "util.h"
#include "ui.h"
#include "player.h"
#include "caps.h"
#include "term.h"
#include "render.h"
#include "rng.h"
#include "platform.h"
#include "autosave.h"
#include "battle.h"
#include "gacha.h"

#define VIEW_WIDTH 25
#define VIEW_HEIGHT 15

/* Floor division for negative numbers, used to map world coordinates to chunks. */
static int floorDiv(int x, int m)
{
    if (x >= 0) return x / m;
    return -((-x + m - 1) / m);
}

/* Pure integer hash: the result depends only on the seed and the cell coordinates. */
unsigned int cellHash(unsigned int seed, int x, int y)
{
    unsigned int h = seed;
    h ^= (unsigned int)x * 374761393u;
    h ^= (unsigned int)y * 668265263u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

/* True when the chunk holds this content type. Chunk-wide rolls keep the odds
   a property of the chunk, not of how much floor happens to fall inside it. */
static int chunkHas(unsigned int seed, int chunkX, int chunkY, int kind, int pct)
{
    unsigned int h = cellHash(seed ^ (0x9E3779B9u * (unsigned int)kind), chunkX, chunkY);
    return (int)(h % 100u) < pct;
}

/* ---- chunk terrain --------------------------------------------------- */

/* Chunk terrain is generated once per (seed, chunk) and memoised. The cache is
   direct-mapped: the index is a hash of the key, so a collision regenerates the
   chunk instead of evicting a neighbour that the view is about to ask for. */
#define CHUNK_CELLS (CHUNK_SIZE * CHUNK_SIZE)
#define CHUNK_CACHE_SLOTS 64
#define TERRAIN_VARIANTS 4

typedef struct {
    unsigned int seed;
    int cx, cy;
    char cell[CHUNK_SIZE][CHUNK_SIZE];  /* [y][x], '#' wall, '.' floor */
} ChunkMap;

static ChunkMap chunkCache[CHUNK_CACHE_SLOTS];

/* Salted chunk roll: same key always yields the same number. */
static unsigned int chunkRand(unsigned int seed, int cx, int cy, unsigned int salt)
{
    return cellHash(seed ^ (0x9E3779B9u * salt), cx, cy);
}

static void carveH(char g[][CHUNK_SIZE], int y, int x0, int x1)
{
    int x;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (x = x0; x <= x1; x++) g[y][x] = '.';
}

static void carveV(char g[][CHUNK_SIZE], int x, int y0, int y1)
{
    int y;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++) g[y][x] = '.';
}

static void carveRect(char g[][CHUNK_SIZE], int x0, int y0, int x1, int y1)
{
    int y;
    for (y = y0; y <= y1; y++) carveH(g, y, x0, x1);
}

/* Gate position on one side of a chunk. The key is built from the shared edge,
   so the two chunks on that edge carve the same cell and stay connected. */
static int edgeGate(unsigned int seed, int cx, int cy, int side)
{
    unsigned int r;
    if (side == 0 || side == 1)
        r = chunkRand(seed, 2 * cx + (side ? 1 : -1), cy, 11u);
    else
        r = chunkRand(seed, cx, 2 * cy + (side == 3 ? 1 : -1), 13u);
    return 1 + (int)(r % (unsigned int)(CHUNK_SIZE - 2));
}

/* Recursive-backtracker maze on the odd lattice: cells at (1+2m, 1+2n). */
static void genMaze(unsigned int seed, int cx, int cy, char g[][CHUNK_SIZE])
{
    unsigned char seen[CHUNK_SIZE / 2][CHUNK_SIZE / 2];
    int stack[CHUNK_SIZE * CHUNK_SIZE / 2][2];
    int top = 0;
    int x0 = cx * CHUNK_SIZE, y0 = cy * CHUNK_SIZE;

    memset(seen, 0, sizeof seen);
    seen[0][0] = 1;
    g[1][1] = '.';
    stack[0][0] = 0;
    stack[0][1] = 0;
    while (top >= 0) {
        int mx = stack[top][0], my = stack[top][1];
        int opts[4][2], n = 0, k;
        int d;
        static const int dx[4] = { -1, 1, 0, 0 };
        static const int dy[4] = { 0, 0, -1, 1 };
        for (d = 0; d < 4; d++) {
            int nx = mx + dx[d], ny = my + dy[d];
            if (nx < 0 || ny < 0 || nx >= CHUNK_SIZE / 2 || ny >= CHUNK_SIZE / 2) continue;
            if (seen[ny][nx]) continue;
            opts[n][0] = nx;
            opts[n][1] = ny;
            n++;
        }
        if (n == 0) {
            top--;
            continue;
        }
        k = (int)(chunkRand(seed, x0 + 1 + 2 * mx, y0 + 1 + 2 * my, 23u) % (unsigned int)n);
        {
            int nx = opts[k][0], ny = opts[k][1];
            seen[ny][nx] = 1;
            g[1 + 2 * ny][1 + 2 * nx] = '.';
            g[1 + 2 * my + (ny - my)][1 + 2 * mx + (nx - mx)] = '.';
            top++;
            stack[top][0] = nx;
            stack[top][1] = ny;
        }
    }
}

/* Two-by-two rooms of hashed size; the connectivity pass cuts the doors. */
static void genRooms(unsigned int seed, int cx, int cy, char g[][CHUNK_SIZE])
{
    int qi, qj;
    int x0 = cx * CHUNK_SIZE, y0 = cy * CHUNK_SIZE;
    for (qj = 0; qj < 2; qj++) {
        for (qi = 0; qi < 2; qi++) {
            unsigned int r = chunkRand(seed, x0 + qi * 8, y0 + qj * 8, 31u);
            int insetX = (int)(r % 2u), insetY = (int)((r >> 4) % 2u);
            int xa = qi * 8 + 1 + insetX, xb = qi * 8 + 6 + insetY;
            int ya = qj * 8 + 1 + insetY, yb = qj * 8 + 6 + insetX;
            carveRect(g, xa, ya, xb, yb);
        }
    }
}

/* Blobby cave from hashed noise, smoothed twice so single cells vanish. */
static void genCavern(unsigned int seed, int cx, int cy, char g[][CHUNK_SIZE])
{
    int pass, x, y;
    int x0 = cx * CHUNK_SIZE, y0 = cy * CHUNK_SIZE;
    char next[CHUNK_SIZE][CHUNK_SIZE];

    for (y = 0; y < CHUNK_SIZE; y++)
        for (x = 0; x < CHUNK_SIZE; x++)
            g[y][x] = (cellHash(seed ^ 0xCAFEu, x0 + x, y0 + y) % 100u < 46u) ? '#' : '.';
    for (pass = 0; pass < 2; pass++) {
        for (y = 0; y < CHUNK_SIZE; y++) {
            for (x = 0; x < CHUNK_SIZE; x++) {
                int walls = 0, dx, dy;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (!dx && !dy) continue;
                        if (nx < 0 || ny < 0 || nx >= CHUNK_SIZE || ny >= CHUNK_SIZE) walls++;
                        else if (g[ny][nx] == '#') walls++;
                    }
                next[y][x] = walls >= 5 ? '#' : '.';
            }
        }
        memcpy(g, next, sizeof next);
    }
}

/* Open hall broken up by a few hashed pillar blocks. */
static void genPillars(unsigned int seed, int cx, int cy, char g[][CHUNK_SIZE])
{
    int i;
    int x0 = cx * CHUNK_SIZE, y0 = cy * CHUNK_SIZE;
    carveRect(g, 0, 0, CHUNK_SIZE - 1, CHUNK_SIZE - 1);
    for (i = 0; i < 5; i++) {
        unsigned int r = chunkRand(seed, x0, y0, 41u + (unsigned int)i);
        int px = 2 + (int)(r % (unsigned int)(CHUNK_SIZE - 6));
        int py = 2 + (int)((r >> 8) % (unsigned int)(CHUNK_SIZE - 6));
        int w = 1 + (int)((r >> 16) % 2u), h = 1 + (int)((r >> 20) % 2u);
        {
            int y, x;
            for (y = py; y < py + h && y < CHUNK_SIZE; y++)
                for (x = px; x < px + w && x < CHUNK_SIZE; x++) g[y][x] = '#';
        }
    }
}

/* Flood fill from the chunk centre and wall off whatever cannot be reached,
   then link every open gate to the centre so neighbouring chunks join up. */
#define GATE_COUNT 8
static void connectChunk(char g[][CHUNK_SIZE], const int gateX[GATE_COUNT],
                         const int gateY[GATE_COUNT], int extra)
{
    static unsigned char seen[CHUNK_SIZE][CHUNK_SIZE];
    int queue[CHUNK_CELLS][2];
    int head = 0, tail = 0, k, x, y;
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };

    g[CHUNK_SIZE / 2][CHUNK_SIZE / 2] = '.';
    memset(seen, 0, sizeof seen);
    seen[CHUNK_SIZE / 2][CHUNK_SIZE / 2] = 1;
    queue[tail][0] = CHUNK_SIZE / 2;
    queue[tail][1] = CHUNK_SIZE / 2;
    tail++;
    while (head < tail) {
        int cx = queue[head][0], cy = queue[head][1], d;
        head++;
        for (d = 0; d < 4; d++) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= CHUNK_SIZE || ny >= CHUNK_SIZE) continue;
            if (seen[ny][nx] || g[ny][nx] == '#') continue;
            seen[ny][nx] = 1;
            queue[tail][0] = nx;
            queue[tail][1] = ny;
            tail++;
        }
    }
    /* gates first: an L-shaped cut from the edge cell to the centre */
    for (k = 0; k < GATE_COUNT; k++) {
        int gx = gateX[k], gy = gateY[k];
        if (seen[gy][gx]) continue;
        if (gx == 0 || gx == CHUNK_SIZE - 1) {
            carveH(g, gy, gx, CHUNK_SIZE / 2);
            carveV(g, CHUNK_SIZE / 2, gy, CHUNK_SIZE / 2);
        } else {
            carveV(g, gx, gy, CHUNK_SIZE / 2);
            carveH(g, CHUNK_SIZE / 2, gx, CHUNK_SIZE / 2);
        }
    }
    if (extra) {
        /* world start sits at chunk-local (1,1) */
        if (!seen[1][1]) {
            carveH(g, 1, 1, CHUNK_SIZE / 2);
            carveV(g, CHUNK_SIZE / 2, 1, CHUNK_SIZE / 2);
        }
    }
    /* wall off pockets that are still sealed from the centre */
    memset(seen, 0, sizeof seen);
    head = tail = 0;
    seen[CHUNK_SIZE / 2][CHUNK_SIZE / 2] = 1;
    queue[tail][0] = CHUNK_SIZE / 2;
    queue[tail][1] = CHUNK_SIZE / 2;
    tail++;
    while (head < tail) {
        int cx = queue[head][0], cy = queue[head][1], d;
        head++;
        for (d = 0; d < 4; d++) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= CHUNK_SIZE || ny >= CHUNK_SIZE) continue;
            if (seen[ny][nx] || g[ny][nx] == '#') continue;
            seen[ny][nx] = 1;
            queue[tail][0] = nx;
            queue[tail][1] = ny;
            tail++;
        }
    }
    for (y = 0; y < CHUNK_SIZE; y++)
        for (x = 0; x < CHUNK_SIZE; x++)
            if (!seen[y][x]) g[y][x] = '#';
}

static void generateChunk(unsigned int seed, int cx, int cy, char g[][CHUNK_SIZE])
{
    int gateX[GATE_COUNT], gateY[GATE_COUNT], k;
    int variant = (int)(chunkRand(seed, cx, cy, 7u) % TERRAIN_VARIANTS);

    memset(g, '#', CHUNK_CELLS);
    switch (variant) {
    case 0: genMaze(seed, cx, cy, g); break;
    case 1: genRooms(seed, cx, cy, g); break;
    case 2: genCavern(seed, cx, cy, g); break;
    default: genPillars(seed, cx, cy, g); break;
    }
    /* Each side gets a two-cell gate so the seam between chunks reads as a
       passage, not as a wall with a single slot. West 0, east 1, north 2, south 3. */
    gateX[0] = 0;  gateY[0] = edgeGate(seed, cx, cy, 0);
    gateX[1] = CHUNK_SIZE - 1; gateY[1] = edgeGate(seed, cx, cy, 1);
    gateX[2] = edgeGate(seed, cx, cy, 2); gateY[2] = 0;
    gateX[3] = edgeGate(seed, cx, cy, 3); gateY[3] = CHUNK_SIZE - 1;
    for (k = 0; k < 4; k++) {
        gateX[4 + k] = gateX[k];
        gateY[4 + k] = gateY[k];
        if (gateX[k] == 0 || gateX[k] == CHUNK_SIZE - 1)
            gateY[4 + k] = gateY[k] + 1 <= CHUNK_SIZE - 2 ? gateY[k] + 1 : gateY[k] - 1;
        else
            gateX[4 + k] = gateX[k] + 1 <= CHUNK_SIZE - 2 ? gateX[k] + 1 : gateX[k] - 1;
    }
    for (k = 0; k < GATE_COUNT; k++) g[gateY[k]][gateX[k]] = '.';
    connectChunk(g, gateX, gateY, cx == 0 && cy == 0);
}

/* Memoised chunk lookup; a cache miss regenerates the chunk in place. */
static const ChunkMap *chunkMap(unsigned int seed, int cx, int cy)
{
    unsigned int slot = (seed * 2654435761u) ^ ((unsigned int)cx * 40503u) ^ ((unsigned int)cy * 6151u);
    ChunkMap *m = &chunkCache[slot % CHUNK_CACHE_SLOTS];
    if (!(m->seed == seed && m->cx == cx && m->cy == cy)) {
        memset(m, 0, sizeof *m);
        m->seed = seed;
        m->cx = cx;
        m->cy = cy;
        generateChunk(seed, cx, cy, m->cell);
    }
    return m;
}

/* Hash-searches the chunk for the n-th floor cell; -1 when the chunk has fewer. */
static int chunkFloorCell(unsigned int seed, int chunkX, int chunkY, int n,
                          int *outX, int *outY)
{
    const ChunkMap *m = chunkMap(seed, chunkX, chunkY);
    int i, seen = 0;
    int start = (int)(cellHash(seed, chunkX + n, chunkY) % CHUNK_CELLS);
    int x0 = chunkX * CHUNK_SIZE, y0 = chunkY * CHUNK_SIZE;

    for (i = 0; i < CHUNK_CELLS; i++) {
        int off = (start + i) % CHUNK_CELLS;
        int lx = off % CHUNK_SIZE, ly = off / CHUNK_SIZE;
        if (m->cell[ly][lx] == '#') continue;
        if (seen++ == n) {
            *outX = x0 + lx;
            *outY = y0 + ly;
            return 1;
        }
    }
    return 0;
}

/* World cell derived from the seed. Each chunk picks a terrain variant (maze,
   rooms, cave, pillars) from its own hash, carves gates on the shared edges,
   and rolls independently for a monster, a chest, and a gold pile. */
char dungeonCell(unsigned int seed, int x, int y)
{
    int chunkX = floorDiv(x, CHUNK_SIZE), chunkY = floorDiv(y, CHUNK_SIZE);
    const ChunkMap *m = chunkMap(seed, chunkX, chunkY);
    int lx = x - chunkX * CHUNK_SIZE, ly = y - chunkY * CHUNK_SIZE;
    int i;
    static const struct { int kind, pct; char mark; } contents[] = {
        { 1, CHUNK_MONSTER_PCT, 'M' },
        { 2, CHUNK_CHEST_PCT, 'C' },
        { 3, CHUNK_GOLD_PCT, 'G' },
    };

    if (m->cell[ly][lx] == '#') return '#';
    for (i = 0; i < 3; i++) {
        int cx, cy;
        if (!chunkHas(seed, chunkX, chunkY, contents[i].kind, contents[i].pct)) continue;
        if (chunkFloorCell(seed, chunkX, chunkY, i + 1, &cx, &cy) && cx == x && cy == y)
            return contents[i].mark;
    }
    if (cellHash(seed, x, y) % 89u == 0) return 'T';
    return '.';
}

static int alreadyTaken(const DungeonState *st, int x, int y)
{
    int i;
    for (i = 0; i < st->takenCount; i++)
        if (st->taken[i].row == y && st->taken[i].col == x) return 1;
    return 0;
}

/* Copies the world from the cached chunks into the area; taken treasures become floor. */
static void buildArea(DungeonState *st, char area[][AREA_MAX])
{
    int r, c;
    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++) {
            int wx = st->originX + c, wy = st->originY + r;
            char cell = dungeonCell(st->seed, wx, wy);
            if ((cell == 'T' || cell == 'G' || cell == 'C' || cell == 'M') && alreadyTaken(st, wx, wy))
                cell = '.';
            area[r][c] = cell;
        }
}

/* Recentres the cache when the player leaves the middle chunk, so the player stays centred. */
static void ensureArea(DungeonState *st, char area[][AREA_MAX])
{
    int chunkX = floorDiv(st->posX, CHUNK_SIZE);
    int chunkY = floorDiv(st->posY, CHUNK_SIZE);
    int left = (chunkX - (CACHED_CHUNKS / 2)) * CHUNK_SIZE;
    int top = (chunkY - (CACHED_CHUNKS / 2)) * CHUNK_SIZE;
    if (left != st->originX || top != st->originY) {
        st->originX = left;
        st->originY = top;
        buildArea(st, area);
    }
}

/* Iterative DFS with an array stack: 1 when 'to' is reachable from 'from'. */
int dfsReachable(char area[][AREA_MAX], Point from, Point to)
{
    Point stack[MAX_PATH_CELLS];
    static int visited[AREA_MAX][AREA_MAX];
    int top = 0;
    int dRow[4] = { -1, 1, 0, 0 };
    int dCol[4] = { 0, 0, -1, 1 };
    int i;

    memset(visited, 0, sizeof visited);
    if (from.row < 0 || to.row < 0) return 0;
    stack[top] = from;
    top++;
    visited[from.row][from.col] = 1;
    while (top > 0) {
        Point t = stack[--top];
        if (t.row == to.row && t.col == to.col) return 1;
        for (i = 0; i < 4; i++) {
            int r = t.row + dRow[i];
            int c = t.col + dCol[i];
            if (r < 0 || r >= AREA_MAX || c < 0 || c >= AREA_MAX) continue;
            if (area[r][c] == '#' || visited[r][c]) continue;
            visited[r][c] = 1;
            stack[top].row = r;
            stack[top].col = c;
            top++;
        }
    }
    return 0;
}

/* BFS with an array queue and a parent grid; fills the path from 'from' to 'to'.
   Returns the number of cells in the path, or -1 when unreachable. */
int bfsShortestPath(char area[][AREA_MAX], Point from, Point to, Point path[])
{
    Point queue[MAX_PATH_CELLS];
    static Point parent[AREA_MAX][AREA_MAX];
    static int visited[AREA_MAX][AREA_MAX];
    int front = 0, rear = 0;
    int dRow[4] = { -1, 1, 0, 0 };
    int dCol[4] = { 0, 0, -1, 1 };
    int i, length = 0;
    Point walk;

    memset(visited, 0, sizeof visited);
    if (from.row < 0 || to.row < 0) return -1;
    queue[rear] = from;
    rear++;
    visited[from.row][from.col] = 1;
    parent[from.row][from.col].row = -1;
    parent[from.row][from.col].col = -1;

    while (front < rear) {
        Point t = queue[front];
        front++;
        if (t.row == to.row && t.col == to.col) {
            walk = t;
            while (walk.row != -1) {
                path[length] = walk;
                length++;
                walk = parent[walk.row][walk.col];
            }
            for (i = 0; i < length / 2; i++) {
                Point tmp = path[i];
                path[i] = path[length - 1 - i];
                path[length - 1 - i] = tmp;
            }
            return length;
        }
        for (i = 0; i < 4; i++) {
            int r = t.row + dRow[i];
            int c = t.col + dCol[i];
            if (r < 0 || r >= AREA_MAX || c < 0 || c >= AREA_MAX) continue;
            if (area[r][c] == '#' || visited[r][c]) continue;
            visited[r][c] = 1;
            parent[r][c] = t;
            queue[rear].row = r;
            queue[rear].col = c;
            rear++;
        }
    }
    return -1;
}

/* BFS for the nearest treasure inside the cached area; fills the path and returns its length. */
static int bfsToTreasure(char area[][AREA_MAX], Point from, Point path[])
{
    Point queue[MAX_PATH_CELLS];
    static Point parent[AREA_MAX][AREA_MAX];
    static int visited[AREA_MAX][AREA_MAX];
    int front = 0, rear = 0;
    int dRow[4] = { -1, 1, 0, 0 };
    int dCol[4] = { 0, 0, -1, 1 };
    int length = 0;
    Point walk;

    memset(visited, 0, sizeof visited);
    queue[rear] = from;
    rear++;
    visited[from.row][from.col] = 1;
    parent[from.row][from.col].row = -1;
    parent[from.row][from.col].col = -1;

    while (front < rear) {
        Point t = queue[front];
        front++;
        if (area[t.row][t.col] == 'T') {
            int i;
            walk = t;
            while (walk.row != -1) {
                path[length] = walk;
                length++;
                walk = parent[walk.row][walk.col];
            }
            for (i = 0; i < length / 2; i++) {
                Point tmp = path[i];
                path[i] = path[length - 1 - i];
                path[length - 1 - i] = tmp;
            }
            return length;
        }
        {
            int i;
            for (i = 0; i < 4; i++) {
                int r = t.row + dRow[i];
                int c = t.col + dCol[i];
                if (r < 0 || r >= AREA_MAX || c < 0 || c >= AREA_MAX) continue;
                if (area[r][c] == '#' || visited[r][c]) continue;
                visited[r][c] = 1;
                parent[r][c] = t;
                queue[rear].row = r;
                queue[rear].col = c;
                rear++;
            }
        }
    }
    return -1;
}

/* Non-interactive status line: same fields as the boxed panel, colorized,
   with a text HP bar so piped output still shows the health ratio. */
static void printDungeonStatus(const DungeonState *st, const Player *p, int steps)
{
    char bar[32];
    int w = 20, filled = p->maxHp > 0 ? p->hp * w / p->maxHp : 0;
    int i;

    if (filled < 0) filled = 0;
    if (filled > w) filled = w;
    for (i = 0; i < w; i++) bar[i] = i < filled ? '#' : '-';
    bar[w] = '\0';
    printf("Posisi dunia (%d,%d) | langkah %d | HP %s%d/%d%s [%s] | gold %d | seed %u\n",
           st->posX, st->posY, steps, hp_color(p->hp, p->maxHp), p->hp, p->maxHp,
           ANSI_RESET, bar, p->gold, st->seed);
}

/* Marks a content cell as used, so it never spawns again in this run. */
static void consumeCell(DungeonState *st, int x, int y, char area[][AREA_MAX])
{
    if (st->takenCount < MAX_TAKEN_CELLS) {
        st->taken[st->takenCount].row = y;
        st->taken[st->takenCount].col = x;
        st->takenCount++;
    }
    area[y - st->originY][x - st->originX] = '.';
}

/* Chest loot: equipment rolls a rarity with the same weights as gacha, then
   picks an item of that rarity from the catalog. */
typedef struct {
    int kind;          /* 0 gold, 1 potion, 2 equipment */
    int amount;
    Item item;
    int rarity;
} Loot;

static void rollLoot(unsigned int seed, int x, int y, const Item catalog[], int catalogCount,
                     Loot *out)
{
    int roll = (int)(cellHash(seed ^ 0x5bf03635u, x, y) % 100u);

    memset(out, 0, sizeof *out);
    if (roll < CHEST_EQUIPMENT_PCT) {
        int rarity = (int)(cellHash(seed ^ 0x2545f491u, x, y) % 100u) + 1;
        Item candidates[MAX_ITEM];
        int n = 0, i;

        if (rarity >= 98) out->rarity = RARITY_LEGENDARY;
        else if (rarity >= 86) out->rarity = RARITY_EPIC;
        else if (rarity >= 61) out->rarity = RARITY_RARE;
        else out->rarity = RARITY_COMMON;
        for (i = 0; i < catalogCount; i++)
            if (catalog[i].rarity == out->rarity) candidates[n++] = catalog[i];
        if (n > 0) {
            out->kind = 2;
            out->item = candidates[cellHash(seed, x, y) % (unsigned int)n];
            return;
        }
        out->kind = 0;
        out->amount = CHEST_GOLD_MAX;
        return;
    }
    if (roll < CHEST_EQUIPMENT_PCT + CHEST_POTION_PCT) {
        out->kind = 1;
        out->amount = CHEST_POTION_COUNT;
        return;
    }
    out->kind = 0;
    out->amount = CHEST_GOLD_MIN
                  + (int)(cellHash(seed, x, y) % (unsigned int)(CHEST_GOLD_MAX - CHEST_GOLD_MIN + 1));
}

/* Horizontal slot: a strip of rarity cells slides right-to-left past a
   viewport and decelerates onto the result. The outcome is already fixed by
   the seed, so the animation never changes it. */
#define SLOT_CELL 8
#define SLOT_VIEW 32

static int slotRarityAt(int index, int salt)
{
    return (int)(cellHash((unsigned int)salt * 2654435761u, index, 7) % 4u) + 1;
}

static void chestAnimation(const Loot *loot)
{
    static const char *names[] = { "Common", "Rare", "Epic", "Legendary" };
    int frame, i, offset = 0, speed = 26;

    if (!caps_interactive()) return;
    for (frame = 0;; frame++) {
        int done = frame >= 28;
        int win = (SLOT_VIEW / SLOT_CELL / 2) * SLOT_CELL;

        if (!done) {
            offset += frame < 8 ? 3 : (frame < 16 ? 2 : 1);
            speed += 6;
        } else {
            offset = win;
        }

        ui_frame("Chest");
        ui_panel(7, 0, UI_COLS, 7, done ? "Hasil" : "Gacha");
        for (i = 0; i <= SLOT_VIEW / SLOT_CELL; i++) {
            int x = i * SLOT_CELL - offset % SLOT_CELL;
            int index = i + offset / SLOT_CELL;
            int rarity = done ? (index == SLOT_VIEW / SLOT_CELL / 2 ? loot->rarity
                                                                   : slotRarityAt(index, 0))
                              : slotRarityAt(index, 0);
            if (x < 0 || x + 7 > SLOT_VIEW) continue;
            /* every cell keeps its own rarity colour while the strip scrolls,
               so the animation shows the spread of outcomes, not a grey blur */
            render_text(9, 2 + x, style_rarity(rarity), "%-7s", names[rarity - 1]);
        }
        render_text(11, 6, ST_ACCENT, "%s", done ? "^" : "");
        ui_footer(NULL);
        ui_flush();
        if (done) break;
        platform_sleep_ms(speed > 120 ? 120 : speed);
    }
}

/* Applies what the player stepped on and writes the message for the frame. */
static void handleContent(Player *p, const Monster monsters[], int monsterCount,
                          const Item catalog[], int catalogCount,
                          char cell, int wx, int wy, char area[][AREA_MAX],
                          char *message)
{
    if (cell == 'T') {
        p->gold += TREASURE_GOLD;
        sprintf(message, "Treasure! +%d gold.", TREASURE_GOLD);
        return;
    }
    if (cell == 'G') {
        int amount = DUNGEON_GOLD_MIN
                     + (int)(cellHash(p->seed ^ 0x27d4eb2fu, wx, wy)
                             % (unsigned int)(DUNGEON_GOLD_MAX - DUNGEON_GOLD_MIN + 1));
        p->gold += amount;
        sprintf(message, "Gold! +%d gold.", amount);
        return;
    }
    if (cell == 'M') {
        if (monsterCount > 0) {
            const Monster *m = &monsters[cellHash(p->seed ^ 0x1b873593u, wx, wy)
                                         % (unsigned int)monsterCount];
            sprintf(message, "Monster %s muncul!", m->name);
            startBattleAgainst(p, m);
        }
        return;
    }
    if (cell == 'C') {
        Loot loot;
        rollLoot(p->seed, wx, wy, catalog, catalogCount, &loot);
        chestAnimation(&loot);
        if (loot.kind == 2) {
            int index = findItemInInventory(p, loot.item.id);
            if (index >= 0) {
                int price = duplicatePrice(loot.item.rarity);
                p->gold += price;
                sprintf(message, "Chest: %s (duplikat) jadi +%d gold.", loot.item.name, price);
            } else if (p->itemCount < MAX_ITEM) {
                p->inventory[p->itemCount++] = loot.item;
                sprintf(message, "Chest: [%s] %s (power %d)!",
                        rarityName(loot.item.rarity), loot.item.name, loot.item.power);
            } else {
                p->gold += duplicatePrice(loot.item.rarity);
                sprintf(message, "Inventory penuh, %s dijual jadi +%d gold.",
                        loot.item.name, duplicatePrice(loot.item.rarity));
            }
        } else if (loot.kind == 1) {
            p->potion += loot.amount;
            if (p->potion > MAX_POTION) p->potion = MAX_POTION;
            sprintf(message, "Chest: +%d potion.", loot.amount);
        } else {
            p->gold += loot.amount;
            sprintf(message, "Chest: +%d gold.", loot.amount);
        }
        (void)area;
    }
}

/* Draws a window of the cached area with the player always in the middle. */
/* Visible radius: cells further than this from the player stay hidden, so
   chunks are still cached around him but only the nearby part is drawn.
   Character cells are about twice as tall as they are wide, so the distance
   test weights the vertical offset to keep the lit area round on screen
   instead of an ellipse. */
#define FOG_RADIUS 6.0f
#define FOG_VERTICAL 2.0f   /* vertical cells per horizontal cell, roughly */

static int fogged(int dr, int dc)
{
    float du = (float)dc;
    float dv = (float)dr * FOG_VERTICAL;
    float d2 = du * du + dv * dv;
    return d2 > FOG_RADIUS * FOG_RADIUS * FOG_VERTICAL;
}

/* View size follows the terminal: width is the terminal width minus the map
   margins, height is what is left above the status and message rows. */
static void viewSize(int *w, int *h)
{
    int cols = caps_interactive() ? caps_cols() : UI_COLS;
    int rows = caps_interactive() ? caps_rows() : UI_ROWS;
    *w = cols - 2;
    *h = rows - 10;
    if (*w > VIEW_WIDTH) *w = VIEW_WIDTH;
    if (*h > VIEW_HEIGHT) *h = VIEW_HEIGHT;
    if (*w < 11) *w = 11;
    if (*h < 7) *h = 7;
}

/* Biome of a world position: each 32x32 block maps to one palette, so walking
   far enough changes the wall colors without touching the map shape. */
static Style biomeStyle(unsigned int seed, int x, int y)
{
    unsigned int h = cellHash(seed ^ 0x7f4a7c15u, floorDiv(x, 32), floorDiv(y, 32));
    return ST_BIOME((int)(h % BIOME_COUNT));
}

/* Draws one map cell: trail first, then the world, hidden beyond the fog. */
static void drawCell(unsigned int seed, char area[][AREA_MAX], char trail[][AREA_MAX],
                     int r, int c, int wx, int wy, int fogged,
                     int interactive, int srow, int scol)
{
    char cell;

    if (fogged) {
        if (interactive) render_text(srow, scol, ST_DIM, " ");
        else printf(" ");
        return;
    }
    if (trail[r][c] == '*') {
        if (interactive) render_text(srow, scol, ST_ACCENT, "*");
        else printf("*");
        return;
    }
    cell = area[r][c];
    if (!interactive) {
        printf("%c", cell == 'T' ? '$' : cell);
        return;
    }
    switch (cell) {
    case '#': render_text(srow, scol, biomeStyle(seed, wx, wy), " "); break;
    case '.': render_text(srow, scol, ST_DIM, "."); break;
    case 'T': render_text(srow, scol, ST_GOLD, "$"); break;
    case 'G': render_text(srow, scol, ST_GOLD, "G"); break;
    case 'C': render_text(srow, scol, ST_GOLD, "C"); break;
    case 'M': render_text(srow, scol, ST_DANGER, "M"); break;
    default: render_text(srow, scol, ST_DIM, "%c", cell); break;
    }
}

static void renderDungeon(char area[][AREA_MAX], const DungeonState *st,
                          char trail[][AREA_MAX], const char *message, int steps,
                          const Player *p)
{
    int ax = st->posX - st->originX;
    int ay = st->posY - st->originY;
    int vw, vh, tx, ty;
    int wid = caps_interactive() ? caps_cols() : UI_COLS;

    viewSize(&vw, &vh);

    if (!caps_interactive()) {
        clear_screen();
        for (ty = 0; ty < vh; ty++) {
            for (tx = 0; tx < vw; tx++) {
                int dr = ty - vh / 2, dc = tx - vw / 2;
                if (ty == vh / 2 && tx == vw / 2) printf("P");
                else drawCell(st->seed, area, trail, ay + dr, ax + dc,
                              st->posX + dc, st->posY + dr,
                              fogged(dr, dc), 0, 0, 0);
            }
            printf("\n");
        }
        printDungeonStatus(st, p, steps);
        printf("%s\n", message);
        printf("Gerak (W/A/S/D, H hint %d gold, Q keluar): ", HINT_COST);
        fflush(stdout);
        return;
    }

    ui_frame("Dungeon");
    for (ty = 0; ty < vh; ty++) {
        for (tx = 0; tx < vw; tx++) {
            int dr = ty - vh / 2, dc = tx - vw / 2;
            int srow = 2 + ty, scol = 1 + tx;
            if (ty == vh / 2 && tx == vw / 2)
                render_text(srow, scol, ST_TITLE, "P");
            else
                drawCell(st->seed, area, trail, ay + dr, ax + dc,
                         st->posX + dc, st->posY + dr,
                         fogged(dr, dc), 1, srow, scol);
        }
    }
    ui_panel(17, 0, wid, 5, "Status");
    render_text_clipped(18, 2, wid - 4, ST_TEXT, "Posisi dunia (%d,%d)", st->posX, st->posY);
    render_text_clipped(18, 21, 10, ST_TEXT, "langkah %d", steps);
    render_text_clipped(18, 34, 12, ST_GOLD, "gold %d", p->gold);
    render_text(19, 2, style_hp(p->hp, p->maxHp), "HP %d/%d", p->hp, p->maxHp);
    ui_hp_bar(20, 2, 24, p->hp, p->maxHp);
    render_text_clipped(20, 28, wid - 30, ST_DIM, "seed %u", st->seed);
    render_text_clipped(22, 2, wid - 4, ST_DIM, "%s", message);
    render_text(23, 2, ST_DIM, "Gerak (W/A/S/D, H hint %d gold, Q keluar)", HINT_COST);
    ui_footer(NULL);
    ui_flush();
}

/* Seed from the clock, used when the player leaves the seed prompt empty. */
unsigned int randomWorldSeed(void)
{
    return (unsigned int)time(NULL);
}

/* Asks for a world seed. Empty input draws a fresh one, non-numeric input is rejected. */
int promptWorldSeedTry(unsigned int *out)
{
    char line[32];

    if (!caps_interactive()) {
        for (;;) {
            printf("Seed dunia (Enter saja untuk acak): ");
            if (!readLine(line, sizeof line)) {
                *out = randomWorldSeed();
                printf("\nSeed acak dipilih: %u\n", *out);
                return 1;
            }
            if (line[0] == '\0') {
                *out = randomWorldSeed();
                printf("Seed acak dipilih: %u\n", *out);
                return 1;
            }
            {
                char *end;
                unsigned long value;
                errno = 0;
                value = strtoul(line, &end, 10);
                while (*end != '\0' && isspace((unsigned char)*end)) end++;
                if (line[0] == '-' || *end != '\0' || errno == ERANGE || value > 4294967295UL) {
                    printf("Seed harus angka bulat 0 sampai 4294967295.\n");
                    continue;
                }
                *out = (unsigned int)value;
                return 1;
            }
        }
    }

    ui_input_screen("Game Baru");
    for (;;) {
        unsigned int seed;
        ui_line_begin("Seed dunia (Enter saja untuk acak):");
        {
            int st = readLineEsc(line, sizeof line);
            if (st < 0) {
                ui_line_end();
                return 0;
            }
            if (st == 0) {
                seed = randomWorldSeed();
                ui_line_note("Seed acak dipilih: %u", seed);
                ui_line_end();
                *out = seed;
                return 1;
            }
        }
        if (line[0] == '\0') {
            seed = randomWorldSeed();
            ui_line_note("Seed acak dipilih: %u", seed);
            ui_line_end();
            *out = seed;
            return 1;
        }
        {
            char *end;
            unsigned long value;
            errno = 0;
            value = strtoul(line, &end, 10);
            while (*end != '\0' && isspace((unsigned char)*end)) end++;
            if (line[0] == '-' || *end != '\0' || errno == ERANGE || value > 4294967295UL) {
                ui_line_error("Seed harus angka bulat 0 sampai 4294967295.");
                continue;
            }
            ui_line_end();
            *out = (unsigned int)value;
            return 1;
        }
    }
}

unsigned int promptWorldSeed(void)
{
    unsigned int seed = 0;
    if (!promptWorldSeedTry(&seed)) seed = randomWorldSeed();
    return seed;
}

/* Turn loop shared by a fresh run and a restored session. */
static void dungeonLoop(Player *p, DungeonState *st, char area[][AREA_MAX],
                        char trail[][AREA_MAX], Point path[],
                        int steps, int hintsUsed, const char *initialMsg,
                        const Item catalog[], int catalogCount,
                        const Monster monsters[], int monsterCount)
{
    int finished = 0;
    int drewBattle = 0;
    char message[128];
    char buf[256];

    snprintf(message, sizeof message, "%s", initialMsg);
    while (!finished) {
        char cmd;
        if (drewBattle) drewBattle = 0;   /* keep the battle frame on screen */
        else renderDungeon(area, st, trail, message, steps, p);
        if (platform_shutdown_requested()) break;
        if (caps_interactive()) {
            char ch;
            TermKey k = term_getkey(&ch);
            if (k == KEY_EOF) break;
            if (k == KEY_RESIZE || k == KEY_ENTER) continue;
            if (k == KEY_UP) cmd = 'W';
            else if (k == KEY_DOWN) cmd = 'S';
            else if (k == KEY_LEFT) cmd = 'A';
            else if (k == KEY_RIGHT) cmd = 'D';
            else if (k == KEY_ESC) cmd = 'Q';
            else if (k == KEY_CHAR) cmd = (char)toupper((unsigned char)ch);
            else continue;
        } else {
            if (!readLine(buf, sizeof buf)) break;
            if (buf[0] == '\0' || buf[1] != '\0') {
                strcpy(message, "Masukkan satu huruf: W/A/S/D, H, atau Q.");
                continue;
            }
            cmd = (char)toupper((unsigned char)buf[0]);
        }
        switch (cmd) {
        case 'W':
        case 'A':
        case 'S':
        case 'D': {
            int dRow = 0, dCol = 0;
            if (cmd == 'W') dRow = -1;
            if (cmd == 'S') dRow = 1;
            if (cmd == 'A') dCol = -1;
            if (cmd == 'D') dCol = 1;
            {
                int wx = st->posX + dCol, wy = st->posY + dRow;
                char cell = dungeonCell(st->seed, wx, wy);
                if (cell == '#') {
                    strcpy(message, "Tembok! Tidak bisa lewat.");
                } else {
                    int treasure = 0;
                    st->posX = wx;
                    st->posY = wy;
                    steps++;
                    ensureArea(st, area);
                    memset(trail, ' ', (size_t)AREA_MAX * AREA_MAX);
                    if ((cell == 'T' || cell == 'G' || cell == 'C' || cell == 'M')
                        && !alreadyTaken(st, wx, wy)) {
                        consumeCell(st, wx, wy, area);
                        treasure = (cell == 'T' || cell == 'G' || cell == 'C');
                        handleContent(p, monsters, monsterCount, catalog, catalogCount,
                                      cell, wx, wy, area, message);
                        /* the battle already drew its own screen, so the loop
                           must not paint over it until the player presses a key */
                        if (cell == 'M') drewBattle = 1;
                    } else {
                        sprintf(message, "Posisi (%d,%d), langkah %d.", wx, wy, steps);
                    }
                    autosave_publish(p, AS_SCREEN_DUNGEON, NULL, 0, st, steps, hintsUsed, treasure);
                }
            }
            break;
        }
        case 'H':
            if (hintsUsed >= MAX_HINTS) {
                sprintf(message, "Hint sudah habis (maks %d kali).", MAX_HINTS);
            } else if (p->gold < HINT_COST) {
                sprintf(message, "Gold kurang, hint butuh %d gold.", HINT_COST);
            } else {
                Point pos;
                int length;
                p->gold -= HINT_COST;
                hintsUsed++;
                memset(trail, ' ', (size_t)AREA_MAX * AREA_MAX);
                pos.row = st->posY - st->originY;
                pos.col = st->posX - st->originX;
                length = bfsToTreasure(area, pos, path);
                if (length < 0) {
                    strcpy(message, "Di sekitar sini tidak ada treasure.");
                } else {
                    int i;
                    for (i = 0; i < length; i++)
                        trail[path[i].row][path[i].col] = '*';
                    sprintf(message, "Treasure terdekat %d langkah dari sini. Sisa hint %d.",
                            length - 1, MAX_HINTS - hintsUsed);
                }
                autosave_publish(p, AS_SCREEN_DUNGEON, NULL, 0, st, steps, hintsUsed, 1);
            }
            break;
        case 'Q':
            if (caps_interactive()) {
                ui_input_screen("Dungeon");
                if (!confirmPrompt("Keluar dungeon? Progress langkah hilang. (y/n): ")) {
                    strcpy(message, "Keluar dungeon dibatalkan.");
                    break;
                }
            }
            if (!caps_interactive()) {
                printf("\033[?25h");
                clear_screen();
            }
            set_message("Keluar dungeon. Langkah total: %d.", steps);
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
            ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
            finished = 1;
            break;
        default:
            strcpy(message, "Gerakan tidak dikenal.");
            break;
        }
    }
    if (!finished && !caps_interactive()) printf("\033[?25h");
}

void runDungeon(Player *p, const Item catalog[], int catalogCount,
                const Monster monsters[], int monsterCount)
{
    char area[AREA_MAX][AREA_MAX];
    char trail[AREA_MAX][AREA_MAX];
    Point path[MAX_PATH_CELLS];
    DungeonState st;
    char message[128];
    Point from, far;
    char entry[16];

    st.seed = p->seed;
    st.posX = 1; st.posY = 1;
    st.takenCount = 0;
    st.originX = (floorDiv(st.posX, CHUNK_SIZE) - CACHED_CHUNKS / 2) * CHUNK_SIZE;
    st.originY = (floorDiv(st.posY, CHUNK_SIZE) - CACHED_CHUNKS / 2) * CHUNK_SIZE;
    buildArea(&st, area);
    memset(trail, ' ', sizeof trail);

    if (caps_interactive()) {
        ui_frame("Dungeon");
        render_text(2, 0, ST_DIM,
                    "Dunia dungeon dibangun dari seed %u. Masukkan seed ini lagi kapan saja untuk peta yang persis sama.", st.seed);
        render_text(3, 0, ST_DIM,
                    "Jalan ke segala arah. Kumpulkan treasure ($), lalu tekan Q kalau sudah puas.");
        render_text(5, 0, ST_TEXT, "Tekan Enter untuk masuk:");
        ui_footer(NULL);
        ui_flush();
        {
            char ch;
            TermKey k;
            do { k = term_getkey(&ch); } while (k != KEY_ENTER && k != KEY_ESC && k != KEY_EOF);
            if (k == KEY_EOF) return;
        }
    } else {
        clear_screen();
        printf("%s", ANSI_DIM);
        printf("Dunia dungeon dibangun dari seed %u. Masukkan seed ini lagi kapan saja untuk peta yang persis sama.\n", st.seed);
        printf("Jalan ke segala arah. Kumpulkan treasure ($), lalu tekan Q kalau sudah puas.\n");
        printf("%s", ANSI_RESET);
        printf("Tekan Enter untuk masuk: ");
        if (!readLine(entry, sizeof entry)) {
            printf("\n");
            return;
        }
        printf("\033[?25l");
    }

    /* DFS sanity check on the starting area: start cell must reach the far corridor end. */
    from.row = st.posY - st.originY;
    from.col = st.posX - st.originX;
    far.row = AREA_MAX - 3;
    far.col = AREA_MAX - 3;
    if (dfsReachable(area, from, far))
        strcpy(message, "Area awal tersambung dari titik awal sampai isi terjauh (DFS).");
    else
        strcpy(message, "Area awal tidak tersambung.");

    dungeonLoop(p, &st, area, trail, path, 0, 0, message, catalog, catalogCount,
                monsters, monsterCount);
}

/* Continues a dungeon run restored from the autosave at the same position. */
void resumeDungeon(Player *p, const DungeonState *saved, int steps, int hints,
                   const Item catalog[], int catalogCount,
                   const Monster monsters[], int monsterCount)
{
    char area[AREA_MAX][AREA_MAX];
    char trail[AREA_MAX][AREA_MAX];
    Point path[MAX_PATH_CELLS];
    DungeonState st = *saved;

    buildArea(&st, area);
    memset(trail, ' ', sizeof trail);
    dungeonLoop(p, &st, area, trail, path, steps, hints, "Sesi dipulihkan dari autosave.",
                catalog, catalogCount, monsters, monsterCount);
}
