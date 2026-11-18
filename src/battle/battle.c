#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "battle.h"
#include "util.h"
#include "player.h"
#include "ui.h"
#include "render.h"
#include "caps.h"
#include "term.h"
#include "rng.h"
#include "platform.h"
#include "autosave.h"
#include "player.h"

typedef struct {
    const Player *p;
    const Monster *enemy;
    int enemyHp;
} BattleCtx;

/* Set when the player picks "menu utama" on the game-over screen; the hub loop
   reads it and hands control back to the main menu instead of the next hub frame. */
static int runAborted;
static int runEnded;

int battleRunAborted(void)
{
    return runAborted;
}

void battleClearRunAbort(void)
{
    runAborted = 0;
}

int battleRunEnded(void)
{
    return runEnded;
}

void battleClearRunEnded(void)
{
    runEnded = 0;
}

/* Game-over screen: the run is over, so the player chooses between a fresh
   reset and leaving for the main menu. HP is never refilled behind the menu. */
static void gameOverDraw(void *ctx, int sel)
{
    static const char *items[] = { "Mulai ulang (reset run)", "Kembali ke menu utama" };
    static const int nums[] = { 1, 2 };
    const Player *p = ctx;

    ui_frame("GAME OVER");
    ui_panel(2, 0, UI_COLS, 7, "GAME OVER");
    render_text(3, 2, ST_DANGER, "Kamu kalah di pertempuran.");
    render_text(4, 2, ST_TEXT, "Pemain : %s (level %d, skor %d)", p->name, p->level, p->score);
    render_text(5, 2, ST_TEXT, "Gold   : %d   Potion: %d   Item: %d",
                p->gold, p->potion, p->itemCount);
    render_text(6, 2, ST_DIM, "Reset mengembalikan level, gold, dan item ke awal run.");
    ui_menu_v(10, 0, items, nums, 2, sel);
    ui_message(14);
}

static int gameOverScreen(Player *p)
{
    static const char *items[] = { "Mulai ulang (reset run)", "Kembali ke menu utama" };
    static const int nums[] = { 1, 2 };
    return ui_menu_pick(items, nums, 2, 1, 2, gameOverDraw, p);
}

int battleGameOverScreen(Player *p)
{
    return gameOverScreen(p);
}

int calculateDamage(int atk, int def)
{
    int base = atk - def;
    if (base < 1) base = 1;
    return base + plat_rand() % 4;
}

/* Single source of the crit rule: callers that need raw damage reuse this. */
static int rollCrit(int dmg, int *critical)
{
    *critical = 0;
    if (plat_rand() % 100 < CRIT_CHANCE) {
        *critical = 1;
        dmg *= CRIT_MULTIPLIER;
    }
    return dmg;
}

int attack(const char *from, const char *to, int atk, int targetDef, int *targetHp, int *critical)
{
    int dmg = rollCrit(calculateDamage(atk, targetDef), critical);
    *targetHp -= dmg;
    {
        char dmgText[40];
        snprintf(dmgText, sizeof dmgText, *critical ? "%d damage KRITIS!" : "%d damage", dmg);
        ui_log_event(from, "menyerang", to, dmgText, ST_ACCENT, ST_DANGER,
                     *critical ? ST_DANGER : ST_GOLD);
    }
    return dmg;
}

int pickMonster(const Monster m[], int count, int playerLevel)
{
    int candidates[MAX_MONSTER];
    int candidateCount = 0, i;

    for (i = 0; i < count; i++)
        if (m[i].minLevel <= playerLevel) {
            candidates[candidateCount] = i;
            candidateCount++;
        }
    if (candidateCount == 0) return -1;
    return candidates[plat_rand() % candidateCount];
}

static void battleDraw(void *ctx, int sel)
{
    static const char *items[] = { "Serang", "Bertahan", "Potion", "Kabur" };
    static const int nums[] = { 1, 2, 3, 4 };
    BattleCtx *bc = ctx;
    char buf[64];
    int i, start, logRow;

    ui_frame("Battle");
    ui_status_panel_at(bc->p, 2, 0, UI_COLS);

    ui_panel(8, 0, UI_COLS, 5, bc->enemy->name);
    snprintf(buf, sizeof buf, "HP  %d/%d", bc->enemyHp, bc->enemy->hp);
    render_text(9, 2, ST_TEXT, "%s", buf);
    ui_hp_bar(10, 2, 44, bc->enemyHp, bc->enemy->hp);
    snprintf(buf, sizeof buf, "ATK %d | DEF %d", bc->enemy->atk, bc->enemy->def);
    render_text(11, 2, ST_TEXT, "%s", buf);

    ui_panel(14, 0, UI_COLS, 6, "Log");
    start = ui_log_count() > 4 ? ui_log_count() - 4 : 0;
    logRow = 15;
    for (i = start; i < ui_log_count(); i++, logRow++)
        ui_log_draw(logRow, 2, UI_COLS - 4, i);

    ui_menu_v(21, 0, items, nums, 4, sel);
    ui_footer(NULL);
}

/* Runs the turn loop until the player wins, loses, flees or input ends. */
static void battleLoop(Player *p, Monster enemy, int enemyHp)
{
    int critical;
    int alive = 1;
    BattleCtx bc;

    bc.p = p;
    bc.enemy = &enemy;
    bc.enemyHp = enemyHp;

    while (alive) {
        int action;
        static const char *items[] = { "Serang", "Bertahan", "Potion", "Kabur" };
        static const int nums[] = { 1, 2, 3, 4 };

        bc.enemyHp = enemyHp;
        action = ui_menu_pick(items, nums, 4, 1, 4, battleDraw, &bc);
        if (action < 0 || feof(stdin) || platform_shutdown_requested()) {
            set_message("Battle dihentikan (input berakhir).");
            break;
        }

        switch (action) {
        case 1:
            attack(p->name, enemy.name, totalAtk(p), enemy.def, &enemyHp, &critical);
            if (enemyHp <= 0) {
                ui_log_event(enemy.name, "kalah!", NULL, NULL, ST_DANGER, ST_TEXT, ST_TEXT);
                ui_log_style(ST_SUCCESS, "%s menang!", p->name);
                ui_log_style(ST_TEXT, "Hadiah: +%d EXP, +%d gold, +%d skor.",
                             enemy.exp, enemy.gold, enemy.exp);
                /* the hub has no log panel, so the reward must also land in the
                   message row or the win goes by unseen */
                set_message("Menang! +%d EXP, +%d gold, +%d skor.",
                            enemy.exp, enemy.gold, enemy.exp);
                addExp(p, enemy.exp);
                p->gold += enemy.gold;
                p->score += enemy.exp;
                alive = 0;
                break;
            }
            attack(enemy.name, p->name, enemy.atk, totalDef(p), &p->hp, &critical);
            break;
        case 2: {
            int dmg;
            ui_log_event(p->name, "bertahan", NULL, NULL, ST_ACCENT, ST_TEXT, ST_TEXT);
            dmg = rollCrit(calculateDamage(enemy.atk, totalDef(p)), &critical);
            {
                char dmgText[40];
                snprintf(dmgText, sizeof dmgText, "%d damage", dmg);
                ui_log_event(enemy.name, "menyerang", p->name, dmgText,
                             ST_DANGER, ST_ACCENT, ST_GOLD);
            }
            dmg /= 2;
            if (dmg < 1) dmg = 1;
            p->hp -= dmg;
            ui_log_style(ST_DIM, "Karena bertahan, damage masuk hanya %d.", dmg);
            break;
        }
        case 3:
            if (p->potion <= 0) {
                ui_log_style(ST_DANGER, "Potion habis! (giliran tidak terpakai)");
                break;
            }
            p->potion--;
            p->hp += POTION_HEAL;
            if (p->hp > p->maxHp) p->hp = p->maxHp;
            {
                char hpText[40];
                snprintf(hpText, sizeof hpText, "HP %d/%d", p->hp, p->maxHp);
                ui_log_event(p->name, "memakai potion,", NULL, hpText,
                             ST_ACCENT, ST_TEXT, ST_SUCCESS);
            }
            attack(enemy.name, p->name, enemy.atk, totalDef(p), &p->hp, &critical);
            break;
        case 4:
            if (plat_rand() % 2 == 0) {
                ui_log_style(ST_SUCCESS, "Kabur berhasil! Battle selesai tanpa hadiah.");
                set_message("Kabur berhasil. Battle selesai tanpa hadiah.");
                alive = 0;
            } else {
                ui_log_style(ST_DANGER, "Kabur gagal!");
                attack(enemy.name, p->name, enemy.atk, totalDef(p), &p->hp, &critical);
            }
            break;
        }

        if (p->hp <= 0) {
            p->hp = 0;
            ui_log_event("Kamu dikalahkan oleh", enemy.name, NULL, "...",
                         ST_TEXT, ST_DANGER, ST_TEXT);
            bc.enemyHp = enemyHp;
            battleDraw(&bc, -1);
            ui_flush();
            runEnded = 1;
            if (gameOverScreen(p) == 1) {
                resetPlayer(p);
                set_message("Run direset. Level, gold, dan item kembali ke awal.");
            } else {
                runAborted = 1;
            }
            alive = 0;
        }
        if (alive) {
            bc.enemyHp = enemyHp;
            autosave_publish(p, AS_SCREEN_BATTLE, &enemy, enemyHp, NULL, 0, 0, 0);
        }
    }
    if (!platform_shutdown_requested())
        autosave_publish(p, runAborted ? AS_SCREEN_MENU : AS_SCREEN_HUB, NULL, 0, NULL, 0, 0, 1);
    if (last_message()[0] == '\0' && !runAborted)
        set_message("Battle selesai.");
}

void startBattle(Player *p, const Monster m[], int count)
{
    int monsterIndex = pickMonster(m, count, p->level);
    Monster enemy;

    if (monsterIndex < 0) {
        set_message("Tidak ada monster yang tersedia.");
        return;
    }
    enemy = m[monsterIndex];
    ui_log_clear();
    battleLoop(p, enemy, enemy.hp);
}

void startBattleAgainst(Player *p, const Monster *enemy)
{
    ui_log_clear();
    battleLoop(p, *enemy, enemy->hp);
}

/* Continues a battle restored from the autosave with the same enemy and HP. */
void resumeBattle(Player *p, Monster enemy, int enemyHp)
{
    ui_log_clear();
    battleLoop(p, enemy, enemyHp);
}