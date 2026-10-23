#ifndef FT_FONT_H
#define FT_FONT_H

/* Fixed-cell bitmap fonts (Spleen 2.1.0, BSD-2-Clause). ASCII 32-126 only;
 * other bytes draw as '?'. Kiswahili needs no accented letters. */

#include <stdint.h>

typedef struct {
    int w, h;
    int first, count;
    int row_bytes;
    const uint8_t *bits; /* count glyphs x h rows x row_bytes, MSB = leftmost pixel */
} ft_font;

extern const ft_font font_12x24; /* hints and help lines */
extern const ft_font font_16x32; /* labels and list items */
extern const ft_font font_32x64; /* screen titles, big results */

#endif
