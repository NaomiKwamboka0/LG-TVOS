#include "art.h"

#include "theme.h"

static const struct {
    ft_color top, bottom;
} GRADIENTS[ART_COUNT] = {
    {FT_RGB(0x5B95FF), FT_RGB(0x2440B0)}, /* Live TV */
    {FT_RGB(0x36D27D), FT_RGB(0x0D7641)}, /* Football */
    {FT_RGB(0xFF9F43), FT_RGB(0xE0326F)}, /* Games */
    {FT_RGB(0x8590A3), FT_RGB(0x394152)}, /* More */
    {FT_RGB(0x95A1B6), FT_RGB(0x414B5F)}, /* Settings */
    {FT_RGB(0x34D6C3), FT_RGB(0x12708A)}, /* Racer */
    {FT_RGB(0x4CCB74), FT_RGB(0x17663A)}, /* Penalty */
    {FT_RGB(0x96D7FF), FT_RGB(0x3A86D4)}, /* Garden */
    {FT_RGB(0xB792FF), FT_RGB(0x5A3BC2)}, /* Profile */
    {FT_RGB(0x60A0FF), FT_RGB(0x2E4FC6)}, /* Channels */
    {FT_RGB(0x8590A3), FT_RGB(0x394152)}, /* Gear */
    {FT_RGB(0x4FC3F7), FT_RGB(0x1565C0)}, /* Info */
    {FT_RGB(0x8590A3), FT_RGB(0x394152)}, /* Volume */
    {FT_RGB(0xFF5252), FT_RGB(0xB3122E)}, /* Play */
    {FT_RGB(0x8590A3), FT_RGB(0x394152)}, /* Search */
    {FT_RGB(0xFF3B3B), FT_RGB(0xB3001B)}, /* YouTube */
    {FT_RGB(0xE50914), FT_RGB(0x8A050C)}, /* Netflix */
};

/* cos and sin of k * 22.5 degrees, times 1000 (gear teeth). */
static const int COS16[16] = {1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924};
static const int SIN16[16] = {0, 383, 707, 924, 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383};

static const int PENT_COS[5] = {0, 951, 588, -588, -951};
static const int PENT_SIN[5] = {-1000, -309, 809, 809, -309};

#define P(v, pct) ((v) * (pct) / 100)

ft_color art_accent(art_id id)
{
    return id < ART_COUNT ? GRADIENTS[id].top : COL_TEXT;
}

static void tv(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int t = s / 7 < 2 ? 2 : s / 7;
    ft_rect screen = {cx - P(s, 85), cy - P(s, 50), P(s, 170), P(s, 105)};
    canvas_round_stroke(c, screen, s / 5, t, ink);
    canvas_line(c, cx - s / 3, cy + P(s, 74), cx + s / 3, cy + P(s, 74), t, ink);
    canvas_line(c, cx, screen.y - 1, cx - s / 3, screen.y - s / 2, t * 2 / 3 < 2 ? 2 : t * 2 / 3, ink);
    canvas_line(c, cx, screen.y - 1, cx + s / 3, screen.y - s / 2, t * 2 / 3 < 2 ? 2 : t * 2 / 3, ink);
    canvas_circle(c, screen.x + screen.w - s / 3, screen.y + s / 3, s / 9 < 2 ? 2 : s / 9, COL_BAD);
}

static void pentagon(ft_canvas *c, int cx, int cy, int r, ft_color color)
{
    int xy[10], i;
    for (i = 0; i < 5; i++) {
        xy[2 * i] = cx + PENT_COS[i] * r / 1000;
        xy[2 * i + 1] = cy + PENT_SIN[i] * r / 1000;
    }
    canvas_polygon(c, xy, 5, color);
}

/* Pentagon whose first corner points along the unit direction (dx, dy) / 1000,
 * clipped to the ball outline. */
static void patch(ft_canvas *c, int px, int py, int r, int dx, int dy, int bx, int by, int br, ft_color color)
{
    static const int CK[5] = {1000, 309, -809, -809, 309};
    static const int SK[5] = {0, 951, 588, -588, -951};
    int xy[10], k;
    for (k = 0; k < 5; k++) {
        long vx = (long)dx * CK[k] - (long)dy * SK[k], vy = (long)dy * CK[k] + (long)dx * SK[k];
        xy[2 * k] = px + (int)(vx * r / 1000000L);
        xy[2 * k + 1] = py + (int)(vy * r / 1000000L);
    }
    canvas_polygon_in_circle(c, xy, 5, bx, by, br, color);
}

/* Classic ball: a centre patch and five outer patches cut off by the edge,
 * joined by thin seams. */
static void ball(ft_canvas *c, int cx, int cy, int r, ft_color white, ft_color dark)
{
    int i, core = P(r, 36), t = r / 18 < 1 ? 1 : r / 18;
    canvas_circle(c, cx, cy, r, white);
    pentagon(c, cx, cy, core, dark);
    for (i = 0; i < 5; i++) {
        /* Outer patches sit between the centre patch's corners. */
        int dx = -PENT_COS[(i + 3) % 5], dy = -PENT_SIN[(i + 3) % 5];
        int px = cx + dx * P(r, 98) / 1000, py = cy + dy * P(r, 98) / 1000;
        patch(c, px, py, P(r, 36), -dx, -dy, cx, cy, r, dark);
        canvas_line(c, cx + PENT_COS[i] * core / 1000, cy + PENT_SIN[i] * core / 1000,
                    cx + PENT_COS[i] * P(r, 66) / 1000, cy + PENT_SIN[i] * P(r, 66) / 1000, t, dark);
    }
}

static void controller(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    ft_color dark = FT_RGB(0x3A1D3A);
    ft_rect body = {cx - s, cy - P(s, 42), 2 * s, P(s, 88)};
    ft_rect pad_h = {cx - P(s, 76), cy - P(s, 7), P(s, 42), P(s, 14)};
    ft_rect pad_v = {cx - P(s, 62), cy - P(s, 21), P(s, 14), P(s, 42)};
    canvas_round(c, body, P(s, 42), ink);
    canvas_circle(c, cx - P(s, 62), cy + P(s, 30), P(s, 38), ink);
    canvas_circle(c, cx + P(s, 62), cy + P(s, 30), P(s, 38), ink);
    canvas_round(c, pad_h, 2, dark);
    canvas_round(c, pad_v, 2, dark);
    canvas_circle(c, cx + P(s, 48), cy - P(s, 8), P(s, 10), dark);
    canvas_circle(c, cx + P(s, 70), cy + P(s, 10), P(s, 10), dark);
}

static void padlock(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    ft_color dark = FT_RGB(0x2C3443);
    ft_rect body = {cx - P(s, 62), cy - P(s, 18), P(s, 124), P(s, 88)};
    ft_rect slot = {cx - P(s, 5), cy + P(s, 16), P(s, 10), P(s, 26)};
    canvas_ring(c, cx, cy - P(s, 26), P(s, 44), P(s, 14), ink);
    canvas_round(c, body, P(s, 16), ink);
    canvas_circle(c, cx, cy + P(s, 16), P(s, 11), dark);
    canvas_round(c, slot, 2, dark);
}

static void person(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    ft_rect shoulders = {cx - P(s, 62), cy + P(s, 14), P(s, 124), P(s, 60)};
    canvas_circle(c, cx, cy - P(s, 30), P(s, 30), ink);
    canvas_round_gradient(c, shoulders, P(s, 30), CORNERS_TOP, ink, ink);
}

/* Eight teeth around a ring. */
static void gear(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int k;
    for (k = 0; k < 16; k += 2) {
        int a = (k + 15) % 16, b = (k + 1) % 16, xy[8];
        xy[0] = cx + COS16[a] * P(s, 50) / 1000;
        xy[1] = cy + SIN16[a] * P(s, 50) / 1000;
        xy[2] = cx + (COS16[k] * 2 + COS16[a]) * P(s, 76) / 3000;
        xy[3] = cy + (SIN16[k] * 2 + SIN16[a]) * P(s, 76) / 3000;
        xy[4] = cx + (COS16[k] * 2 + COS16[b]) * P(s, 76) / 3000;
        xy[5] = cy + (SIN16[k] * 2 + SIN16[b]) * P(s, 76) / 3000;
        xy[6] = cx + COS16[b] * P(s, 50) / 1000;
        xy[7] = cy + SIN16[b] * P(s, 50) / 1000;
        canvas_polygon(c, xy, 4, ink);
    }
    canvas_ring(c, cx, cy, P(s, 56), P(s, 30), ink);
}

/* Lower-case i in a ring. */
static void info(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    ft_rect stem = {cx - P(s, 9), cy - P(s, 8), P(s, 18), P(s, 46)};
    canvas_ring(c, cx, cy, P(s, 72), P(s, 12), ink);
    canvas_circle(c, cx, cy - P(s, 30), P(s, 11), ink);
    canvas_round(c, stem, P(s, 6), ink);
}

/* Speaker with two sound bars. */
static void speaker(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int cone[8] = {cx - P(s, 30), cy - P(s, 22), cx + P(s, 6), cy - P(s, 56),
                   cx + P(s, 6), cy + P(s, 56), cx - P(s, 30), cy + P(s, 22)};
    ft_rect body = {cx - P(s, 62), cy - P(s, 22), P(s, 36), P(s, 44)};
    canvas_round(c, body, 2, ink);
    canvas_polygon(c, cone, 4, ink);
    canvas_line(c, cx + P(s, 30), cy - P(s, 20), cx + P(s, 30), cy + P(s, 20), P(s, 12), ink);
    canvas_line(c, cx + P(s, 56), cy - P(s, 40), cx + P(s, 56), cy + P(s, 40), P(s, 12), ink);
}

static void channels(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int i;
    for (i = 0; i < 3; i++) {
        int y = cy - P(s, 45) + i * P(s, 45);
        canvas_circle(c, cx - P(s, 72), y, P(s, 10), ink);
        canvas_line(c, cx - P(s, 46), y, cx + P(s, 76), y, P(s, 14), ink);
    }
}

static void racer_art(ft_canvas *c, int cx, int cy, int s)
{
    ft_rect road = {cx - P(s, 60), cy - P(s, 125), P(s, 120), P(s, 250)};
    ft_rect me = {cx + P(s, 8), cy + P(s, 36), P(s, 40), P(s, 64)};
    ft_rect them = {cx - P(s, 48), cy - P(s, 96), P(s, 40), P(s, 64)};
    ft_rect glass1 = {me.x + 5, me.y + 8, me.w - 10, me.h / 4};
    ft_rect glass2 = {them.x + 5, them.y + 8, them.w - 10, them.h / 4};
    int y;
    canvas_round(c, road, s / 8, FT_RGB(0x262B34));
    for (y = road.y + 6; y < road.y + road.h - 10; y += P(s, 50))
        canvas_line(c, cx, y, cx, y + P(s, 22), 3, 0xCCFFFFFFu);
    canvas_round(c, them, 6, FT_RGB(0xFF9F43));
    canvas_round(c, glass2, 3, 0xAA101820u);
    canvas_round(c, me, 6, COL_TEXT);
    canvas_round(c, glass1, 3, 0xAA101820u);
}

static void goal_art(ft_canvas *c, int cx, int cy, int s)
{
    int t = s / 9 < 2 ? 2 : s / 9, x;
    for (x = cx - s + P(s, 25); x < cx + s; x += P(s, 25))
        canvas_line(c, x, cy - P(s, 50), x, cy + P(s, 50), 1, 0x40FFFFFFu);
    canvas_line(c, cx - s, cy + P(s, 55), cx - s, cy - P(s, 55), t, COL_TEXT);
    canvas_line(c, cx - s, cy - P(s, 55), cx + s, cy - P(s, 55), t, COL_TEXT);
    canvas_line(c, cx + s, cy - P(s, 55), cx + s, cy + P(s, 55), t, COL_TEXT);
    ball(c, cx + P(s, 28), cy + P(s, 26), P(s, 30), COL_TEXT, FT_RGB(0x1B2430));
}

static void garden_art(ft_canvas *c, int cx, int cy, int s)
{
    static const ft_color tiles[3][4] = {
        {0, FT_RGB(0xB4513A), FT_RGB(0xB4513A), 0},
        {FT_RGB(0x4CAF50), FT_RGB(0xB4513A), FT_RGB(0xB4513A), FT_RGB(0x4CAF50)},
        {FT_RGB(0x8B5E3C), FT_RGB(0x8B5E3C), FT_RGB(0x2F80D8), FT_RGB(0x8B5E3C)},
    };
    int q = P(s, 46), gap = q / 8 < 1 ? 1 : q / 8, x0 = cx - 2 * q - gap * 3 / 2, y0 = cy - q * 3 / 2 - gap;
    int row, col;
    for (row = 0; row < 3; row++)
        for (col = 0; col < 4; col++) {
            ft_rect r = {x0 + col * (q + gap), y0 + row * (q + gap), q, q};
            if (tiles[row][col])
                canvas_round(c, r, q / 6, tiles[row][col]);
        }
    canvas_circle(c, x0 + q / 2, y0 + q + gap + q / 2, q / 4, FT_RGB(0xF06292));
}

/* A play button: a rounded triangle pointing right. */
static void play(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int tri[6] = {cx - P(s, 34), cy - P(s, 44), cx + P(s, 46), cy, cx - P(s, 34), cy + P(s, 44)};
    canvas_polygon(c, tri, 3, ink);
    canvas_circle(c, cx - P(s, 30), cy - P(s, 38), P(s, 8), ink);
    canvas_circle(c, cx - P(s, 30), cy + P(s, 38), P(s, 8), ink);
}

/* A magnifying glass. */
static void magnifier(ft_canvas *c, int cx, int cy, int s, ft_color ink)
{
    int t = P(s, 16) < 2 ? 2 : P(s, 16);
    canvas_ring(c, cx - P(s, 12), cy - P(s, 12), P(s, 44), t, ink);
    canvas_line(c, cx + P(s, 22), cy + P(s, 22), cx + P(s, 58), cy + P(s, 58), t + 1, ink);
}

/* Simple marks for the two services, in their colours: a red badge with a
 * white play triangle, and a red N made of two bars and a slanted stroke. */
static void youtube_mark(ft_canvas *c, int cx, int cy, int s)
{
    ft_rect badge = {cx - P(s, 90), cy - P(s, 63), P(s, 180), P(s, 126)};
    int tri[6] = {cx - P(s, 22), cy - P(s, 34), cx + P(s, 40), cy, cx - P(s, 22), cy + P(s, 34)};
    canvas_round(c, badge, P(s, 34), FT_RGB(0xFF0000));
    canvas_polygon(c, tri, 3, COL_TEXT);
}

static void netflix_mark(ft_canvas *c, int cx, int cy, int s)
{
    int bar = P(s, 30), h = P(s, 130), hh = h / 2;
    int slant[8] = {cx - P(s, 42), cy - hh, cx - P(s, 42) + bar, cy - hh, cx + P(s, 42), cy + hh,
                    cx + P(s, 42) - bar, cy + hh};
    canvas_fill(c, (ft_rect){cx - P(s, 42), cy - hh, bar, h}, FT_RGB(0xB1060F));
    canvas_fill(c, (ft_rect){cx + P(s, 42) - bar, cy - hh, bar, h}, FT_RGB(0xB1060F));
    canvas_polygon(c, slant, 4, FT_RGB(0xE50914));
}

void art_glyph(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink)
{
    int i;
    switch (id) {
    case ART_LIVE_TV: tv(c, cx, cy, s, ink); break;
    case ART_FOOTBALL: ball(c, cx, cy, P(s, 82), ink, FT_RGB(0x16301F)); break;
    case ART_GAMES: controller(c, cx, cy, s, ink); break;
    case ART_MORE:
        for (i = -1; i <= 1; i++)
            canvas_circle(c, cx + i * P(s, 60), cy, P(s, 16), ink);
        break;
    case ART_SETTINGS: padlock(c, cx, cy, s, ink); break;
    case ART_PROFILE: person(c, cx, cy, s, ink); break;
    case ART_GEAR: gear(c, cx, cy, s, ink); break;
    case ART_INFO: info(c, cx, cy, s, ink); break;
    case ART_VOLUME: speaker(c, cx, cy, s, ink); break;
    case ART_CHANNELS: channels(c, cx, cy, s, ink); break;
    case ART_RACER: racer_art(c, cx, cy, s); break;
    case ART_PENALTY: goal_art(c, cx, cy, s); break;
    case ART_GARDEN: garden_art(c, cx, cy, s); break;
    case ART_PLAY: play(c, cx, cy, s, ink); break;
    case ART_SEARCH: magnifier(c, cx, cy, s, ink); break;
    case ART_YOUTUBE: youtube_mark(c, cx, cy, s); break;
    case ART_NETFLIX: netflix_mark(c, cx, cy, s); break;
    case ART_COUNT:
        break;
    }
}

static int clamp256(int v)
{
    return v < -256 ? -256 : v > 256 ? 256 : v;
}

void art_slab(ft_canvas *c, ft_rect r, int radius, ft_color top, ft_color bottom, int focused, int tilt_x, int tilt_y)
{
    int depth = (r.h / 26 < 3 ? 3 : r.h / 26) + (focused ? 2 : 0), ox, oy;
    ft_rect edge = {r.x - clamp256(tilt_x) * depth / 512, r.y + depth, r.w, r.h};
    ft_rect inner = {r.x + radius * 3 / 10, r.y + radius * 3 / 10, r.w - radius * 3 / 5, r.h - radius * 3 / 5};
    ft_canvas clip;
    tilt_x = clamp256(tilt_x);
    tilt_y = clamp256(tilt_y);
    canvas_round(c, edge, radius, color_mix(bottom, FT_RGB(0x000000), 150));
    canvas_round_gradient(c, r, radius, CORNERS_ALL, top, bottom);
    if (canvas_clip(c, inner, &clip, &ox, &oy)) {
        /* light from where the pointer is (from above when upright) */
        int lx = r.x + r.w * (50 + tilt_x * 40 / 256) / 100 - ox, ly = r.y + r.h * (20 + tilt_y * 40 / 256) / 100 - oy;
        /* a glossy band across the slab that slides as it leans */
        int bx = r.x + r.w * (18 + (tilt_x + 256) * 30 / 512) / 100 - ox, bw = r.w / 5, skew = r.h / 2;
        int band[8] = {bx, r.y - oy, bx + bw, r.y - oy, bx + bw - skew, r.y + r.h - oy, bx - skew, r.y + r.h - oy};
        if (focused) { /* only the focused slab: polygons are the costly part of a frame */
            canvas_glow(&clip, lx, ly, r.w * 7 / 10, r.h * 8 / 10, 0x4CFFFFFFu);
            canvas_polygon(&clip, band, 4, 0x1CFFFFFFu);
        }
    }
    { /* bevel: light catches the top edge, the bottom edge falls into shade */
        ft_rect sheen = {r.x, r.y, r.w, r.h / 2};
        canvas_round_gradient(c, sheen, radius, CORNERS_TOP, focused ? 0x30FFFFFFu : 0x12FFFFFFu, 0x00FFFFFFu);
        canvas_line(c, r.x + radius, r.y + 1, r.x + r.w - radius, r.y + 1, 1, focused ? 0x99FFFFFFu : 0x52FFFFFFu);
        canvas_line(c, r.x + radius, r.y + r.h - 2, r.x + r.w - radius, r.y + r.h - 2, 1, 0x47000000u);
    }
    if (focused)
        canvas_round_stroke(c, r, radius, 1, 0x47FFFFFFu);
}

/* Glyphs drawn in one ink can be extruded; the game pictures have their own
 * colours, so they get a soft shadow instead. */
static int one_ink(art_id id)
{
    return id != ART_RACER && id != ART_PENALTY && id != ART_GARDEN && id != ART_YOUTUBE && id != ART_NETFLIX;
}

void art_glyph_3d(ft_canvas *c, art_id id, int cx, int cy, int s, ft_color ink, ft_color edge)
{
    int depth = s / 7 < 2 ? 2 : s / 7, d;
    if (!one_ink(id)) {
        ft_rect sh = {cx - s + depth, cy - s / 2 + depth * 2, 2 * s - 2 * depth, s + depth};
        canvas_shadow(c, sh, s / 3, 0x73000000u);
    } else {
        int step = (depth + 2) / 3; /* three layers read as solid depth */
        for (d = depth; d > 0; d -= step) { /* back to front, darker the deeper */
            ft_color shade = color_mix(edge, FT_RGB(0x000000), (unsigned)(d * 90 / depth));
            if (id == ART_FOOTBALL) /* the ball's side is a plain disc: its patches are costly polygons */
                canvas_circle(c, cx + d / 2, cy + d, P(s, 82), shade);
            else
                art_glyph(c, id, cx + d / 2, cy + d, s, shade);
        }
    }
    art_glyph(c, id, cx, cy, s, ink);
}

void art_tile_lean(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text, int tilt_x,
                   int tilt_y)
{
    art_id safe = id < ART_COUNT ? id : ART_MORE;
    ft_color top = GRADIENTS[safe].top, bottom = GRADIENTS[safe].bottom;
    art_slab(c, r, radius, top, bottom, focused, tilt_x, tilt_y);
    if (text) {
        ft_rect inner = {r.x + 12, r.y, r.w - 24, r.h};
        canvas_text_center(c, r.h > 110 ? &font_title : &font_label, inner, text, COL_TEXT);
    } else { /* the glyph floats a little in front, so it shifts more as the slab leans */
        int s = P(r.h, 34);
        art_glyph_3d(c, safe, r.x + r.w / 2 + clamp256(tilt_x) * s / 1800, r.y + r.h / 2 + clamp256(tilt_y) * s / 2400, s,
                     COL_TEXT, color_mix(bottom, FT_RGB(0x000000), 60));
    }
}

void art_tile(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text)
{
    art_tile_lean(c, r, radius, id, focused, text, 0, 0);
}
