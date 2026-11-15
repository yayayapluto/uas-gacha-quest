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

typedef struct {
    const Player *p;
    const Monster *enemy;
    int enemyHp;
} BattleCtx;

int calculateDamage(int atk, int def)
{
    int base = atk - def;
    if (base < 1) base = 1;
    return base + rand() % 4;
}

int attack(const char *from, const char *to, int atk, int targetDef, int *targetHp, int *critical)
{
    int dmg = calculateDamage(atk, targetDef);
    *critical = 0;
    if (rand() % 100 < 10) {
        dmg *= 2;
        *critical = 1;
    }
    *targetHp -= dmg;
    if (*critical)
        ui_log_style(ST_DANGER, "%s menyerang %s: %d damage (KRITIS!)", from, to, dmg);
    else
        ui_log("%s menyerang %s: %d damage", from, to, dmg);
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
    return candidates[rand() % candidateCount];
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

    ui_panel(8, 0, 50, 5, bc->enemy->name);
    snprintf(buf, sizeof buf, "HP  %d/%d", bc->enemyHp, bc->enemy->hp);
    render_text(9, 2, ST_TEXT, "%s", buf);
    ui_hp_bar(10, 2, 44, bc->enemyHp, bc->enemy->hp);
    snprintf(buf, sizeof buf, "ATK %d | DEF %d", bc->enemy->atk, bc->enemy->def);
    render_text(11, 2, ST_TEXT, "%s", buf);

    logRow = 14;
    start = ui_log_count() > 4 ? ui_log_count() - 4 : 0;
    for (i = start; i < ui_log_count(); i++)
        render_text(logRow++, 0, ui_log_style_at(i), "%s", ui_log_text(i));

    ui_menu_v(18, 0, items, nums, 4, sel);
    ui_footer(NULL);
}

void startBattle(Player *p, const Monster m[], int count)
{
    int monsterIndex = pickMonster(m, count, p->level);
    Monster enemy;
    int enemyHp;
    int critical;
    int alive = 1;
    BattleCtx bc;

    if (monsterIndex < 0) {
        set_message("Tidak ada monster yang tersedia.");
        return;
    }
    enemy = m[monsterIndex];
    enemyHp = enemy.hp;
    bc.p = p;
    bc.enemy = &enemy;
    bc.enemyHp = enemyHp;

    ui_log_clear();

    while (alive) {
        int action;
        static const char *items[] = { "Serang", "Bertahan", "Potion", "Kabur" };
        static const int nums[] = { 1, 2, 3, 4 };

        bc.enemyHp = enemyHp;
        action = ui_menu_pick(items, nums, 4, 1, 4, battleDraw, &bc);
        if (feof(stdin)) {
            set_message("Battle dihentikan (input berakhir).");
            break;
        }

        switch (action) {
        case 1:
            attack(p->name, enemy.name, totalAtk(p), enemy.def, &enemyHp, &critical);
            if (enemyHp <= 0) {
                ui_log_style(ST_SUCCESS, "%s kalah! Kamu menang!", enemy.name);
                ui_log("Hadiah: +%d EXP, +%d gold, +%d skor.", enemy.exp, enemy.gold, enemy.exp);
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
            ui_log("Kamu bertahan.");
            dmg = calculateDamage(enemy.atk, totalDef(p));
            if (rand() % 100 < 10) {
                dmg *= 2;
                ui_log_style(ST_DANGER, "%s menyerang: %d damage (KRITIS!)", enemy.name, dmg);
            } else {
                ui_log("%s menyerang: %d damage", enemy.name, dmg);
            }
            dmg /= 2;
            if (dmg < 1) dmg = 1;
            p->hp -= dmg;
            ui_log("Karena bertahan, damage masuk hanya %d.", dmg);
            break;
        }
        case 3:
            if (p->potion <= 0) {
                ui_log("Potion habis! (giliran tidak terpakai)");
                break;
            }
            p->potion--;
            p->hp += 50;
            if (p->hp > p->maxHp) p->hp = p->maxHp;
            ui_log("Potion dipakai, HP sekarang %d/%d.", p->hp, p->maxHp);
            attack(enemy.name, p->name, enemy.atk, totalDef(p), &p->hp, &critical);
            break;
        case 4:
            if (rand() % 2 == 0) {
                ui_log("Kabur berhasil! Battle selesai tanpa hadiah.");
                alive = 0;
            } else {
                ui_log("Kabur gagal!");
                attack(enemy.name, p->name, enemy.atk, totalDef(p), &p->hp, &critical);
            }
            break;
        }

        if (p->hp <= 0) {
            ui_log_style(ST_DANGER, "Kamu dikalahkan oleh %s...", enemy.name);
            p->gold /= 2;
            p->hp = p->maxHp;
            ui_log("Gold tinggal %d, HP dipulihkan penuh. Kembali ke hub.", p->gold);
            alive = 0;
        }
        if (alive) {
            bc.enemyHp = enemyHp;
            battleDraw(&bc, -1);
            ui_wait_enter("Tekan Enter untuk lanjut...");
        }
    }
    ui_wait_enter("Tekan Enter untuk lanjut...");
    set_message("");
}