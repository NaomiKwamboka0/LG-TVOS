#include "canvas.h"

#include <string.h>

/* ---- colour ----------------------------------------------------------- */

static uint32_t mul255(uint32_t x, uint32_t a)
{
    x = x * a + 128;
    return (x + (x >> 8)) >> 8; /* x * a / 255, rounded */
}

ft_color color_alpha(ft_color c, unsigned alpha)
{
    return (c & 0x00FFFFFFu) | ((uint32_t)(alpha > 255 ? 255 : alpha) << 24);
}

ft_color color_mix(ft_color a, ft_color b, unsigned t)
{
    ft_color out = 0;
    int shift;
    if (t > 255)
        t = 255;
    for (shift = 0; shift < 32; shift += 8) {
        int ca = (int)((a >> shift) & 0xFFu), cb = (int)((b >> shift) & 0xFFu);
        int v = ca + ((cb - ca) * (int)t + (cb >= ca ? 127 : -127)) / 255;
        out |= (uint32_t)v << shift;
    }
    return out;
}

static void blend_px(uint32_t *d, ft_color s, uint32_t a)
{
    uint32_t dv, ia;
    if (a == 0)
        return;
    if (a >= 255) {
        *d = s | 0xFF000000u;
        return;
    }
    dv = *d;
    ia = 255 - a;
    *d = 0xFF000000u |
         ((mul255((s >> 16) & 0xFFu, a) + mul255((dv >> 16) & 0xFFu, ia)) << 16) |
         ((mul255((s >> 8) & 0xFFu, a) + mul255((dv >> 8) & 0xFFu, ia)) << 8) |
         (mul255(s & 0xFFu, a) + mul255(dv & 0xFFu, ia));
}

/* ---- basics ----------------------------------------------------------- */

void canvas_init(ft_canvas *c, uint32_t *px, int w, int h, int stride)
{
    c->px = px;
    c->w = w;
    c->h = h;
    c->stride = stride;
}

ft_color canvas_get(const ft_canvas *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return 0;
    return c->px[(long)y * c->stride + x];
}

static void px_blend(ft_canvas *c, int x, int y, ft_color color, uint32_t coverage)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return;
    blend_px(&c->px[(long)y * c->stride + x], color, mul255(color >> 24, coverage));
}

static void span(ft_canvas *c, int x0, int x1, int y, ft_color color)
{
    uint32_t a = color >> 24, *row;
    int x;
    if (y < 0 || y >= c->h || a == 0)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > c->w)
        x1 = c->w;
    row = c->px + (long)y * c->stride;
    for (x = x0; x < x1; x++)
        blend_px(&row[x], color, a);
}

void canvas_clear(ft_canvas *c, ft_color color)
{
    int x, y;
    for (y = 0; y < c->h; y++)
        for (x = 0; x < c->w; x++)
            c->px[(long)y * c->stride + x] = color | 0xFF000000u;
}

void canvas_fill(ft_canvas *c, ft_rect r, ft_color color)
{
    int y;
    for (y = r.y; y < r.y + r.h; y++)
        span(c, r.x, r.x + r.w, y, color);
}

void canvas_gradient(ft_canvas *c, ft_rect r, ft_color top, ft_color bottom)
{
    int dy;
    for (dy = 0; dy < r.h; dy++)
        span(c, r.x, r.x + r.w, r.y + dy, r.h > 1 ? color_mix(top, bottom, (unsigned)(dy * 255 / (r.h - 1))) : top);
}

/* ---- anti-aliased rounded rectangles ---------------------------------- */

#define MAX_RADIUS 96
#define MASK_SLOTS 8

typedef struct {
    int radius;
    uint8_t m[MAX_RADIUS * MAX_RADIUS];
} corner_mask;

static corner_mask masks[MASK_SLOTS];
static int next_mask;

/* Coverage of a top-left corner of the given radius, 4x4 supersampled. */
static const uint8_t *corner(int r)
{
    corner_mask *slot;
    int i, x, y, sx, sy;
    long rr = 64L * r * r;
    for (i = 0; i < MASK_SLOTS; i++)
        if (masks[i].radius == r)
            return masks[i].m;
    slot = &masks[next_mask];
    next_mask = (next_mask + 1) % MASK_SLOTS;
    slot->radius = r;
    for (y = 0; y < r; y++)
        for (x = 0; x < r; x++) {
            int count = 0;
            for (sy = 0; sy < 4; sy++)
                for (sx = 0; sx < 4; sx++) {
                    long dx = 8L * r - (8L * x + 2 * sx + 1), dy = 8L * r - (8L * y + 2 * sy + 1);
                    if (dx * dx + dy * dy <= rr)
                        count++;
                }
            slot->m[y * r + x] = (uint8_t)((count * 255 + 8) / 16);
        }
    return slot->m;
}

static int clamp_radius(ft_rect r, int radius)
{
    if (radius * 2 > r.w)
        radius = r.w / 2;
    if (radius * 2 > r.h)
        radius = r.h / 2;
    if (radius > MAX_RADIUS)
        radius = MAX_RADIUS;
    return radius < 0 ? 0 : radius;
}

void canvas_round_gradient(ft_canvas *c, ft_rect r, int radius, unsigned corners, ft_color top, ft_color bottom)
{
    const uint8_t *m;
    int dy, x;
    if (r.w <= 0 || r.h <= 0)
        return;
    radius = clamp_radius(r, radius);
    m = radius > 0 ? corner(radius) : NULL;
    for (dy = 0; dy < r.h; dy++) {
        int y = r.y + dy, x0 = r.x, x1 = r.x + r.w, my = -1;
        unsigned left = 0, right = 0;
        ft_color color;
        if (y < 0 || y >= c->h)
            continue;
        color = r.h > 1 ? color_mix(top, bottom, (unsigned)(dy * 255 / (r.h - 1))) : top;
        if (dy < radius) {
            my = dy;
            left = corners & CORNER_TL;
            right = corners & CORNER_TR;
        } else if (dy >= r.h - radius) {
            my = r.h - 1 - dy;
            left = corners & CORNER_BL;
            right = corners & CORNER_BR;
        }
        if (my >= 0 && left) {
            for (x = 0; x < radius; x++)
                px_blend(c, r.x + x, y, color, m[my * radius + x]);
            x0 += radius;
        }
        if (my >= 0 && right) {
            for (x = 0; x < radius; x++)
                px_blend(c, r.x + r.w - 1 - x, y, color, m[my * radius + x]);
            x1 -= radius;
        }
        span(c, x0, x1, y, color);
    }
}

void canvas_round(ft_canvas *c, ft_rect r, int radius, ft_color color)
{
    canvas_round_gradient(c, r, radius, CORNERS_ALL, color, color);
}

static uint32_t rr_coverage(ft_rect r, int radius, const uint8_t *m, int x, int y)
{
    int dx = x - r.x, dy = y - r.y, mx = -1, my = -1;
    if (dx < 0 || dy < 0 || dx >= r.w || dy >= r.h)
        return 0;
    if (radius <= 0)
        return 255;
    if (dx < radius)
        mx = dx;
    else if (dx >= r.w - radius)
        mx = r.w - 1 - dx;
    if (dy < radius)
        my = dy;
    else if (dy >= r.h - radius)
        my = r.h - 1 - dy;
    return (mx >= 0 && my >= 0) ? m[my * radius + mx] : 255;
}

void canvas_round_stroke(ft_canvas *c, ft_rect r, int radius, int thickness, ft_color color)
{
    ft_rect in = {r.x + thickness, r.y + thickness, r.w - 2 * thickness, r.h - 2 * thickness};
    int ro = clamp_radius(r, radius), ri, x, y;
    const uint8_t *mo, *mi;
    if (r.w <= 0 || r.h <= 0 || thickness <= 0)
        return;
    ri = in.w > 0 && in.h > 0 ? clamp_radius(in, radius - thickness) : 0;
    mo = ro > 0 ? corner(ro) : NULL;
    mi = ri > 0 ? corner(ri) : NULL; /* may evict mo only if MASK_SLOTS < 2 */
    for (y = r.y; y < r.y + r.h; y++) {
        int skip0 = 0, skip1 = 0;
        if (y < 0 || y >= c->h)
            continue;
        if (in.w > 0 && y >= in.y + ri && y < in.y + in.h - ri) {
            skip0 = in.x;
            skip1 = in.x + in.w;
        }
        for (x = r.x; x < r.x + r.w; x++) {
            uint32_t co, ci;
            if (x >= skip0 && x < skip1)
                continue;
            co = rr_coverage(r, ro, mo, x, y);
            ci = (in.w > 0 && in.h > 0) ? rr_coverage(in, ri, mi, x, y) : 0;
            if (co > ci)
                px_blend(c, x, y, color, co - ci);
        }
    }
}

void canvas_circle(ft_canvas *c, int cx, int cy, int radius, ft_color color)
{
    ft_rect r = {cx - radius, cy - radius, 2 * radius, 2 * radius};
    canvas_round(c, r, radius, color);
}

/* ---- supersampled shapes ---------------------------------------------- */

typedef int (*inside_fn)(long sx8, long sy8, const void *ctx); /* coordinates in 1/8 pixel */

static void shape(ft_canvas *c, ft_rect box, inside_fn inside, const void *ctx, ft_color color)
{
    int x, y, i, j;
    int x0 = box.x < 0 ? 0 : box.x, y0 = box.y < 0 ? 0 : box.y;
    int x1 = box.x + box.w > c->w ? c->w : box.x + box.w;
    int y1 = box.y + box.h > c->h ? c->h : box.y + box.h;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            int count = 0;
            for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                    count += inside(8L * x + 2 * i + 1, 8L * y + 2 * j + 1, ctx);
            if (count)
                px_blend(c, x, y, color, (uint32_t)((count * 255 + 8) / 16));
        }
}

typedef struct {
    long cx, cy, rin2, rout2;
} ring_ctx;

static int in_ring(long sx, long sy, const void *p)
{
    const ring_ctx *r = (const ring_ctx *)p;
    long dx = sx - r->cx, dy = sy - r->cy, d2 = dx * dx + dy * dy;
    return d2 <= r->rout2 && d2 >= r->rin2;
}

void canvas_ring(ft_canvas *c, int cx, int cy, int radius, int thickness, ft_color color)
{
    ring_ctx r;
    ft_rect box = {cx - radius - 1, cy - radius - 1, 2 * radius + 2, 2 * radius + 2};
    long ri = radius - thickness < 0 ? 0 : radius - thickness;
    r.cx = 8L * cx;
    r.cy = 8L * cy;
    r.rout2 = 64L * radius * radius;
    r.rin2 = 64L * ri * ri;
    shape(c, box, in_ring, &r, color);
}

typedef struct {
    long x0, y0, x1, y1, r2;
} line_ctx;

static int in_line(long px, long py, const void *p)
{
    const line_ctx *l = (const line_ctx *)p;
    long vx = l->x1 - l->x0, vy = l->y1 - l->y0, wx = px - l->x0, wy = py - l->y0;
    long len2 = vx * vx + vy * vy, t = wx * vx + wy * vy;
    if (len2 == 0 || t <= 0)
        return wx * wx + wy * wy <= l->r2;
    if (t >= len2) {
        long ex = px - l->x1, ey = py - l->y1;
        return ex * ex + ey * ey <= l->r2;
    }
    {
        long long cross = (long long)wx * vy - (long long)wy * vx;
        return cross * cross <= (long long)l->r2 * len2;
    }
}

void canvas_line(ft_canvas *c, int x0, int y0, int x1, int y1, int thickness, ft_color color)
{
    line_ctx l;
    int pad = thickness / 2 + 2;
    ft_rect box;
    box.x = (x0 < x1 ? x0 : x1) - pad;
    box.y = (y0 < y1 ? y0 : y1) - pad;
    box.w = (x0 > x1 ? x0 - x1 : x1 - x0) + 2 * pad + 1;
    box.h = (y0 > y1 ? y0 - y1 : y1 - y0) + 2 * pad + 1;
    l.x0 = 8L * x0 + 4;
    l.y0 = 8L * y0 + 4;
    l.x1 = 8L * x1 + 4;
    l.y1 = 8L * y1 + 4;
    l.r2 = 16L * thickness * thickness; /* (thickness / 2 * 8)^2 */
    shape(c, box, in_line, &l, color);
}

#define MAX_POLY 16

typedef struct {
    long x[MAX_POLY], y[MAX_POLY];
    int n, sign;
    long clip_cx, clip_cy, clip_r2; /* clip_r2 == 0: no circular clip */
} poly_ctx;

static int in_poly(long px, long py, const void *p)
{
    const poly_ctx *g = (const poly_ctx *)p;
    int i;
    if (g->clip_r2) {
        long dx = px - g->clip_cx, dy = py - g->clip_cy;
        if (dx * dx + dy * dy > g->clip_r2)
            return 0;
    }
    for (i = 0; i < g->n; i++) {
        int j = (i + 1) % g->n;
        long cross = (g->x[j] - g->x[i]) * (py - g->y[i]) - (g->y[j] - g->y[i]) * (px - g->x[i]);
        if (cross * g->sign < 0)
            return 0;
    }
    return 1;
}

static void polygon(ft_canvas *c, const int *xy, int npoints, long clip_cx, long clip_cy, long clip_r2,
                    ft_color color)
{
    poly_ctx g;
    ft_rect box;
    int i, minx, miny, maxx, maxy;
    long area = 0;
    if (npoints < 3 || npoints > MAX_POLY)
        return;
    minx = maxx = xy[0];
    miny = maxy = xy[1];
    for (i = 0; i < npoints; i++) {
        g.x[i] = 8L * xy[2 * i];
        g.y[i] = 8L * xy[2 * i + 1];
        if (xy[2 * i] < minx) minx = xy[2 * i];
        if (xy[2 * i] > maxx) maxx = xy[2 * i];
        if (xy[2 * i + 1] < miny) miny = xy[2 * i + 1];
        if (xy[2 * i + 1] > maxy) maxy = xy[2 * i + 1];
    }
    for (i = 0; i < npoints; i++) {
        int j = (i + 1) % npoints;
        area += g.x[i] * g.y[j] - g.x[j] * g.y[i];
    }
    g.n = npoints;
    g.sign = area >= 0 ? 1 : -1;
    g.clip_cx = clip_cx;
    g.clip_cy = clip_cy;
    g.clip_r2 = clip_r2;
    box.x = minx - 1;
    box.y = miny - 1;
    box.w = maxx - minx + 2;
    box.h = maxy - miny + 2;
    shape(c, box, in_poly, &g, color);
}

void canvas_polygon(ft_canvas *c, const int *xy, int npoints, ft_color color)
{
    polygon(c, xy, npoints, 0, 0, 0, color);
}

void canvas_polygon_in_circle(ft_canvas *c, const int *xy, int npoints, int cx, int cy, int radius, ft_color color)
{
    polygon(c, xy, npoints, 8L * cx, 8L * cy, 64L * radius * radius, color);
}

void canvas_chevron(ft_canvas *c, int cx, int cy, int half_w, int thickness, int up, ft_color color)
{
    int dy = up ? -half_w / 2 : half_w / 2;
    canvas_line(c, cx - half_w, cy - dy, cx, cy + dy, thickness, color);
    canvas_line(c, cx, cy + dy, cx + half_w, cy - dy, thickness, color);
}

/* ---- shadows and glows ------------------------------------------------ */

static uint32_t ramp(int d, int blur)
{
    long t;
    if (d < 0)
        return 0;
    if (d >= 2 * blur)
        return 255;
    t = (2L * d + 1) * 256 / (4L * blur); /* 0..256 across the falloff */
    return (uint32_t)(t * t * (768 - 2 * t) / 65536 * 255 / 256);
}

#define SHADOW_MAX 4096
static uint8_t shadow_fx[SHADOW_MAX];

void canvas_shadow(ft_canvas *c, ft_rect r, int blur, ft_color color)
{
    int x, y, bx, by, bw, bh;
    uint32_t a = color >> 24;
    if (blur < 1)
        blur = 1;
    bx = r.x - blur;
    by = r.y - blur;
    bw = r.w + 2 * blur;
    bh = r.h + 2 * blur;
    if (bw > SHADOW_MAX || bw <= 0 || bh <= 0)
        return;
    for (x = 0; x < bw; x++) {
        uint32_t l = ramp(x, blur), rt = ramp(bw - 1 - x, blur);
        shadow_fx[x] = (uint8_t)(l < rt ? l : rt);
    }
    for (y = 0; y < bh; y++) {
        uint32_t t = ramp(y, blur), b = ramp(bh - 1 - y, blur), fy = t < b ? t : b;
        int py = by + y;
        if (py < 0 || py >= c->h || fy == 0)
            continue;
        for (x = 0; x < bw; x++) {
            int px = bx + x;
            if (px < 0 || px >= c->w)
                continue;
            blend_px(&c->px[(long)py * c->stride + px], color, mul255(a, mul255(shadow_fx[x], fy)));
        }
    }
}

static long glow_q[SHADOW_MAX];

void canvas_glow(ft_canvas *c, int cx, int cy, int rx, int ry, ft_color color)
{
    int x, y, x0, x1, y0, y1;
    uint32_t a = color >> 24;
    if (rx <= 0 || ry <= 0)
        return;
    x0 = cx - rx < 0 ? 0 : cx - rx;
    x1 = cx + rx > c->w ? c->w : cx + rx;
    y0 = cy - ry < 0 ? 0 : cy - ry;
    y1 = cy + ry > c->h ? c->h : cy + ry;
    if (x1 - x0 > SHADOW_MAX)
        return;
    for (x = x0; x < x1; x++) {
        long d = 2L * (x - cx) + 1;
        glow_q[x - x0] = d * d * 65536 / (4L * rx * rx);
    }
    for (y = y0; y < y1; y++) {
        long d = 2L * (y - cy) + 1, qy = d * d * 65536 / (4L * ry * ry);
        for (x = x0; x < x1; x++) {
            long q = qy + glow_q[x - x0], t, f;
            if (q >= 65536)
                continue;
            t = 65536 - q;
            f = (t * t) >> 16;
            blend_px(&c->px[(long)y * c->stride + x], color, (uint32_t)((a * (uint32_t)f) >> 16));
        }
    }
}

void canvas_glow_mask(uint8_t *mask, int w, int h, int cx, int cy, int rx, int ry, unsigned alpha)
{
    int x, y, x0, x1, y0, y1;
    if (rx <= 0 || ry <= 0)
        return;
    x0 = cx - rx < 0 ? 0 : cx - rx;
    x1 = cx + rx > w ? w : cx + rx;
    y0 = cy - ry < 0 ? 0 : cy - ry;
    y1 = cy + ry > h ? h : cy + ry;
    if (x1 - x0 > SHADOW_MAX)
        return;
    for (x = x0; x < x1; x++) {
        long d = 2L * (x - cx) + 1;
        glow_q[x - x0] = d * d * 65536 / (4L * rx * rx);
    }
    for (y = y0; y < y1; y++) {
        long d = 2L * (y - cy) + 1, qy = d * d * 65536 / (4L * ry * ry);
        for (x = x0; x < x1; x++) {
            long q = qy + glow_q[x - x0], t;
            uint32_t a, m;
            if (q >= 65536)
                continue;
            t = 65536 - q;
            a = (uint32_t)((alpha * (uint32_t)((t * t) >> 16)) >> 16);
            m = mask[(long)y * w + x];
            mask[(long)y * w + x] = (uint8_t)(m + a - mul255(m, a));
        }
    }
}

/* ---- text --------------------------------------------------------------- */

static const ft_glyph *glyph_of(const ft_font *f, unsigned char ch)
{
    int index = (ch >= f->first && ch < f->first + f->count) ? ch - f->first : '?' - f->first;
    return &f->glyphs[index];
}

static int kern(const ft_font *f, unsigned char a, unsigned char b)
{
    unsigned pair = ((unsigned)a << 8) | b;
    int lo = 0, hi = f->nkerns - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->kerns[mid].pair == pair)
            return f->kerns[mid].adjust;
        if (f->kerns[mid].pair < pair)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

static long advance_q6(const ft_font *f, const char *text, size_t n)
{
    long pen = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        pen += glyph_of(f, (unsigned char)text[i])->advance;
        if (i + 1 < n)
            pen += kern(f, (unsigned char)text[i], (unsigned char)text[i + 1]);
    }
    return pen;
}

int text_width(const ft_font *f, const char *text)
{
    return (int)((advance_q6(f, text, strlen(text)) + 32) >> 6);
}

static void draw_glyph(ft_canvas *c, const ft_glyph *g, const uint8_t *alpha, int x, int baseline, ft_color color)
{
    int row, col, stride = (g->w + 1) / 2;
    int ox = x + g->x0, oy = baseline + g->y0;
    const uint8_t *bits = alpha + g->offset;
    if (ox >= c->w || oy >= c->h || ox + g->w <= 0 || oy + g->h <= 0)
        return;
    for (row = 0; row < g->h; row++)
        for (col = 0; col < g->w; col++) {
            uint8_t b = bits[row * stride + col / 2];
            uint32_t a4 = (col & 1) ? (b & 0x0Fu) : (uint32_t)(b >> 4);
            if (a4)
                px_blend(c, ox + col, oy + row, color, a4 * 17);
        }
}

static int draw_run(ft_canvas *c, const ft_font *f, int x, int y, const char *text, size_t n, ft_color color)
{
    long pen = (long)x * 64;
    int baseline = y + f->ascent;
    size_t i;
    for (i = 0; i < n; i++) {
        const ft_glyph *g = glyph_of(f, (unsigned char)text[i]);
        draw_glyph(c, g, f->alpha, (int)((pen + 32) >> 6), baseline, color);
        pen += g->advance;
        if (i + 1 < n)
            pen += kern(f, (unsigned char)text[i], (unsigned char)text[i + 1]);
    }
    return (int)((pen + 32) >> 6);
}

int canvas_text(ft_canvas *c, const ft_font *f, int x, int y, const char *text, ft_color color)
{
    return draw_run(c, f, x, y, text, strlen(text), color);
}

void canvas_text_fit(ft_canvas *c, const ft_font *f, int x, int y, int max_w, const char *text, ft_color color)
{
    size_t n = strlen(text);
    long limit = (long)max_w * 64, dots = advance_q6(f, "...", 3);
    if (advance_q6(f, text, n) <= limit) {
        draw_run(c, f, x, y, text, n, color);
        return;
    }
    while (n > 0 && advance_q6(f, text, n) + dots > limit)
        n--;
    while (n > 0 && text[n - 1] == ' ')
        n--;
    x = draw_run(c, f, x, y, text, n, color);
    if (dots <= limit)
        draw_run(c, f, x, y, "...", 3, color);
}

int canvas_text_wrap(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color)
{
    int lines = 0, max_lines = r.h / f->line_height;
    long limit = (long)r.w * 64;
    const char *p = text;
    while (*p && lines < max_lines) {
        size_t take = 0, end;
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        /* Grow the line word by word while it fits. */
        for (end = 0; p[end]; end++) {
            if (p[end + 1] == ' ' || p[end + 1] == '\0') {
                if (advance_q6(f, p, end + 1) <= limit)
                    take = end + 1;
                else
                    break;
            }
        }
        if (take == 0) /* a single word longer than the line */
            take = strlen(p);
        if (lines == max_lines - 1 && p[take] != '\0') {
            canvas_text_fit(c, f, r.x, r.y + lines * f->line_height, r.w, p, color);
            return lines + 1;
        }
        if (advance_q6(f, p, take) > limit) {
            char word[256];
            size_t n = take < sizeof word - 1 ? take : sizeof word - 1;
            memcpy(word, p, n);
            word[n] = '\0';
            canvas_text_fit(c, f, r.x, r.y + lines * f->line_height, r.w, word, color);
        } else {
            draw_run(c, f, r.x, r.y + lines * f->line_height, p, take, color);
        }
        p += take;
        lines++;
    }
    return lines;
}

void canvas_text_center(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color)
{
    int w = text_width(f, text), y = r.y + (r.h - f->line_height) / 2;
    if (w > r.w)
        canvas_text_fit(c, f, r.x, y, r.w, text, color);
    else
        canvas_text(c, f, r.x + (r.w - w) / 2, y, text, color);
}

void canvas_text_right(ft_canvas *c, const ft_font *f, int right_x, int y, const char *text, ft_color color)
{
    canvas_text(c, f, right_x - text_width(f, text), y, text, color);
}

uint32_t canvas_hash(const ft_canvas *c)
{
    uint32_t h = 2166136261u;
    int x, y, b;
    for (y = 0; y < c->h; y++)
        for (x = 0; x < c->w; x++) {
            uint32_t v = c->px[(long)y * c->stride + x];
            for (b = 0; b < 4; b++) {
                h ^= (v >> (8 * b)) & 0xFFu;
                h *= 16777619u;
            }
        }
    return h;
}
