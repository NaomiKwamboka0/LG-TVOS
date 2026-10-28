#ifndef FT_FONT_H
#define FT_FONT_H

/* Anti-aliased proportional fonts from Inter 4.1 (SIL OFL 1.1). The 1x
 * (720p) fonts are pre-rendered into ui/font_ui_data.c by tools/fontbake.c
 * and always available; sharper copies for bigger screens are rasterised
 * from the TTF files at start-up (font_scale_init). ASCII 32-126 only;
 * other bytes draw as '?'. Kiswahili needs no accented letters. */

#include <stdint.h>

typedef struct {
    int16_t x0, y0;   /* bitmap offset from pen position (y0 from baseline, negative = up) */
    uint8_t w, h;     /* bitmap size in pixels */
    uint16_t advance; /* pen advance in 1/64 pixel */
    uint32_t offset;  /* into alpha[]: h rows of (w + 1) / 2 bytes, high nibble = left pixel */
} ft_glyph;

typedef struct {
    uint16_t pair;  /* (left << 8) | right */
    int16_t adjust; /* 1/64 pixel */
} ft_kern;

typedef struct {
    const char *name;
    int size;        /* pixels per em */
    int ascent;      /* pixels above baseline */
    int line_height; /* pixels */
    int first, count;
    const ft_glyph *glyphs;
    const uint8_t *alpha;
    const ft_kern *kerns; /* sorted by pair */
    int nkerns;
} ft_font;

extern const ft_font font_display; /* Inter Bold 46: screen and hero titles */
extern const ft_font font_title;   /* Inter SemiBold 30: section titles, big numbers */
extern const ft_font font_label;   /* Inter SemiBold 24: tile names, list rows */
extern const ft_font font_body;    /* Inter Medium 20: values and descriptions */
extern const ft_font font_caption; /* Inter Medium 18: hints */

/* Loads the Inter TTFs from dir (ending in a separator). 0 when all four
 * were read; otherwise only the 1x fonts are available. */
int font_scale_init(const char *dir);
/* The copy of f for a canvas scale (CANVAS_*), made on first use; f itself
 * at 1x or when the TTFs are missing. */
const ft_font *font_scaled(const ft_font *f, int scale);
int font_scale_ready(void);

#endif
