#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "style.h"
#include "caps.h"

typedef struct { unsigned char r, g, b; } Rgb;

static const Rgb pal[ST_HP_GRAD_BASE] = {
    { 0xE2, 0xE8, 0xF0 }, /* ST_TEXT */
    { 0x5F, 0xB3, 0xFF }, /* ST_TITLE (bold added at emit) */
    { 0x5F, 0xB3, 0xFF }, /* ST_ACCENT */
    { 0x4A, 0x55, 0x68 }, /* ST_BORDER */
    { 0x71, 0x80, 0x96 }, /* ST_DIM */
    { 0xF6, 0xC4, 0x53 }, /* ST_GOLD */
    { 0xF5, 0x65, 0x65 }, /* ST_DANGER */
    { 0x48, 0xBB, 0x78 }, /* ST_SUCCESS */
    { 0xA0, 0xAE, 0xC0 }, /* ST_RARITY_COMMON */
    { 0x42, 0x99, 0xE1 }, /* ST_RARITY_RARE */
    { 0xEC, 0xC9, 0x4B }, /* ST_RARITY_EPIC */
    { 0xD5, 0x3F, 0x8C }, /* ST_RARITY_LEGENDARY */
    { 0xCD, 0x7F, 0x32 }, /* ST_BRONZE */
    { 0x2D, 0x37, 0x48 }, /* ST_SELECTED (background) */
};
static const Rgb hpEnds[3] = {
    { 0x48, 0xBB, 0x78 }, /* hp_good */
    { 0xEC, 0xC9, 0x4B }, /* hp_mid */
    { 0xF5, 0x65, 0x65 }, /* hp_low */
};

/* The 16 base ANSI colors, dark then bright row. */
static const Rgb ansi16[16] = {
    { 0, 0, 0 },       { 205, 0, 0 },     { 0, 205, 0 },     { 205, 205, 0 },
    { 0, 0, 238 },     { 205, 0, 205 },   { 0, 205, 205 },   { 229, 229, 229 },
    { 127, 127, 127 }, { 255, 0, 0 },     { 0, 255, 0 },     { 255, 255, 0 },
    { 92, 92, 255 },   { 255, 0, 255 },   { 0, 255, 255 },   { 255, 255, 255 },
};

static char fgSeq[ST_COUNT][24];
static char bgSeq[ST_COUNT][24];

static int clamp255(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int to6(int v) { return (v * 5 + 127) / 255; }

/* RGB to the xterm 256 palette: grayscale ramp for near-gray, 6x6x6 cube else. */
static int rgbTo256(const Rgb *c)
{
    int max = c->r > c->g ? (c->r > c->b ? c->r : c->b) : (c->g > c->b ? c->g : c->b);
    int min = c->r < c->g ? (c->r < c->b ? c->r : c->b) : (c->g < c->b ? c->g : c->b);
    if (max - min < 9 && max < 245)
        return 232 + (max * 23) / 255;
    return 16 + 36 * to6(c->r) + 6 * to6(c->g) + to6(c->b);
}

static int rgbTo16(const Rgb *c)
{
    int best = 0, bestD = 1 << 30, i;
    for (i = 0; i < 16; i++) {
        int dr = c->r - ansi16[i].r, dg = c->g - ansi16[i].g, db = c->b - ansi16[i].b;
        int d = dr * dr * 2 + dg * dg * 4 + db * db;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

static void buildFg(Style s, const Rgb *c)
{
    ColorMode m = caps_color();
    if (m == COLOR_TRUE) sprintf(fgSeq[s], "\033[38;2;%d;%d;%dm", c->r, c->g, c->b);
    else if (m == COLOR_256) sprintf(fgSeq[s], "\033[38;5;%dm", rgbTo256(c));
    else if (m == COLOR_16) {
        int i = rgbTo16(c);
        sprintf(fgSeq[s], "\033[%dm", i < 8 ? 30 + i : 90 + (i - 8));
    } else fgSeq[s][0] = '\0';
}

static void buildBg(Style s, const Rgb *c)
{
    ColorMode m = caps_color();
    if (m == COLOR_TRUE) sprintf(bgSeq[s], "\033[48;2;%d;%d;%dm", c->r, c->g, c->b);
    else if (m == COLOR_256) sprintf(bgSeq[s], "\033[48;5;%dm", rgbTo256(c));
    else if (m == COLOR_16) {
        int i = rgbTo16(c);
        sprintf(bgSeq[s], "\033[%dm", i < 8 ? 40 + i : 100 + (i - 8));
    } else bgSeq[s][0] = '\0';
}

void style_init(void)
{
    int i;
    for (i = 0; i < ST_HP_GRAD_BASE; i++) {
        buildFg((Style)i, &pal[i]);
        buildBg((Style)i, &pal[i]);
    }
    /* HP gradient: good -> mid -> low, 11 slots */
    for (i = 0; i <= 10; i++) {
        Rgb g;
        int slot = ST_HP_GRAD_BASE + i;
        if (i <= 5) {
            float t = i / 5.0f;
            g.r = (unsigned char)clamp255((int)(hpEnds[0].r + (hpEnds[1].r - hpEnds[0].r) * t));
            g.g = (unsigned char)clamp255((int)(hpEnds[0].g + (hpEnds[1].g - hpEnds[0].g) * t));
            g.b = (unsigned char)clamp255((int)(hpEnds[0].b + (hpEnds[1].b - hpEnds[0].b) * t));
        } else {
            float t = (i - 5) / 5.0f;
            g.r = (unsigned char)clamp255((int)(hpEnds[1].r + (hpEnds[2].r - hpEnds[1].r) * t));
            g.g = (unsigned char)clamp255((int)(hpEnds[1].g + (hpEnds[2].g - hpEnds[1].g) * t));
            g.b = (unsigned char)clamp255((int)(hpEnds[1].b + (hpEnds[2].b - hpEnds[1].b) * t));
        }
        buildFg((Style)slot, &g);
        bgSeq[slot][0] = '\0';
    }
}

const char *style_fg(Style s)
{
    if ((int)s < 0 || (int)s >= ST_COUNT) return "";
    return fgSeq[s];
}

const char *style_bg(Style s)
{
    if ((int)s < 0 || (int)s >= ST_COUNT) return "";
    return bgSeq[s];
}

Style style_rarity(int rarity)
{
    switch (rarity) {
    case 4: return ST_RARITY_LEGENDARY;
    case 3: return ST_RARITY_EPIC;
    case 2: return ST_RARITY_RARE;
    default: return ST_RARITY_COMMON;
    }
}

Style style_hp(int hp, int maxHp)
{
    int pct = maxHp > 0 ? hp * 100 / maxHp : 0;
    if (pct > 60) return ST_SUCCESS;
    if (pct > 30) return ST_GOLD;
    return ST_DANGER;
}
