#ifndef FT_ART_H
#define FT_ART_H

/* Original vector artwork for tiles, shelf cards and section headers. No
 * third-party logos: apps without our own art show their name as text. */

#include "canvas.h"

typedef enum {
    ART_LIVE_TV, ART_YOUTUBE, ART_FOOTBALL, ART_GAMES, ART_NETFLIX, ART_MORE,
    ART_SETTINGS, ART_RACER, ART_PENALTY, ART_GARDEN, ART_PROFILE, ART_SEARCH, ART_CHANNELS,
    ART_COUNT
} art_id;

/* Rounded gradient tile with its glyph. focused adds the glossy highlight.
 * text is drawn instead of a glyph for apps without art (may be NULL). */
void art_tile(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text);
ft_color art_accent(art_id id);

/* Glyph only, centred at cx, cy, scaled by s (about a third of the tile height). */
void art_glyph(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink);

#endif
