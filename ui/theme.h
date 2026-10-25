#ifndef FT_THEME_H
#define FT_THEME_H

/* Visual language inspired by Apple TV: dark backdrop, a "top shelf" that
 * follows focus, glossy rounded tiles that lift when focused, white focused
 * rows, translucent panels. Our own colours and artwork; no Apple assets or
 * fonts. Logical 1280x720 (plan p.14). Focus is always shown by size and
 * shadow as well as colour. */

#include "canvas.h"

#define VIEW_W 1280
#define VIEW_H 720
#define VIEW_MARGIN 72 /* keeps content inside typical TV overscan */

#define COL_BG_TOP FT_RGB(0x111827)
#define COL_BG_BOTTOM FT_RGB(0x05070C)
#define COL_BG COL_BG_BOTTOM

#define COL_TEXT FT_RGB(0xFFFFFF)
#define COL_TEXT_2 0xA8EBEBF5u /* secondary */
#define COL_TEXT_3 0x6BEBEBF5u /* hints */

#define COL_GLASS 0xC21B1E28u      /* translucent dark panel */
#define COL_GLASS_EDGE 0x29FFFFFFu /* hairline on panels */
#define COL_ROW 0x1CFFFFFFu        /* resting list row */
#define COL_FOCUS_BG FT_RGB(0xF4F4F6)
#define COL_ON_FOCUS FT_RGB(0x15161B)
#define COL_ON_FOCUS_2 0xB015161Bu
#define COL_SHADOW 0x9E000000u
#define COL_DIM 0xA6000000u /* behind modal cards */

#define COL_GOOD FT_RGB(0x31C46C)
#define COL_BAD FT_RGB(0xFF5B4F)
#define COL_GOLD FT_RGB(0xFFD24A)

#endif
