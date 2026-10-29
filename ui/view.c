#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"

/* ---- layout ------------------------------------------------------------ */

#define TOP_Y 28
#define TOP_H 44
#define CONTENT_TOP 96
#define CONTENT_BOTTOM 668
#define HERO_H 260
#define BANNER_H 190
#define CARD_W 236
#define CARD_H 133
#define CARD_GAP 24
#define CARD_STEP (CARD_W + CARD_GAP)
#define CARDS_VISIBLE 4 /* fully visible per row; the next one peeks in */
#define ROW_TITLE_H 40
#define GUIDE_H 60
#define GUIDE_STEP 70
#define SEARCH_H 64
#define KEY_W 92
#define KEY_H 64
#define KEY_GAP 12
#define KEYS_TOP 300
#define LIST_X 560
#define LIST_TOP 116
#define LIST_W 648
#define LIST_ROW_H 64
#define LIST_STEP 72
#define LIST_VISIBLE 7
#define HINT_Y 684
#define PANEL_W 440
#define PANEL_TOP 104
#define PANEL_STEP 58
#define PANEL_VISIBLE 8

static ft_rect lift(ft_rect r, int dx, int dy)
{
    ft_rect out = {r.x - dx, r.y - dy, r.w + 2 * dx, r.h + 2 * dy};
    return out;
}

/* ---- shared pieces ------------------------------------------------------ */

/* The glowing backdrop is the most expensive layer. Its two glows are
 * drawn once per layout into alpha masks a little larger than the screen;
 * each frame only looks the masks up through a 256-entry colour table per
 * row. Still screens cache the result until the accent changes. On the home
 * screen the masks drift slowly in different directions and the colour
 * breathes, which is the moving background at the cost of a recolour.
 * Physical pixels throughout, so the buffers follow the screen size. */
#define AMBIENT_PAD 40 /* logical px each glow may drift */

static uint32_t *backdrop_cache;
static uint8_t *glow_a, *glow_b;
static int cache_w, cache_h, mask_w, mask_h, mask_pad;
static struct {
    int valid, glow_x, glow_y, fade;
    ft_color accent;
} backdrop_key;
static struct {
    int valid, glow_x, glow_y, w, h;
} mask_key;

/* sin(i * 2pi / 64) * 1000 */
static const int SIN64[64] = {
    0, 98, 195, 290, 383, 471, 556, 634, 707, 773, 831, 882, 924, 957, 981, 995,
    1000, 995, 981, 957, 924, 882, 831, 773, 707, 634, 556, 471, 383, 290, 195, 98,
    0, -98, -195, -290, -383, -471, -556, -634, -707, -773, -831, -882, -924, -957, -981, -995,
    -1000, -995, -981, -957, -924, -882, -831, -773, -707, -634, -556, -471, -383, -290, -195, -98
};

/* A slow wave: period in ms, -1000..1000. */
static int wave(uint32_t t, uint32_t period, int phase)
{
    return SIN64[(t % period * 64 / period + (uint32_t)phase) % 64];
}

static int backdrop_buffers(int w, int h, int pad)
{
    int mw = w + 2 * pad, mh = h + 2 * pad;
    if (w == cache_w && h == cache_h && backdrop_cache && mask_pad == pad)
        return 1;
    free(backdrop_cache);
    free(glow_a);
    free(glow_b);
    backdrop_cache = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    glow_a = (uint8_t *)malloc((size_t)mw * (size_t)mh);
    glow_b = (uint8_t *)malloc((size_t)mw * (size_t)mh);
    backdrop_key.valid = mask_key.valid = 0;
    if (!backdrop_cache || !glow_a || !glow_b) {
        free(backdrop_cache);
        free(glow_a);
        free(glow_b);
        backdrop_cache = NULL;
        glow_a = glow_b = NULL;
        cache_w = cache_h = 0;
        return 0;
    }
    cache_w = w;
    cache_h = h;
    mask_w = mw;
    mask_h = mh;
    mask_pad = pad;
    return 1;
}

static void backdrop(ft_canvas *c, ft_color accent, int glow_x, int glow_y, int fade, const ui_motion *m)
{
    int W = c->w, H = c->h, k = c->scale, y, pad = AMBIENT_PAD * k / 2;
    int low_y = 360 * k / 2, low_h = H - low_y;
    int ax = pad, ay = pad, bx = pad, by = pad;
    ft_color lit_accent = accent | 0xFF000000u;
    if (W * 2 != VIEW_W * k || H * 2 != VIEW_H * k)
        return;
    if (!backdrop_buffers(W, H, pad)) {
        canvas_clear(c, COL_BG);
        return;
    }
    if (!m && backdrop_key.valid && backdrop_key.accent == accent && backdrop_key.glow_x == glow_x &&
        backdrop_key.glow_y == glow_y && backdrop_key.fade == fade) {
        for (y = 0; y < H; y++)
            memcpy(c->px + (long)y * c->stride, backdrop_cache + (long)y * W, (size_t)W * sizeof(uint32_t));
        return;
    }
    if (!mask_key.valid || mask_key.glow_x != glow_x || mask_key.glow_y != glow_y || mask_key.w != W) {
        memset(glow_a, 0, (size_t)mask_w * (size_t)mask_h);
        memset(glow_b, 0, (size_t)mask_w * (size_t)mask_h);
        canvas_glow_mask(glow_a, mask_w, mask_h, glow_x * k / 2 + pad, glow_y * k / 2 + pad, 820 * k / 2,
                         470 * k / 2, 0x7A);
        canvas_glow_mask(glow_b, mask_w, mask_h, (VIEW_W - glow_x / 3) * k / 2 + pad, (glow_y - 60) * k / 2 + pad,
                         520 * k / 2, 300 * k / 2, 0x2E);
        mask_key.valid = 1;
        mask_key.glow_x = glow_x;
        mask_key.glow_y = glow_y;
        mask_key.w = W;
    }
    if (m) { /* drift on long, unrelated periods, and breathe */
        ax = pad + pad * wave(m->ambient, 23000, 0) / 1000;
        ay = pad + pad * wave(m->ambient, 31000, 11) / 1000;
        bx = pad + pad * wave(m->ambient, 19000, 37) / 1000;
        by = pad + pad * wave(m->ambient, 27000, 5) / 1000;
        lit_accent = color_mix(lit_accent, COL_BG_TOP, (unsigned)(40 + 40 * wave(m->ambient, 9000, 0) / 1000));
    }
    /* Each row is one gradient colour, so every pixel is that colour mixed
     * towards the accent by its mask value: a 256-entry table per row turns
     * the whole layer into lookups. The home fade is folded into the table. */
    for (y = 0; y < H; y++) {
        uint32_t lut[256], *row = c->px + (long)y * c->stride;
        ft_color base = color_mix(COL_BG_TOP, COL_BG_BOTTOM, (unsigned)(y * 255 / (H - 1)));
        ft_color lit = lit_accent;
        const uint8_t *ma = glow_a + (long)(y + ay) * mask_w + ax, *mb = glow_b + (long)(y + by) * mask_w + bx;
        long acc[3], step[3];
        int ch, i, x;
        if (fade && y >= low_y) { /* keeps the lower rows legible over a bright glow */
            unsigned f = (unsigned)((y - low_y) * 0xB3 / (low_h - 1));
            base = color_mix(base, COL_BG_BOTTOM, f);
            lit = color_mix(lit, COL_BG_BOTTOM, f);
        }
        for (ch = 0; ch < 3; ch++) {
            long b = (long)((base >> (8 * ch)) & 0xFFu), l = (long)((lit >> (8 * ch)) & 0xFFu);
            acc[ch] = b * 65536 + 32768;
            step[ch] = (l - b) * 65536 / 255; /* may be negative: no shifts */
        }
        for (i = 0; i < 256; i++) {
            lut[i] = 0xFF000000u | (uint32_t)((acc[2] >> 16) << 16) | (uint32_t)((acc[1] >> 16) << 8) |
                     (uint32_t)(acc[0] >> 16);
            for (ch = 0; ch < 3; ch++)
                acc[ch] += step[ch];
        }
        for (x = 0; x < W; x++) {
            unsigned v = (unsigned)ma[x] + mb[x]; /* the glows add up like stacked layers */
            row[x] = lut[v > 255 ? 255 : v];
        }
    }
    if (m)
        return;
    for (y = 0; y < H; y++)
        memcpy(backdrop_cache + (long)y * W, c->px + (long)y * c->stride, (size_t)W * sizeof(uint32_t));
    backdrop_key.valid = 1;
    backdrop_key.accent = accent;
    backdrop_key.glow_x = glow_x;
    backdrop_key.glow_y = glow_y;
    backdrop_key.fade = fade;
}

/* Opaque background rows with optional stripes, pre-mixed instead of blended. */
static void striped_rows(ft_canvas *c, ft_color top, ft_color bottom, int band, int offset, int vertical)
{
    int y, x;
    for (y = 0; y < VIEW_H; y++) {
        ft_color row = color_mix(top, bottom, (unsigned)(y * 255 / (VIEW_H - 1)));
        ft_color lit = color_mix(row, COL_TEXT, 14);
        ft_rect line = {0, y, VIEW_W, 1};
        if (!vertical) {
            canvas_fill(c, line, (((y - offset) % (2 * band) + 2 * band) % (2 * band)) < band ? lit : row);
            continue;
        }
        for (x = 0; x < VIEW_W; x += band) {
            ft_rect seg = {x, y, band, 1};
            canvas_fill(c, seg, (x / band) % 2 ? row : lit);
        }
    }
}

static void glass(ft_canvas *c, ft_rect r, int radius)
{
    ft_rect sh = {r.x + 6, r.y + 12, r.w - 12, r.h - 6};
    canvas_shadow(c, sh, 26, 0x66000000u);
    canvas_round(c, r, radius, COL_GLASS);
    canvas_round_stroke(c, r, radius, 1, COL_GLASS_EDGE);
}

static void toast(ft_canvas *c, const char *status, int y)
{
    int w;
    ft_rect r, text;
    if (!status || !status[0])
        return;
    w = text_width(&font_body, status) + 56;
    if (w > 760)
        w = 760;
    r.x = (VIEW_W - w) / 2;
    r.y = y;
    r.w = w;
    r.h = 46;
    glass(c, r, 23);
    text.x = r.x + 28;
    text.y = r.y;
    text.w = r.w - 56;
    text.h = r.h;
    canvas_text_center(c, &font_body, text, status, COL_TEXT);
}

static void hint(ft_canvas *c, const char *text)
{
    canvas_text_fit(c, &font_caption, VIEW_MARGIN, HINT_Y, VIEW_W - 2 * VIEW_MARGIN, text, COL_TEXT_3);
}

/* ---- top bar ------------------------------------------------------------ */

static const char *profile_name(const shell *s)
{
    return tr(s->cfg.lang, s->cfg.profile == PROFILE_PARENT ? STR_PARENT : STR_CHILD);
}

static int clock_left(const char *clock_text)
{
    return VIEW_W - VIEW_MARGIN - text_width(&font_body, clock_text ? clock_text : "--:--");
}

ft_rect view_top_rect(const shell *s, int item)
{
    ft_rect r = {VIEW_MARGIN, TOP_Y, 0, TOP_H};
    int i, chip_w = 44 + text_width(&font_caption, profile_name(s)) + 18;
    if (item < TAB_COUNT) {
        for (i = 0; i <= item && i < TAB_COUNT; i++) {
            r.x += r.w + (i ? 8 : 0);
            r.w = text_width(&font_label, shell_tab_label(s->cfg.lang, i)) + 40;
        }
        return r;
    }
    /* Right side, left to right: gear, profile chip, then the clock
     * ("00:00" in font_body is about 55 px wide). */
    r.w = item == TOP_PROFILE ? chip_w : TOP_H;
    r.x = VIEW_W - VIEW_MARGIN - 64 - 20 - chip_w;
    if (item == TOP_GEAR)
        r.x -= TOP_H + 12;
    return r;
}

/* tabs: draw the tabs and gear (home only); lists show just clock and profile. */
static void top_bar(ft_canvas *c, const shell *s, const char *clock_text, int tabs, const ui_motion *m)
{
    const char *clock = clock_text ? clock_text : "--:--";
    int on_bar = s->screen == SCR_HOME && s->row < 0;
    int parent = s->cfg.profile == PROFILE_PARENT, i;
    const char *name = profile_name(s);
    ft_rect chip = view_top_rect(s, TOP_PROFILE);
    char initial[2] = {name[0], '\0'};
    ft_rect avatar = {chip.x + 6, chip.y + 7, 30, 30};

    canvas_text(c, &font_body, clock_left(clock), TOP_Y + (TOP_H - font_body.line_height) / 2, clock, COL_TEXT);
    if (!tabs) {
        chip.x = clock_left(clock) - 20 - chip.w;
    } else {
        { /* a floating glass bar holds the tabs; one highlight glides between them */
            ft_rect first = view_top_rect(s, 0), last = view_top_rect(s, TAB_COUNT - 1);
            ft_rect bar = {first.x - 6, first.y - 6, last.x + last.w - first.x + 12, TOP_H + 12};
            ft_rect pill = view_top_rect(s, s->tab);
            int lit = on_bar && s->top < TAB_COUNT;
            canvas_shadow(c, (ft_rect){bar.x + 8, bar.y + 10, bar.w - 16, bar.h - 4}, 20, 0x73000000u);
            canvas_round(c, bar, bar.h / 2, 0x991B1E28u);
            canvas_round_stroke(c, bar, bar.h / 2, 1, 0x29FFFFFFu);
            canvas_line(c, bar.x + bar.h / 2, bar.y + 1, bar.x + bar.w - bar.h / 2, bar.y + 1, 1, 0x40FFFFFFu);
            if (m) {
                pill.x = m->now.tab_x;
                pill.w = m->now.tab_w;
            }
            if (lit)
                canvas_shadow(c, lift(pill, 2, 2), 16, 0x66FFFFFFu);
            art_slab(c, pill, TOP_H / 2, lit ? FT_RGB(0xFFFFFF) : FT_RGB(0x3A3E4A), lit ? FT_RGB(0xDADCE2) : FT_RGB(0x23262E),
                     lit, 0, 0);
        }
        for (i = 0; i < TAB_COUNT; i++) {
            ft_rect r = view_top_rect(s, i);
            int focused = on_bar && s->top == i, selected = (int)s->tab == i;
            canvas_text_center(c, &font_label, r, shell_tab_label(s->cfg.lang, i),
                               focused ? COL_ON_FOCUS : selected ? COL_TEXT : COL_TEXT_2);
        }
        {
            ft_rect g = view_top_rect(s, TOP_GEAR);
            int focused = on_bar && s->top == TOP_GEAR;
            canvas_circle(c, g.x + g.w / 2, g.y + g.h / 2, g.h / 2, focused ? COL_FOCUS_BG : COL_ROW);
            art_glyph(c, ART_GEAR, g.x + g.w / 2, g.y + g.h / 2, 15, focused ? COL_ON_FOCUS : COL_TEXT);
        }
    }
    avatar.x = chip.x + 6;
    {
        int focused = tabs && on_bar && s->top == TOP_PROFILE;
        canvas_round(c, chip, chip.h / 2, focused ? COL_FOCUS_BG : COL_ROW);
        canvas_circle(c, avatar.x + 15, avatar.y + 15, 15, art_accent(parent ? ART_LIVE_TV : ART_PROFILE));
        canvas_text_center(c, &font_caption, avatar, initial, COL_TEXT);
        canvas_text(c, &font_caption, chip.x + 44, chip.y + (chip.h - font_caption.line_height) / 2, name,
                    focused ? COL_ON_FOCUS : COL_TEXT_2);
    }
}

/* Centred modal card over a dimmed screen. */
static void overlay(ft_canvas *c, const char *title, const char *line, const char *help)
{
    ft_rect all = {0, 0, VIEW_W, VIEW_H};
    ft_rect card = {340, 210, 600, 300};
    ft_rect t = {card.x + 24, card.y + 46, card.w - 48, font_display.line_height};
    ft_rect l = {card.x + 24, card.y + 128, card.w - 48, font_label.line_height};
    ft_rect h = {card.x + 24, card.y + 214, card.w - 48, font_caption.line_height};
    canvas_fill(c, all, COL_DIM);
    glass(c, card, 28);
    canvas_text_center(c, &font_display, t, title, COL_TEXT);
    if (line && line[0])
        canvas_text_center(c, &font_label, l, line, COL_TEXT_2);
    if (help && help[0])
        canvas_text_center(c, &font_caption, h, help, COL_TEXT_3);
}

/* Help strings separate actions with three spaces; show one action per line.
 * With c == NULL it only counts the lines. */
static int help_lines(ft_canvas *c, const ft_font *f, int x, int y, int max_w, const char *text, ft_color color)
{
    char buf[256];
    const char *p = text;
    int n = 0;
    while (*p) {
        const char *sep = strstr(p, "   ");
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        if (len >= sizeof buf)
            len = sizeof buf - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
        if (c)
            canvas_text_fit(c, f, x, y + n * f->line_height, max_w, buf, color);
        n++;
        if (!sep)
            break;
        for (p = sep; *p == ' '; p++)
            ;
    }
    return n;
}

/* Text with a soft drop shadow, for big results over busy backgrounds. */
static void shadowed_center(ft_canvas *c, const ft_font *f, ft_rect r, const char *text, ft_color color)
{
    ft_rect sh = {r.x + 2, r.y + 3, r.w, r.h};
    canvas_text_center(c, f, sh, text, 0x8C000000u);
    canvas_text_center(c, f, r, text, color);
}

/* ---- home: tabs over rows of cards ------------------------------------- */

typedef struct {
    int top[SHELL_MAX_ROWS], height[SHELL_MAX_ROWS];
    int first_row; /* rows above this have scrolled away */
    int offset;
} home_geo;

static int row_height(const shell_row *r)
{
    switch (r->style) {
    case ROW_HERO: return HERO_H + 32;
    case ROW_BANNER: return BANNER_H + 32;
    case ROW_CARDS: return ROW_TITLE_H + CARD_H + 56;
    case ROW_GUIDE: return (r->title != STR_COUNT ? ROW_TITLE_H : 0) + GUIDE_STEP;
    case ROW_SEARCH: return SEARCH_H + 28;
    }
    return 0;
}

/* Scrolls by whole rows, just far enough that the focused row fits. */
static void home_geometry(const shell *s, const shell_row *rows, int n, home_geo *g)
{
    int y = CONTENT_TOP, i, bottom;
    for (i = 0; i < n; i++) {
        g->top[i] = y;
        g->height[i] = row_height(&rows[i]);
        y += g->height[i];
    }
    g->first_row = 0;
    g->offset = 0;
    if (s->row <= 0 || s->row >= n)
        return;
    bottom = g->top[s->row] + g->height[s->row];
    while (g->first_row < s->row && bottom - (g->top[g->first_row] - CONTENT_TOP) > CONTENT_BOTTOM)
        g->first_row++;
    g->offset = g->top[g->first_row] - CONTENT_TOP;
}

/* First card shown in a row: the focus moves right until the third card,
 * then the row slides, but never past its end. */
static int first_card(int col, int n)
{
    int first = col - (CARDS_VISIBLE - 2), last = n - CARDS_VISIBLE;
    if (first > last)
        first = last;
    return first < 0 ? 0 : first;
}

static int home_layout(const shell *s, shell_row *rows, home_geo *g)
{
    int n = shell_home_rows(s, rows, SHELL_MAX_ROWS);
    home_geometry(s, rows, n, g);
    return n;
}

/* Where a card is drawn: from the layout, or where the motion has got to. */
static ft_rect card_rect(const shell *s, const shell_row *rows, int n, const home_geo *g, int row, int col,
                         const ui_motion *m)
{
    ft_rect r = {0, 0, 0, 0};
    int y;
    if (row < 0 || row >= n)
        return r;
    y = g->top[row] - (m ? m->now.page_y : g->offset);
    switch (rows[row].style) {
    case ROW_HERO:
    case ROW_BANNER:
        r.x = VIEW_MARGIN;
        r.y = y;
        r.w = VIEW_W - 2 * VIEW_MARGIN;
        r.h = rows[row].style == ROW_HERO ? HERO_H : BANNER_H;
        break;
    case ROW_CARDS:
        r.x = VIEW_MARGIN + col * CARD_STEP -
              (m ? m->now.row_x[row] : first_card(s->col[row], rows[row].n) * CARD_STEP);
        r.y = y + ROW_TITLE_H;
        r.w = CARD_W;
        r.h = CARD_H;
        break;
    case ROW_GUIDE:
        r.x = VIEW_MARGIN;
        r.y = y + (rows[row].title != STR_COUNT ? ROW_TITLE_H : 0);
        r.w = VIEW_W - 2 * VIEW_MARGIN;
        r.h = GUIDE_H;
        break;
    case ROW_SEARCH:
        r.x = VIEW_MARGIN;
        r.y = y + 4;
        r.w = VIEW_W - 2 * VIEW_MARGIN;
        r.h = SEARCH_H;
        break;
    }
    return r;
}

ft_rect view_home_card_rect(const shell *s, int row, int col)
{
    static shell_row rows[SHELL_MAX_ROWS];
    home_geo g;
    int n = home_layout(s, rows, &g);
    return card_rect(s, rows, n, &g, row, col, NULL);
}

static int inside(ft_rect r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static int list_first(int item)
{
    return item >= LIST_VISIBLE ? item - LIST_VISIBLE + 1 : 0;
}

view_hit view_shell_hit(const shell *s, int x, int y)
{
    static shell_row rows[SHELL_MAX_ROWS];
    view_hit h = {HIT_NONE, 0, 0, 0};
    home_geo g;
    int n, i, j, first;

    if (s->screen == SCR_PIN) {
        for (i = 0; i < SHELL_PIN_DIGITS; i++)
            if (inside(lift(view_pin_box_rect(i), 6, 6), x, y)) {
                h.kind = HIT_PIN;
                h.col = i;
                h.whole = 1;
            }
        return h;
    }
    if (s->screen == SCR_KEYBOARD) {
        for (i = 0; i < SHELL_KEYS; i++)
            if (inside(view_key_rect(i), x, y)) {
                h.kind = HIT_ITEM;
                h.col = i;
                h.whole = 1;
            }
        return h;
    }
    if (s->screen != SCR_HOME) {
        char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
        n = shell_list(s, labels, SHELL_MAX_ITEMS);
        first = list_first(s->item);
        for (i = first; i < n && i < first + LIST_VISIBLE; i++)
            if (inside(view_list_row_rect(i - first), x, y)) {
                h.kind = HIT_ITEM;
                h.col = i;
                h.whole = 1;
            }
        return h;
    }
    for (i = 0; i < TOP_COUNT; i++)
        if (inside(view_top_rect(s, i), x, y)) {
            h.kind = HIT_TOP;
            h.col = i;
            h.whole = 1;
            return h;
        }
    if (y < CONTENT_TOP || y >= CONTENT_BOTTOM)
        return h;
    n = home_layout(s, rows, &g);
    for (i = 0; i < n; i++)
        for (j = 0; j < rows[i].n; j++) {
            ft_rect r = card_rect(s, rows, n, &g, i, j, NULL);
            if (rows[i].style == ROW_CARDS && r.x >= VIEW_W)
                break;
            if (inside(r, x, y)) {
                h.kind = HIT_CARD;
                h.row = i;
                h.col = j;
                h.whole = r.x >= 0 && r.x + r.w <= VIEW_W && r.y >= CONTENT_TOP - 8 && r.y + r.h <= CONTENT_BOTTOM;
                return h;
            }
        }
    return h;
}

int view_live_hit(const live *l, int nch, int x, int y)
{
    int first = l->panel_item - PANEL_VISIBLE / 2, i;
    if (!l->panel_open)
        return -1;
    if (first > nch - PANEL_VISIBLE)
        first = nch - PANEL_VISIBLE;
    if (first < 0)
        first = 0;
    for (i = first; i < nch && i < first + PANEL_VISIBLE; i++)
        if (inside(view_live_panel_row_rect(i - first), x, y))
            return i;
    return -1;
}

void view_home_targets(const shell *s, ui_targets *t)
{
    static shell_row rows[SHELL_MAX_ROWS];
    home_geo g;
    int n = home_layout(s, rows, &g), i;
    ft_rect pill = view_top_rect(s, s->tab);
    memset(t, 0, sizeof *t);
    t->page_y = g.offset;
    for (i = 0; i < n; i++)
        if (rows[i].style == ROW_CARDS)
            t->row_x[i] = first_card(s->col[i], rows[i].n) * CARD_STEP;
    t->tab_x = pill.x;
    t->tab_w = pill.w;
    t->focus_key = (int)s->tab * 100000 + (s->row + 1) * 1000 + (s->row >= 0 ? s->col[s->row] : 900 + s->top);
}

static void live_pill(ft_canvas *c, int x, int y, ft_lang L)
{
    const char *t = tr(L, STR_LIVE_BADGE);
    ft_rect r = {x, y, text_width(&font_caption, t) + 30, 26};
    canvas_round(c, r, 13, COL_LIVE);
    canvas_circle(c, r.x + 11, r.y + 13, 4, COL_TEXT);
    canvas_text(c, &font_caption, r.x + 19, r.y + (r.h - font_caption.line_height) / 2, t, COL_TEXT);
}

/* Channel number on the channel's colour. */
static void number_badge(ft_canvas *c, ft_rect r, const channel *ch, const ft_font *f)
{
    char num[8];
    snprintf(num, sizeof num, "%d", ch->number);
    canvas_round_gradient(c, r, r.h / 4, CORNERS_ALL, color_mix(ch->accent, COL_TEXT, 40),
                          color_mix(ch->accent, FT_RGB(0x000000), 70));
    canvas_text_center(c, f, r, num, COL_TEXT);
}

static void ring(ft_canvas *c, ft_rect r, int radius)
{
    ft_rect outer = lift(r, 5, 5);
    canvas_round_stroke(c, outer, radius + 5, 3, COL_RING);
}

static void spinner(ft_canvas *c, int cx, int cy, int radius, uint32_t anim_ms)
{
    static const int DX[8] = {0, 707, 1000, 707, 0, -707, -1000, -707};
    static const int DY[8] = {-1000, -707, 0, 707, 1000, 707, 0, -707};
    int head = (int)(anim_ms / 110 % 8), i;
    for (i = 0; i < 8; i++) {
        int age = (head - i + 8) % 8;
        canvas_circle(c, cx + DX[i] * radius / 1000, cy + DY[i] * radius / 1000, radius / 5,
                      color_alpha(COL_TEXT, (unsigned)(255 - age * 28)));
    }
}

/* How far a card leans toward the mouse pointer: only the focused one, on
 * the home screen, and only while the pointer is over it. */
static void lean(const ui_motion *m, int focused, int *tx, int *ty)
{
    *tx = focused && m ? m->now.tilt_x : 0;
    *ty = focused && m ? m->now.tilt_y : 0;
}

/* What floats a card off the page: a tight contact shadow and a wide soft
 * one thrown away from the light. The focused card floats higher, and its
 * own colour glows around it. */
static void float_card(ft_canvas *c, ft_rect r, ft_color accent, int focused, int tx, int ty)
{
    ft_rect soft = {r.x + 10 - tx * 16 / 256, r.y + (focused ? 30 : 16) - ty * 10 / 256, r.w - 20, r.h - 8};
    ft_rect contact = {r.x + 4, r.y + 8, r.w - 8, r.h - 2};
    ft_rect under = lift(r, -6, -6); /* clear of the rounded corners (radius 14), so surely covered */
    if (tx || ty) /* a turned card does not cover its whole rectangle */
        under.w = 0;
    if (focused)
        canvas_shadow_under(c, lift(r, 4, 4), 34, color_alpha(accent, 0x8C), under);
    canvas_shadow_under(c, soft, focused ? 36 : 22, focused ? 0xB8000000u : 0x80000000u, under);
    if (focused) /* resting cards make do with one shadow */
        canvas_shadow_under(c, contact, 8, 0x59000000u, under);
}

/* No LIVE badge here: whether a channel is live is only known once it plays. */
static void channel_card(ft_canvas *c, ft_rect r, const channel *ch, int focused, int tx, int ty)
{
    ft_rect badge = {r.x + 12, r.y + 12, 44, 34};
    ft_color top = color_mix(ch->accent, COL_TEXT, 30), bottom = color_mix(ch->accent, FT_RGB(0x000000), 120);
    char num[8];
    art_slab(c, r, 14, top, bottom, focused, tx, ty);
    art_glyph_3d(c, ART_LIVE_TV, r.x + r.w - 34 + tx * 4 / 256, r.y + 30 + ty * 3 / 256, 15, 0xD9FFFFFFu,
                 color_mix(ch->accent, FT_RGB(0x000000), 80));
    canvas_round(c, badge, 8, 0x4D000000u);
    canvas_round_stroke(c, badge, 8, 1, 0x26FFFFFFu);
    snprintf(num, sizeof num, "%d", ch->number);
    canvas_text_center(c, &font_label, badge, num, COL_TEXT);
    canvas_text_fit(c, text_width(&font_title, ch->name) <= r.w - 28 ? &font_title : &font_label, r.x + 14,
                    r.y + r.h - 14 - font_title.line_height, r.w - 28, ch->name, COL_TEXT);
    if (focused)
        ring(c, r, 14);
}

static int sports_live_count(const shell *s)
{
    int i, n = 0;
    for (i = 0; i < s->cfg.sports.n; i++)
        n += s->cfg.sport[i].state == SPORT_LIVE;
    return n;
}

/* The Sports thumbnail, all our own drawing: a floodlit stadium at night,
 * a pitch in perspective and a ball. Big versions add the words. */
static void sports_art(ft_canvas *c, ft_rect r, int radius, const shell *s, int words)
{
    ft_lang L = s->cfg.lang;
    int w = r.w, h = r.h, x = r.x, y = r.y, live = sports_live_count(s);
    int beam_l[8] = {x + w * 8 / 100, y + 4, x + w * 14 / 100, y + 4, x + w * 52 / 100, y + h - 6, x + w * 22 / 100, y + h - 6};
    int beam_r[8] = {x + w * 86 / 100, y + 4, x + w * 92 / 100, y + 4, x + w * 78 / 100, y + h - 6, x + w * 48 / 100, y + h - 6};
    int pitch[8] = {x + w * 6 / 100, y + h - 3, x + w * 94 / 100, y + h - 3, x + w * 80 / 100, y + h * 60 / 100,
                    x + w * 20 / 100, y + h * 60 / 100};
    int bx = x + w * (words ? 76 : 62) / 100, by = y + h * (words ? 58 : 52) / 100, br = h * (words ? 22 : 26) / 100;
    art_slab(c, r, radius, FT_RGB(0x12305E), FT_RGB(0x060B18), 0, 0, 0);
    canvas_polygon(c, pitch, 4, FT_RGB(0x1C7A3C));
    canvas_line(c, x + w / 2, y + h * 61 / 100, x + w / 2, y + h - 4, 2, 0x8CFFFFFFu);
    canvas_line(c, x + w * 14 / 100, y + h * 80 / 100, x + w * 86 / 100, y + h * 80 / 100, 2, 0x5CFFFFFFu);
    canvas_polygon(c, beam_l, 4, 0x1FFFFFFFu);
    canvas_polygon(c, beam_r, 4, 0x1FFFFFFFu);
    canvas_glow(c, x + w * 11 / 100, y + 10, h / 5, h / 7, 0xC0FFF6D5u);
    canvas_glow(c, x + w * 89 / 100, y + 10, h / 5, h / 7, 0xC0FFF6D5u);
    canvas_circle(c, x + w * 11 / 100, y + 9, h / 30 + 2, COL_TEXT);
    canvas_circle(c, x + w * 89 / 100, y + 9, h / 30 + 2, COL_TEXT);
    canvas_shadow(c, (ft_rect){bx - br, by + br / 2, 2 * br, br}, br / 2, 0x99000000u);
    art_glyph_3d(c, ART_FOOTBALL, bx, by, br * 100 / 82, COL_TEXT, FT_RGB(0x5A6B7D));
    if (words) {
        ft_rect line = {x + 44, y + 100, w * 55 / 100, 2 * font_body.line_height};
        canvas_text(c, &font_display, x + 44, y + 38, tr(L, STR_SPORTS), COL_TEXT);
        canvas_text_wrap(c, &font_body, line, tr(L, STR_SPORTS_LINE), COL_TEXT_2);
    }
    if (live > 0) {
        char t[48];
        ft_rect pill = {0, 0, 0, 30};
        snprintf(t, sizeof t, "%d %s", live, tr(L, STR_LIVE_NOW));
        pill.w = text_width(&font_caption, t) + 36;
        pill.x = words ? x + 44 : x + 12;
        pill.y = words ? y + h - 52 : y + 12;
        canvas_round(c, pill, 15, COL_LIVE);
        canvas_circle(c, pill.x + 14, pill.y + 15, 5, COL_TEXT);
        canvas_text(c, &font_caption, pill.x + 25, pill.y + (pill.h - font_caption.line_height) / 2, t, COL_TEXT);
    }
}

static art_id card_art(shell_card k)
{
    static const art_id games[GAME_COUNT] = {ART_RACER, ART_PENALTY, ART_GARDEN};
    switch (k.kind) {
    case CARD_GAME: return k.arg >= 0 && k.arg < GAME_COUNT ? games[k.arg] : ART_GAMES;
    case CARD_SETTINGS: return ART_SETTINGS;
    case CARD_PROFILE: return ART_PROFILE;
    case CARD_ABOUT: return ART_INFO;
    case CARD_SPORTS:
    case CARD_SPORT:
    case CARD_FOUND:
    case CARD_SEARCHING: return ART_FOOTBALL;
    case CARD_VIDEO:
    case CARD_YT_LOADING: return ART_PLAY;
    case CARD_YT_SEARCH: return ART_SEARCH;
    case CARD_ASK_PARENT: return ART_SETTINGS;
    case CARD_CHANNEL:
    case CARD_APP:
    case CARD_NO_CHANNELS: break;
    }
    return ART_CHANNELS;
}

/* Streaming services show their name, never a logo. */
static void app_tile(ft_canvas *c, ft_rect r, int app, int focused, int tx, int ty)
{
    static const ft_color ACCENT[APP_COUNT] = {FT_RGB(0xE50914), FT_RGB(0xFF0033)};
    ft_color accent = ACCENT[app % APP_COUNT];
    ft_rect name = {r.x, r.y, r.w, r.h - 8};
    ft_canvas clip;
    int ox, oy;
    art_slab(c, r, 14, FT_RGB(0x2C2D34), FT_RGB(0x0F1013), focused, tx, ty);
    if (canvas_clip(c, (ft_rect){r.x + 4, r.y + 4, r.w - 8, r.h - 8}, &clip, &ox, &oy)) /* the brand colour glows up */
        canvas_glow(&clip, r.x + r.w / 2 - ox, r.y + r.h - oy, r.w * 6 / 10, r.h * 5 / 10, color_alpha(accent, 0x99));
    canvas_text_center(c, &font_title, (ft_rect){name.x + 1, name.y + 2, name.w, name.h}, shell_app_name(app), 0x8C000000u);
    canvas_text_center(c, &font_title, name, shell_app_name(app), COL_TEXT);
}

/* An official sports channel: its colour, its name, and whether a match is
 * on now or coming up. */
static void sport_tile(ft_canvas *c, ft_rect r, const shell *s, int i, int focused, int tx, int ty)
{
    const channel *ch = &s->cfg.sports.ch[i];
    const sport_status *st = &s->cfg.sport[i];
    ft_lang L = s->cfg.lang;
    art_slab(c, r, 14, color_mix(ch->accent, COL_TEXT, 30), color_mix(ch->accent, FT_RGB(0x000000), 130), focused, tx,
             ty);
    art_glyph_3d(c, ART_LIVE_TV, r.x + r.w - 36 + tx * 5 / 256, r.y + 32 + ty * 4 / 256, 17, COL_TEXT,
                 color_mix(ch->accent, FT_RGB(0x000000), 90));
    if (st->state == SPORT_LIVE)
        live_pill(c, r.x + 12, r.y + 12, L);
    else if (st->state == SPORT_UPCOMING || st->state == SPORT_CHECKING) {
        const char *t = tr(L, st->state == SPORT_UPCOMING ? STR_COMING_UP : STR_CHECKING);
        ft_rect chip = {r.x + 12, r.y + 12, text_width(&font_caption, t) + 20, 26};
        canvas_round(c, chip, 13, 0x59000000u);
        canvas_text_center(c, &font_caption, chip, t, COL_TEXT_2);
    }
    canvas_text_fit(c, text_width(&font_title, ch->name) <= r.w - 28 ? &font_title : &font_label, r.x + 14,
                    r.y + r.h - 14 - font_title.line_height, r.w - 28, ch->name, COL_TEXT);
}

/* "12.4k watching" */
static void viewers_text(char *out, size_t n, int v, ft_lang L)
{
    if (v >= 1000)
        snprintf(out, n, "%d.%dk %s", v / 1000, v % 1000 / 100, tr(L, STR_VIEWERS));
    else
        snprintf(out, n, "%d %s", v, tr(L, STR_VIEWERS));
}

/* A live football stream found by search: a floodlit pitch with the
 * match's own title on it. */
static void found_tile(ft_canvas *c, ft_rect r, const shell *s, int i, int focused, int tx, int ty)
{
    const sport_found *f = &s->cfg.found[i];
    ft_lang L = s->cfg.lang;
    ft_rect title = {r.x + 14, r.y + 46, r.w - 28, 2 * font_label.line_height};
    int x = r.x, y = r.y, w = r.w, h = r.h;
    int pitch[8] = {x + w * 4 / 100, y + h - 4, x + w * 96 / 100, y + h - 4, x + w * 78 / 100, y + h * 52 / 100,
                    x + w * 22 / 100, y + h * 52 / 100};
    ft_canvas clip;
    int ox, oy;
    art_slab(c, r, 14, FT_RGB(0x15335E), FT_RGB(0x07101F), focused, tx, ty);
    canvas_polygon(c, pitch, 4, 0x8C1C7A3Cu);
    canvas_line(c, x + w / 2, y + h * 53 / 100, x + w / 2, y + h - 5, 1, 0x4DFFFFFFu);
    if (canvas_clip(c, (ft_rect){x + 4, y + 4, w - 8, h - 8}, &clip, &ox, &oy)) { /* floodlights */
        canvas_glow(&clip, x + w * 12 / 100 - ox, y + 6 - oy, h / 4, h / 6, 0x80FFF6D5u);
        canvas_glow(&clip, x + w * 88 / 100 - ox, y + 6 - oy, h / 4, h / 6, 0x80FFF6D5u);
    }
    live_pill(c, r.x + 12, r.y + 12, L);
    if (f->viewers > 0) {
        char v[40];
        viewers_text(v, sizeof v, f->viewers, L);
        canvas_text_right(c, &font_caption, r.x + r.w - 14, r.y + 16, v, COL_TEXT_2);
    }
    canvas_text_wrap(c, &font_label, (ft_rect){title.x + 1, title.y + 2, title.w, title.h}, f->title, 0x99000000u);
    canvas_text_wrap(c, &font_label, title, f->title, COL_TEXT);
}

/* Stands in for the search results while they come, or when there are none. */
static void searching_tile(ft_canvas *c, ft_rect r, const shell *s, int focused, int tx, int ty, const ui_motion *m)
{
    ft_rect line = {r.x + 16, r.y + r.h - 16 - 2 * font_body.line_height, r.w - 32, 2 * font_body.line_height};
    art_slab(c, r, 14, FT_RGB(0x2A2E38), FT_RGB(0x14161B), focused, tx, ty);
    if (s->cfg.searching)
        spinner(c, r.x + r.w / 2, r.y + 40, 16, m ? m->ambient : 0);
    else
        art_glyph_3d(c, ART_FOOTBALL, r.x + r.w / 2, r.y + 40, 22, COL_TEXT_2, FT_RGB(0x2A2E38));
    canvas_text_wrap(c, &font_body, line, shell_card_label(s, (shell_card){CARD_SEARCHING, 0}), COL_TEXT_2);
}

/* A YouTube video. No pictures (they would need fetching); each video gets
 * a deep colour of its own, the play button and its title. */
static void video_tile(ft_canvas *c, ft_rect r, const shell *s, int i, int focused, int tx, int ty)
{
    static const ft_color DEEP[6] = {FT_RGB(0x7A1630), FT_RGB(0x1D3E7A), FT_RGB(0x1F5E4A), FT_RGB(0x5B2A86),
                                     FT_RGB(0x80441A), FT_RGB(0x234B66)};
    const yt_video *v = &s->cfg.videos[i];
    ft_rect title = {r.x + 14, r.y + r.h - 12 - 2 * font_label.line_height, r.w - 28, 2 * font_label.line_height};
    unsigned h = 0;
    const char *p;
    ft_color tone;
    char d[16];
    for (p = v->id; *p; p++)
        h = h * 31u + (unsigned char)*p;
    tone = DEEP[h % 6];
    art_slab(c, r, 14, color_mix(tone, COL_TEXT, 40), color_mix(tone, FT_RGB(0x000000), 110), focused, tx, ty);
    art_glyph_3d(c, ART_PLAY, r.x + 36 + tx * 4 / 256, r.y + 34 + ty * 3 / 256, 18, COL_TEXT,
                 color_mix(tone, FT_RGB(0x000000), 60));
    if (v->live)
        live_pill(c, r.x + r.w - 92, r.y + 12, s->cfg.lang);
    else {
        yt_duration(d, sizeof d, v->seconds);
        if (d[0]) {
            ft_rect chip = {0, r.y + 12, text_width(&font_caption, d) + 18, 26};
            chip.x = r.x + r.w - 12 - chip.w;
            canvas_round(c, chip, 8, 0xB3000000u);
            canvas_text_center(c, &font_caption, chip, d, COL_TEXT);
        }
    }
    canvas_text_wrap(c, &font_label, (ft_rect){title.x + 1, title.y + 2, title.w, title.h}, v->title, 0x99000000u);
    canvas_text_wrap(c, &font_label, title, v->title, COL_TEXT);
}

/* Stands in for a YouTube row while it loads, or when nothing came. */
static void loading_tile(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused, int tx, int ty,
                         const ui_motion *m)
{
    ft_rect line = {r.x + 16, r.y + r.h - 16 - 2 * font_body.line_height, r.w - 32, 2 * font_body.line_height};
    art_slab(c, r, 14, FT_RGB(0x2A2E38), FT_RGB(0x14161B), focused, tx, ty);
    if (s->cfg.yt_loading & (1u << k.arg))
        spinner(c, r.x + r.w / 2, r.y + 40, 16, m ? m->ambient : 0);
    else
        art_glyph_3d(c, ART_PLAY, r.x + r.w / 2, r.y + 40, 18, COL_TEXT_2, FT_RGB(0x2A2E38));
    canvas_text_wrap(c, &font_body, line, shell_card_label(s, k), COL_TEXT_2);
}

/* The YouTube search bar: glass, a magnifier and what was searched. */
static void search_bar(ft_canvas *c, ft_rect r, const shell *s, int focused)
{
    const char *q = s->cfg.yt_query;
    ft_color ink = focused ? COL_ON_FOCUS : COL_TEXT;
    if (focused) {
        canvas_shadow(c, (ft_rect){r.x + 8, r.y + 14, r.w - 16, r.h - 6}, 24, 0x99000000u);
        art_slab(c, r, r.h / 2, FT_RGB(0xFFFFFF), FT_RGB(0xDDDFE5), 1, 0, 0);
    } else {
        canvas_round(c, r, r.h / 2, 0x991B1E28u);
        canvas_round_stroke(c, r, r.h / 2, 1, 0x33FFFFFFu);
        canvas_line(c, r.x + r.h / 2, r.y + 1, r.x + r.w - r.h / 2, r.y + 1, 1, 0x40FFFFFFu);
    }
    art_glyph(c, ART_SEARCH, r.x + 40, r.y + r.h / 2, 18, ink);
    canvas_text_fit(c, &font_label, r.x + 76, r.y + (r.h - font_label.line_height) / 2, r.w - 120,
                    q[0] ? q : tr(s->cfg.lang, STR_YT_SEARCH), q[0] ? ink : focused ? COL_ON_FOCUS_2 : COL_TEXT_3);
}

/* Under the card: what it is (games, apps) or what is on (sports). */
static const char *card_caption(const shell *s, shell_card k)
{
    if (k.kind == CARD_APP)
        return tr(s->cfg.lang, STR_OPENS_BROWSER);
    if (k.kind == CARD_SPORT && k.arg >= 0 && k.arg < s->cfg.sports.n) {
        const sport_status *st = &s->cfg.sport[k.arg];
        return st->state == SPORT_LIVE || st->state == SPORT_UPCOMING ? st->title : "";
    }
    if (k.kind == CARD_FOUND && k.arg >= 0 && k.arg < s->cfg.nfound)
        return s->cfg.found[k.arg].by;
    if (k.kind == CARD_SEARCHING || k.kind == CARD_YT_LOADING)
        return "";
    if (k.kind == CARD_VIDEO)
        return s->cfg.videos[k.arg].by;
    return shell_card_label(s, k);
}

static ft_color card_accent(const shell *s, shell_card k)
{
    switch (k.kind) {
    case CARD_CHANNEL: return s->cfg.channels.ch[k.arg].accent;
    case CARD_APP: return FT_RGB(0xE50914);
    case CARD_SPORT: return s->cfg.sports.ch[k.arg].accent;
    case CARD_SPORTS:
    case CARD_FOUND:
    case CARD_SEARCHING: return FT_RGB(0x1C7A3C);
    case CARD_VIDEO:
    case CARD_YT_LOADING:
    case CARD_YT_SEARCH: return FT_RGB(0xC4122F);
    case CARD_ASK_PARENT:
    case CARD_GAME:
    case CARD_SETTINGS:
    case CARD_PROFILE:
    case CARD_ABOUT:
    case CARD_NO_CHANNELS: break;
    }
    return art_accent(card_art(k));
}

/* The card itself, without its shadow or caption. */
static void card_body(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused, int tx, int ty,
                      const ui_motion *m)
{
    if (k.kind == CARD_CHANNEL)
        channel_card(c, r, &s->cfg.channels.ch[k.arg], focused, tx, ty);
    else if (k.kind == CARD_SPORTS)
        sports_art(c, r, 14, s, 0);
    else if (k.kind == CARD_APP)
        app_tile(c, r, k.arg, focused, tx, ty);
    else if (k.kind == CARD_SPORT)
        sport_tile(c, r, s, k.arg, focused, tx, ty);
    else if (k.kind == CARD_FOUND)
        found_tile(c, r, s, k.arg, focused, tx, ty);
    else if (k.kind == CARD_SEARCHING)
        searching_tile(c, r, s, focused, tx, ty, m);
    else if (k.kind == CARD_VIDEO)
        video_tile(c, r, s, k.arg, focused, tx, ty);
    else if (k.kind == CARD_YT_LOADING)
        loading_tile(c, r, s, k, focused, tx, ty, m);
    else
        art_tile_lean(c, r, 14, card_art(k), focused, NULL, tx, ty);
    if (focused && k.kind != CARD_CHANNEL)
        ring(c, r, 14);
}

/* The focused card under the mouse pointer turns toward it in 3D: drawn on
 * its own over a copy of what is behind it, then put on the page in
 * perspective. The side under the pointer dips away, as if pressed. */
#define TURN_X_MAX 175 /* about 10 degrees */
#define TURN_Y_MAX 245 /* about 14 degrees */
#define TURN_PAD 8     /* the focus ring sits outside the card */

static void turned_card(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int tx, int ty, const ui_motion *m)
{
    static uint32_t *buf;
    static size_t cap;
    ft_rect big = lift(r, TURN_PAD, TURN_PAD);
    int k2 = c->scale, x0 = big.x * k2 / 2, y0 = big.y * k2 / 2, w = big.w * k2 / 2, h = big.h * k2 / 2, y, x;
    ft_canvas t;
    if ((size_t)w * (size_t)h > cap) {
        uint32_t *nb = (uint32_t *)realloc(buf, (size_t)w * (size_t)h * sizeof *nb);
        if (!nb) {
            card_body(c, r, s, k, 1, tx, ty, m);
            return;
        }
        buf = nb;
        cap = (size_t)w * (size_t)h;
    }
    for (y = 0; y < h; y++) /* what is behind the card, so its edges blend */
        for (x = 0; x < w; x++) {
            int sx = x0 + x, sy = y0 + y;
            buf[(long)y * w + x] = sx >= 0 && sy >= 0 && sx < c->w && sy < c->h ? c->px[(long)sy * c->stride + sx] : COL_BG;
        }
    canvas_init(&t, buf, w, h, w);
    canvas_set_scale(&t, k2);
    card_body(&t, (ft_rect){TURN_PAD, TURN_PAD, r.w, r.h}, s, k, 1, tx, ty, m);
    canvas_turn(c, &t, x0, y0, (14 + TURN_PAD) * k2 / 2, ty * TURN_X_MAX / 256, -tx * TURN_Y_MAX / 256);
}

static void draw_card(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused, const ui_motion *m)
{
    ft_rect label = {r.x, r.y + r.h + 14, r.w, font_label.line_height};
    int tx, ty;
    lean(m, focused, &tx, &ty);
    if (k.kind != CARD_SEARCHING && k.kind != CARD_YT_LOADING)
        float_card(c, r, card_accent(s, k), focused, tx, ty);
    if (tx || ty)
        turned_card(c, r, s, k, tx, ty, m);
    else
        card_body(c, r, s, k, focused, tx, ty, m);
    if (k.kind != CARD_CHANNEL)
        canvas_text_fit(c, focused ? &font_label : &font_body, label.x, label.y, label.w, card_caption(s, k),
                        focused ? COL_TEXT : COL_TEXT_2);
}

static void hero(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused)
{
    ft_lang L = s->cfg.lang;
    const channel *ch = k.kind == CARD_CHANNEL ? &s->cfg.channels.ch[k.arg] : NULL;
    ft_color accent = ch ? ch->accent : art_accent(ART_CHANNELS);
    float_card(c, r, accent, focused, 0, 0);
    art_slab(c, r, 22, FT_RGB(0x23252C), FT_RGB(0x111216), focused, 0, 0);
    canvas_glow(c, r.x + r.w - 300, r.y + r.h / 2, 300, r.h / 2, color_alpha(accent, 0xC8));
    art_glyph_3d(c, ch ? ART_LIVE_TV : ART_CHANNELS, r.x + r.w - 300, r.y + r.h / 2, 56, 0xF2FFFFFFu,
                 color_mix(accent, FT_RGB(0x000000), 60));
    if (ch) {
        ft_rect badge = {r.x + 44, r.y + 32, 56, 40};
        ft_rect button = {r.x + 44, r.y + r.h - 74, 0, 48};
        const char *watch = tr(L, STR_WATCH);
        number_badge(c, badge, ch, &font_label);
        canvas_text(c, &font_body, badge.x + badge.w + 16, badge.y + (badge.h - font_body.line_height) / 2,
                    "YouTube", COL_TEXT_3);
        canvas_text_fit(c, &font_display, r.x + 44, r.y + 80, 620, ch->name, COL_TEXT);
        canvas_text_fit(c, &font_body, r.x + 44, r.y + 140, 620, tr(L, STR_HERO_LINE), COL_TEXT_2);
        button.w = text_width(&font_label, watch) + 56;
        canvas_round(c, button, 24, focused ? COL_FOCUS_BG : COL_ROW);
        canvas_text_center(c, &font_label, button, watch, focused ? COL_ON_FOCUS : COL_TEXT);
    } else {
        ft_rect hint_r = {r.x + 44, r.y + 150, 560, 2 * font_body.line_height};
        canvas_text_fit(c, &font_display, r.x + 44, r.y + 80, 620, tr(L, STR_NO_CHANNELS), COL_TEXT);
        canvas_text_wrap(c, &font_body, hint_r, tr(L, STR_NO_CHANNELS_HINT), COL_TEXT_2);
    }
    if (focused)
        canvas_round_stroke(c, r, 22, 3, COL_RING);
}

/* A list row: the focused one is a light slab floating above the page,
 * the rest are glass with a bevel. */
static void focus_row(ft_canvas *c, ft_rect r, int focused)
{
    if (focused) {
        ft_rect big = lift(r, 8, 3);
        ft_rect sh = {big.x + 8, big.y + 16, big.w - 16, big.h - 6};
        canvas_shadow(c, sh, 24, 0x99000000u);
        art_slab(c, big, 16, FT_RGB(0xFFFFFF), FT_RGB(0xDDDFE5), 1, 0, 0);
    } else {
        canvas_round(c, r, 14, COL_ROW);
        canvas_line(c, r.x + 14, r.y + 1, r.x + r.w - 14, r.y + 1, 1, 0x1FFFFFFFu);
    }
}

static void guide_row(ft_canvas *c, ft_rect r, const shell *s, int index, int focused)
{
    const channel *ch = &s->cfg.channels.ch[index];
    ft_rect badge = {r.x + 12, r.y + 10, 56, 40};
    int ty = r.y + (r.h - font_label.line_height) / 2;
    focus_row(c, r, focused);
    number_badge(c, badge, ch, &font_label);
    canvas_text_fit(c, &font_label, r.x + 92, ty, r.w - 300, ch->name, focused ? COL_ON_FOCUS : COL_TEXT);
    canvas_text_right(c, &font_body, r.x + r.w - 28, r.y + (r.h - font_body.line_height) / 2, "YouTube",
                      focused ? COL_ON_FOCUS_2 : COL_TEXT_3);
}

static void row_title(ft_canvas *c, const shell *s, const shell_row *r, int y)
{
    ft_lang L = s->cfg.lang;
    const char *t = tr(L, r->title);
    canvas_text(c, &font_label, VIEW_MARGIN, y + 4, t, COL_TEXT);
    if (r->title == STR_GAMES && s->cfg.profile == PROFILE_CHILD && s->cfg.child_limit_s != 0) {
        char line[96];
        ft_rect chip = {VIEW_MARGIN + text_width(&font_label, t) + 20, y, 0, 32};
        snprintf(line, sizeof line, "%s: %u %s", tr(L, STR_TIME_LEFT),
                 (unsigned)((s->cfg.play_remaining_s + 59) / 60), tr(L, STR_MINUTES));
        chip.w = text_width(&font_caption, line) + 28;
        canvas_round(c, chip, 16, COL_ROW);
        canvas_text(c, &font_caption, chip.x + 14, chip.y + (chip.h - font_caption.line_height) / 2, line, COL_GOLD);
    }
}

static ft_color home_accent(const shell *s, const shell_row *rows, int n)
{
    const shell_card *k;
    if (s->row < 0 || s->row >= n || rows[s->row].n == 0)
        return s->cfg.channels.n > 0 ? s->cfg.channels.ch[0].accent : art_accent(ART_LIVE_TV);
    k = &rows[s->row].cards[s->col[s->row] < rows[s->row].n ? s->col[s->row] : 0];
    if (k->kind == CARD_CHANNEL)
        return s->cfg.channels.ch[k->arg].accent;
    if (k->kind == CARD_SPORTS || k->kind == CARD_FOUND || k->kind == CARD_SEARCHING)
        return FT_RGB(0x1C7A3C);
    if (k->kind == CARD_SPORT)
        return s->cfg.sports.ch[k->arg].accent;
    if (k->kind == CARD_APP)
        return FT_RGB(0x8B1A1A);
    return art_accent(card_art(*k));
}

static void home(ft_canvas *c, const shell *s, const char *clock_text, const ui_motion *m)
{
    static shell_row rows[SHELL_MAX_ROWS];
    home_geo g;
    int n = home_layout(s, rows, &g), i, j, offset;
    int zoom = m ? m->zoom : MOTION_ZOOM_FULL;
    ft_rect top_fade = {0, 0, VIEW_W, CONTENT_TOP};
    ft_rect bottom_fade = {0, CONTENT_BOTTOM - 24, VIEW_W, 32};
    ft_rect bottom_band = {0, CONTENT_BOTTOM + 8, VIEW_W, VIEW_H - CONTENT_BOTTOM - 8};

    offset = m ? m->now.page_y : g.offset;
    backdrop(c, home_accent(s, rows, n), 980, 40, 1, m);
    for (i = 0; i < n; i++) {
        const shell_row *r = &rows[i];
        int y = g.top[i] - offset, row_focus = s->row == i;
        int col = s->col[i] < r->n ? s->col[i] : 0;
        if (m ? y + g.height[i] <= CONTENT_TOP - 48 : i < g.first_row)
            continue; /* scrolled away above */
        if (y >= VIEW_H)
            break;
        if (r->title != STR_COUNT)
            row_title(c, s, r, y);
        switch (r->style) {
        case ROW_HERO:
            hero(c, card_rect(s, rows, n, &g, i, 0, m), s, r->cards[0], row_focus);
            break;
        case ROW_BANNER: {
            ft_rect b = card_rect(s, rows, n, &g, i, 0, m);
            float_card(c, b, FT_RGB(0x1C7A3C), row_focus, 0, 0);
            sports_art(c, b, 22, s, 1);
            if (row_focus)
                canvas_round_stroke(c, b, 22, 3, COL_RING);
            break;
        }
        case ROW_GUIDE:
            guide_row(c, card_rect(s, rows, n, &g, i, 0, m), s, r->cards[0].arg, row_focus);
            break;
        case ROW_SEARCH:
            search_bar(c, card_rect(s, rows, n, &g, i, 0, m), s, row_focus);
            break;
        case ROW_CARDS:
            /* Resting cards first, so the focused one lands on top. */
            for (j = 0; j < r->n; j++) {
                ft_rect k = card_rect(s, rows, n, &g, i, j, m);
                if (k.x + k.w <= 0)
                    continue;
                if (k.x >= VIEW_W)
                    break;
                if (row_focus && j == col)
                    continue;
                draw_card(c, k, s, r->cards[j], 0, m);
            }
            if (row_focus && r->n > 0) {
                ft_rect k = card_rect(s, rows, n, &g, i, col, m);
                k = lift(k, k.w * zoom / (25 * MOTION_ZOOM_FULL), k.h * zoom / (25 * MOTION_ZOOM_FULL));
                draw_card(c, k, s, r->cards[col], 1, m);
            }
            break;
        }
    }
    if (offset > 0)
        canvas_gradient(c, top_fade, color_alpha(COL_BG_TOP, 0xF5), color_alpha(COL_BG_TOP, 0x00));
    canvas_gradient(c, bottom_fade, color_alpha(COL_BG_BOTTOM, 0x00), color_alpha(COL_BG_BOTTOM, 0xF2));
    canvas_fill(c, bottom_band, color_alpha(COL_BG_BOTTOM, 0xF2));
    top_bar(c, s, clock_text, 1, m);
    toast(c, s->status, 600);
    hint(c, tr(s->cfg.lang, STR_HELP_HOME));
}

/* ---- lists (Settings, Parent settings) --------------------------------- */

ft_rect view_list_row_rect(int visible_row)
{
    ft_rect r = {LIST_X, LIST_TOP + visible_row * LIST_STEP, LIST_W, LIST_ROW_H};
    return r;
}

/* "Label: value" rows show the value right-aligned, Settings style. */
static void split_label(const char *in, char *label, char *value, size_t n)
{
    const char *sep = strstr(in, ": "), *next;
    while (sep && (next = strstr(sep + 2, ": ")) != NULL)
        sep = next;
    if (!sep) {
        snprintf(label, n, "%s", in);
        value[0] = '\0';
        return;
    }
    snprintf(label, n, "%.*s", (int)(sep - in), in);
    snprintf(value, n, "%s", sep + 2);
}

static void list_row(ft_canvas *c, ft_rect r, const char *text, int focused)
{
    char label[SHELL_LABEL_MAX], value[SHELL_LABEL_MAX];
    int value_w, ty;
    split_label(text, label, value, sizeof label);
    focus_row(c, r, focused);
    value_w = value[0] ? text_width(&font_body, value) : 0;
    ty = r.y + (r.h - font_label.line_height) / 2;
    canvas_text_fit(c, &font_label, r.x + 28, ty, r.w - 56 - (value_w ? value_w + 24 : 0), label,
                    focused ? COL_ON_FOCUS : COL_TEXT);
    if (value[0])
        canvas_text_right(c, &font_body, r.x + r.w - 28, r.y + (r.h - font_body.line_height) / 2, value,
                          focused ? COL_ON_FOCUS_2 : COL_TEXT_2);
}

static void list(ft_canvas *c, const shell *s, const char *clock_text)
{
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int n = shell_list(s, labels, SHELL_MAX_ITEMS), first = 0, i;
    ft_lang L = s->cfg.lang;
    art_id art = s->screen == SCR_SETTINGS ? ART_SETTINGS : ART_GEAR;
    ft_rect title = {0, 34, VIEW_W, font_title.line_height};
    ft_rect hero = {96, 190, 360, 216};
    ft_rect hero_sh = {hero.x + 8, hero.y + 20, hero.w - 16, hero.h - 8};
    ft_rect desc = {hero.x, hero.y + hero.h + 26, hero.w, 3 * font_body.line_height};

    backdrop(c, art_accent(art), 260, 200, 0, NULL);
    canvas_text_center(c, &font_title, title, shell_screen_title(s), COL_TEXT);
    top_bar(c, s, clock_text, 0, NULL);
    canvas_shadow(c, hero_sh, 30, 0x99000000u);
    art_tile(c, hero, 24, art, 0, NULL);
    canvas_text_wrap(c, &font_body, desc, tr(L, s->screen == SCR_SETTINGS ? STR_DESC_SETTINGS : STR_DESC_MORE),
                     COL_TEXT_2);

    first = list_first(s->item);
    for (i = first; i < n && i < first + LIST_VISIBLE; i++)
        if (i != s->item)
            list_row(c, view_list_row_rect(i - first), labels[i], 0);
    if (s->item >= first && s->item < n && s->item < first + LIST_VISIBLE)
        list_row(c, view_list_row_rect(s->item - first), labels[s->item], 1);
    if (first > 0)
        canvas_chevron(c, LIST_X + LIST_W / 2, LIST_TOP - 18, 14, 4, 1, COL_TEXT_3);
    if (first + LIST_VISIBLE < n)
        canvas_chevron(c, LIST_X + LIST_W / 2, LIST_TOP + LIST_VISIBLE * LIST_STEP + 2, 14, 4, 0, COL_TEXT_3);
    toast(c, s->status, 620);
    hint(c, tr(L, STR_HELP_LIST));
}

/* ---- PIN ---------------------------------------------------------------- */

ft_rect view_pin_box_rect(int digit)
{
    int w = 92, gap = 28, total = SHELL_PIN_DIGITS * w + (SHELL_PIN_DIGITS - 1) * gap;
    ft_rect r = {(VIEW_W - total) / 2 + digit * (w + gap), 286, w, 112};
    return r;
}

/* Only the digit being changed is drawn. Entered digits show as dots and the
 * rest as empty rings, so the picture never depends on the other digits. */
static void pin(ft_canvas *c, const shell *s, const char *clock_text)
{
    ft_lang L = s->cfg.lang;
    ft_rect title = {0, 150, VIEW_W, font_title.line_height};
    ft_rect sub = {0, 200, VIEW_W, font_body.line_height};
    int i;

    backdrop(c, art_accent(ART_SETTINGS), 640, 60, 0, NULL);
    top_bar(c, s, clock_text, 0, NULL);
    art_glyph(c, ART_SETTINGS, VIEW_W / 2, 104, 30, COL_TEXT);
    canvas_text_center(c, &font_title, title, shell_screen_title(s), COL_TEXT);
    canvas_text_center(c, &font_body, sub, tr(L, STR_PIN_HINT), COL_TEXT_2);

    for (i = 0; i < SHELL_PIN_DIGITS; i++) {
        ft_rect r = view_pin_box_rect(i);
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        if (i == s->pin_pos) {
            ft_rect big = lift(r, 6, 6);
            ft_rect sh = {big.x + 6, big.y + 14, big.w - 12, big.h - 8};
            char d[2] = {s->pin_digits[i], '\0'};
            canvas_shadow(c, sh, 22, 0x99000000u);
            canvas_round(c, big, 20, COL_FOCUS_BG);
            canvas_text_center(c, &font_display, big, d, COL_ON_FOCUS);
            canvas_chevron(c, cx, big.y - 24, 14, 4, 1, COL_TEXT);
            canvas_chevron(c, cx, big.y + big.h + 24, 14, 4, 0, COL_TEXT);
        } else {
            canvas_round(c, r, 18, COL_ROW);
            canvas_round_stroke(c, r, 18, 1, COL_GLASS_EDGE);
            if (i < s->pin_pos)
                canvas_circle(c, cx, cy, 10, COL_TEXT);
            else
                canvas_ring(c, cx, cy, 10, 2, COL_TEXT_3);
        }
    }
    toast(c, s->status, 478);
    hint(c, tr(L, STR_HELP_PIN));
}

/* ---- search keyboard ---------------------------------------------------- */

ft_rect view_key_rect(int key)
{
    int total = SHELL_KEY_COLS * KEY_W + (SHELL_KEY_COLS - 1) * KEY_GAP;
    ft_rect r = {(VIEW_W - total) / 2 + key % SHELL_KEY_COLS * (KEY_W + KEY_GAP),
                 KEYS_TOP + key / SHELL_KEY_COLS * (KEY_H + KEY_GAP), KEY_W, KEY_H};
    return r;
}

/* Every key is a little slab; the focused one lifts and lights up. */
static void keyboard(ft_canvas *c, const shell *s, const char *clock_text)
{
    ft_lang L = s->cfg.lang;
    ft_rect title = {0, 110, VIEW_W, font_title.line_height};
    ft_rect field = {VIEW_MARGIN + 120, 170, VIEW_W - 2 * VIEW_MARGIN - 240, 72};
    const char *q = s->cfg.yt_query;
    int i, tw;
    backdrop(c, FT_RGB(0xC4122F), 640, 120, 0, NULL);
    top_bar(c, s, clock_text, 0, NULL);
    canvas_text_center(c, &font_title, title, tr(L, STR_YT_SEARCH), COL_TEXT);
    canvas_shadow(c, (ft_rect){field.x + 8, field.y + 14, field.w - 16, field.h - 6}, 24, 0x99000000u);
    art_slab(c, field, 20, FT_RGB(0x2C2F38), FT_RGB(0x16181D), 1, 0, 0);
    art_glyph(c, ART_SEARCH, field.x + 42, field.y + field.h / 2, 20, COL_TEXT_2);
    canvas_text_fit(c, &font_display, field.x + 80, field.y + (field.h - font_display.line_height) / 2, field.w - 120,
                    q[0] ? q : tr(L, STR_YT_TYPE_HINT), q[0] ? COL_TEXT : COL_TEXT_3);
    tw = q[0] ? text_width(&font_display, q) : -6;
    if (q[0] && field.x + 84 + tw < field.x + field.w - 30) /* the caret */
        canvas_fill(c, (ft_rect){field.x + 84 + tw, field.y + 16, 3, field.h - 32}, 0xE6FFFFFFu);
    for (i = 0; i < SHELL_KEYS; i++) {
        ft_rect r = view_key_rect(i);
        int focused = i == s->item, wide = i >= KEY_SPACE;
        ft_color top = i == KEY_SEARCH ? FT_RGB(0xE53950) : FT_RGB(0x3A3E4A);
        ft_color bottom = i == KEY_SEARCH ? FT_RGB(0x9C0F28) : FT_RGB(0x1F2229);
        if (focused) {
            r = lift(r, 5, 5);
            canvas_shadow(c, (ft_rect){r.x + 6, r.y + 14, r.w - 12, r.h - 6}, 20, 0xB3000000u);
            if (i != KEY_SEARCH) {
                top = FT_RGB(0xFFFFFF);
                bottom = FT_RGB(0xDADCE2);
            }
        }
        art_slab(c, r, 12, top, bottom, focused, 0, 0);
        canvas_text_center(c, wide ? &font_body : &font_title, r, shell_key_label(s, i),
                           focused && i != KEY_SEARCH ? COL_ON_FOCUS : COL_TEXT);
    }
    toast(c, s->status, 620);
    hint(c, tr(L, STR_HELP_KEYBOARD));
}

void view_shell(ft_canvas *c, const shell *s, const char *clock_text)
{
    view_shell_motion(c, s, clock_text, NULL);
}

void view_shell_motion(ft_canvas *c, const shell *s, const char *clock_text, const ui_motion *m)
{
    if (s->screen == SCR_HOME)
        home(c, s, clock_text, m);
    else if (s->screen == SCR_PIN)
        pin(c, s, clock_text);
    else if (s->screen == SCR_KEYBOARD)
        keyboard(c, s, clock_text);
    else
        list(c, s, clock_text);
}

/* ---- live TV ------------------------------------------------------------ */

ft_rect view_live_panel_row_rect(int visible_row)
{
    ft_rect r = {24, PANEL_TOP + visible_row * PANEL_STEP, PANEL_W - 48, PANEL_STEP - 8};
    return r;
}

static void error_text(const player_status *ps, ft_str *title, ft_str *line)
{
    switch (ps->error) {
    case PERR_OFFLINE: *title = STR_ERR_OFFLINE; *line = STR_ERR_OFFLINE_LINE; return;
    case PERR_NOT_LIVE: *title = STR_ERR_NOT_LIVE; *line = STR_ERR_NOT_LIVE_LINE; return;
    case PERR_NO_RESOLVER: *title = STR_ERR_ENGINE; *line = STR_ERR_NO_YTDLP_LINE; return;
    case PERR_NO_JS_RUNTIME: *title = STR_ERR_ENGINE; *line = STR_ERR_NO_DENO_LINE; return;
    case PERR_NO_ENGINE: *title = STR_ERR_ENGINE; *line = STR_ERR_NO_ENGINE_LINE; return;
    case PERR_BLOCKED: *title = STR_ERR_BLOCKED; *line = STR_ERR_BLOCKED_LINE; return;
    case PERR_BAD_CHANNEL: *title = STR_ERR_BAD_CHANNEL; *line = STR_ERR_BAD_CHANNEL_LINE; return;
    case PERR_TIMEOUT: *title = STR_ERR_TIMEOUT; *line = STR_ERR_TIMEOUT_LINE; return;
    case PERR_NONE:
    case PERR_PLAYBACK:
    case PERR_COUNT: break;
    }
    *title = STR_ERR_PLAYBACK;
    *line = STR_ERR_PLAYBACK_LINE;
}

static void error_card(ft_canvas *c, const player_status *ps, const channel *ch, ft_lang L)
{
    ft_rect card = {290, 190, 700, 320};
    ft_rect name = {card.x + 40, card.y + 32, card.w - 80, font_caption.line_height};
    ft_rect title = {card.x + 40, card.y + 70, card.w - 80, font_title.line_height};
    ft_rect line = {card.x + 60, card.y + 130, card.w - 120, 3 * font_body.line_height};
    ft_str t, l;
    error_text(ps, &t, &l);
    glass(c, card, 28);
    canvas_text_center(c, &font_caption, name, ch->name, COL_TEXT_3);
    canvas_text_center(c, &font_title, title, tr(L, t), COL_TEXT);
    canvas_text_wrap(c, &font_body, line, tr(L, l), COL_TEXT_2);
    help_lines(c, &font_caption, card.x + 60, card.y + 232, card.w - 120, tr(L, STR_HELP_ERROR), COL_TEXT_3);
}

/* The banner rises in over a fifth of a second and sinks away at the end. */
static int banner_drop(const live *l)
{
    int shown_for = LIVE_BANNER_MS - l->banner_ms;
    if (l->tune_wait_ms > 0 || l->banner_ms <= 0)
        return 0;
    if (shown_for < 220)
        return (220 - shown_for) * 80 / 220;
    if (l->banner_ms < 300)
        return (300 - l->banner_ms) * 250 / 300;
    return 0;
}

/* The channel list slides in from the left. */
static int panel_shift(const live *l)
{
    return l->panel_ms < 180 ? -(int)((180 - l->panel_ms) * PANEL_W / 180) : 0;
}

static void banner(ft_canvas *c, const live *l, const player_status *ps, const channel *ch, ft_lang L,
                   const char *clock_text)
{
    int dy = banner_drop(l);
    ft_rect fade = {0, 470 + dy, VIEW_W, VIEW_H - 470};
    ft_rect badge = {VIEW_MARGIN, 560 + dy, 104, 76};
    int x = badge.x + badge.w + 24;
    canvas_gradient(c, fade, 0x00000000u, 0xE6000000u);
    number_badge(c, badge, ch, &font_display);
    canvas_text_fit(c, &font_display, x, 552 + dy, 760, ch->name, COL_TEXT);
    if (ps->state == PLAYER_PLAYING && l->shown == l->current && l->tune_wait_ms == 0) {
        live_pill(c, x, 612 + dy, L);
        x += text_width(&font_caption, tr(L, STR_LIVE_BADGE)) + 44;
    }
    if (ps->video_h > 0 && l->shown == l->current && ps->state == PLAYER_PLAYING) {
        char q[16];
        ft_rect chip = {x, 612 + dy, 0, 26};
        snprintf(q, sizeof q, "%dp", ps->video_h);
        chip.w = text_width(&font_caption, q) + 20;
        canvas_round_stroke(c, chip, 6, 1, COL_TEXT_2);
        canvas_text_center(c, &font_caption, chip, q, COL_TEXT_2);
        x += chip.w + 14;
    }
    canvas_text(c, &font_caption, x, 612 + dy + (26 - font_caption.line_height) / 2, "YouTube", COL_TEXT_3);
    if (clock_text)
        canvas_text_right(c, &font_title, VIEW_W - VIEW_MARGIN, 566 + dy, clock_text, COL_TEXT);
    canvas_text_fit(c, &font_caption, VIEW_MARGIN, HINT_Y + dy, VIEW_W - 2 * VIEW_MARGIN, tr(L, STR_HELP_LIVE),
                    COL_TEXT_3);
}

static void volume_bar(ft_canvas *c, int volume)
{
    ft_rect pill = {VIEW_W - VIEW_MARGIN - 340, 40, 340, 60};
    ft_rect track = {pill.x + 70, pill.y + 27, 190, 6};
    ft_rect fill = track;
    char num[12];
    glass(c, pill, 30);
    art_glyph(c, ART_VOLUME, pill.x + 38, pill.y + 30, 15, COL_TEXT);
    canvas_round(c, track, 3, COL_ROW);
    fill.w = track.w * volume / 100;
    if (fill.w > 0)
        canvas_round(c, fill, 3, COL_TEXT);
    snprintf(num, sizeof num, "%d", volume);
    canvas_text_right(c, &font_label, pill.x + pill.w - 24, pill.y + (pill.h - font_label.line_height) / 2, num,
                      COL_TEXT);
}

static void channel_panel(ft_canvas *c, const live *l, const channel_list *chs, ft_lang L)
{
    int dx = panel_shift(l);
    ft_rect panel = {dx, 0, PANEL_W, VIEW_H};
    ft_rect edge = {dx + PANEL_W - 1, 0, 1, VIEW_H};
    int first = l->panel_item - PANEL_VISIBLE / 2, i;
    if (first > chs->n - PANEL_VISIBLE)
        first = chs->n - PANEL_VISIBLE;
    if (first < 0)
        first = 0;
    canvas_fill(c, panel, COL_PANEL);
    canvas_fill(c, edge, COL_GLASS_EDGE);
    canvas_text(c, &font_title, dx + 40, 40, tr(L, STR_ALL_CHANNELS), COL_TEXT);
    for (i = first; i < chs->n && i < first + PANEL_VISIBLE; i++) {
        ft_rect r = view_live_panel_row_rect(i - first);
        ft_rect badge;
        r.x += dx;
        badge.x = r.x + 10;
        badge.y = r.y + 7;
        badge.w = 50;
        badge.h = r.h - 14;
        int focused = i == l->panel_item;
        if (focused)
            canvas_round(c, r, 12, COL_FOCUS_BG);
        number_badge(c, badge, &chs->ch[i], &font_body);
        canvas_text_fit(c, &font_label, r.x + 76, r.y + (r.h - font_label.line_height) / 2, r.w - 120,
                        chs->ch[i].name, focused ? COL_ON_FOCUS : COL_TEXT);
        if (i == l->current)
            canvas_circle(c, r.x + r.w - 20, r.y + r.h / 2, 6, COL_LIVE);
    }
    if (first > 0)
        canvas_chevron(c, dx + PANEL_W / 2, PANEL_TOP - 14, 12, 4, 1, COL_TEXT_3);
    if (first + PANEL_VISIBLE < chs->n)
        canvas_chevron(c, dx + PANEL_W / 2, PANEL_TOP + PANEL_VISIBLE * PANEL_STEP + 4, 12, 4, 0, COL_TEXT_3);
    help_lines(c, &font_caption, dx + 40, 628, PANEL_W - 80, tr(L, STR_HELP_PANEL), COL_TEXT_3);
}

static uint32_t mix(uint32_t h, uint32_t v)
{
    return (h ^ v) * 16777619u;
}

uint32_t view_live_signature(const live *l, const player_status *ps, const shell *s, const channel_list *chs,
                             const char *clock_text, int has_picture)
{
    int zapping = l->tune_wait_ms > 0 || l->shown != l->current;
    int spinning = !has_picture || zapping || ps->state == PLAYER_BUFFERING;
    uint32_t h = 2166136261u;
    const char *p;
    h = mix(h, (uint32_t)l->shown);
    h = mix(h, (uint32_t)l->current);
    h = mix(h, (uint32_t)zapping);
    h = mix(h, (uint32_t)(l->banner_ms > 0));
    h = mix(h, (uint32_t)banner_drop(l));
    h = mix(h, (uint32_t)(l->panel_open ? panel_shift(l) : 0));
    h = mix(h, (uint32_t)(l->volume_ms > 0 ? l->volume + 1 : 0));
    h = mix(h, (uint32_t)(l->panel_open ? l->panel_item + 1 : 0));
    h = mix(h, spinning ? l->anim_ms / 110 + 1 : 0); /* the spinner's step */
    h = mix(h, (uint32_t)ps->state);
    h = mix(h, (uint32_t)ps->error);
    h = mix(h, (uint32_t)ps->video_h);
    h = mix(h, (uint32_t)s->cfg.lang);
    h = mix(h, (uint32_t)(uintptr_t)chs);
    h = mix(h, (uint32_t)has_picture);
    for (p = clock_text ? clock_text : ""; *p; p++)
        h = mix(h, (unsigned char)*p);
    return h;
}

void view_live(ft_canvas *c, const live *l, const player_status *ps, const shell *s, const channel_list *chs,
               const char *clock_text, int has_picture)
{
    ft_lang L = s->cfg.lang;
    const channel *shown;
    int zapping = l->tune_wait_ms > 0 || l->shown != l->current;
    int failed = ps->state == PLAYER_ERROR && !zapping;

    if (chs->n <= 0 || l->shown < 0 || l->shown >= chs->n) {
        canvas_clear(c, FT_RGB(0x000000));
        return;
    }
    shown = &chs->ch[l->shown];
    if (!has_picture || zapping || failed) {
        /* Never leave the old channel's picture under a new name. */
        canvas_clear(c, FT_RGB(0x000000));
        canvas_glow(c, VIEW_W / 2, 330, 560, 320, color_alpha(shown->accent, 0x58));
    }
    if (failed) {
        error_card(c, ps, shown, L);
    } else if (!has_picture || zapping) {
        ft_rect name = {0, 236, VIEW_W, font_display.line_height};
        ft_rect what = {0, 404, VIEW_W, font_body.line_height};
        char line[96];
        snprintf(line, sizeof line, "%s %s...", tr(L, STR_CONNECTING), shown->name);
        canvas_text_center(c, &font_display, name, shown->name, COL_TEXT);
        spinner(c, VIEW_W / 2, 350, 24, l->anim_ms);
        canvas_text_center(c, &font_body, what, ps->state == PLAYER_BUFFERING ? tr(L, STR_BUFFERING) : line,
                           COL_TEXT_2);
    } else if (ps->state == PLAYER_BUFFERING) {
        spinner(c, VIEW_W / 2, VIEW_H / 2, 24, l->anim_ms);
    }
    if (l->banner_ms > 0 || zapping)
        banner(c, l, ps, shown, L, clock_text);
    if (l->volume_ms > 0)
        volume_bar(c, l->volume);
    if (l->panel_open)
        channel_panel(c, l, chs, L);
}

/* ---- start-up ------------------------------------------------------------ */

/* 0..256 over [from, from + len) ms, eased out (fast, then settling). */
static int ease_out(uint32_t t, uint32_t from, uint32_t len)
{
    int u;
    if (t <= from)
        return 0;
    if (t >= from + len)
        return 256;
    u = (int)((t - from) * 256 / len);
    return 256 - (256 - u) * (256 - u) / 256;
}

/* 0..256 with a small overshoot past 256 before settling ("back" easing). */
static int ease_back(uint32_t t, uint32_t from, uint32_t len)
{
    long v;
    if (t <= from)
        return 0;
    if (t >= from + len)
        return 256;
    v = (long)((t - from) * 256 / len) - 256; /* -256..0 */
    return (int)(256 + 691 * v * v * v / (256L * 256 * 256) + 435 * v * v / (256L * 256));
}

/* A TV switching on: a glow opens up, the Family TV icon pops in with a
 * small overshoot, a ring pulses out, the name rises in, then the whole
 * picture fades into the home screen. Pure function of time. */
void view_startup(ft_canvas *c, uint32_t t, ft_lang lang)
{
    int glow = ease_out(t, 0, 700), pop = ease_back(t, 150, 650), words = ease_out(t, 750, 600);
    int ring = ease_out(t, 600, 900), out = ease_out(t, 2150, 450);
    int size = 40 + 160 * pop / 256, cx = VIEW_W / 2, cy = 290;
    ft_rect tile;
    ft_rect name = {0, 420 + 30 * (256 - words) / 256, VIEW_W, font_display.line_height};
    ft_rect sub = {0, 486 + 30 * (256 - words) / 256, VIEW_W, font_body.line_height};

    canvas_clear(c, FT_RGB(0x000000));
    canvas_glow(c, cx, cy, 120 + 560 * glow / 256, 80 + 300 * glow / 256, color_alpha(FT_RGB(0x2440B0), (unsigned)(0xA0 * glow / 256)));
    if (ring > 0 && ring < 256)
        canvas_ring(c, cx, cy, 110 + 260 * ring / 256, 3, color_alpha(COL_TEXT, (unsigned)(0x90 * (256 - ring) / 256)));
    if (pop > 0) {
        tile.x = cx - size / 2;
        tile.y = cy - size / 2;
        tile.w = size;
        tile.h = size;
        canvas_shadow(c, (ft_rect){tile.x + 8, tile.y + 20, tile.w - 16, tile.h - 8}, 30, 0x99000000u);
        art_tile(c, tile, size / 5, ART_LIVE_TV, 1, NULL);
    }
    if (words > 0) {
        canvas_text_center(c, &font_display, name, tr(lang, STR_FAMILY_TV), color_alpha(COL_TEXT, (unsigned)words * 255 / 256));
        canvas_text_center(c, &font_body, sub, tr(lang, STR_STARTUP_LINE),
                           color_alpha(COL_TEXT_2, (unsigned)(0xB3 * words / 256)));
    }
    if (out > 0) {
        ft_rect all = {0, 0, VIEW_W, VIEW_H};
        canvas_fill(c, all, color_alpha(COL_BG_TOP, (unsigned)(out * 255 / 256)));
    }
}

/* ---- Family Racer ------------------------------------------------------- */

#define ROAD_LANE_W 130
#define ROAD_W (ROAD_LANE_W * RACER_LANES)
#define ROAD_X ((VIEW_W - ROAD_W) / 2)
#define ROW_H (VIEW_H / RACER_ROWS)

static void car(ft_canvas *c, int lane, int row, ft_color top, ft_color bottom)
{
    ft_rect b = {ROAD_X + lane * ROAD_LANE_W + 27, row * ROW_H + 5, ROAD_LANE_W - 54, ROW_H - 10};
    ft_rect sh = {b.x + 4, b.y + 8, b.w - 8, b.h - 4};
    ft_rect front = {b.x + 10, b.y + 10, b.w - 20, 12};
    ft_rect rear = {b.x + 12, b.y + b.h - 15, b.w - 24, 7};
    canvas_shadow(c, sh, 10, 0x99000000u);
    canvas_round_gradient(c, b, 14, CORNERS_ALL, top, bottom);
    canvas_round(c, front, 5, 0xC2101820u);
    canvas_round(c, rear, 3, 0x8C101820u);
}

static const char *level_name(ft_lang L, racer_level level)
{
    static const ft_str names[RACER_LEVELS] = {STR_LEVEL_EASY, STR_LEVEL_MEDIUM, STR_LEVEL_HARD};
    return tr(L, names[level < RACER_LEVELS ? level : RACER_EASY]);
}

static void stat(ft_canvas *c, int x, int y, const char *label, const char *value)
{
    canvas_text(c, &font_caption, x, y, label, COL_TEXT_2);
    canvas_text(c, &font_title, x, y + 20, value, COL_TEXT);
}

void view_racer(ft_canvas *c, const racer *r, const uint32_t best[RACER_LEVELS], ft_lang L, const char *status)
{
    ft_rect road = {ROAD_X, 0, ROAD_W, VIEW_H};
    ft_rect hud = {36, 36, 340, 272}, help;
    uint32_t left = r->tick >= RACER_DURATION_TICKS ? 0 : (RACER_DURATION_TICKS - r->tick) / FT_TICK_HZ;
    int offset = (int)(r->tick * 6 % 96), row, lane, i, y;
    char value[32];

    striped_rows(c, FT_RGB(0x236A3F), FT_RGB(0x113922), 48, offset, 0);
    {
        /* Shadow only along the edges; the road itself covers the middle. */
        ft_rect left = {ROAD_X, -40, 1, VIEW_H + 80}, right = {ROAD_X + ROAD_W - 1, -40, 1, VIEW_H + 80};
        canvas_shadow(c, left, 26, 0xA6000000u);
        canvas_shadow(c, right, 26, 0xA6000000u);
    }
    canvas_gradient(c, road, FT_RGB(0x363C48), FT_RGB(0x2A2F39));
    {
        ft_rect el = {ROAD_X - 6, 0, 6, VIEW_H}, er = {ROAD_X + ROAD_W, 0, 6, VIEW_H};
        canvas_fill(c, el, 0xE6FFFFFFu);
        canvas_fill(c, er, 0xE6FFFFFFu);
    }
    for (lane = 1; lane < RACER_LANES; lane++)
        for (y = offset % 60 - 60; y < VIEW_H; y += 60) {
            ft_rect dash = {ROAD_X + lane * ROAD_LANE_W - 3, y, 6, 30};
            canvas_round(c, dash, 3, 0xB8FFFFFFu);
        }
    for (row = 0; row < RACER_ROWS; row++)
        for (lane = 0; lane < RACER_LANES; lane++)
            if (r->rows[row] & (1u << lane))
                car(c, lane, row, FT_RGB(0xFFA552), FT_RGB(0xE5591A));
    car(c, r->lane, RACER_CAR_ROW, FT_RGB(0x5EEAD4), FT_RGB(0x0F9488));

    glass(c, hud, 24);
    canvas_text(c, &font_label, 60, 54, tr(L, STR_GAME_RACER), COL_TEXT);
    {
        const char *chip_text = r->practice ? tr(L, STR_PRACTICE) : level_name(L, r->level);
        ft_rect chip = {60, 92, text_width(&font_caption, chip_text) + 28, 30};
        canvas_round(c, chip, 15, r->practice ? 0x33FFD24Au : COL_ROW);
        canvas_text(c, &font_caption, chip.x + 14, chip.y + (chip.h - font_caption.line_height) / 2, chip_text,
                    r->practice ? COL_GOLD : COL_TEXT_2);
    }
    snprintf(value, sizeof value, "%u", (unsigned)r->score);
    stat(c, 60, 140, tr(L, STR_SCORE), value);
    snprintf(value, sizeof value, "%u", (unsigned)best[r->level]);
    stat(c, 210, 140, tr(L, STR_BEST), value);
    snprintf(value, sizeof value, "%u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
    stat(c, 60, 214, tr(L, STR_TIME), value);
    if (!r->practice) {
        canvas_text(c, &font_caption, 210, 214, tr(L, STR_LIVES), COL_TEXT_2);
        for (i = 0; i < RACER_LIVES; i++)
            canvas_circle(c, 222 + i * 32, 254, 10, i < r->lives ? COL_BAD : 0x33FFFFFFu);
    }

    {
        const char *text = tr(L, r->state == RACER_START ? STR_RACER_START_HELP : STR_RACER_HELP);
        help.x = ROAD_X + ROAD_W + 36;
        help.y = 36;
        help.w = VIEW_W - help.x - 36;
        help.h = 36 + help_lines(NULL, &font_body, 0, 0, 0, text, 0) * font_body.line_height;
        glass(c, help, 20);
        help_lines(c, &font_body, help.x + 22, help.y + 18, help.w - 44, text, COL_TEXT_2);
    }

    switch (r->state) {
    case RACER_START:
        overlay(c, tr(L, STR_READY), r->practice ? tr(L, STR_PRACTICE) : level_name(L, r->level),
                tr(L, STR_RACER_START_HELP));
        break;
    case RACER_PAUSED:
        overlay(c, tr(L, STR_PAUSED), NULL, tr(L, STR_PAUSED_HELP));
        break;
    case RACER_CRASHED:
    case RACER_FINISHED:
        snprintf(value, sizeof value, "%s %u", tr(L, STR_SCORE), (unsigned)r->score);
        overlay(c, tr(L, r->state == RACER_FINISHED ? STR_FINISHED : STR_RACE_OVER), value, tr(L, STR_AGAIN_HELP));
        break;
    case RACER_RUNNING:
    case RACER_EXITED:
        break;
    }
    toast(c, status, 24);
}

/* ---- Penalty Cup -------------------------------------------------------- */

#define GOAL_X 340
#define GOAL_Y 100
#define GOAL_W 600
#define GOAL_H 300

static void target_pos(int t, int *x, int *y)
{
    static const int tx[PEN_TARGETS] = {430, 430, 640, 850, 850};
    static const int ty[PEN_TARGETS] = {170, 330, 250, 330, 170};
    *x = tx[t];
    *y = ty[t];
}

static void keeper(ft_canvas *c, int x, int y)
{
    ft_rect body = {x - 34, y - 40, 68, 100};
    canvas_glow(c, x, y + 92, 80, 16, 0x8C000000u);
    canvas_line(c, x - 28, y - 26, x - 70, y - 48, 18, FT_RGB(0xF7B733));
    canvas_line(c, x + 28, y - 26, x + 70, y - 48, 18, FT_RGB(0xF7B733));
    canvas_circle(c, x - 72, y - 50, 13, COL_TEXT);
    canvas_circle(c, x + 72, y - 50, 13, COL_TEXT);
    canvas_round_gradient(c, body, 20, CORNERS_ALL, FT_RGB(0xFFD84D), FT_RGB(0xF09A12));
    canvas_circle(c, x, y - 64, 22, FT_RGB(0x7A4A2E));
}

static void ball_at(ft_canvas *c, int x, int y)
{
    canvas_glow(c, x + 4, y + 24, 30, 9, 0x80000000u);
    art_glyph(c, ART_FOOTBALL, x, y, 34, COL_TEXT);
}

static void shot_marks(ft_canvas *c, const penalty *p, int player, int x, int y, ft_lang L)
{
    char line[48];
    int shots = p->shots[player], shown = shots > PEN_REGULATION ? shots : PEN_REGULATION;
    int start = shown > 5 ? shown - 5 : 0, i, to_shoot = penalty_turn(p) == player && p->phase == PEN_AIMING;
    snprintf(line, sizeof line, "%s %d", tr(L, STR_PLAYER), player + 1);
    canvas_text(c, &font_label, x, y, line, to_shoot ? COL_GOLD : COL_TEXT);
    snprintf(line, sizeof line, "%d", p->goals[player]);
    canvas_text_right(c, &font_title, x + 224, y - 4, line, COL_TEXT);
    for (i = start; i < shown; i++) {
        int cx = x + 16 + (i - start) * 44, cy = y + 62;
        ft_rect mark = {cx - 14, cy - 14, 28, 28};
        if (i >= shots) {
            canvas_ring(c, cx, cy, 14, 2, COL_TEXT_3);
            continue;
        }
        if ((p->history[player] >> i) & 1u) {
            canvas_circle(c, cx, cy, 14, COL_GOOD);
            canvas_text_center(c, &font_caption, mark, "O", COL_TEXT);
        } else {
            canvas_circle(c, cx, cy, 14, COL_BAD);
            canvas_text_center(c, &font_caption, mark, "X", COL_TEXT);
        }
    }
}

void view_penalty(ft_canvas *c, const penalty *p, ft_lang L, const char *status)
{
    ft_rect goal = {GOAL_X, GOAL_Y, GOAL_W, GOAL_H};
    ft_rect board = {32, 100, 280, 250};
    int i, x, y, kx = 640, ky = 300;
    char line[96];

    striped_rows(c, FT_RGB(0x2F8E50), FT_RGB(0x165229), 80, 0, 1);
    {
        ft_rect sh = {goal.x, goal.y + 14, goal.w, goal.h};
        ft_rect line_r = {0, GOAL_Y + GOAL_H, VIEW_W, 3};
        canvas_shadow(c, sh, 30, 0x73000000u);
        canvas_fill(c, goal, 0x52000000u);
        canvas_fill(c, line_r, 0xB3FFFFFFu);
    }
    for (x = GOAL_X + 40; x < GOAL_X + GOAL_W; x += 40) {
        ft_rect v = {x, GOAL_Y, 2, GOAL_H};
        canvas_fill(c, v, 0x30FFFFFFu);
    }
    for (y = GOAL_Y + 40; y < GOAL_Y + GOAL_H; y += 40) {
        ft_rect h = {GOAL_X, y, GOAL_W, 2};
        canvas_fill(c, h, 0x30FFFFFFu);
    }
    {
        ft_rect bar = {GOAL_X - 14, GOAL_Y - 14, GOAL_W + 28, 14};
        ft_rect post_l = {GOAL_X - 14, GOAL_Y - 14, 14, GOAL_H + 14};
        ft_rect post_r = {GOAL_X + GOAL_W, GOAL_Y - 14, 14, GOAL_H + 14};
        canvas_round(c, post_l, 6, COL_TEXT);
        canvas_round(c, post_r, 6, COL_TEXT);
        canvas_round(c, bar, 6, COL_TEXT);
    }

    for (i = 0; i < PEN_TARGETS; i++) {
        target_pos(i, &x, &y);
        if (p->phase == PEN_AIMING && i == p->aim) {
            canvas_glow(c, x, y, 76, 76, 0x66FFFFFFu);
            canvas_circle(c, x, y, 34, COL_FOCUS_BG);
            canvas_line(c, x - 14, y, x + 14, y, 4, COL_ON_FOCUS);
            canvas_line(c, x, y - 14, x, y + 14, 4, COL_ON_FOCUS);
            canvas_ring(c, x, y, 42, 3, COL_TEXT);
        } else {
            canvas_ring(c, x, y, 32, 4, 0xCCFFFFFFu);
        }
    }

    if (p->phase == PEN_RESULT || p->phase == PEN_OVER)
        target_pos(p->keeper, &kx, &ky);
    keeper(c, kx, ky + 20);
    if (p->phase == PEN_RESULT || p->phase == PEN_OVER) {
        target_pos(p->aim, &x, &y);
        if (!p->scored) {
            x = kx + 62;
            y = ky + 66;
        }
        ball_at(c, x, y);
    } else {
        ball_at(c, 640, 456);
    }

    glass(c, board, 22);
    shot_marks(c, p, 0, 56, 120, L);
    shot_marks(c, p, 1, 56, 236, L);
    if (p->sudden_death) {
        const char *sd = tr(L, STR_SUDDEN_DEATH);
        ft_rect chip = {board.x, board.y + board.h + 14, text_width(&font_caption, sd) + 28, 30};
        canvas_round(c, chip, 15, COL_ROW);
        canvas_text(c, &font_caption, chip.x + 14, chip.y + (chip.h - font_caption.line_height) / 2, sd, COL_TEXT_2);
    }

    switch (p->phase) {
    case PEN_AIMING: {
        ft_rect pill;
        snprintf(line, sizeof line, "%s %d %s", tr(L, STR_PLAYER), penalty_turn(p) + 1, tr(L, STR_TO_SHOOT));
        pill.w = text_width(&font_label, line) + 64;
        pill.h = 52;
        pill.x = (VIEW_W - pill.w) / 2;
        pill.y = 520;
        glass(c, pill, 26);
        canvas_text_center(c, &font_label, pill, line, COL_TEXT);
        hint(c, tr(L, STR_PEN_HELP));
        break;
    }
    case PEN_RESULT: {
        ft_rect big = {GOAL_X - 100, 492, GOAL_W + 200, font_display.line_height};
        shadowed_center(c, &font_display, big, tr(L, p->scored ? STR_GOAL : STR_SAVED), p->scored ? COL_GOLD : COL_TEXT);
        if (p->winner == PEN_NO_WINNER) {
            ft_rect sub = {big.x, big.y + big.h + 6, big.w, font_body.line_height};
            snprintf(line, sizeof line, "%s %s %d", tr(L, STR_PASS_REMOTE), tr(L, STR_PLAYER), penalty_turn(p) + 1);
            canvas_text_center(c, &font_body, sub, line, COL_TEXT_2);
        }
        hint(c, tr(L, STR_CONTINUE_HELP));
        break;
    }
    case PEN_OVER:
        if (p->winner == PEN_TIE)
            snprintf(line, sizeof line, "%s", tr(L, STR_DRAW));
        else
            snprintf(line, sizeof line, "%s %d %s", tr(L, STR_PLAYER), p->winner + 1, tr(L, STR_WINS));
        overlay(c, tr(L, STR_GAME_PENALTY), line, tr(L, STR_NEW_MATCH_HELP));
        break;
    case PEN_PAUSED:
        overlay(c, tr(L, STR_PAUSED), NULL, tr(L, STR_PAUSED_HELP));
        break;
    case PEN_EXITED:
        break;
    }
    toast(c, status, 24);
}

/* ---- Block Garden ------------------------------------------------------- */

#define CELL 18
#define MAP_X ((VIEW_W - GARDEN_W * CELL) / 2)
#define MAP_Y 72
#define MAP_ROWS 27

static const ft_str TILE_NAMES[GARDEN_TILE_TYPES] = {
    STR_TILE_ERASE, STR_TILE_GRASS, STR_TILE_DIRT, STR_TILE_STONE,
    STR_TILE_WOOD, STR_TILE_WATER, STR_TILE_FLOWER, STR_TILE_BRICK
};

/* Each tile has a pattern as well as a colour, so tiles differ by shape too. */
static void tile(ft_canvas *c, int x, int y, int size, int t)
{
    ft_rect r = {x, y, size, size};
    int q = size / 4;
    switch (t) {
    case 1: {
        ft_rect top = {x, y, size, q};
        canvas_fill(c, r, FT_RGB(0x4CAF50));
        canvas_fill(c, top, FT_RGB(0x7CCB62));
        break;
    }
    case 2: {
        ft_rect d1 = {x + q, y + q, 2, 2}, d2 = {x + 3 * q, y + 2 * q, 2, 2}, d3 = {x + q + 1, y + 3 * q, 2, 2};
        canvas_fill(c, r, FT_RGB(0x8B5E3C));
        canvas_fill(c, d1, FT_RGB(0x5E3E26));
        canvas_fill(c, d2, FT_RGB(0x5E3E26));
        canvas_fill(c, d3, FT_RGB(0x5E3E26));
        break;
    }
    case 3: {
        ft_rect crack = {x + q, y + 2 * q, 2 * q, 2}, dot = {x + 3 * q, y + q, 2, 2};
        canvas_fill(c, r, FT_RGB(0x9AA5B1));
        canvas_fill(c, crack, FT_RGB(0x6C7682));
        canvas_fill(c, dot, FT_RGB(0x6C7682));
        break;
    }
    case 4: {
        ft_rect a = {x + q, y, 2, size}, b = {x + 3 * q, y, 2, size};
        canvas_fill(c, r, FT_RGB(0xB9853F));
        canvas_fill(c, a, FT_RGB(0x8A5F28));
        canvas_fill(c, b, FT_RGB(0x8A5F28));
        break;
    }
    case 5: {
        ft_rect w1 = {x + 2, y + q, size / 2, 2}, w2 = {x + size / 2 - 2, y + 3 * q, size / 2, 2};
        canvas_fill(c, r, FT_RGB(0x2F80D8));
        canvas_fill(c, w1, FT_RGB(0x9CC9F5));
        canvas_fill(c, w2, FT_RGB(0x9CC9F5));
        break;
    }
    case 6:
        canvas_fill(c, r, FT_RGB(0x4CAF50));
        canvas_circle(c, x + size / 2, y + size / 2, size / 3, FT_RGB(0xF06292));
        canvas_circle(c, x + size / 2, y + size / 2, size / 8 + 1, FT_RGB(0xFFE066));
        break;
    case 7: {
        ft_rect m1 = {x, y + size / 2 - 1, size, 2}, m2 = {x + size / 2 - 1, y, 2, size / 2},
                m3 = {x + q - 1, y + size / 2, 2, size / 2};
        canvas_fill(c, r, FT_RGB(0xB4513A));
        canvas_fill(c, m1, FT_RGB(0xE8D6C8));
        canvas_fill(c, m2, FT_RGB(0xE8D6C8));
        canvas_fill(c, m3, FT_RGB(0xE8D6C8));
        break;
    }
    default:
        canvas_fill(c, r, FT_RGB(0xCFE8F7));
        break;
    }
}

void view_garden(ft_canvas *c, const garden *g, ft_lang L, const char *status)
{
    static const ft_str MENU[GMI_COUNT] = {
        STR_MENU_RESUME, STR_MENU_TILE, STR_MENU_SAVE, STR_MENU_SAVE_EXIT, STR_MENU_EXIT
    };
    ft_rect all = {0, 0, VIEW_W, VIEW_H};
    ft_rect frame = {MAP_X - 8, MAP_Y - 8, GARDEN_W * CELL + 16, MAP_ROWS * CELL + 16};
    int y0 = g->cy - MAP_ROWS / 2, x, y, i;
    const char *state = tr(L, g->dirty ? STR_UNSAVED : STR_SAVED_STATE);

    if (y0 < 0)
        y0 = 0;
    if (y0 > GARDEN_H - MAP_ROWS)
        y0 = GARDEN_H - MAP_ROWS;

    backdrop(c, art_accent(ART_GARDEN), 640, -160, 0, NULL);
    canvas_text(c, &font_label, MAP_X, 22, tr(L, STR_GAME_GARDEN), COL_TEXT);
    if (status && status[0])
        canvas_text_right(c, &font_body, VIEW_W - MAP_X, 26, status, COL_GOLD);
    else
        canvas_text_right(c, &font_body, VIEW_W - MAP_X, 26, state, g->dirty ? COL_GOLD : COL_TEXT_2);

    glass(c, frame, 16);
    for (y = 0; y < MAP_ROWS; y++)
        for (x = 0; x < GARDEN_W; x++)
            tile(c, MAP_X + x * CELL, MAP_Y + y * CELL, CELL, garden_get(g, x, y0 + y));
    {
        int cx = MAP_X + g->cx * CELL, cy = MAP_Y + (g->cy - y0) * CELL;
        ft_rect outer = {cx - 5, cy - 5, CELL + 10, CELL + 10}, inner = {cx - 1, cy - 1, CELL + 2, CELL + 2};
        canvas_shadow(c, outer, 8, 0x99000000u);
        tile(c, cx, cy, CELL, g->selected);
        canvas_round_stroke(c, outer, 6, 3, COL_TEXT);
        canvas_round_stroke(c, inner, 2, 1, 0xCC000000u);
    }

    /* Palette is always visible (plan p.23). */
    for (i = 0; i < GARDEN_TILE_TYPES; i++) {
        ft_rect r = {MAP_X + i * 144, 588, 136, 60};
        int on = i == g->selected;
        if (on) {
            ft_rect big = lift(r, g->mode == GM_PALETTE ? 6 : 3, g->mode == GM_PALETTE ? 4 : 2);
            ft_rect sh = {big.x + 4, big.y + 10, big.w - 8, big.h - 4};
            canvas_shadow(c, sh, 16, 0x99000000u);
            canvas_round(c, big, 16, COL_FOCUS_BG);
        } else {
            canvas_round(c, r, 14, COL_ROW);
        }
        tile(c, r.x + 12, r.y + 14, 32, i);
        canvas_text_fit(c, &font_caption, r.x + 54, r.y + (r.h - font_caption.line_height) / 2, r.w - 60,
                        tr(L, TILE_NAMES[i]), on ? COL_ON_FOCUS : COL_TEXT);
    }

    if (g->mode == GM_MENU) {
        ft_rect card = {390, 110, 500, 450};
        canvas_fill(c, all, COL_DIM);
        glass(c, card, 28);
        canvas_text(c, &font_title, card.x + 32, card.y + 26, tr(L, STR_MENU), COL_TEXT);
        for (i = 0; i < GMI_COUNT; i++) {
            ft_rect row = {card.x + 24, card.y + 92 + i * 68, card.w - 48, 56};
            if (i == g->menu) {
                ft_rect big = lift(row, 6, 2);
                canvas_shadow(c, big, 14, 0x80000000u);
                canvas_round(c, big, 14, COL_FOCUS_BG);
            } else {
                canvas_round(c, row, 12, COL_ROW);
            }
            canvas_text_fit(c, &font_label, row.x + 22, row.y + (row.h - font_label.line_height) / 2, row.w - 44,
                            tr(L, MENU[i]), i == g->menu ? COL_ON_FOCUS : COL_TEXT);
        }
        hint(c, tr(L, STR_HELP_LIST));
    } else {
        hint(c, tr(L, g->mode == GM_PALETTE ? STR_PALETTE_HELP : STR_GARDEN_HELP));
    }
}
