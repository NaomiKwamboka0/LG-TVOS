#ifndef FT_FONT_H
#define FT_FONT_H

/* Anti-aliased proportional fonts, pre-rendered on the computer from Inter
 * 4.1 (SIL OFL 1.1) by tools/fontbake.c. The TV only blends ready-made
 * 4-bit alpha glyphs; it never parses font files. ASCII 32-126 only; other
 * bytes draw as '?'. Kiswahili needs no accented letters. */

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

#endif
