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

#define VIEW_WIDTH 25
#define VIEW_HEIGHT 15

/* Non-negative modulo, so the world pattern keeps repeating on negative coordinates. */
static int floorMod(int x, int m)
{
    int r = x % m;
    if (r < 0) r += m;
    return r;
}

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

/* World cell derived from the seed: corridors on y = 1 (mod 4) and shafts on x = 1 (mod 6),
   with deterministic treasure at roughly one cell in 89. */
char dungeonCell(unsigned int seed, int x, int y)
{
    int shaftX = floorMod(x, 6);
    int corridorY = floorMod(y, 4);
    if (shaftX != 1 && corridorY != 1) return '#';
    if (x == 1 && y == 1) return '.';
    if (cellHash(seed, x, y) % 89u == 0) return 'T';
    return '.';
}

static int alreadyTaken(const DungeonState *st, int x, int y)
{
    int i;
    for (i = 0; i < st->takenCount; i++)
        if (st->takenTreasures[i].row == y && st->takenTreasures[i].col == x) return 1;
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
            if (cell == 'T' && alreadyTaken(st, wx, wy)) cell = '.';
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

/* Draws a window of the cached area with the player always in the middle. */
static void renderDungeon(char area[][AREA_MAX], const DungeonState *st,
                          char trail[][AREA_MAX], const char *message, int steps,
                          const Player *p)
{
    int ax = st->posX - st->originX;
    int ay = st->posY - st->originY;
    int tx, ty;

    if (!caps_interactive()) {
        clear_screen();
        for (ty = 0; ty < VIEW_HEIGHT; ty++) {
            for (tx = 0; tx < VIEW_WIDTH; tx++) {
                int r = ay - VIEW_HEIGHT / 2 + ty;
                int c = ax - VIEW_WIDTH / 2 + tx;
                char cell;
                if (ty == VIEW_HEIGHT / 2 && tx == VIEW_WIDTH / 2) printf("P");
                else if (trail[r][c] == '*') printf("*");
                else {
                    cell = area[r][c];
                    if (cell == 'T') printf("$");
                    else printf("%c", cell);
                }
            }
            printf("\n");
        }
        printf("Posisi dunia (%d,%d) | langkah %d | HP %s%d/%d%s | gold %d | seed %u\n",
               st->posX, st->posY, steps,
               hp_color(p->hp, p->maxHp), p->hp, p->maxHp, ANSI_RESET,
               p->gold, st->seed);
        printf("%s\n", message);
        printf("Gerak (W/A/S/D, H hint %d gold, Q keluar): ", HINT_COST);
        fflush(stdout);
        return;
    }

    ui_frame("Dungeon");
    for (ty = 0; ty < VIEW_HEIGHT; ty++) {
        for (tx = 0; tx < VIEW_WIDTH; tx++) {
            int r = ay - VIEW_HEIGHT / 2 + ty;
            int c = ax - VIEW_WIDTH / 2 + tx;
            char cell;
            int srow = 2 + ty, scol = 1 + tx;
            if (ty == VIEW_HEIGHT / 2 && tx == VIEW_WIDTH / 2)
                render_text(srow, scol, ST_TITLE, "P");
            else if (trail[r][c] == '*')
                render_text(srow, scol, ST_ACCENT, "*");
            else {
                cell = area[r][c];
                if (cell == 'T') render_text(srow, scol, ST_GOLD, "$");
                else if (cell == '#') render_text(srow, scol, ST_DIM, "#");
                else if (cell == '.') render_text(srow, scol, ST_DIM, ".");
                else if (cell == ' ') render_text(srow, scol, ST_DIM, " ");
                else render_text(srow, scol, ST_DIM, "%c", cell);
            }
        }
    }
    render_text(18, 2, ST_TEXT, "Posisi dunia (%d,%d) | langkah %d | HP %d/%d | gold %d | seed %u",
                st->posX, st->posY, steps, p->hp, p->maxHp, p->gold, st->seed);
    render_text(19, 2, ST_DIM, "%s", message);
    render_text(21, 2, ST_DIM, "Gerak (W/A/S/D, H hint %d gold, Q keluar)", HINT_COST);
    ui_footer(NULL);
    ui_flush();
}

/* Seed from the clock, used when the player leaves the seed prompt empty. */
unsigned int randomWorldSeed(void)
{
    return (unsigned int)time(NULL);
}

/* Asks for a world seed. Empty input draws a fresh one, non-numeric input is rejected. */
unsigned int promptWorldSeed(void)
{
    char line[32];

    if (!caps_interactive()) {
        for (;;) {
            printf("Seed dunia (Enter saja untuk acak): ");
            if (!readLine(line, sizeof line)) {
                unsigned int seed = randomWorldSeed();
                printf("\nSeed acak dipilih: %u\n", seed);
                return seed;
            }
            if (line[0] == '\0') {
                unsigned int seed = randomWorldSeed();
                printf("Seed acak dipilih: %u\n", seed);
                return seed;
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
                return (unsigned int)value;
            }
        }
    }

    ui_input_screen("Game Baru");
    for (;;) {
        unsigned int seed;
        ui_line_begin("Seed dunia (Enter saja untuk acak):");
        if (!readLine(line, sizeof line)) {
            seed = randomWorldSeed();
            ui_line_note("Seed acak dipilih: %u", seed);
            ui_line_end();
            return seed;
        }
        if (line[0] == '\0') {
            seed = randomWorldSeed();
            ui_line_note("Seed acak dipilih: %u", seed);
            ui_line_end();
            return seed;
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
            return (unsigned int)value;
        }
    }
}

/* Turn loop shared by a fresh run and a restored session. */
static void dungeonLoop(Player *p, DungeonState *st, char area[][AREA_MAX],
                        char trail[][AREA_MAX], Point path[],
                        int steps, int hintsUsed, const char *initialMsg)
{
    int finished = 0;
    char message[128];
    char buf[256];

    snprintf(message, sizeof message, "%s", initialMsg);
    while (!finished) {
        char cmd;
        renderDungeon(area, st, trail, message, steps, p);
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
                    if (cell == 'T' && !alreadyTaken(st, wx, wy)) {
                        if (st->takenCount < MAX_TAKEN_TREASURES) {
                            st->takenTreasures[st->takenCount].row = wy;
                            st->takenTreasures[st->takenCount].col = wx;
                            st->takenCount++;
                        }
                        area[wy - st->originY][wx - st->originX] = '.';
                        p->gold += TREASURE_GOLD;
                        treasure = 1;
                        sprintf(message, "Treasure! +%d gold. Sudah %d treasure di run ini.",
                                TREASURE_GOLD, st->takenCount);
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
            if (!caps_interactive()) {
                printf("\033[?25h");
                clear_screen();
            }
            set_message("Keluar dungeon. Langkah total: %d.", steps);
            autosave_publish(p, AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
            ui_wait_enter("Tekan Enter untuk lanjut...");
            finished = 1;
            break;
        default:
            strcpy(message, "Gerakan tidak dikenal.");
            break;
        }
    }
    if (!finished && !caps_interactive()) printf("\033[?25h");
}

void runDungeon(Player *p)
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
        strcpy(message, "Area awal tersambung dari titik awal sampai ujung koridor (DFS).");
    else
        strcpy(message, "Area awal tidak tersambung.");

    dungeonLoop(p, &st, area, trail, path, 0, 0, message);
}

/* Continues a dungeon run restored from the autosave at the same position. */
void resumeDungeon(Player *p, const DungeonState *saved, int steps, int hints)
{
    char area[AREA_MAX][AREA_MAX];
    char trail[AREA_MAX][AREA_MAX];
    Point path[MAX_PATH_CELLS];
    DungeonState st = *saved;

    buildArea(&st, area);
    memset(trail, ' ', sizeof trail);
    dungeonLoop(p, &st, area, trail, path, steps, hints, "Sesi dipulihkan dari autosave.");
}
