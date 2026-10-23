#ifndef FT_THEME_H
#define FT_THEME_H

/* Colour tokens and safe-area layout for 1280x720 (plan p.14: large labels,
 * high contrast, no animated backgrounds). Focus is always shown by shape
 * (thick border) as well as colour. */

#define VIEW_W 1280
#define VIEW_H 720
#define VIEW_MARGIN 72 /* keeps text inside typical TV overscan */

#define COL_BG 0xFF142A43u
#define COL_ROW 0xFF1D3856u
#define COL_TILE 0xFFE3EAF1u
#define COL_TILE_FOCUS 0xFFE4F7F2u
#define COL_FOCUS 0xFF14877Du
#define COL_TEXT_DARK 0xFF132A43u
#define COL_TEXT_MUTED 0xFF52637Au
#define COL_TEXT_LIGHT 0xFFFFFFFFu
#define COL_TEXT_SOFT 0xFFC3D2E0u
#define COL_STATUS 0xFF0E6A63u
#define COL_OVERLAY 0xFF0D1E31u
#define COL_GOOD 0xFF2E9E5Bu
#define COL_BAD 0xFFC8413Bu
#define COL_GOLD 0xFFF2C230u

#endif
