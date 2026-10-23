#include "canvas.h"

#include <string.h>

void canvas_init(ft_canvas *c, uint32_t *px, int w, int h, int stride)
{
    c->px = px;
    c->w = w;
    c->h = h;
    c->stride = stride;
}

static int clip(const ft_canvas *c, ft_rect *r)
{
    int x0 = r->x < 0 ? 0 : r->x;
    int y0 = r->y < 0 ? 0 : r->y;
    int x1 = r->x + r->w > c->w ? c->w : r->x + r->w;
    int y1 = r->y + r->h > c->h ? c->h : r->y + r->h;
    if (r->w <= 0 || r->h <= 0 || x1 <= x0 || y1 <= y0)
        return 0;
    r->x = x0;
    r->y = y0;
    r->w = x1 - x0;
    r->h = y1 - y0;
    return 1;
}

static void span(ft_canvas *c, int x0, int x1, int y, ft_color color)
{
    uint32_t *row;
    int x;
    if (y < 0 || y >= c->h)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > c->w)
        x1 = c->w;
    row = c->px + (long)y * c->stride;
    for (x = x0; x < x1; x++)
        row[x] = color;
}

void canvas_fill(ft_canvas *c, ft_rect r, ft_color color)
{
    int y;
    if (!clip(c, &r))
        return;
    for (y = r.y; y < r.y + r.h; y++)
        span(c, r.x, r.x + r.w, y, color);
}

void canvas_clear(ft_canvas *c, ft_color color)
{
    ft_rect all = {0, 0, c->w, c->h};
    canvas_fill(c, all, color);
}

/* Horizontal inset of a rounded corner at row dy (0 = top edge). */
static int corner_inset(int radius, int dy)
{
    int ry = radius - dy, dx = radius;
    if (ry <= 0)
        return 0;
    while (dx > 0 && (dx - 1) * (dx - 1) + ry * ry > radius * radius)
        dx--;
    return radius - dx;
}

void canvas_fill_round(ft_canvas *c, ft_rect r, int radius, ft_color color)
{
    int dy;
    if (r.w <= 0 || r.h <= 0)
        return;
    if (radius * 2 > r.w)
        radius = r.w / 2;
    if (radius * 2 > r.h)
        radius = r.h / 2;
    for (dy = 0; dy < r.h; dy++) {
        int inset = 0;
        if (dy < radius)
            inset = corner_inset(radius, dy);
        else if (dy >= r.h - radius)
            inset = corner_inset(radius, r.h - 1 - dy);
        span(c, r.x + inset, r.x + r.w - inset, r.y + dy, color);
    }
}

void canvas_panel(ft_canvas *c, ft_rect r, int radius, int border, ft_color edge, ft_color fill)
{
    ft_rect inner;
    canvas_fill_round(c, r, radius, edge);
    inner.x = r.x + border;
    inner.y = r.y + border;
    inner.w = r.w - 2 * border;
    inner.h = r.h - 2 * border;
    canvas_fill_round(c, inner, radius > border ? radius - border : 0, fill);
}

void canvas_circle(ft_canvas *c, int cx, int cy, int radius, ft_color color)
{
    int dy;
    for (dy = -radius; dy <= radius; dy++) {
        int dx = radius;
        while (dx > 0 && dx * dx + dy * dy > radius * radius)
            dx--;
        span(c, cx - dx, cx + dx + 1, cy + dy, color);
    }
}

void canvas_triangle(ft_canvas *c, int cx, int tip_y, int half_w, int height, int up, ft_color color)
{
    int i;
    for (i = 0; i < height; i++) {
        int hw = height > 1 ? half_w * i / (height - 1) : half_w;
        span(c, cx - hw, cx + hw + 1, up ? tip_y + i : tip_y - i, color);
    }
}

ft_color canvas_get(const ft_canvas *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return 0;
    return c->px[(long)y * c->stride + x];
}

int text_width(const ft_font *f, const char *text)
{
    return (int)strlen(text) * f->w;
}

static void glyph(ft_canvas *c, const ft_font *f, int x, int y, unsigned char ch, ft_color color)
{
    int gy, gx, index;
    const uint8_t *g;
    if (x >= c->w || y >= c->h || x + f->w <= 0 || y + f->h <= 0)
        return;
    index = (ch >= f->first && ch < f->first + f->count) ? ch - f->first : '?' - f->first;
    g = f->bits + (long)index * f->h * f->row_bytes;
    for (gy = 0; gy < f->h; gy++) {
        int py = y + gy;
        const uint8_t *row = g + gy * f->row_bytes;
        if (py < 0 || py >= c->h)
            continue;
        for (gx = 0; gx < f->w; gx++) {
            int px = x + gx;
            if (px >= 0 && px < c->w && (row[gx >> 3] & (0x80u >> (gx & 7))))
                c->px[(long)py * c->stride + px] = color;
        }
    }
}

int canvas_text(ft_canvas *c, const ft_font *f, int x, int y, const char *text, ft_color color)
{
    for (; *text; text++, x += f->w)
        glyph(c, f, x, y, (unsigned char)*text, color);
    return x;
}

void canvas_text_fit(ft_canvas *c, const ft_font *f, int x, int y, int max_w, const char *text, ft_color color)
{
    int fits = max_w / f->w, len = (int)strlen(text), i;
    if (len <= fits) {
        canvas_text(c, f, x, y, text, color);
        return;
    }
    for (i = 0; i < fits - 3 && i < len; i++)
        glyph(c, f, x + i * f->w, y, (unsigned char)text[i], color);
    for (; i < fits; i++)
        glyph(c, f, x + i * f->w, y, '.', color);
}

int canvas_text_wrap(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color)
{
    int per_line = r.w / f->w, max_lines = r.h / f->h, lines = 0;
    const char *p = text;
    if (per_line <= 0 || max_lines <= 0)
        return 0;
    while (*p && lines < max_lines) {
        int len = (int)strlen(p), take = len, i;
        while (*p == ' ')
            p++, len--;
        take = len;
        if (len > per_line) {
            take = per_line;
            for (i = per_line; i > 0; i--)
                if (p[i] == ' ') {
                    take = i;
                    break;
                }
        }
        if (lines == max_lines - 1 && len > take) {
            char last[256];
            int n = len < (int)sizeof last - 1 ? len : (int)sizeof last - 1;
            memcpy(last, p, (size_t)n);
            last[n] = '\0';
            canvas_text_fit(c, f, r.x, r.y + lines * f->h, r.w, last, color);
            return lines + 1;
        }
        for (i = 0; i < take; i++)
            glyph(c, f, r.x + i * f->w, r.y + lines * f->h, (unsigned char)p[i], color);
        p += take;
        lines++;
    }
    return lines;
}

void canvas_text_center(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color)
{
    int w = text_width(f, text);
    if (w > r.w) {
        canvas_text_fit(c, f, r.x, r.y + (r.h - f->h) / 2, r.w, text, color);
        return;
    }
    canvas_text(c, f, r.x + (r.w - w) / 2, r.y + (r.h - f->h) / 2, text, color);
}

uint32_t canvas_hash(const ft_canvas *c)
{
    uint32_t h = 2166136261u;
    int x, y;
    for (y = 0; y < c->h; y++)
        for (x = 0; x < c->w; x++) {
            uint32_t v = c->px[(long)y * c->stride + x];
            int b;
            for (b = 0; b < 4; b++) {
                h ^= (v >> (8 * b)) & 0xFFu;
                h *= 16777619u;
            }
        }
    return h;
}
