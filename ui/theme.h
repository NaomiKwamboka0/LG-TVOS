#ifndef FT_THEME_H
#define FT_THEME_H

/* Visual language after TCL's Google TV launcher: near-black backdrop
 * tinted by the focused channel, tabs along the top, a wide banner, rows of
 * 16:9 cards that grow and take a white ring when focused, and a channel
 * guide. Our own colours and artwork; no TCL or Google assets. Logical
 * 1280x720. Focus is always shown by size or a ring as well as colour. */

#include "canvas.h"

#define VIEW_W 1280
#define VIEW_H 720
#define VIEW_MARGIN 72 /* keeps content inside typical TV overscan */

#define COL_BG_TOP FT_RGB(0x17181C)
#define COL_BG_BOTTOM FT_RGB(0x0F1012)
#define COL_BG COL_BG_BOTTOM

#define COL_TEXT FT_RGB(0xFFFFFF)
#define COL_TEXT_2 0xB3E8EAEDu /* secondary */
#define COL_TEXT_3 0x75E8EAEDu /* hints */

#define COL_GLASS 0xC21B1E28u      /* translucent dark panel */
#define COL_GLASS_EDGE 0x29FFFFFFu /* hairline on panels */
#define COL_ROW 0x1CFFFFFFu        /* resting list row */
#define COL_FOCUS_BG FT_RGB(0xF4F4F6)
#define COL_ON_FOCUS FT_RGB(0x15161B)
#define COL_ON_FOCUS_2 0xB015161Bu
#define COL_SHADOW 0x9E000000u
#define COL_DIM 0xA6000000u /* behind modal cards */
#define COL_RING FT_RGB(0xFFFFFF) /* focus ring on cards */
#define COL_LIVE FT_RGB(0xE53935)  /* LIVE badge */
#define COL_PANEL 0xEB121316u      /* channel list over the picture */

#define COL_GOOD FT_RGB(0x31C46C)
#define COL_BAD FT_RGB(0xFF5B4F)
#define COL_GOLD FT_RGB(0xFFD24A)

#endif
