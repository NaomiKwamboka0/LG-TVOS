#include "view.h"

#include <stdio.h>
#include <string.h>

#include "art.h"

/* ---- layout ------------------------------------------------------------ */

#define TOP_Y 28
#define TOP_H 44
#define CONTENT_TOP 96
#define CONTENT_BOTTOM 668
#define HERO_H 260
#define CARD_W 236
#define CARD_H 133
#define CARD_GAP 24
#define CARD_STEP (CARD_W + CARD_GAP)
#define CARDS_VISIBLE 4 /* fully visible per row; the next one peeks in */
#define ROW_TITLE_H 40
#define GUIDE_H 60
#define GUIDE_STEP 70
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

/* The glowing backdrop is the most expensive layer. The glow shape depends
 * only on the screen layout, so its alpha mask is built once per layout and
 * recoloured per accent; the finished backdrop is kept until the accent
 * changes. Buffers: 3.5 MiB + 0.9 MiB of the plan's render budget (p.24). */
static uint32_t backdrop_cache[VIEW_W * VIEW_H];
static uint8_t glow_mask[VIEW_W * VIEW_H];
static struct {
    int valid, glow_x, glow_y, fade;
    ft_color accent;
} backdrop_key;
static struct {
    int valid, glow_x, glow_y;
} mask_key;

static void backdrop(ft_canvas *c, ft_color accent, int glow_x, int glow_y, int fade)
{
    ft_rect low = {0, 360, VIEW_W, VIEW_H - 360};
    int y;
    if (c->w != VIEW_W || c->h != VIEW_H)
        return;
    if (backdrop_key.valid && backdrop_key.accent == accent && backdrop_key.glow_x == glow_x &&
        backdrop_key.glow_y == glow_y && backdrop_key.fade == fade) {
        for (y = 0; y < VIEW_H; y++)
            memcpy(c->px + (long)y * c->stride, backdrop_cache + (long)y * VIEW_W, VIEW_W * sizeof(uint32_t));
        return;
    }
    if (!mask_key.valid || mask_key.glow_x != glow_x || mask_key.glow_y != glow_y) {
        memset(glow_mask, 0, sizeof glow_mask);
        canvas_glow_mask(glow_mask, VIEW_W, VIEW_H, glow_x, glow_y, 820, 470, 0x7A);
        canvas_glow_mask(glow_mask, VIEW_W, VIEW_H, VIEW_W - glow_x / 3, glow_y - 60, 520, 300, 0x2E);
        mask_key.valid = 1;
        mask_key.glow_x = glow_x;
        mask_key.glow_y = glow_y;
    }
    /* Each row is one gradient colour, so every pixel is that colour mixed
     * towards the accent by its mask value: a 256-entry table per row turns
     * the whole layer into lookups. The home fade is folded into the table. */
    for (y = 0; y < VIEW_H; y++) {
        uint32_t lut[256], *row = c->px + (long)y * c->stride;
        ft_color base = color_mix(COL_BG_TOP, COL_BG_BOTTOM, (unsigned)(y * 255 / (VIEW_H - 1)));
        ft_color lit = accent | 0xFF000000u;
        const uint8_t *m = glow_mask + (long)y * VIEW_W;
        long acc[3], step[3];
        int ch, i, x;
        if (fade && y >= low.y) { /* keeps the app row legible over a bright shelf */
            unsigned f = (unsigned)((y - low.y) * 0xB3 / (low.h - 1));
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
        for (x = 0; x < VIEW_W; x++)
            row[x] = lut[m[x]];
    }
    for (y = 0; y < VIEW_H; y++)
        memcpy(backdrop_cache + (long)y * VIEW_W, c->px + (long)y * c->stride, VIEW_W * sizeof(uint32_t));
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
    /* Right side, left to right: gear, profile chip, then the clock. */
    r.w = item == TOP_PROFILE ? chip_w : TOP_H;
    r.x = VIEW_W - VIEW_MARGIN - 112 - chip_w; /* room for the clock */
    if (item == TOP_PROFILE)
        r.x += TOP_H + 12;
    return r;
}

/* tabs: draw the tabs and gear (home only); lists show just clock and profile. */
static void top_bar(ft_canvas *c, const shell *s, const char *clock_text, int tabs)
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
        for (i = 0; i < TAB_COUNT; i++) {
            ft_rect r = view_top_rect(s, i);
            int focused = on_bar && s->top == i, selected = (int)s->tab == i;
            if (focused)
                canvas_round(c, r, TOP_H / 2, COL_FOCUS_BG);
            else if (selected)
                canvas_round(c, r, TOP_H / 2, COL_ROW);
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
    case ROW_CARDS: return ROW_TITLE_H + CARD_H + 56;
    case ROW_GUIDE: return (r->title != STR_COUNT ? ROW_TITLE_H : 0) + GUIDE_STEP;
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

ft_rect view_home_card_rect(const shell *s, int row, int col)
{
    static shell_row rows[SHELL_MAX_ROWS];
    home_geo g;
    int n = home_layout(s, rows, &g);
    ft_rect r = {0, 0, 0, 0};
    int y;
    if (row < 0 || row >= n)
        return r;
    y = g.top[row] - g.offset;
    switch (rows[row].style) {
    case ROW_HERO:
        r.x = VIEW_MARGIN;
        r.y = y;
        r.w = VIEW_W - 2 * VIEW_MARGIN;
        r.h = HERO_H;
        break;
    case ROW_CARDS:
        r.x = VIEW_MARGIN + (col - first_card(s->col[row], rows[row].n)) * CARD_STEP;
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
    }
    return r;
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

/* No LIVE badge here: whether a channel is live is only known once it plays. */
static void channel_card(ft_canvas *c, ft_rect r, const channel *ch, int focused)
{
    ft_rect sheen = {r.x, r.y, r.w, r.h / 2};
    ft_rect badge = {r.x + 12, r.y + 12, 44, 34};
    ft_color top = color_mix(ch->accent, COL_TEXT, 25), bottom = color_mix(ch->accent, FT_RGB(0x000000), 120);
    if (focused) {
        ft_rect sh = {r.x + 8, r.y + 18, r.w - 16, r.h - 8};
        canvas_shadow(c, sh, 26, 0xB0000000u);
    }
    canvas_round_gradient(c, r, 14, CORNERS_ALL, top, bottom);
    canvas_round_gradient(c, sheen, 14, CORNERS_TOP, focused ? 0x40FFFFFFu : 0x14FFFFFFu, 0x00FFFFFFu);
    canvas_round(c, badge, 8, 0x4D000000u);
    {
        char num[8];
        snprintf(num, sizeof num, "%d", ch->number);
        canvas_text_center(c, &font_label, badge, num, COL_TEXT);
    }
    canvas_text_fit(c, text_width(&font_title, ch->name) <= r.w - 28 ? &font_title : &font_label, r.x + 14,
                    r.y + r.h - 14 - font_title.line_height, r.w - 28, ch->name, COL_TEXT);
    if (focused)
        ring(c, r, 14);
}

static art_id card_art(shell_card k)
{
    static const art_id games[GAME_COUNT] = {ART_RACER, ART_PENALTY, ART_GARDEN};
    switch (k.kind) {
    case CARD_GAME: return k.arg >= 0 && k.arg < GAME_COUNT ? games[k.arg] : ART_GAMES;
    case CARD_SETTINGS: return ART_SETTINGS;
    case CARD_PROFILE: return ART_PROFILE;
    case CARD_ABOUT: return ART_INFO;
    case CARD_CHANNEL:
    case CARD_NO_CHANNELS: break;
    }
    return ART_CHANNELS;
}

static void app_card(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused)
{
    ft_rect label = {r.x, r.y + r.h + 12, r.w, font_label.line_height};
    if (focused) {
        ft_rect sh = {r.x + 8, r.y + 18, r.w - 16, r.h - 8};
        canvas_shadow(c, sh, 26, 0xB0000000u);
    }
    art_tile(c, r, 14, card_art(k), focused, NULL);
    if (focused)
        ring(c, r, 14);
    canvas_text_fit(c, focused ? &font_label : &font_body, label.x, label.y, label.w, shell_card_label(s, k),
                    focused ? COL_TEXT : COL_TEXT_2);
}

static void hero(ft_canvas *c, ft_rect r, const shell *s, shell_card k, int focused)
{
    ft_lang L = s->cfg.lang;
    const channel *ch = k.kind == CARD_CHANNEL ? &s->cfg.channels.ch[k.arg] : NULL;
    ft_color accent = ch ? ch->accent : art_accent(ART_CHANNELS);
    ft_rect sh = {r.x + 10, r.y + 22, r.w - 20, r.h - 10};
    canvas_shadow(c, sh, 30, 0x99000000u);
    canvas_round_gradient(c, r, 22, CORNERS_ALL, FT_RGB(0x1E2026), FT_RGB(0x131418));
    canvas_glow(c, r.x + r.w - 300, r.y + r.h / 2, 300, r.h / 2, color_alpha(accent, 0xC8));
    art_glyph(c, ch ? ART_LIVE_TV : ART_CHANNELS, r.x + r.w - 300, r.y + r.h / 2, 56, 0xE6FFFFFFu);
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

static void guide_row(ft_canvas *c, ft_rect r, const shell *s, int index, int focused)
{
    const channel *ch = &s->cfg.channels.ch[index];
    ft_rect badge = {r.x + 12, r.y + 10, 56, 40};
    int ty = r.y + (r.h - font_label.line_height) / 2;
    if (focused) {
        ft_rect big = lift(r, 8, 3);
        ft_rect sh = {big.x + 6, big.y + 12, big.w - 12, big.h - 6};
        canvas_shadow(c, sh, 20, 0x8C000000u);
        canvas_round(c, big, 16, COL_FOCUS_BG);
    } else {
        canvas_round(c, r, 14, COL_ROW);
    }
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
    return art_accent(card_art(*k));
}

static void home(ft_canvas *c, const shell *s, const char *clock_text)
{
    static shell_row rows[SHELL_MAX_ROWS];
    home_geo g;
    int n = home_layout(s, rows, &g), i, j;
    ft_rect top_fade = {0, 0, VIEW_W, CONTENT_TOP};
    ft_rect bottom_fade = {0, CONTENT_BOTTOM - 24, VIEW_W, 32};
    ft_rect bottom_band = {0, CONTENT_BOTTOM + 8, VIEW_W, VIEW_H - CONTENT_BOTTOM - 8};

    backdrop(c, home_accent(s, rows, n), 980, 40, 1);
    for (i = g.first_row; i < n; i++) {
        const shell_row *r = &rows[i];
        int y = g.top[i] - g.offset, row_focus = s->row == i;
        int col = s->col[i] < r->n ? s->col[i] : 0;
        if (y >= VIEW_H)
            break;
        if (r->title != STR_COUNT)
            row_title(c, s, r, y);
        switch (r->style) {
        case ROW_HERO:
            hero(c, view_home_card_rect(s, i, 0), s, r->cards[0], row_focus);
            break;
        case ROW_GUIDE:
            guide_row(c, view_home_card_rect(s, i, 0), s, r->cards[0].arg, row_focus);
            break;
        case ROW_CARDS:
            /* Resting cards first, so the focused one lands on top. */
            for (j = first_card(col, r->n); j < r->n; j++) {
                ft_rect k = view_home_card_rect(s, i, j);
                if (k.x >= VIEW_W)
                    break;
                if (row_focus && j == col)
                    continue;
                if (r->cards[j].kind == CARD_CHANNEL)
                    channel_card(c, k, &s->cfg.channels.ch[r->cards[j].arg], 0);
                else
                    app_card(c, k, s, r->cards[j], 0);
            }
            if (row_focus && r->n > 0) {
                ft_rect k = view_home_card_rect(s, i, col);
                k = lift(k, k.w / 25, k.h / 25);
                if (r->cards[col].kind == CARD_CHANNEL)
                    channel_card(c, k, &s->cfg.channels.ch[r->cards[col].arg], 1);
                else
                    app_card(c, k, s, r->cards[col], 1);
            }
            break;
        }
    }
    if (g.offset > 0)
        canvas_gradient(c, top_fade, color_alpha(COL_BG_TOP, 0xF5), color_alpha(COL_BG_TOP, 0x00));
    canvas_gradient(c, bottom_fade, color_alpha(COL_BG_BOTTOM, 0x00), color_alpha(COL_BG_BOTTOM, 0xF2));
    canvas_fill(c, bottom_band, color_alpha(COL_BG_BOTTOM, 0xF2));
    top_bar(c, s, clock_text, 1);
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
    if (focused) {
        ft_rect big = lift(r, 8, 3);
        ft_rect sh = {big.x + 6, big.y + 12, big.w - 12, big.h - 6};
        canvas_shadow(c, sh, 20, 0x8C000000u);
        canvas_round(c, big, 16, COL_FOCUS_BG);
    } else {
        canvas_round(c, r, 14, COL_ROW);
    }
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

    backdrop(c, art_accent(art), 260, 200, 0);
    canvas_text_center(c, &font_title, title, shell_screen_title(s), COL_TEXT);
    top_bar(c, s, clock_text, 0);
    canvas_shadow(c, hero_sh, 30, 0x99000000u);
    art_tile(c, hero, 24, art, 0, NULL);
    canvas_text_wrap(c, &font_body, desc, tr(L, s->screen == SCR_SETTINGS ? STR_DESC_SETTINGS : STR_DESC_MORE),
                     COL_TEXT_2);

    if (s->item >= LIST_VISIBLE)
        first = s->item - LIST_VISIBLE + 1;
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

    backdrop(c, art_accent(ART_SETTINGS), 640, 60, 0);
    top_bar(c, s, clock_text, 0);
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

void view_shell(ft_canvas *c, const shell *s, const char *clock_text)
{
    if (s->screen == SCR_HOME)
        home(c, s, clock_text);
    else if (s->screen == SCR_PIN)
        pin(c, s, clock_text);
    else
        list(c, s, clock_text);
}

/* ---- live TV ------------------------------------------------------------ */

ft_rect view_live_panel_row_rect(int visible_row)
{
    ft_rect r = {24, PANEL_TOP + visible_row * PANEL_STEP, PANEL_W - 48, PANEL_STEP - 8};
    return r;
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

static void banner(ft_canvas *c, const live *l, const player_status *ps, const channel *ch, ft_lang L,
                   const char *clock_text)
{
    ft_rect fade = {0, 470, VIEW_W, VIEW_H - 470};
    ft_rect badge = {VIEW_MARGIN, 560, 104, 76};
    int x = badge.x + badge.w + 24;
    canvas_gradient(c, fade, 0x00000000u, 0xE6000000u);
    number_badge(c, badge, ch, &font_display);
    canvas_text_fit(c, &font_display, x, 552, 760, ch->name, COL_TEXT);
    if (ps->state == PLAYER_PLAYING && l->shown == l->current && l->tune_wait_ms == 0) {
        live_pill(c, x, 612, L);
        x += text_width(&font_caption, tr(L, STR_LIVE_BADGE)) + 44;
    }
    if (ps->video_h > 0 && l->shown == l->current && ps->state == PLAYER_PLAYING) {
        char q[16];
        ft_rect chip = {x, 612, 0, 26};
        snprintf(q, sizeof q, "%dp", ps->video_h);
        chip.w = text_width(&font_caption, q) + 20;
        canvas_round_stroke(c, chip, 6, 1, COL_TEXT_2);
        canvas_text_center(c, &font_caption, chip, q, COL_TEXT_2);
        x += chip.w + 14;
    }
    canvas_text(c, &font_caption, x, 612 + (26 - font_caption.line_height) / 2, "YouTube", COL_TEXT_3);
    if (clock_text)
        canvas_text_right(c, &font_title, VIEW_W - VIEW_MARGIN, 566, clock_text, COL_TEXT);
    hint(c, tr(L, STR_HELP_LIVE));
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
    ft_rect panel = {0, 0, PANEL_W, VIEW_H};
    ft_rect edge = {PANEL_W - 1, 0, 1, VIEW_H};
    int first = l->panel_item - PANEL_VISIBLE / 2, i;
    if (first > chs->n - PANEL_VISIBLE)
        first = chs->n - PANEL_VISIBLE;
    if (first < 0)
        first = 0;
    canvas_fill(c, panel, COL_PANEL);
    canvas_fill(c, edge, COL_GLASS_EDGE);
    canvas_text(c, &font_title, 40, 40, tr(L, STR_ALL_CHANNELS), COL_TEXT);
    for (i = first; i < chs->n && i < first + PANEL_VISIBLE; i++) {
        ft_rect r = view_live_panel_row_rect(i - first);
        ft_rect badge = {r.x + 10, r.y + 7, 50, r.h - 14};
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
        canvas_chevron(c, PANEL_W / 2, PANEL_TOP - 14, 12, 4, 1, COL_TEXT_3);
    if (first + PANEL_VISIBLE < chs->n)
        canvas_chevron(c, PANEL_W / 2, PANEL_TOP + PANEL_VISIBLE * PANEL_STEP + 4, 12, 4, 0, COL_TEXT_3);
    help_lines(c, &font_caption, 40, 628, PANEL_W - 80, tr(L, STR_HELP_PANEL), COL_TEXT_3);
}

static uint32_t mix(uint32_t h, uint32_t v)
{
    return (h ^ v) * 16777619u;
}

uint32_t view_live_signature(const live *l, const player_status *ps, const shell *s, const char *clock_text,
                             int has_picture)
{
    int zapping = l->tune_wait_ms > 0 || l->shown != l->current;
    int spinning = !has_picture || zapping || ps->state == PLAYER_BUFFERING;
    uint32_t h = 2166136261u;
    const char *p;
    h = mix(h, (uint32_t)l->shown);
    h = mix(h, (uint32_t)l->current);
    h = mix(h, (uint32_t)zapping);
    h = mix(h, (uint32_t)(l->banner_ms > 0));
    h = mix(h, (uint32_t)(l->volume_ms > 0 ? l->volume + 1 : 0));
    h = mix(h, (uint32_t)(l->panel_open ? l->panel_item + 1 : 0));
    h = mix(h, spinning ? l->anim_ms / 110 + 1 : 0); /* the spinner's step */
    h = mix(h, (uint32_t)ps->state);
    h = mix(h, (uint32_t)ps->error);
    h = mix(h, (uint32_t)ps->video_h);
    h = mix(h, (uint32_t)s->cfg.lang);
    h = mix(h, (uint32_t)has_picture);
    for (p = clock_text ? clock_text : ""; *p; p++)
        h = mix(h, (unsigned char)*p);
    return h;
}

void view_live(ft_canvas *c, const live *l, const player_status *ps, const shell *s, const char *clock_text,
               int has_picture)
{
    const channel_list *chs = &s->cfg.channels;
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

    backdrop(c, art_accent(ART_GARDEN), 640, -160, 0);
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
