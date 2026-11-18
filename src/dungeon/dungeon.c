#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

/* Centres the cached area under the viewport, so every drawn cell has a valid
   area index whatever the camera does. The camera moves at most one cell per
   player step, so a rebuild costs one area pass per frame that scrolls. */
static void syncArea(DungeonState *st, char area[][AREA_MAX], int camX, int camY,
                     int vw, int vh)
{
    int left = camX - (AREA_MAX - vw) / 2;
    int top = camY - (AREA_MAX - vh) / 2;

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
        else out->rarity = 1;
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
/* Reel geometry: the longest label ("Legendary") is 9 cells wide, so each
   cell reserves ten and every label is padded to nine, leaving one blank
   column between names instead of letting them bleed into each other. */
#define SLOT_CELL 10
#define SLOT_LABEL 9
#define SLOT_VIEW 40

static int slotRarityAt(int index, int salt)
{
    return (int)(cellHash((unsigned int)salt * 2654435761u, index, 7) % 4u) + 1;
}

#define SLOT_SCROLL_FRAMES 30   /* strip travel, about 2.6 s of decelerating motion */
#define SLOT_SETTLE_FRAMES 6    /* highlight blinks after the strip lands */
#define SLOT_FRAMES (SLOT_SCROLL_FRAMES + SLOT_SETTLE_FRAMES)

/* Applies the rolled chest loot and writes the message for the frame. */
static void applyLoot(Player *p, const Loot *loot, char *message)
{
    if (loot->kind == 2) {
        int index = findItemInInventory(p, loot->item.id);
        if (index >= 0) {
            int price = duplicatePrice(loot->item.rarity);
            p->gold += price;
            sprintf(message, "Chest: %s (duplikat) jadi +%d gold.", loot->item.name, price);
        } else if (p->itemCount < MAX_ITEM) {
            p->inventory[p->itemCount++] = loot->item;
            sprintf(message, "Chest: [%s] %s (power %d)!",
                    rarityName(loot->item.rarity), loot->item.name, loot->item.power);
        } else {
            p->gold += duplicatePrice(loot->item.rarity);
            sprintf(message, "Inventory penuh, %s dijual jadi +%d gold.",
                    loot->item.name, duplicatePrice(loot->item.rarity));
        }
    } else if (loot->kind == 1) {
        p->potion += loot->amount;
        if (p->potion > MAX_POTION) p->potion = MAX_POTION;
        sprintf(message, "Chest: +%d potion.", loot->amount);
    } else {
        p->gold += loot->amount;
        sprintf(message, "Chest: +%d gold.", loot->amount);
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
        applyLoot(p, &loot, message);
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

/* Fog opacity as a distance band: 0 = fully lit, 1..3 = progressively dimmer,
   4 = hidden. The ring edges are fractions of FOG_RADIUS, so the light keeps
   fading out towards the rim instead of ending on a hard circle. */
#define FOG_TIERS 4
static int fogTier(int dr, int dc)
{
    float du = (float)dc;
    float dv = (float)dr * FOG_VERTICAL;
    float d2 = du * du + dv * dv;
    float limit = FOG_RADIUS * FOG_RADIUS * FOG_VERTICAL;
    float t;

    if (d2 > limit) return FOG_TIERS;
    t = d2 / limit;                     /* 0 at the player, 1 at the rim */
    if (t <= 0.16f) return 0;           /* 40% of the radius stays lit */
    if (t <= 0.42f) return 1;
    if (t <= 0.72f) return 2;
    return 3;
}

#define CAMERA_MARGIN 3     /* safe cells kept between the player and the border */
#define PANEL_ROWS 7        /* the Battle/Gacha box: five content rows + borders */
#define STATUS_ROWS 5
#define STATUS_ROWS_NARROW 6
#define STATUS_LABEL_W 8    /* fixed label column, so every value lines up */

/* Camera is the world coordinate shown in the viewport's top-left cell. */
typedef struct { int x, y; } Camera;

/* Battle resolved inside the dungeon screen: the map keeps drawing, so the
   encounter lives between the Peta and Status panels instead of taking over
   the whole terminal. */
typedef struct {
    int active;
    Monster enemy;
    int enemyHp;
    char log[3][80];       /* newest last, at most three rows */
    int logCount;
} InlineBattle;

/* Chest gacha slot, drawn in the same panel slot as the battle: the map keeps
   rendering around it and the strip animation advances one frame per loop
   pass instead of taking over the screen. */
typedef struct {
    int active;
    Loot loot;
    int frame;
} InlineSlot;

/* Screen position of a world coordinate: the camera is the only offset. */
static int world_ke_screen(int world, int cam)
{
    return world - cam;
}

/* The Status panel needs one extra row when the Posisi line has to be split. */
static int statusRows(int wid)
{
    return wid < 50 ? STATUS_ROWS_NARROW : STATUS_ROWS;
}

/* Viewport = the whole interior of the Peta box. Frame, Status panel, message
   row and (during a battle) the Battle panel come out of the terminal first.
   Width is capped by the cached area, which must cover every drawn cell. */
static void hitung_viewport(int panelActive, int *w, int *h)
{
    int cols = caps_interactive() ? caps_cols() : UI_COLS;
    int rows = caps_interactive() ? caps_rows() : UI_ROWS;
    int extra = panelActive ? PANEL_ROWS : 0;

    *w = cols - 2;
    if (*w > AREA_MAX - 2) *w = AREA_MAX - 2;
    if (*w < 11) *w = 11;
    /* 2 frame rows + 1 message row + the gap above the footer */
    *h = rows - (7 + statusRows(*w) + extra);
    if (*h > VIEW_HEIGHT) *h = VIEW_HEIGHT;
    if (*h < 5) *h = 5;
}

/* Keeps the player inside the edge margin: the camera only moves when he
   reaches the safe border, so he can walk freely inside the box before the
   map scrolls. It also re-fits him after a resize. The world is procedural and
   endless, so the camera is never clamped to a world edge. */
static void update_kamera(Camera *cam, int posX, int posY, int vw, int vh, int margin)
{
    int mh = vw > 2 * margin + 1 ? margin : 0;
    int mv = vh > 2 * margin + 1 ? margin : 0;

    if (world_ke_screen(posX, cam->x) < mh) cam->x = posX - mh;
    if (world_ke_screen(posX, cam->x) > vw - 1 - mh) cam->x = posX - (vw - 1 - mh);
    if (world_ke_screen(posY, cam->y) < mv) cam->y = posY - mv;
    if (world_ke_screen(posY, cam->y) > vh - 1 - mv) cam->y = posY - (vh - 1 - mv);
}

/* Biome of a world position: each 32x32 block maps to one palette, so walking
   far enough changes the wall colors without touching the map shape. */
static int biomeIndex(unsigned int seed, int x, int y)
{
    unsigned int h = cellHash(seed ^ 0x7f4a7c15u, floorDiv(x, 32), floorDiv(y, 32));
    return (int)(h % BIOME_COUNT);
}

static Style biomeStyle(unsigned int seed, int x, int y)
{
    return ST_BIOME(biomeIndex(seed, x, y));
}

/* Draws one map cell: trail first, then the world, hidden beyond the fog.
   `tier` is fogTier(): 0 is fully lit, 1..3 fade the same glyph towards the
   background, so the light has an opacity falloff instead of a hard edge. */
static void drawCell(unsigned int seed, char area[][AREA_MAX], char trail[][AREA_MAX],
                     int r, int c, int wx, int wy, int tier,
                     int interactive, int srow, int scol)
{
    char cell;
    Style faded = tier > 0 ? ST_FOG_LEVEL(tier - 1) : ST_TEXT;

    if (tier >= FOG_TIERS) {
        if (interactive) render_text(srow, scol, ST_DIM, " ");
        else printf(" ");
        return;
    }
    if (trail[r][c] == '*') {
        if (interactive) render_text(srow, scol, tier > 0 ? faded : ST_ACCENT, "*");
        else printf("*");
        return;
    }
    cell = area[r][c];
    if (!interactive) {
        printf("%c", cell == 'T' ? '$' : cell);
        return;
    }
    if (tier > 0) {
        /* inside the falloff band: same glyph, dimmer colour */
        if (cell == '#')
            render_text(srow, scol, ST_BIOME_FOG(tier - 1, biomeIndex(seed, wx, wy)), " ");
        else if (cell == '.')
            render_text(srow, scol, faded, ".");
        else if (cell == 'T')
            render_text(srow, scol, faded, "$");
        else
            render_text(srow, scol, faded, "%c", cell);
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

/* Draws the viewport: screen cell (sx, sy) shows world (cam + sx, cam + sy),
   and the fog circle is measured from the player, so it follows him and may be
   clipped by the box edge. Cells outside the cached area stay dark. */
static void renderPeta(char area[][AREA_MAX], const DungeonState *st, const Camera *cam,
                       char trail[][AREA_MAX], int vw, int vh, int wid)
{
    int sy, sx;

    ui_panel(2, 0, wid, vh + 2, "Peta");
    for (sy = 0; sy < vh; sy++) {
        int wy = cam->y + sy;
        for (sx = 0; sx < vw; sx++) {
            int wx = cam->x + sx;
            int ar = wy - st->originY, ac = wx - st->originX;
            int srow = 3 + sy, scol = 1 + sx;
            if (wx == st->posX && wy == st->posY) {
                render_text(srow, scol, ST_TITLE, "P");
            } else if (ar < 0 || ar >= AREA_MAX || ac < 0 || ac >= AREA_MAX) {
                render_text(srow, scol, ST_DIM, " ");
            } else {
                drawCell(st->seed, area, trail, ar, ac, wx, wy,
                         fogTier(wy - st->posY, wx - st->posX), 1, srow, scol);
            }
        }
    }
}

static void battleLogPush(InlineBattle *ib, const char *fmt, ...)
{
    va_list ap;

    if (ib->logCount == 3) {
        memmove(ib->log[0], ib->log[1], sizeof ib->log[0]);
        memmove(ib->log[1], ib->log[2], sizeof ib->log[1]);
        ib->logCount = 2;
    }
    va_start(ap, fmt);
    vsnprintf(ib->log[ib->logCount], sizeof ib->log[0], fmt, ap);
    va_end(ap);
    ib->logCount++;
}

/* Enemy and monster come from the seed exactly like the full battle screen, so
   the same cell always meets the same monster. */
static int startInlineBattle(InlineBattle *ib, const Player *p, const Monster monsters[],
                             int monsterCount, int wx, int wy)
{
    const Monster *m;

    if (monsterCount <= 0) return 0;
    m = &monsters[cellHash(p->seed ^ 0x1b873593u, wx, wy) % (unsigned int)monsterCount];
    memset(ib, 0, sizeof *ib);
    ib->enemy = *m;
    ib->enemyHp = m->hp;
    ib->active = 1;
    battleLogPush(ib, "%s menghadang!", m->name);
    return 1;
}

/* Same crit rule as battle.c, kept local so the dungeon never reaches into
   the full-screen battle module's statics. */
static int rollDamage(int atk, int def, int *critical)
{
    int dmg = calculateDamage(atk, def);

    *critical = 0;
    if (plat_rand() % 100 < CRIT_CHANCE) {
        *critical = 1;
        dmg *= CRIT_MULTIPLIER;
    }
    return dmg;
}

static void enemyTurn(InlineBattle *ib, Player *p)
{
    int critical;
    int dmg = rollDamage(ib->enemy.atk, totalDef(p), &critical);

    p->hp -= dmg;
    battleLogPush(ib, "%s menyerang, damage %d%s", ib->enemy.name, dmg,
                  critical ? " KRITIS!" : "");
}

static void battleSerang(InlineBattle *ib, Player *p, char *message)
{
    int critical;
    int dmg = rollDamage(totalAtk(p), ib->enemy.def, &critical);

    ib->enemyHp -= dmg;
    battleLogPush(ib, "Kamu menyerang, damage %d%s", dmg, critical ? " KRITIS!" : "");
    if (ib->enemyHp <= 0) {
        ib->enemyHp = 0;
        battleLogPush(ib, "%s kalah!", ib->enemy.name);
        addExp(p, ib->enemy.exp);
        p->gold += ib->enemy.gold;
        p->score += ib->enemy.exp;
        ib->active = 0;
        sprintf(message, "%s kalah. +%d EXP, +%d gold, +%d skor.",
                ib->enemy.name, ib->enemy.exp, ib->enemy.gold, ib->enemy.exp);
        return;
    }
    enemyTurn(ib, p);
}

static void battleDefend(InlineBattle *ib, Player *p, char *message)
{
    int critical;
    int raw = rollDamage(ib->enemy.atk, totalDef(p), &critical);
    int dmg = raw / 2;

    if (dmg < 1) dmg = 1;
    p->hp -= dmg;
    battleLogPush(ib, "Bertahan: damage masuk %d (dari %d)", dmg, raw);
    sprintf(message, "Bertahan, damage masuk %d.", dmg);
}

static void battlePotion(InlineBattle *ib, Player *p, char *message)
{
    if (p->potion <= 0) {
        battleLogPush(ib, "Potion habis!");
        strcpy(message, "Potion habis, giliran tidak terpakai.");
        return;
    }
    p->potion--;
    p->hp += POTION_HEAL;
    if (p->hp > p->maxHp) p->hp = p->maxHp;
    battleLogPush(ib, "Potion: HP %d/%d", p->hp, p->maxHp);
    sprintf(message, "Potion dipakai, HP %d/%d.", p->hp, p->maxHp);
    enemyTurn(ib, p);
}

static void battleFlee(InlineBattle *ib, Player *p, char *message)
{
    if (plat_rand() % 2 == 0) {
        ib->active = 0;
        battleLogPush(ib, "Kabur berhasil.");
        strcpy(message, "Kabur berhasil, tanpa hadiah.");
        return;
    }
    battleLogPush(ib, "Kabur gagal!");
    strcpy(message, "Kabur gagal!");
    enemyTurn(ib, p);
}

/* Battle panel: monster name + HP bar, three action log rows, action keys. */
static void renderBattle(const InlineBattle *ib, int row, int wid)
{
    char num[24];
    int nameLen = ui_visible_len(ib->enemy.name);
    int maxName = wid / 2 - 2;
    int valueCol, numLen, barW, i;

    if (maxName < 4) maxName = 4;
    if (nameLen > maxName) nameLen = maxName;
    valueCol = 2 + nameLen + 2;
    snprintf(num, sizeof num, "%d/%d", ib->enemyHp, ib->enemy.hp);
    numLen = ui_visible_len(num);
    barW = wid - valueCol - numLen - 4;
    if (barW > 30) barW = 30;
    if (barW < 10) barW = 10;

    ui_panel(row, 0, wid, PANEL_ROWS, "Battle");
    render_text_clipped(row + 1, 2, nameLen, ST_DANGER, "%s", ib->enemy.name);
    ui_hp_bar_n(row + 1, valueCol, barW, ib->enemyHp, ib->enemy.hp);
    render_text(row + 1, valueCol + barW + 2, style_hp(ib->enemyHp, ib->enemy.hp),
                "%s", num);
    for (i = 0; i < 3; i++) {
        int idx = ib->logCount - 3 + i;
        if (idx >= 0)
            render_text_clipped(row + 2 + i, 2, wid - 4, ST_TEXT, "%s", ib->log[idx]);
    }
    render_text_clipped(row + 5, 2, wid - 4, ST_DIM,
                        "[A] Serang   [D] Bertahan   [P] Potion   [R] Kabur");
}

/* Chest gacha panel. The strip decelerates with a squared offset so the last
   frames crawl, then the winning cell keeps blinking under a highlight and the
   result is spelled out below it. */
static void renderSlot(const InlineSlot *sl, int row, int wid)
{
    static const char *names[] = { "Common", "Rare", "Epic", "Legendary" };
    int view = SLOT_VIEW;
    int win, settled = sl->frame >= SLOT_SCROLL_FRAMES;
    int blink = settled && ((sl->frame - SLOT_SCROLL_FRAMES) % 2 == 1);
    int x0, offset, i;

    /* narrow terminals: drop whole cells rather than draw past the border */
    while (view > SLOT_CELL && view > wid - 4) view -= SLOT_CELL;
    win = (view / SLOT_CELL / 2) * SLOT_CELL;
    x0 = 1 + (wid - 2 - view) / 2;
    if (x0 < 1) x0 = 1;
    if (settled) {
        offset = win;
    } else {
        int left = SLOT_SCROLL_FRAMES - sl->frame;
        offset = win + (left * left) / 8;
    }

    ui_panel(row, 0, wid, PANEL_ROWS, "Gacha");
    /* edge fade: empty blocks mark where the strip leaves the box, so the
       reel reads as a window instead of a loose run of words */
    if (x0 > 2) {
        render_text(row + 1, x0 - 2, ST_BORDER, "%s", caps_glyphs()->blockEmpty);
        render_text(row + 1, x0 + view, ST_BORDER, "%s", caps_glyphs()->blockEmpty);
    }
    for (i = 0; i <= view / SLOT_CELL; i++) {
        int x = i * SLOT_CELL - offset % SLOT_CELL;
        int index = i + offset / SLOT_CELL;
        int winCell = settled && x == win;
        int rarity = winCell ? (sl->loot.rarity > 0 ? sl->loot.rarity : 1)
                             : slotRarityAt(index, 0);
        /* the winning cell owns the colour; the neighbours stay grey so the
           eye lands on one name even while the strip is still moving */
        Style st = winCell ? (blink ? ST_SELECTED : ST_TITLE) : ST_DIM;

        if (x < 0 || x + SLOT_LABEL > view) continue;
        render_text(row + 1, x0 + x, st, "%-*s", SLOT_LABEL, names[rarity - 1]);
    }
    /* the caret tracks the winning cell once the reel stops */
    render_text(row + 2, x0 + win + SLOT_LABEL / 2, ST_ACCENT, "%s", settled ? "^" : "");

    if (settled) {
        int rarity = sl->loot.rarity > 0 ? sl->loot.rarity : 1;
        render_text(row + 3, 2, ST_BORDER, "%s", caps_glyphs()->marker);
        if (sl->loot.kind == 2)
            render_text_clipped(row + 3, 4, wid - 6, style_rarity(rarity), "%s (power %d)",
                                sl->loot.item.name, sl->loot.item.power);
        else if (sl->loot.kind == 1)
            render_text_clipped(row + 3, 4, wid - 6, ST_SUCCESS, "Potion +%d", sl->loot.amount);
        else
            render_text_clipped(row + 3, 4, wid - 6, ST_GOLD, "Gold +%d", sl->loot.amount);
        render_text_clipped(row + 4, 2, wid - 4, ST_DIM, "Hadiah masuk otomatis.");
    } else {
        /* the dots advance with the frame counter, so the box keeps moving
           even between scroll ticks */
        static const char *dots[] = { "", ".", "..", "..." };
        render_text_clipped(row + 3, 2, wid - 4, ST_DIM, "Mengocok hadiah%s",
                            dots[(sl->frame / 2) % 4]);
    }
}

/* Status panel: HP bar, Posisi/Langkah/Gold and seed, labels in a fixed
   eight-cell column so the values line up. Narrow boxes split the middle row. */
static void renderStatus(const DungeonState *st, const Player *p, int steps,
                         int row, int wid)
{
    char num[24], pos[32], stepText[48];
    int valueCol = 2 + STATUS_LABEL_W;
    int numLen, barW, x, narrow = wid < 50;

    ui_panel(row, 0, wid, narrow ? STATUS_ROWS_NARROW : STATUS_ROWS, "Status");

    snprintf(num, sizeof num, "%d/%d", p->hp, p->maxHp);
    numLen = ui_visible_len(num);
    barW = wid - valueCol - numLen - 4;
    if (barW > 30) barW = 30;
    if (barW < 10) barW = 10;
    render_text(row + 1, 2, ST_DIM, "HP");
    ui_hp_bar_n(row + 1, valueCol, barW, p->hp, p->maxHp);
    render_text(row + 1, valueCol + barW + 2, style_hp(p->hp, p->maxHp), "%s", num);

    snprintf(pos, sizeof pos, "(%d, %d)", st->posX, st->posY);
    snprintf(stepText, sizeof stepText, "Langkah %d", steps);
    render_text(row + 2, 2, ST_DIM, "Posisi");
    render_text(row + 2, valueCol, ST_TEXT, "%s", pos);
    x = valueCol + ui_visible_len(pos) + 3;
    if (narrow) {
        render_text(row + 3, valueCol, ST_TEXT, "%s", stepText);
        x = valueCol + ui_visible_len(stepText) + 3;
        render_text_clipped(row + 3, x, wid - x - 2, ST_GOLD, "Gold %d", p->gold);
    } else {
        render_text_clipped(row + 2, x, wid - x - 2, ST_TEXT, "%s", stepText);
        x += ui_visible_len(stepText) + 3;
        render_text_clipped(row + 2, x, wid - x - 2, ST_GOLD, "Gold %d", p->gold);
    }

    render_text(row + (narrow ? 4 : 3), 2, ST_DIM, "Seed");
    render_text(row + (narrow ? 4 : 3), valueCol, ST_DIM, "%u", st->seed);
}

static void renderDungeon(char area[][AREA_MAX], DungeonState *st, char trail[][AREA_MAX],
                          Camera *cam, const InlineBattle *ib, const InlineSlot *sl,
                          const char *message, int steps, const Player *p)
{
    int vw, vh, wid, statusRow;

    hitung_viewport(ib->active || sl->active, &vw, &vh);
    wid = caps_interactive() ? vw + 2 : UI_COLS;

    if (!caps_interactive()) {
        int ax = st->posX - st->originX;
        int ay = st->posY - st->originY;
        int tx, ty;
        clear_screen();
        for (ty = 0; ty < vh; ty++) {
            for (tx = 0; tx < vw; tx++) {
                int dr = ty - vh / 2, dc = tx - vw / 2;
                if (ty == vh / 2 && tx == vw / 2) printf("P");
                else drawCell(st->seed, area, trail, ay + dr, ax + dc,
                              st->posX + dc, st->posY + dr,
                              fogTier(dr, dc), 0, 0, 0);
            }
            printf("\n");
        }
        printDungeonStatus(st, p, steps);
        printf("%s\n", message);
        printf("Gerak (W/A/S/D, H hint %d gold, Q keluar): ", HINT_COST);
        fflush(stdout);
        return;
    }

    /* the viewport changed (battle started/ended, resize): re-fit the camera
       and the cached area before anything is drawn */
    update_kamera(cam, st->posX, st->posY, vw, vh, CAMERA_MARGIN);
    syncArea(st, area, cam->x, cam->y, vw, vh);

    ui_frame("Dungeon");
    renderPeta(area, st, cam, trail, vw, vh, wid);
    statusRow = 2 + vh + 2 + ((ib->active || sl->active) ? PANEL_ROWS : 0);
    if (ib->active) renderBattle(ib, 2 + vh + 2, wid);
    else if (sl->active) renderSlot(sl, 2 + vh + 2, wid);
    renderStatus(st, p, steps, statusRow, wid);
    render_text_clipped(statusRow + statusRows(wid), 2, wid - 4, ST_DIM, "%s", message);
    if (statusRow + statusRows(wid) + 2 < caps_rows()) {
        char hint[96];
        snprintf(hint, sizeof hint,
                 "Gerak W/A/S/D/panah   H hint %d gold   Q keluar   $ treasure",
                 HINT_COST);
        ui_footer_line(hint);
    }
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
    Camera cam;
    InlineBattle ib;
    InlineSlot slot;
    int vw, vh;
    char message[128];
    char buf[256];

    memset(&ib, 0, sizeof ib);
    memset(&slot, 0, sizeof slot);
    snprintf(message, sizeof message, "%s", initialMsg);
    /* start with the player in the middle of the viewport */
    hitung_viewport(0, &vw, &vh);
    cam.x = st->posX - vw / 2;
    cam.y = st->posY - vh / 2;
    while (!finished) {
        char cmd;
        if (drewBattle) drewBattle = 0;   /* keep the battle frame on screen */
        else renderDungeon(area, st, trail, &cam, &ib, &slot, message, steps, p);
        if (platform_shutdown_requested()) break;
        if (slot.active) {
            /* the gacha strip advances one frame per loop pass; no key is read
               until it lands, so movement cannot interleave with the roll */
            slot.frame++;
            if (slot.frame >= SLOT_FRAMES) {
                applyLoot(p, &slot.loot, message);
                slot.active = 0;
                autosave_publish(p, AS_SCREEN_DUNGEON, NULL, 0, st, steps, hintsUsed, 0);
                continue;
            }
            platform_sleep_ms(slot.frame <= SLOT_SCROLL_FRAMES ? 46 + slot.frame * 3 : 130);
            continue;
        }
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
        if (ib.active) {
            /* movement is disabled until the encounter ends: only battle
               actions are accepted here */
            switch (cmd) {
            case 'A': battleSerang(&ib, p, message); break;
            case 'D': battleDefend(&ib, p, message); break;
            case 'P': battlePotion(&ib, p, message); break;
            case 'R':
            case 'Q': battleFlee(&ib, p, message); break;
            default:
                strcpy(message, "Battle: A serang, D bertahan, P potion, R kabur.");
                continue;
            }
            if (p->hp <= 0) {
                p->hp = 0;
                ib.active = 0;
                if (battleGameOverScreen(p) == 1) {
                    resetPlayer(p);
                    strcpy(message, "Run direset. Level, gold, dan item kembali ke awal.");
                    hitung_viewport(0, &vw, &vh);
                    cam.x = st->posX - vw / 2;
                    cam.y = st->posY - vh / 2;
                } else {
                    autosave_publish(p, AS_SCREEN_MENU, NULL, 0, NULL, 0, 0, 1);
                    strcpy(message, "Run berakhir. Kembali ke menu utama.");
                    finished = 1;
                }
            } else if (!ib.active) {
                autosave_publish(p, AS_SCREEN_DUNGEON, NULL, 0, st, steps, hintsUsed, 1);
            }
            continue;
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
                    memset(trail, ' ', (size_t)AREA_MAX * AREA_MAX);
                    if ((cell == 'T' || cell == 'G' || cell == 'C' || cell == 'M')
                        && !alreadyTaken(st, wx, wy)) {
                        consumeCell(st, wx, wy, area);
                        treasure = (cell == 'T' || cell == 'G' || cell == 'C');
                        if (cell == 'C' && caps_interactive()) {
                            /* the Gacha panel takes the same slot as the Battle panel;
                               loot is applied when the strip lands */
                            rollLoot(p->seed, wx, wy, catalog, catalogCount, &slot.loot);
                            slot.frame = 0;
                            slot.active = 1;
                            strcpy(message, "Chest terbuka, mengocok hadiah...");
                        } else if (cell == 'M' && caps_interactive()
                            && startInlineBattle(&ib, p, monsters, monsterCount, wx, wy)) {
                            /* the Battle panel takes over the action keys until
                               the encounter is resolved */
                            sprintf(message, "Monster %s muncul! A serang, D bertahan, P potion, R kabur.",
                                    ib.enemy.name);
                        } else {
                            handleContent(p, monsters, monsterCount, catalog, catalogCount,
                                          cell, wx, wy, area, message);
                            /* the full battle screen already drew its own frame, so
                               the loop must not paint over it until the next key */
                            if (cell == 'M' && battleRunEnded()) {
                                /* the run is over: leave the dungeon instead of
                                   walking on after a game-over screen */
                                battleClearRunEnded();
                                autosave_publish(p, battleRunAborted() ? AS_SCREEN_MENU : AS_SCREEN_HUB,
                                                 NULL, 0, NULL, 0, 0, 1);
                                strcpy(message, battleRunAborted()
                                                   ? "Run berakhir. Kembali ke menu utama."
                                                   : "Kamu kalah. Run direset dari awal.");
                                finished = 1;
                                break;
                            }
                            if (cell == 'M') drewBattle = 1;
                        }
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
                if (!confirmPrompt("Keluar dungeon? Progress langkah hilang. (y/n): ")) {
                    strcpy(message, "Keluar dungeon dibatalkan.");
                    break;
                }
            }
            if (!caps_interactive()) {
                printf("\033[?25h");
                clear_screen();
            }
            /* the summary rides on the hub message row, so quitting needs no
               extra keypress between the map and the menu */
            set_message("Keluar dungeon. Langkah total: %d.", steps);
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
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

    /* the fullscreen TUI drops straight into the map: the seed and the goal
       are shown in the Status panel and the first message row, so a separate
       "Tekan Enter untuk masuk" gate would only cost one key. Piped runs keep
       the intro because their transcripts are matched line by line. */
    if (!caps_interactive()) {
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
