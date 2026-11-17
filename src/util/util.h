#ifndef UTIL_H
#define UTIL_H

int readLine(char *buf, int max);
int readLineEsc(char *buf, int max);   /* 1 line, 0 EOF, -1 Esc */
int readInt(const char *prompt, int min, int max);   /* Esc/EOF -> min */
int readIntTry(const char *prompt, int min, int max, int *out);  /* 0 on Esc/EOF */
int readStringTry(const char *prompt, char *out, int max);       /* 0 on Esc/EOF */
int confirmPrompt(const char *prompt);                           /* y/n, Esc = no */
void readString(const char *prompt, char *out, int max);
void clearScreen(void);
void printLine(void);
void shutdownGame(void);

#endif
