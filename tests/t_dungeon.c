#include <stdio.h>
#include <string.h>
#include "config.h"
#include "dungeon.h"

#define CHECK(cond) do { if (!(cond)) { printf("FAIL: %s line %d\n", #cond, __LINE__); return 1; } } while (0)

static int sameChunk(char area[][AREA_MAX], int ci, int cj, int di, int dj)
{
    int x, y;
    for (y = 0; y < CHUNK_SIZE; y++)
        for (x = 0; x < CHUNK_SIZE; x++)
            if (area[cj * CHUNK_SIZE + y][ci * CHUNK_SIZE + x] !=
                area[dj * CHUNK_SIZE + y][di * CHUNK_SIZE + x])
                return 0;
    return 1;
}

/* Terrain is generated per chunk, so a world that fell back to one repeating
   shape would show up here as a single distinct chunk. */
static int distinctChunks(char area[][AREA_MAX])
{
    int n = AREA_MAX / CHUNK_SIZE, idx, count = 0;
    for (idx = 0; idx < n * n; idx++) {
        int ci = idx % n, cj = idx / n, p, dup = 0;
        for (p = 0; p < idx; p++)
            if (sameChunk(area, ci, cj, p % n, p / n)) {
                dup = 1;
                break;
            }
        if (!dup) count++;
    }
    return count;
}

int main(void)
{
    char area[AREA_MAX][AREA_MAX];
    char again[AREA_MAX][AREA_MAX];
    Point path[MAX_PATH_CELLS];
    unsigned int seed = 20240601u;
    unsigned int other = 20240602u;
    int r, c, len, treasures = 0, differences = 0, walkable = 0, reachable = 0;
    int wallRow = -1, wallCol = -1;

    /* The generator is pure: repeated calls with the same arguments agree. */
    CHECK(dungeonCell(seed, 5, -3) == dungeonCell(seed, 5, -3));
    CHECK(cellHash(seed, 7, 2) == cellHash(seed, 7, 2));

    /* The world start is open, whatever terrain the first chunk rolled. */
    CHECK(dungeonCell(seed, 1, 1) == '.');
    CHECK(dungeonCell(other, 1, 1) == '.');

    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++)
            area[r][c] = dungeonCell(seed, c, r);
    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++)
            again[r][c] = dungeonCell(seed, c, r);

    /* Same seed reproduces the world cell by cell. */
    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++)
            CHECK(area[r][c] == again[r][c]);

    /* A different seed produces a different world. */
    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++)
            if (area[r][c] != dungeonCell(other, c, r)) differences++;
    CHECK(differences > 0);

    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++) {
            if (area[r][c] == 'T') treasures++;
            if (area[r][c] == '#' && wallRow < 0) {
                wallRow = r;
                wallCol = c;
            }
            if (area[r][c] != '#') walkable++;
        }
    CHECK(treasures > 0);
    CHECK(wallRow >= 0);

    /* The seed yields several chunk shapes: maze, rooms, cave, pillars. */
    CHECK(distinctChunks(area) >= 3);

    /* Every open cell in the cached area is reachable from the start cell, so
       the gate carving really links neighbouring chunks. */
    for (r = 0; r < AREA_MAX; r++)
        for (c = 0; c < AREA_MAX; c++)
            if (area[r][c] != '#' && dfsReachable(area, (Point){1, 1}, (Point){r, c}))
                reachable++;
    CHECK(reachable == walkable);

    /* BFS: the path starts and ends exactly on the endpoints and never crosses a wall. */
    len = bfsShortestPath(area, (Point){1, 1}, (Point){AREA_MAX - 3, AREA_MAX - 3}, path);
    CHECK(len > 0);
    CHECK(path[0].row == 1 && path[0].col == 1);
    CHECK(path[len - 1].row == AREA_MAX - 3 && path[len - 1].col == AREA_MAX - 3);
    CHECK(area[path[len / 2].row][path[len / 2].col] != '#');

    /* Walls are unreachable. */
    CHECK(dfsReachable(area, (Point){1, 1}, (Point){wallRow, wallCol}) == 0);
    CHECK(bfsShortestPath(area, (Point){1, 1}, (Point){wallRow, wallCol}, path) == -1);

    printf("All dungeon tests passed.\n");
    return 0;
}
