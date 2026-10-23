#ifndef FT_CANVAS_H
#define FT_CANVAS_H

/* Software 2D canvas (plan ADR-02: small native UI, software-rendered 2D
 * baseline). Draws into a caller-owned 32-bit 0xAARRGGBB buffer; every
 * operation is clipped, so nothing writes outside the buffer. No heap use.
 * The TV display backend converts this to the panel's real pixel format
 * once gate I05 identifies it (plan p.24 buffer budget). */

#include <stdint.h>

#include "font.h"

typedef uint32_t ft_color;

typedef struct {
    uint32_t *px;
    int w, h;
    int stride; /* pixels per row */
} ft_canvas;

typedef struct {
    int x, y, w, h;
} ft_rect;

void canvas_init(ft_canvas *c, uint32_t *px, int w, int h, int stride);
void canvas_clear(ft_canvas *c, ft_color color);
void canvas_fill(ft_canvas *c, ft_rect r, ft_color color);
void canvas_fill_round(ft_canvas *c, ft_rect r, int radius, ft_color color);
/* Rounded panel with a border of the given thickness. */
void canvas_panel(ft_canvas *c, ft_rect r, int radius, int border, ft_color edge, ft_color fill);
void canvas_circle(ft_canvas *c, int cx, int cy, int radius, ft_color color);
/* Isosceles triangle pointing up (up != 0) or down, apex at (cx, tip_y). */
void canvas_triangle(ft_canvas *c, int cx, int tip_y, int half_w, int height, int up, ft_color color);
ft_color canvas_get(const ft_canvas *c, int x, int y);

int text_width(const ft_font *f, const char *text);
/* Draws one line; returns the x after the last glyph. */
int canvas_text(ft_canvas *c, const ft_font *f, int x, int y, const char *text, ft_color color);
/* Draws a line cut to max_w pixels, ending in "..." if it did not fit. */
void canvas_text_fit(ft_canvas *c, const ft_font *f, int x, int y, int max_w, const char *text, ft_color color);
/* Word-wraps inside r; returns the number of lines drawn. */
int canvas_text_wrap(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color);
void canvas_text_center(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color);

/* FNV-1a over visible pixels, for rendering regression tests. */
uint32_t canvas_hash(const ft_canvas *c);

#endif
