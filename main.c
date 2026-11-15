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

/* Shows the main menu and returns the player choice. */
int mainMenu(void)
{
    int choice;

    clear_screen();
    printf("%s", ANSI_CYAN);
    printf("           GACHA QUEST\n");
    printf("%s", ANSI_RESET);
    draw_menu(1, "Game Baru");
    draw_menu(2, "Lanjutkan");
    draw_menu(3, "Leaderboard");
    draw_menu(0, "Keluar");
    if (last_message()[0] != '\0')
        printf("%s%s%s\n", ANSI_CYAN, last_message(), ANSI_RESET);
    printf("Pilih: ");
    choice = readInt("", 0, 3);
    set_message("");
    return choice;
}

/* Shows the game hub and returns the player choice. */
int hubMenu(const Player *p)
{
    int choice;

    clear_screen();
    draw_header("HUB", p);
    draw_menu(1, "Battle");
    draw_menu(2, "Gacha");
    draw_menu(3, "Inventory");
    draw_menu(4, "Shop");
    draw_menu(5, "Dungeon");
    draw_menu(6, "Status");
    draw_menu(7, "Simpan");
    draw_menu(0, "Kembali ke menu utama");
    if (last_message()[0] != '\0')
        printf("%s%s%s\n", ANSI_CYAN, last_message(), ANSI_RESET);
    printf("Pilih: ");
    choice = readInt("", 0, 7);
    return choice;
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
