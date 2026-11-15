#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "ui.h"

/* Reads one line from stdin; returns 1 when data is available, 0 on EOF. */
int readLine(char *buf, int max)
{
    if (fgets(buf, max, stdin) == NULL) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

int readInt(const char *prompt, int min, int max)
{
    char buf[256];
    char *end;
    long value;

    ui_line_begin();
    for (;;) {
        ui_line_prompt("%s", prompt);
        if (!readLine(buf, sizeof buf)) {
            ui_line_error("Input berakhir (EOF), memakai nilai %d.", min);
            break;
        }
        value = strtol(buf, &end, 10);
        if (end == buf) {
            ui_line_error("Input tidak valid, masukkan angka %d sampai %d.", min, max);
            continue;
        }
        while (*end != '\0' && isspace((unsigned char)*end)) end++;
        if (*end != '\0') {
            ui_line_error("Input tidak valid, masukkan angka %d sampai %d.", min, max);
            continue;
        }
        if (value < min || value > max) {
            ui_line_error("Angka harus %d sampai %d.", min, max);
            continue;
        }
        ui_line_end();
        return (int)value;
    }
    ui_line_end();
    return min;
}

void readString(const char *prompt, char *out, int max)
{
    char buf[256];
    out[0] = '\0';
    ui_line_begin();
    for (;;) {
        ui_line_prompt("%s", prompt);
        if (!readLine(buf, sizeof buf)) {
            ui_line_error("Input berakhir (EOF).");
            break;
        }
        if (buf[0] == '\0') {
            ui_line_error("Input tidak boleh kosong.");
            continue;
        }
        strncpy(out, buf, (size_t)max - 1);
        out[max - 1] = '\0';
        break;
    }
    ui_line_end();
}

void clearScreen(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printLine(void)
{
    printf("------------------------------\n");
}

/* Single exit path: goodbye message, then flush all output. */
void shutdownGame(void)
{
    printf("Terima kasih sudah bermain Gacha Quest!\n");
    fflush(stdout);
}
