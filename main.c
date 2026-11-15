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
#include "platform.h"
#include "autosave.h"
#include "rng.h"

/* Slot picker shared by Load Game and Simpan. Rows show the saved player name
   so the player can tell saves apart; a free slot reads "(kosong)". When
   allowCreate is set, a "Buat save baru" row targets the first free slot.
   Returns the chosen slot number, or -1 on cancel. */
typedef struct {
    int allowCreate;
    int freeSlot;
} SlotPickerCtx;

static const char *slotRowLabel(int slot, char *buf, int size)
{
    char name[MAX_NAME];

    if (saveSlotExists(slot) && saveSlotName(slot, name, MAX_NAME))
        snprintf(buf, (size_t)size, "Slot %d  %s", slot, name);
    else
        snprintf(buf, (size_t)size, "Slot %d  (kosong)", slot);
    return buf;
}

static void slotPickerDraw(void *ctx, int sel)
{
    const SlotPickerCtx *c = ctx;
    int i, row = 3;

    ui_frame(c->allowCreate ? "Simpan Game" : "Load Game");
    for (i = 0; i < SAVE_SLOTS; i++) {
        char label[MAX_NAME + 24];
        render_text(row++, 2, i == sel ? ST_SELECTED : ST_TEXT, "%s",
                    slotRowLabel(i + 1, label, (int)sizeof label));
    }
    if (c->allowCreate) {
        char label[48];
        if (c->freeSlot > 0)
            snprintf(label, sizeof label, "Buat save baru (slot %d)", c->freeSlot);
        else
            snprintf(label, sizeof label, "Buat save baru (penuh)");
        render_text(row++, 2, sel == SAVE_SLOTS ? ST_SELECTED : ST_TEXT, "%s", label);
    }
    render_text(row + 1, 2, sel == row - 3 ? ST_SELECTED : ST_TEXT, "Kembali");
    ui_message(15);
    ui_footer(NULL);
}

static int pickSaveSlot(int allowCreate)
{
    SlotPickerCtx ctx;
    int count = SAVE_SLOTS + (allowCreate ? 2 : 1);
    int sel;

    ctx.allowCreate = allowCreate;
    ctx.freeSlot = saveSlotFirstFree();
    sel = ui_pick_row(count, slotPickerDraw, &ctx);
    if (sel < 0 || sel == count - 1) return -1;
    if (sel == SAVE_SLOTS) return ctx.freeSlot > 0 ? ctx.freeSlot : -1;
    return sel + 1;
}

/* Shows the main menu and returns the player choice. */
static void mainMenuDraw(void *ctx, int sel)
{
    static const char *items[] = { "Game Baru", "Load Game", "Leaderboard", "Keluar" };
    static const int nums[] = { 1, 2, 3, 0 };
    (void)ctx;

    ui_frame("Menu Utama");
    ui_menu_v(2, 0, items, nums, 4, sel);
    ui_message(7);
}

int mainMenu(void)
{
    static const char *items[] = { "Game Baru", "Load Game", "Leaderboard", "Keluar" };
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
    ui_menu_v(9, 0, items, nums, 8, sel);
    ui_message(18);
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
        int choice;
        char err[160];
        if (platform_shutdown_requested()) return;
        if (autosave_poll_error(err, (int)sizeof err))
            set_message("Autosave gagal: %s", err);
        choice = hubMenu(p);
        if (choice < 0 || platform_shutdown_requested()) {
            if (!platform_shutdown_requested()) autosave_publish(p, AS_SCREEN_MENU, NULL, 0, NULL, 0, 0, 1);
            return;
        }
        switch (choice) {
        case 1: startBattle(p, monsters, monsterCount); break;
        case 2: gachaMenu(p, catalog, catalogCount); break;
        case 3: inventoryMenu(p); break;
        case 4: shopMenu(p); break;
        case 5: runDungeon(p, catalog, catalogCount, monsters, monsterCount); break;
        case 6: showStatus(p); break;
        case 7: {
            int slot = pickSaveSlot(1);
            if (slot > 0) saved = saveGameSlot(p, slot);
            else saved = 0;
            break;
        }
        case 0: {
            char answer[MAX_NAME];
            if (!saved) {
                ui_input_screen("Hub");
                readString("Simpan sebelum keluar? (y/n): ", answer, MAX_NAME);
                if (answer[0] == 'y' || answer[0] == 'Y') saveGame(p);
            }
            recordScore(p);
            autosave_publish(p, AS_SCREEN_MENU, NULL, 0, NULL, 0, 0, 1);
            if (last_message()[0] != '\0')
                set_message("%s Kembali ke menu utama.", last_message());
            else
                set_message("Kembali ke menu utama.");
            ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
            return;
        }
        }
    }
}

/* Entry point: load catalog and monsters, then run the main menu loop. */
static void resumeSession(const AutoSnapshot *snap, const Item catalog[], int catalogCount,
                          const Monster monsters[], int monsterCount)
{
    Player p = snap->player;
    plat_rng_set(snap->rng);
    switch (snap->screen) {
    case AS_SCREEN_BATTLE:
        if (snap->battleActive) {
            resumeBattle(&p, snap->battleEnemy, snap->battleEnemyHp);
            runHub(&p, catalog, catalogCount, monsters, monsterCount);
            return;
        }
        break;
    case AS_SCREEN_DUNGEON:
        if (snap->dungeonActive) {
            resumeDungeon(&p, &snap->dungeon, snap->dungeonSteps, snap->dungeonHints,
                          catalog, catalogCount, monsters, monsterCount);
            runHub(&p, catalog, catalogCount, monsters, monsterCount);
            return;
        }
        break;
    default:
        break;
    }
    runHub(&p, catalog, catalogCount, monsters, monsterCount);
}

int main(void)
{
    Item catalog[MAX_ITEM];
    Monster monsters[MAX_MONSTER];
    Player player;
    AutoSnapshot snap;
    int catalogCount, monsterCount;
    int running = 1;

    platform_init();
    plat_srand((unsigned)time(NULL));

    catalogCount = loadCatalog("data/item.txt", catalog);
    monsterCount = loadMonsters("data/monster.txt", monsters);
    if (catalogCount == 0 || monsterCount == 0) {
        printf("Katalog item atau monster tidak bisa dimuat, program berhenti.\n");
        return 1;
    }
    memset(&player, 0, sizeof player);

    if (platform_previous_running() && autosave_load(&snap)) {
        char answer[MAX_NAME];
        printf("Pulihkan sesi sebelumnya? (y/n)");
        fflush(stdout);
        if (readLine(answer, sizeof answer) && (answer[0] == 'y' || answer[0] == 'Y')) {
            resumeSession(&snap, catalog, catalogCount, monsters, monsterCount);
            return 0;
        }
    }

    ui_init();
    while (running) {
        int choice = mainMenu();
        switch (choice) {
        case 1: {
            char name[MAX_NAME];
            ui_input_screen("Game Baru");
            readString("Masukkan nama pemain (maks 29 karakter): ", name, MAX_NAME);
            initPlayer(&player, name);
            player.seed = promptWorldSeed();
            set_message("Selamat datang, %s! Kamu dapat 200 gold.", player.name);
            runHub(&player, catalog, catalogCount, monsters, monsterCount);
            break;
        }
        case 2: {
            int slot = pickSaveSlot(0);
            if (slot > 0 && loadGameSlot(&player, catalog, catalogCount, slot)) {
                runHub(&player, catalog, catalogCount, monsters, monsterCount);
            } else {
                if (slot > 0 && last_message()[0] == 0) set_message("Slot %d tidak bisa dimuat.", slot);
                ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
            }
            break;
        }
        case 3:
            showLeaderboard();
            ui_wait_enter("Tekan Enter untuk lanjut (panah baru aktif setelah ini).");
            break;
        case 0:
            shutdownGame();
            running = 0;
            break;
        }
    }
    return 0;
}
