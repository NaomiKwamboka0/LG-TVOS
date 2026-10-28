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
    case ART_COUNT:
        break;
    }
}

void art_tile(ft_canvas *c, ft_rect r, int radius, art_id id, int focused, const char *text)
{
    ft_rect sheen = {r.x, r.y, r.w, r.h / 2};
    art_id safe = id < ART_COUNT ? id : ART_MORE;
    canvas_round_gradient(c, r, radius, CORNERS_ALL, GRADIENTS[safe].top, GRADIENTS[safe].bottom);
    canvas_round_gradient(c, sheen, radius, CORNERS_TOP, focused ? 0x47FFFFFFu : 0x17FFFFFFu, 0x00FFFFFFu);
    if (text) {
        ft_rect inner = {r.x + 12, r.y, r.w - 24, r.h};
        canvas_text_center(c, r.h > 110 ? &font_title : &font_label, inner, text, COL_TEXT);
    } else {
        art_glyph(c, safe, r.x + r.w / 2, r.y + r.h / 2, P(r.h, 34), COL_TEXT);
    }
    if (focused)
        canvas_round_stroke(c, r, radius, 2, 0x38FFFFFFu);
}
