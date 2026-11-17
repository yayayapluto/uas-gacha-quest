#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "util.h"
#include "ui.h"
#include "caps.h"
#include "term.h"

/* Reads one line from stdin; returns 1 when data is available, 0 on EOF. */
int readLine(char *buf, int max)
{
    if (fgets(buf, max, stdin) == NULL) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* Line reader that honours Esc in interactive mode. Returns 1 for a line,
   0 for EOF, -1 when the player pressed Esc. */
int readLineEsc(char *buf, int max)
{
    char ch;
    int len = 0;

    if (!caps_interactive()) return readLine(buf, max) ? 1 : 0;
    term_raw();
    for (;;) {
        TermKey k = term_getkey(&ch);
        if (k == KEY_ENTER) {
            buf[len] = '\0';
            fputc('\n', stdout);
            fflush(stdout);
            return 1;
        }
        if (k == KEY_ESC) {
            buf[0] = '\0';
            fputc('\n', stdout);
            fflush(stdout);
            return -1;
        }
        if (k == KEY_EOF) {
            buf[len] = '\0';
            return 0;
        }
        if (k != KEY_CHAR) continue;
        if (ch == '\r' || ch == '\n') {
            buf[len] = '\0';
            fputc('\n', stdout);
            fflush(stdout);
            return 1;
        }
        if (ch == 0x7f || ch == '\b') {
            if (len > 0) {
                len--;
                fputs("\b \b", stdout);
                fflush(stdout);
            }
            continue;
        }
        if ((unsigned char)ch >= 0x20 && len < max - 1) {
            buf[len++] = ch;
            fputc(ch, stdout);
            fflush(stdout);
        }
    }
}

/* Returns 1 when a value was entered, 0 on Esc or EOF (out is then min). */
int readIntTry(const char *prompt, int min, int max, int *out)
{
    char buf[256];
    char *end;
    long value;

    ui_line_begin("%s", prompt);
    for (;;) {
        ui_line_prompt();
        {
            int st = readLineEsc(buf, sizeof buf);
            if (st < 0) {
                ui_line_end();
                *out = min;
                return 0;
            }
            if (st == 0) {
                ui_line_error("Input berakhir (EOF), memakai nilai %d.", min);
                break;
            }
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
        *out = (int)value;
        return 1;
    }
    ui_line_end();
    *out = min;
    return 0;
}

int readInt(const char *prompt, int min, int max)
{
    int value = min;
    readIntTry(prompt, min, max, &value);
    return value;
}

/* Returns 1 when text was entered, 0 on Esc or EOF (out is then empty). */
int readStringTry(const char *prompt, char *out, int max)
{
    char buf[256];

    out[0] = '\0';
    ui_line_begin("%s", prompt);
    for (;;) {
        ui_line_prompt();
        {
            int st = readLineEsc(buf, sizeof buf);
            if (st < 0) {
                ui_line_end();
                return 0;
            }
            if (st == 0) {
                ui_line_error("Input berakhir (EOF).");
                break;
            }
        }
        if (buf[0] == '\0') {
            ui_line_error("Input tidak boleh kosong.");
            continue;
        }
        strncpy(out, buf, (size_t)max - 1);
        out[max - 1] = '\0';
        ui_line_end();
        return 1;
    }
    ui_line_end();
    return 0;
}

void readString(const char *prompt, char *out, int max)
{
    readStringTry(prompt, out, max);
}

/* y/n confirmation. Esc and EOF count as "no". */
int confirmPrompt(const char *prompt)
{
    char answer[MAX_NAME];
    if (!readStringTry(prompt, answer, MAX_NAME)) return 0;
    return answer[0] == 'y' || answer[0] == 'Y';
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
