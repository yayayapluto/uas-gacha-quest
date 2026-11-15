#include <stdio.h>
#include <string.h>
#include "config.h"
#include "storage.h"
#include "util.h"
#include "ui.h"
#include "render.h"

/* Reads the item catalog; returns how many items were loaded. */
int loadCatalog(const char *file, Item items[])
{
    FILE *fp = fopen(file, "r");
    char line[256];
    Item item;
    int count = 0;

    if (fp == NULL) {
        set_message("File katalog %s tidak bisa dibuka.", file);
        return 0;
    }
    while (count < MAX_ITEM && fgets(line, sizeof line, fp) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d|%29[^|]| %c|%d|%d", &item.id, item.name, &item.type, &item.rarity, &item.power) == 5) {
            if (item.rarity < RARITY_COMMON || item.rarity > RARITY_LEGENDARY) continue;
            if (item.type != 'S' && item.type != 'A') continue;
            items[count] = item;
            count++;
        }
    }
    fclose(fp);
    return count;
}

/* Reads the monster table; returns how many monsters were loaded. */
int loadMonsters(const char *file, Monster monsters[])
{
    FILE *fp = fopen(file, "r");
    char line[256];
    Monster monster;
    int count = 0;

    if (fp == NULL) {
        set_message("File monster %s tidak bisa dibuka.", file);
        return 0;
    }
    while (count < MAX_MONSTER && fgets(line, sizeof line, fp) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d|%29[^|]|%d|%d|%d|%d|%d|%d",
                   &monster.id, monster.name, &monster.hp, &monster.atk, &monster.def,
                   &monster.exp, &monster.gold, &monster.minLevel) == 8) {
            monsters[count] = monster;
            count++;
        }
    }
    fclose(fp);
    return count;
}

/* Writes the player progress to data/save.txt (PRD section 9, plus the world seed); 1 on success. */
int saveGame(const Player *p)
{
    FILE *fp = fopen("data/save.txt", "w");
    int weaponId = -1, armorId = -1, i;

    if (fp == NULL) {
        set_message("Gagal membuka data/save.txt untuk menulis.");
        return 0;
    }
    if (p->weaponIndex >= 0) weaponId = p->inventory[p->weaponIndex].id;
    if (p->armorIndex >= 0) armorId = p->inventory[p->armorIndex].id;
    fprintf(fp, "%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%u|%d|%d\n",
            p->name, p->hp, p->maxHp, p->level, p->exp, p->gold, p->atk, p->def,
            p->potion, p->pity, p->totalPull, p->score, p->seed, weaponId, armorId);
    fprintf(fp, "%d\n", p->itemCount);
    for (i = 0; i < p->itemCount; i++)
        fprintf(fp, "%d\n", p->inventory[i].id);
    fclose(fp);
    set_message("Progres tersimpan di data/save.txt.");
    return 1;
}

/* Reads progress from data/save.txt; 0 when the file is missing or damaged. */
int loadGame(Player *p, const Item items[], int count)
{
    FILE *fp = fopen("data/save.txt", "r");
    char line[512];
    Player tmp;
    int weaponId, armorId, itemCount, i, j, id;

    if (fp == NULL) {
        set_message("Gagal memuat: data/save.txt tidak ada.");
        return 0;
    }
    memset(&tmp, 0, sizeof tmp);
    if (fgets(line, sizeof line, fp) == NULL ||
        sscanf(line, "%29[^|]|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%u|%d|%d",
               tmp.name, &tmp.hp, &tmp.maxHp, &tmp.level, &tmp.exp, &tmp.gold,
               &tmp.atk, &tmp.def, &tmp.potion, &tmp.pity, &tmp.totalPull,
               &tmp.score, &tmp.seed, &weaponId, &armorId) != 15) {
        fclose(fp);
        set_message("Gagal memuat: format data/save.txt rusak.");
        return 0;
    }
    if (fgets(line, sizeof line, fp) == NULL || sscanf(line, "%d", &itemCount) != 1 ||
        itemCount < 0 || itemCount > MAX_ITEM) {
        fclose(fp);
        set_message("Gagal memuat: jumlah item tidak valid.");
        return 0;
    }
    for (i = 0; i < itemCount; i++) {
        if (fgets(line, sizeof line, fp) == NULL || sscanf(line, "%d", &id) != 1) {
            fclose(fp);
            set_message("Gagal memuat: daftar item tidak lengkap.");
            return 0;
        }
        for (j = 0; j < count; j++) {
            if (items[j].id == id) break;
        }
        if (j == count) {
            fclose(fp);
            set_message("Gagal memuat: item ID %d tidak ada di katalog.", id);
            return 0;
        }
        tmp.inventory[i] = items[j];
    }
    if (tmp.hp < 0 || tmp.maxHp <= 0 || tmp.hp > tmp.maxHp || tmp.level < 1 || tmp.level > MAX_LEVEL ||
        tmp.exp < 0 || tmp.gold < 0 || tmp.atk < 0 || tmp.def < 0 ||
        tmp.potion < 0 || tmp.potion > MAX_POTION ||
        tmp.pity < 0 || tmp.pity > PITY_THRESHOLD || tmp.totalPull < 0 || tmp.score < 0 ||
        weaponId < -1 || armorId < -1) {
        fclose(fp);
        set_message("Gagal memuat: nilai di luar rentang.");
        return 0;
    }
    tmp.itemCount = itemCount;
    tmp.weaponIndex = -1;
    tmp.armorIndex = -1;
    for (i = 0; i < itemCount; i++) {
        if (weaponId == tmp.inventory[i].id) tmp.weaponIndex = i;
        if (armorId == tmp.inventory[i].id) tmp.armorIndex = i;
    }
    *p = tmp;
    fclose(fp);
    set_message("Selamat datang kembali, %s!", tmp.name);
    return 1;
}

/* Adds the player score to the leaderboard, sorted descending, keeping the top 10. */
void recordScore(const Player *p)
{
    char names[MAX_LEADERBOARD][MAX_NAME];
    int levels[MAX_LEADERBOARD], scores[MAX_LEADERBOARD];
    FILE *fp;
    char line[512];
    int count = 0, i, j;
    char readName[MAX_NAME];
    int readLevel, readScore;

    fp = fopen("data/leaderboard.txt", "r");
    if (fp != NULL) {
        while (count < MAX_LEADERBOARD && fgets(line, sizeof line, fp) != NULL) {
            if (sscanf(line, "%29[^|]|%d|%d", readName, &readLevel, &readScore) == 3) {
                strcpy(names[count], readName);
                levels[count] = readLevel;
                scores[count] = readScore;
                count++;
            }
        }
        fclose(fp);
    }
    if (count < MAX_LEADERBOARD) {
        strcpy(names[count], p->name);
        levels[count] = p->level;
        scores[count] = p->score;
        count++;
    } else {
        count = MAX_LEADERBOARD - 1;
        strcpy(names[count], p->name);
        levels[count] = p->level;
        scores[count] = p->score;
        count++;
    }
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (scores[j] < scores[j + 1]) {
                int tmpLevel = levels[j], tmpScore = scores[j];
                char tmpName[MAX_NAME];
                strcpy(tmpName, names[j]);
                levels[j] = levels[j + 1]; scores[j] = scores[j + 1]; strcpy(names[j], names[j + 1]);
                levels[j + 1] = tmpLevel; scores[j + 1] = tmpScore; strcpy(names[j + 1], tmpName);
            }
        }
    }
    fp = fopen("data/leaderboard.txt", "w");
    if (fp == NULL) {
        set_message("Gagal membuka data/leaderboard.txt untuk menulis.");
        return;
    }
    for (i = 0; i < count && i < MAX_LEADERBOARD; i++)
        fprintf(fp, "%s|%d|%d\n", names[i], levels[i], scores[i]);
    fclose(fp);
    set_message("Skor %s (%d) dicatat ke leaderboard.", p->name, p->score);
}

/* Shows the top 5 of the leaderboard as a ranked table. */
void showLeaderboard(void)
{
    FILE *fp = fopen("data/leaderboard.txt", "r");
    char line[512];
    char name[MAX_NAME];
    int level, score, rank = 0, row;
    char buf[96];

    ui_frame("Leaderboard");
    ui_panel(4, 0, 50, 9, "Leaderboard (Top 5)");

    if (fp == NULL) {
        render_text(8, 2, ST_DANGER, "Belum ada data leaderboard.");
    } else {
        while (rank < TOP_DISPLAY && fgets(line, sizeof line, fp) != NULL) {
            Style st;
            if (sscanf(line, "%29[^|]|%d|%d", name, &level, &score) != 3) continue;
            if (rank == 0) st = ST_GOLD;
            else if (rank == 1) st = ST_RARITY_COMMON;
            else if (rank == 2) st = ST_BRONZE;
            else st = ST_TEXT;
            row = 5 + rank;
            snprintf(buf, sizeof buf, "%d. %-15s level %2d skor %d", rank + 1, name, level, score);
            render_text(row, 2, st, "%s", buf);
            rank++;
        }
        fclose(fp);
        if (rank == 0)
            render_text(8, 2, ST_DANGER, "Belum ada data leaderboard.");
    }

    ui_footer(NULL);
    ui_flush();
}
