/* Probe for the platform layer: RNG round-trip, autosave snapshot round-trip,
   truncated file rejection, .bak fallback, and lock file content. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "rng.h"
#include "atomicfile.h"
#include "lock.h"
#include "autosave.h"
#include "player.h"
#include "ui.h"

static int total = 0, failed = 0;
static void check(int ok, const char *what)
{
    total++;
    if (!ok) { failed++; printf("GAGAL  %s\n", what); }
    else printf("LULUS  %s\n", what);
}

int main(void)
{
    AutoSnapshot in, out;
    unsigned long long saved;
    unsigned a, b;
    FILE *fp;

    /* RNG: state round-trip resumes the exact same sequence */
    plat_srand(12345u);
    for (int i = 0; i < 7; i++) plat_rand();
    saved = plat_rng_state();
    a = plat_rand(); b = plat_rand();
    plat_rng_set(saved);
    check(plat_rand() == a && plat_rand() == b, "rng state round-trip");

    /* autosave snapshot round-trip */
    memset(&in, 0, sizeof in);
    set_message("");
    initPlayer(&in.player, "UjiSimpan");
    in.player.seed = 4242u;
    in.player.gold = 777;
    in.screen = AS_SCREEN_BATTLE;
    in.battleActive = 1;
    strcpy(in.battleEnemy.name, "Slime Uji");
    in.battleEnemy.hp = 60; in.battleEnemy.atk = 9; in.battleEnemy.def = 3;
    in.battleEnemy.exp = 12; in.battleEnemy.gold = 20;
    in.battleEnemyHp = 41;
    plat_rng_set(saved);
    autosave_publish(&in.player, in.screen, &in.battleEnemy, in.battleEnemyHp, NULL, 0, 0, 1);
    autosave_stop();
    check(autosave_load(&out) == 1, "autosave load after publish");
    check(strcmp(out.player.name, "UjiSimpan") == 0 && out.player.gold == 777 &&
          out.battleActive == 1 && out.battleEnemyHp == 41 &&
          strcmp(out.battleEnemy.name, "Slime Uji") == 0 && out.rng == saved,
          "autosave payload preserved");

    /* corrupt the main file past its length -> .bak fallback */
    fp = fopen("data/autosave.dat", "rb");
    if (fp != NULL) {
        long size;
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
        fp = fopen("data/autosave.dat", "r+b");
        if (fp != NULL) {
            fseek(fp, size - 4, SEEK_SET);
            fputc(0, fp);
            fclose(fp);
        }
    }
    check(autosave_load(&out) == 0, "corrupt autosave rejected");

    /* truncated header entirely */
    fp = fopen("data/autosave.dat", "wb");
    if (fp != NULL) { fputs("GQ", fp); fclose(fp); }
    check(autosave_load(&out) == 0, "truncated autosave rejected");

    /* atomic write + orphan tmp cleanup */
    check(file_write_atomic("data/probe_out.txt", "halo\n", 5) == 1, "atomic write");
    file_cleanup_tmp("data");
    remove("data/probe_out.txt");

    /* lock file content */
    check(lock_acquire() == 1, "lock acquire");
    fp = fopen("data/session.lock", "rb");
    if (fp != NULL) {
        char line[128];
        if (fgets(line, sizeof line, fp) == NULL) line[0] = '\0';
        fclose(fp);
        check(strncmp(line, "RUNNING pid=", 12) == 0, "lock content RUNNING pid=");
    } else {
        check(0, "lock content RUNNING pid= (unreadable while held)");
    }
    lock_mark_clean();
    fp = fopen("data/session.lock", "rb");
    if (fp != NULL) {
        char line[128];
        if (fgets(line, sizeof line, fp) == NULL) line[0] = '\0';
        fclose(fp);
        check(strcmp(line, "CLEAN\n") == 0, "lock content CLEAN after mark_clean");
    } else {
        check(0, "lock content CLEAN after mark_clean");
    }

    printf("--- %d dari %d\n", total - failed, total);
    return failed == 0 ? 0 : 1;
}
