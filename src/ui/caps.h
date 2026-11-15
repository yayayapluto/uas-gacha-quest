#ifndef CAPS_H
#define CAPS_H

typedef enum { COLOR_MONO = 0, COLOR_16, COLOR_256, COLOR_TRUE } ColorMode;

/* Border/glyph set: unicode thin lines and blocks, or pure ASCII. */
typedef struct {
    const char *tl, *tr, *bl, *br, *hz, *vr;
    const char *blockFull, *blockEmpty;
    const char *marker;      /* selected item marker */
    const char *arrowUD;     /* footer hint for up/down */
} Glyphs;

void caps_detect(void);

ColorMode caps_color(void);
int caps_unicode(void);
int caps_interactive(void);
int caps_cols(void);
int caps_rows(void);
void caps_set_size(int cols, int rows);
const Glyphs *caps_glyphs(void);

#endif
