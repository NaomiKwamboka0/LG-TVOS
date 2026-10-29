#ifndef FT_CANVAS_H
#define FT_CANVAS_H

/* Software 2D canvas (plan ADR-02: small native UI, software-rendered 2D
 * baseline). Draws into a caller-owned 32-bit 0xAARRGGBB buffer with alpha
 * blending, anti-aliased rounded shapes, gradients, soft shadows and
 * anti-aliased proportional text. Every operation is clipped, so nothing
 * writes outside the buffer; nothing allocates. Integer maths only. The
 * window frontend uploads the result to the screen as an SDL texture. */

#include <stdint.h>

#include "font.h"

typedef uint32_t ft_color; /* 0xAARRGGBB; alpha 0xFF = opaque */

typedef struct {
    uint32_t *px;
    int w, h;   /* physical pixels */
    int stride; /* pixels per row */
    int scale;  /* physical pixels per logical pixel, in halves: CANVAS_1X ... */
} ft_canvas;

/* Drawing is in logical pixels (the 1280x720 layout); the scale maps them
 * onto the real screen. Text is laid out with the 1x metrics and drawn
 * with fonts rasterised at the real size (font_scaled). */
enum { CANVAS_1X = 2, CANVAS_1_5X = 3, CANVAS_2X = 4, CANVAS_3X = 6 };

typedef struct {
    int x, y, w, h;
} ft_rect;

/* Which corners of a rounded rectangle are rounded. */
enum { CORNER_TL = 1, CORNER_TR = 2, CORNER_BL = 4, CORNER_BR = 8, CORNERS_ALL = 15, CORNERS_TOP = 3 };

#define FT_RGB(rgb) (0xFF000000u | (uint32_t)(rgb))
ft_color color_alpha(ft_color c, unsigned alpha); /* replace alpha (0-255) */
ft_color color_mix(ft_color a, ft_color b, unsigned t); /* t: 0 = a, 255 = b */

void canvas_init(ft_canvas *c, uint32_t *px, int w, int h, int stride); /* scale 1x */
void canvas_set_scale(ft_canvas *c, int scale);
/* Physical pixel (tests and frame dumps). */
ft_color canvas_get(const ft_canvas *c, int x, int y);
void canvas_clear(ft_canvas *c, ft_color color);

/* All of the following take logical coordinates and blend using the colour's alpha. */
void canvas_fill(ft_canvas *c, ft_rect r, ft_color color);
void canvas_gradient(ft_canvas *c, ft_rect r, ft_color top, ft_color bottom);
void canvas_round(ft_canvas *c, ft_rect r, int radius, ft_color color);
void canvas_round_gradient(ft_canvas *c, ft_rect r, int radius, unsigned corners, ft_color top, ft_color bottom);
/* Rounded outline of the given thickness (inside the rect). */
void canvas_round_stroke(ft_canvas *c, ft_rect r, int radius, int thickness, ft_color color);
void canvas_circle(ft_canvas *c, int cx, int cy, int radius, ft_color color);
void canvas_ring(ft_canvas *c, int cx, int cy, int radius, int thickness, ft_color color);
/* Soft shadow for a rounded rect; blur is the falloff width in pixels. */
void canvas_shadow(ft_canvas *c, ft_rect r, int blur, ft_color color);
/* Same, leaving alone the pixels inside covered, which something opaque is
 * drawn over next: most of a card's shadow is under the card. */
void canvas_shadow_under(ft_canvas *c, ft_rect r, int blur, ft_color color, ft_rect covered);
/* Radial soft glow filling an ellipse, strongest in the centre. */
void canvas_glow(ft_canvas *c, int cx, int cy, int rx, int ry, ft_color color);
/* Same falloff as canvas_glow (in the mask's own pixels), accumulated into an 8-bit w x h alpha mask
 * (alphas combine like stacked layers), so a fixed glow layout can be
 * recoloured cheaply. */
void canvas_glow_mask(uint8_t *mask, int w, int h, int cx, int cy, int rx, int ry, unsigned alpha);
/* Thick line with round ends. */
void canvas_line(ft_canvas *c, int x0, int y0, int x1, int y1, int thickness, ft_color color);
/* Filled convex polygon (points in order). */
void canvas_polygon(ft_canvas *c, const int *xy, int npoints, ft_color color);
/* Same, but only the part inside the circle (cx, cy, radius) is drawn. */
void canvas_polygon_in_circle(ft_canvas *c, const int *xy, int npoints, int cx, int cy, int radius, ft_color color);
/* Chevron "v" or "^" (up != 0) centred at cx, cy. */
void canvas_chevron(ft_canvas *c, int cx, int cy, int half_w, int thickness, int up, ft_color color);

/* Text: y is the top of the line box; height is font->line_height. */
int text_width(const ft_font *f, const char *text);
int canvas_text(ft_canvas *c, const ft_font *f, int x, int y, const char *text, ft_color color);
/* Cuts the line to max_w pixels, ending in "..." if it did not fit. */
void canvas_text_fit(ft_canvas *c, const ft_font *f, int x, int y, int max_w, const char *text, ft_color color);
/* Word-wraps inside r; returns the number of lines drawn. */
int canvas_text_wrap(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color);
void canvas_text_center(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color);
void canvas_text_right(ft_canvas *c, const ft_font *f, int right_x, int y, const char *text, ft_color color);

/* Drawing clipped to r (logical): out is a canvas over just that part of
 * c's pixels, with its own (0, 0) at r's top-left corner (moved onto the
 * screen if r starts off it); *ox, *oy are that corner in c's logical
 * coordinates. Returns 0 if no part of r is on screen. */
int canvas_clip(const ft_canvas *c, ft_rect r, ft_canvas *out, int *ox, int *oy);

/* Draws src (a card drawn on its own, physical size) onto c with its
 * top-left at physical (x, y), turned in 3D and seen in perspective: about
 * the vertical axis by turn_y and the horizontal axis by turn_x, in
 * thousandths of a radian (keep them under 300). Only src's rounded
 * rectangle (corner radius in physical px) is drawn, with smooth edges. */
void canvas_turn(ft_canvas *c, const ft_canvas *src, int x, int y, int radius, int turn_x, int turn_y);

/* FNV-1a over physical pixels, for rendering regression tests. */
uint32_t canvas_hash(const ft_canvas *c);

#endif
