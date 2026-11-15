#ifndef STYLE_H
#define STYLE_H

/* Semantic style tokens. Screens pick a token, style.c turns it into the best
   ANSI sequence the detected terminal supports (mono drops everything). */
typedef enum {
    ST_TEXT = 0,
    ST_TITLE,       /* bold + accent */
    ST_ACCENT,
    ST_BORDER,
    ST_DIM,
    ST_GOLD,
    ST_DANGER,
    ST_SUCCESS,
    ST_RARITY_COMMON,
    ST_RARITY_RARE,
    ST_RARITY_EPIC,
    ST_RARITY_LEGENDARY,
    ST_BRONZE,
    ST_SELECTED,    /* selection background; foreground chosen by the widget */
    ST_HP_GRAD_BASE,/* +0..10 gradient slots, do not use directly */
    ST_COUNT
} Style;

#define ST_HP_GRAD(i) ((Style)(ST_HP_GRAD_BASE + (i)))

void style_init(void);           /* build ANSI strings for the detected mode */

Style style_rarity(int rarity);   /* Item rarity 1..4 -> token */
Style style_hp(int hp, int maxHp);/* threshold token for 256/16/mono modes */

const char *style_fg(Style s);    /* "" in mono */
const char *style_bg(Style s);    /* "" in mono; selected uses bg color or reverse */

#endif
