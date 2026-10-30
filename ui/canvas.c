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
    c->scale = CANVAS_1X;
}

void canvas_set_scale(ft_canvas *c, int scale)
{
    c->scale = scale >= CANVAS_1X ? scale : CANVAS_1X;
}

/* Logical to physical: v * scale / 2, rounding down (also below zero), so
 * shapes that touch in logical units still touch on screen. */
static int ps(const ft_canvas *c, int v)
{
    long t = (long)v * c->scale;
    return (int)(t >= 0 ? t / 2 : -((-t + 1) / 2));
}

static ft_rect pr(const ft_canvas *c, ft_rect r)
{
    ft_rect o;
    o.x = ps(c, r.x);
    o.y = ps(c, r.y);
    o.w = ps(c, r.x + r.w) - o.x;
    o.h = ps(c, r.y + r.h) - o.y;
    return o;
}

/* Lengths (radii, thickness, blur): never shrink a visible line to 0. */
static int pl(const ft_canvas *c, int v)
{
    int p = ps(c, v);
    return v > 0 && p < 1 ? 1 : p;
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

static void canvas_fill_px(ft_canvas *c, ft_rect r, ft_color color)
{
    int y;
    for (y = r.y; y < r.y + r.h; y++)
        span(c, r.x, r.x + r.w, y, color);
}

static void canvas_gradient_px(ft_canvas *c, ft_rect r, ft_color top, ft_color bottom)
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

static void canvas_round_gradient_px(ft_canvas *c, ft_rect r, int radius, unsigned corners, ft_color top, ft_color bottom)
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

static void canvas_round_px(ft_canvas *c, ft_rect r, int radius, ft_color color)
{
    canvas_round_gradient_px(c, r, radius, CORNERS_ALL, color, color);
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

static void canvas_round_stroke_px(ft_canvas *c, ft_rect r, int radius, int thickness, ft_color color)
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

static void canvas_circle_px(ft_canvas *c, int cx, int cy, int radius, ft_color color)
{
    ft_rect r = {cx - radius, cy - radius, 2 * radius, 2 * radius};
    canvas_round_px(c, r, radius, color);
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

static void canvas_ring_px(ft_canvas *c, int cx, int cy, int radius, int thickness, ft_color color)
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

static void canvas_line_px(ft_canvas *c, int x0, int y0, int x1, int y1, int thickness, ft_color color)
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

static void canvas_polygon_px(ft_canvas *c, const int *xy, int npoints, ft_color color)
{
    polygon(c, xy, npoints, 0, 0, 0, color);
}

static void canvas_polygon_in_circle_px(ft_canvas *c, const int *xy, int npoints, int cx, int cy, int radius, ft_color color)
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

#define SHADOW_MAX 8192 /* a full-width shadow or glow at 3x (4K) */
static uint8_t shadow_fx[SHADOW_MAX];

/* skip: physical pixels something opaque will cover next (w == 0: none). */
static void canvas_shadow_px(ft_canvas *c, ft_rect r, int blur, ft_color color, ft_rect skip)
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
        int py = by + y, hidden = skip.w > 0 && py >= skip.y && py < skip.y + skip.h;
        if (py < 0 || py >= c->h || fy == 0)
            continue;
        for (x = 0; x < bw; x++) {
            int px = bx + x;
            if (px < 0 || px >= c->w)
                continue;
            if (hidden && px >= skip.x && px < skip.x + skip.w) {
                x = skip.x + skip.w - bx - 1; /* jump over the covered span */
                continue;
            }
            blend_px(&c->px[(long)py * c->stride + px], color, mul255(a, mul255(shadow_fx[x], fy)));
        }
    }
}

/* 64-bit maths: long is 32 bits on Windows and d*d*65536 overflows it. */
static int32_t glow_q[SHADOW_MAX];

static void canvas_glow_px(ft_canvas *c, int cx, int cy, int rx, int ry, ft_color color)
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
        int64_t d = 2 * (int64_t)(x - cx) + 1;
        glow_q[x - x0] = (int32_t)(d * d * 65536 / (4 * (int64_t)rx * rx));
    }
    for (y = y0; y < y1; y++) {
        int64_t d = 2 * (int64_t)(y - cy) + 1, qy = d * d * 65536 / (4 * (int64_t)ry * ry);
        for (x = x0; x < x1; x++) {
            int64_t q = qy + glow_q[x - x0], t, f;
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
        int64_t d = 2 * (int64_t)(x - cx) + 1;
        glow_q[x - x0] = (int32_t)(d * d * 65536 / (4 * (int64_t)rx * rx));
    }
    for (y = y0; y < y1; y++) {
        int64_t d = 2 * (int64_t)(y - cy) + 1, qy = d * d * 65536 / (4 * (int64_t)ry * ry);
        for (x = x0; x < x1; x++) {
            int64_t q = qy + glow_q[x - x0], t;
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


/* ---- logical-coordinate entry points ------------------------------------ */

void canvas_fill(ft_canvas *c, ft_rect r, ft_color color)
{
    canvas_fill_px(c, pr(c, r), color);
}

void canvas_gradient(ft_canvas *c, ft_rect r, ft_color top, ft_color bottom)
{
    canvas_gradient_px(c, pr(c, r), top, bottom);
}

void canvas_round(ft_canvas *c, ft_rect r, int radius, ft_color color)
{
    canvas_round_px(c, pr(c, r), pl(c, radius), color);
}

void canvas_round_gradient(ft_canvas *c, ft_rect r, int radius, unsigned corners, ft_color top, ft_color bottom)
{
    canvas_round_gradient_px(c, pr(c, r), pl(c, radius), corners, top, bottom);
}

void canvas_round_stroke(ft_canvas *c, ft_rect r, int radius, int thickness, ft_color color)
{
    canvas_round_stroke_px(c, pr(c, r), pl(c, radius), pl(c, thickness), color);
}

void canvas_circle(ft_canvas *c, int cx, int cy, int radius, ft_color color)
{
    canvas_circle_px(c, ps(c, cx), ps(c, cy), pl(c, radius), color);
}

void canvas_ring(ft_canvas *c, int cx, int cy, int radius, int thickness, ft_color color)
{
    canvas_ring_px(c, ps(c, cx), ps(c, cy), pl(c, radius), pl(c, thickness), color);
}

void canvas_line(ft_canvas *c, int x0, int y0, int x1, int y1, int thickness, ft_color color)
{
    canvas_line_px(c, ps(c, x0), ps(c, y0), ps(c, x1), ps(c, y1), pl(c, thickness), color);
}

static int scale_points(const ft_canvas *c, const int *xy, int npoints, int *out)
{
    int i;
    if (npoints < 0 || npoints > MAX_POLY)
        return 0;
    for (i = 0; i < 2 * npoints; i++)
        out[i] = ps(c, xy[i]);
    return npoints;
}

void canvas_polygon(ft_canvas *c, const int *xy, int npoints, ft_color color)
{
    int p[2 * MAX_POLY];
    canvas_polygon_px(c, p, scale_points(c, xy, npoints, p), color);
}

void canvas_polygon_in_circle(ft_canvas *c, const int *xy, int npoints, int cx, int cy, int radius, ft_color color)
{
    int p[2 * MAX_POLY];
    canvas_polygon_in_circle_px(c, p, scale_points(c, xy, npoints, p), ps(c, cx), ps(c, cy), pl(c, radius), color);
}

void canvas_shadow(ft_canvas *c, ft_rect r, int blur, ft_color color)
{
    ft_rect none = {0, 0, 0, 0};
    canvas_shadow_px(c, pr(c, r), pl(c, blur), color, none);
}

void canvas_shadow_under(ft_canvas *c, ft_rect r, int blur, ft_color color, ft_rect covered)
{
    canvas_shadow_px(c, pr(c, r), pl(c, blur), color, pr(c, covered));
}

void canvas_glow(ft_canvas *c, int cx, int cy, int rx, int ry, ft_color color)
{
    canvas_glow_px(c, ps(c, cx), ps(c, cy), pl(c, rx), pl(c, ry), color);
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

/* Pens advance by the 1x metrics, so text measures the same at every
 * scale; each glyph is drawn from the scaled font at the scaled pen. */
static int draw_run(ft_canvas *c, const ft_font *f, int x, int y, const char *text, size_t n, ft_color color)
{
    const ft_font *fp = font_scaled(f, c->scale);
    long pen = (long)x * 64;
    int baseline = ps(c, y) + (fp != f ? fp->ascent : ps(c, f->ascent));
    size_t i;
    for (i = 0; i < n; i++) {
        const ft_glyph *g = glyph_of(f, (unsigned char)text[i]);
        long px = pen * c->scale / 2;
        draw_glyph(c, glyph_of(fp, (unsigned char)text[i]), fp->alpha, (int)((px + 32) >> 6), baseline, color);
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

int canvas_clip(const ft_canvas *c, ft_rect r, ft_canvas *out, int *ox, int *oy)
{
    int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
    int px0 = x0 * c->scale / 2, py0 = y0 * c->scale / 2;
    int px1 = (r.x + r.w) * c->scale / 2, py1 = (r.y + r.h) * c->scale / 2;
    if (px1 > c->w)
        px1 = c->w;
    if (py1 > c->h)
        py1 = c->h;
    if (px0 >= px1 || py0 >= py1)
        return 0;
    *out = *c;
    out->px = c->px + (long)py0 * c->stride + px0;
    out->w = px1 - px0;
    out->h = py1 - py0;
    *ox = x0;
    *oy = y0;
    return 1;
}

/* Small angles only: no maths library needed. */
static double small_sin(double a)
{
    return a - a * a * a / 6 + a * a * a * a * a / 120;
}

static double small_cos(double a)
{
    return 1 - a * a / 2 + a * a * a * a / 24;
}

/* Coverage of a pixel at (x, y) by a w x h rounded rectangle: 0..256. */
static int rounded_cover(double x, double y, int w, int h, int radius)
{
    double cx = x < radius ? radius : x > w - radius ? w - radius : x;
    double cy = y < radius ? radius : y > h - radius ? h - radius : y;
    double dx = x - cx, dy = y - cy, d, edge;
    if (x < 0 || y < 0 || x > w || y > h)
        return 0;
    if (dx == 0 || dy == 0) { /* straight edges: distance to the border */
        edge = x < y ? x : y;
        if (w - x < edge)
            edge = w - x;
        if (h - y < edge)
            edge = h - y;
        return edge >= 1 ? 256 : (int)(edge * 256);
    }
    d = dx * dx + dy * dy;
    if (d <= (radius - 1.0) * (radius - 1.0))
        return 256;
    if (d >= (double)radius * radius)
        return 0;
    /* within a pixel of the corner's arc: the distance decides, by Newton */
    {
        double r = radius - 0.5;
        r = (r + d / r) / 2;
        r = (r + d / r) / 2;
        edge = radius - r;
        return edge <= 0 ? 0 : (int)(edge * 256);
    }
}

void canvas_turn(ft_canvas *c, const ft_canvas *src, int x, int y, int radius, int turn_x, int turn_y)
{
    double ax = turn_x / 1000.0, ay = turn_y / 1000.0, sx = small_sin(ax), cxs = small_cos(ax);
    double sy = small_sin(ay), cys = small_cos(ay), w = src->w, h = src->h, dist = 2.6 * (w > h ? w : h);
    double qx[4], qy[4], a, b, cc, d, e, f, g, hh, A, B, C, D, E, F, G, H, I;
    int i, px, py, x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    /* The corners turned about the centre, then seen from dist away. */
    for (i = 0; i < 4; i++) {
        double u = (i == 1 || i == 2) ? w / 2 : -w / 2, v = i >= 2 ? h / 2 : -h / 2;
        double x1r = u * cys, z1 = -u * sy;           /* about the vertical axis */
        double y2 = v * cxs - z1 * sx, z2 = v * sx + z1 * cxs; /* about the horizontal axis */
        double k = dist / (dist + z2);
        qx[i] = x + w / 2 + x1r * k;
        qy[i] = y + h / 2 + y2 * k;
        if ((int)qx[i] < x0)
            x0 = (int)qx[i];
        if ((int)qy[i] < y0)
            y0 = (int)qy[i];
        if ((int)qx[i] + 1 > x1)
            x1 = (int)qx[i] + 1;
        if ((int)qy[i] + 1 > y1)
            y1 = (int)qy[i] + 1;
    }
    { /* the unit square onto that quad (Heckbert), then inverted */
        double sxq = qx[0] - qx[1] + qx[2] - qx[3], syq = qy[0] - qy[1] + qy[2] - qy[3];
        double dx1 = qx[1] - qx[2], dx2 = qx[3] - qx[2], dy1 = qy[1] - qy[2], dy2 = qy[3] - qy[2];
        double den = dx1 * dy2 - dx2 * dy1;
        if (den == 0)
            return;
        g = (sxq * dy2 - dx2 * syq) / den;
        hh = (dx1 * syq - sxq * dy1) / den;
        a = qx[1] - qx[0] + g * qx[1];
        b = qx[3] - qx[0] + hh * qx[3];
        cc = qx[0];
        d = qy[1] - qy[0] + g * qy[1];
        e = qy[3] - qy[0] + hh * qy[3];
        f = qy[0];
        A = e - f * hh;
        B = cc * hh - b;
        C = b * f - cc * e;
        D = f * g - d;
        E = a - cc * g;
        F = cc * d - a * f;
        G = d * hh - e * g;
        H = b * g - a * hh;
        I = a * e - b * d;
    }
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > c->w)
        x1 = c->w;
    if (y1 > c->h)
        y1 = c->h;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (long)py * c->stride;
        for (px = x0; px < x1; px++) {
            double fx = px + 0.5, fy = py + 0.5, q = G * fx + H * fy + I, u, v, su, sv;
            int cover, ix, iy, tx, ty, ch;
            uint32_t p00, p10, p01, p11, out = 0;
            if (q == 0)
                continue;
            u = (A * fx + B * fy + C) / q;
            v = (D * fx + E * fy + F) / q;
            if (u < 0 || u > 1 || v < 0 || v > 1)
                continue;
            su = u * w;
            sv = v * h;
            cover = rounded_cover(su, sv, src->w, src->h, radius);
            if (cover <= 0)
                continue;
            su -= 0.5;
            sv -= 0.5;
            ix = su < 0 ? 0 : (int)su;
            iy = sv < 0 ? 0 : (int)sv;
            if (ix > src->w - 2)
                ix = src->w - 2;
            if (iy > src->h - 2)
                iy = src->h - 2;
            tx = (int)((su - ix) * 256);
            ty = (int)((sv - iy) * 256);
            tx = tx < 0 ? 0 : tx > 256 ? 256 : tx;
            ty = ty < 0 ? 0 : ty > 256 ? 256 : ty;
            p00 = src->px[(long)iy * src->stride + ix];
            p10 = src->px[(long)iy * src->stride + ix + 1];
            p01 = src->px[(long)(iy + 1) * src->stride + ix];
            p11 = src->px[(long)(iy + 1) * src->stride + ix + 1];
            for (ch = 0; ch < 24; ch += 8) {
                int top = (int)((p00 >> ch) & 0xFF) * (256 - tx) + (int)((p10 >> ch) & 0xFF) * tx;
                int bot = (int)((p01 >> ch) & 0xFF) * (256 - tx) + (int)((p11 >> ch) & 0xFF) * tx;
                int s = (top * (256 - ty) + bot * ty) >> 16, dv = (int)((row[px] >> ch) & 0xFF);
                out |= (uint32_t)(dv + (s - dv) * cover / 256) << ch;
            }
            row[px] = 0xFF000000u | out;
        }
    }
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
