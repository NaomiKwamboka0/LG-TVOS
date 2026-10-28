#ifndef FT_ART_H
#define FT_ART_H

/* Original vector artwork for cards, buttons and section headers. No
 * third-party logos: channels are shown by name and colour, never a logo. */

#include "canvas.h"

typedef enum {
    ART_LIVE_TV, ART_FOOTBALL, ART_GAMES, ART_MORE,
    ART_SETTINGS, ART_RACER, ART_PENALTY, ART_GARDEN, ART_PROFILE, ART_CHANNELS,
    ART_GEAR, ART_INFO, ART_VOLUME,
    ART_COUNT
} art_id;

/* Rounded gradient tile with its glyph. focused adds the glossy highlight.
 * text is drawn instead of the glyph when given (may be NULL). */
void art_tile(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text);
ft_color art_accent(art_id id);

/* Glyph only, centred at cx, cy, scaled by s (about a third of the tile height). */
void art_glyph(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink);

#endif
