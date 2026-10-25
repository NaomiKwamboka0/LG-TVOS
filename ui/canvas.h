#ifndef FT_CANVAS_H
#define FT_CANVAS_H

/* Software 2D canvas (plan ADR-02: small native UI, software-rendered 2D
 * baseline). Draws into a caller-owned 32-bit 0xAARRGGBB buffer with alpha
 * blending, anti-aliased rounded shapes, gradients, soft shadows and
 * anti-aliased proportional text. Every operation is clipped, so nothing
 * writes outside the buffer; nothing allocates. Integer maths only, so the
 * TV's floating-point ABI does not matter. The TV display backend converts
 * the result to the panel's real pixel format once gate I05 identifies it. */

#include <stdint.h>

#include "font.h"

typedef uint32_t ft_color; /* 0xAARRGGBB; alpha 0xFF = opaque */

typedef struct {
    uint32_t *px;
    int w, h;
    int stride; /* pixels per row */
} ft_canvas;

typedef struct {
    int x, y, w, h;
} ft_rect;

/* Which corners of a rounded rectangle are rounded. */
enum { CORNER_TL = 1, CORNER_TR = 2, CORNER_BL = 4, CORNER_BR = 8, CORNERS_ALL = 15, CORNERS_TOP = 3 };

#define FT_RGB(rgb) (0xFF000000u | (uint32_t)(rgb))
ft_color color_alpha(ft_color c, unsigned alpha); /* replace alpha (0-255) */
ft_color color_mix(ft_color a, ft_color b, unsigned t); /* t: 0 = a, 255 = b */

void canvas_init(ft_canvas *c, uint32_t *px, int w, int h, int stride);
ft_color canvas_get(const ft_canvas *c, int x, int y);
void canvas_clear(ft_canvas *c, ft_color color);

/* All of the following blend using the colour's alpha. */
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
/* Radial soft glow filling an ellipse, strongest in the centre. */
void canvas_glow(ft_canvas *c, int cx, int cy, int rx, int ry, ft_color color);
/* Same falloff as canvas_glow, accumulated into an 8-bit w x h alpha mask
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

/* FNV-1a over visible pixels, for rendering regression tests. */
uint32_t canvas_hash(const ft_canvas *c);

#endif
