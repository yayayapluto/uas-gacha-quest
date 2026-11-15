#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "util.h"
#include "player.h"
#include "battle.h"
#include "gacha.h"
#include "inventory.h"
#include "shop.h"
#include "storage.h"
#include "dungeon.h"
#include "ui.h"
#include "render.h"
#include "caps.h"

/* Shows the main menu and returns the player choice. */
static void mainMenuDraw(void *ctx, int sel)
{
    static const char *items[] = { "Game Baru", "Lanjutkan", "Leaderboard", "Keluar" };
    static const int nums[] = { 1, 2, 3, 0 };
    const Glyphs *g = caps_glyphs();
    int i;
    (void)ctx;

    ui_frame("Menu Utama");
    render_text(2, 0, ST_TITLE, "G A C H A   Q U E S T");
    for (i = 0; i < 22; i++)
        render_text(3, i, ST_BORDER, "%s", g->hz);
    ui_menu_v(5, 2, items, nums, 4, sel);
    ui_message(20);
}

int mainMenu(void)
{
    static const char *items[] = { "Game Baru", "Lanjutkan", "Leaderboard", "Keluar" };
    static const int nums[] = { 1, 2, 3, 0 };
    int choice = ui_menu_pick(items, nums, 4, 0, 3, mainMenuDraw, NULL);
    set_message("");
    return choice;
}

static void hubMenuDraw(void *ctx, int sel)
{
    static const char *items[] = { "Battle", "Gacha", "Inventory", "Shop", "Dungeon",
                                   "Status", "Simpan", "Kembali ke menu utama" };
    static const int nums[] = { 1, 2, 3, 4, 5, 6, 7, 0 };
    const Player *p = ctx;

    ui_frame("Hub");
    ui_status_panel_at(p, 2, 0, UI_COLS);
    ui_menu_v(9, 2, items, nums, 8, sel);
    ui_message(20);
}

/* Shows the game hub and returns the player choice. */
int hubMenu(const Player *p)
{
    static const char *items[] = { "Battle", "Gacha", "Inventory", "Shop", "Dungeon",
                                   "Status", "Simpan", "Kembali ke menu utama" };
    static const int nums[] = { 1, 2, 3, 4, 5, 6, 7, 0 };
    return ui_menu_pick(items, nums, 8, 0, 7, hubMenuDraw, (void *)p);
}

/* Hub loop: every feature is reachable from here. */
void runHub(Player *p, const Item catalog[], int catalogCount, const Monster monsters[], int monsterCount)
{
    int saved = 0;

    for (;;) {
        int choice = hubMenu(p);
        switch (choice) {
        case 1: startBattle(p, monsters, monsterCount); break;
        case 2: gachaMenu(p, catalog, catalogCount); break;
        case 3: inventoryMenu(p); break;
        case 4: shopMenu(p); break;
        case 5: runDungeon(p); break;
        case 6: showStatus(p); break;
        case 7: saved = saveGame(p); break;
        case 0: {
            char answer[MAX_NAME];
            if (!saved) {
                readString("Simpan sebelum keluar? (y/n): ", answer, MAX_NAME);
                if (answer[0] == 'y' || answer[0] == 'Y') saveGame(p);
            }
            recordScore(p);
            if (last_message()[0] != '\0')
                set_message("%s Kembali ke menu utama.", last_message());
            else
                set_message("Kembali ke menu utama.");
            ui_wait_enter("Tekan Enter untuk lanjut...");
            return;
        }
        }
    }
}

/* Entry point: load catalog and monsters, then run the main menu loop. */
int main(void)
{
    Item catalog[MAX_ITEM];
    Monster monsters[MAX_MONSTER];
    Player player;
    int catalogCount, monsterCount;
    int running = 1;

    srand((unsigned)time(NULL));
    ui_init();
    catalogCount = loadCatalog("data/item.txt", catalog);
    monsterCount = loadMonsters("data/monster.txt", monsters);
    if (catalogCount == 0 || monsterCount == 0) {
        printf("Katalog item atau monster tidak bisa dimuat, program berhenti.\n");
        return 1;
    }
    memset(&player, 0, sizeof player);

    while (running) {
        int choice = mainMenu();
        switch (choice) {
        case 1: {
            char name[MAX_NAME];
            readString("Masukkan nama pemain: ", name, MAX_NAME);
            initPlayer(&player, name);
            set_message("Selamat datang, %s! Kamu dapat 200 gold.", player.name);
            player.seed = promptWorldSeed();
            runHub(&player, catalog, catalogCount, monsters, monsterCount);
            break;
        }
        case 2:
            if (loadGame(&player, catalog, catalogCount)) {
                runHub(&player, catalog, catalogCount, monsters, monsterCount);
            } else {
                ui_wait_enter("Tekan Enter untuk lanjut...");
            }
            break;
        case 3:
            showLeaderboard();
            ui_wait_enter("Tekan Enter untuk lanjut...");
            break;
        case 0:
            shutdownGame();
            running = 0;
            break;
        }
    }
    return 0;
}
