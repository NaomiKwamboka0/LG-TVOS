#ifndef FT_ART_H
#define FT_ART_H

/* Original vector artwork for cards, buttons and section headers. No
 * third-party logos: channels are shown by name and colour, never a logo. */

#include "canvas.h"

typedef enum {
    ART_LIVE_TV, ART_FOOTBALL, ART_GAMES, ART_MORE,
    ART_SETTINGS, ART_RACER, ART_PENALTY, ART_GARDEN, ART_PROFILE, ART_CHANNELS,
    ART_GEAR, ART_INFO, ART_VOLUME, ART_PLAY, ART_SEARCH, ART_YOUTUBE, ART_NETFLIX,
    ART_COUNT
} art_id;

/* A 3D slab, the body of every card: a thick tile (its darker edge shows
 * below and, as it leans, at the side), lit from above with a glossy
 * diagonal sheen and bevelled edges. The focused one glows from inside
 * where the light (the mouse pointer) is. tilt_x, tilt_y: -256..256, how
 * far the card leans toward the pointer; 0, 0 = upright. No shadow: the
 * caller decides how high the card floats. */
void art_slab(ft_canvas *c, ft_rect r, int radius, ft_color top, ft_color bottom, int focused, int tilt_x, int tilt_y);
/* A glyph with depth: extruded toward the bottom right in the edge colour. */
void art_glyph_3d(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink, ft_color edge);
/* A slab in the artwork's colours with its 3D glyph, or text instead of
 * the glyph when given (may be NULL). */
void art_tile(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text);
void art_tile_lean(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text, int tilt_x,
                   int tilt_y);
ft_color art_accent(art_id id);

/* Glyph only, centred at cx, cy, scaled by s (about a third of the tile height). */
void art_glyph(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink);

#endif
