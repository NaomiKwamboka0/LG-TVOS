#include "view.h"

#include <stdio.h>
#include <string.h>

#include "art.h"

/* ---- layout ------------------------------------------------------------ */

#define APP_TOP 440
#define APP_GAP 36
#define APP_MAX_W 232
#define SHELF_TOP 214
#define SHELF_W 200
#define SHELF_H 112
#define LIST_X 560
#define LIST_TOP 116
#define LIST_W 648
#define LIST_ROW_H 64
#define LIST_STEP 72
#define LIST_VISIBLE 7
#define HINT_Y 684

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
            acc[ch] = (b << 16) + 32768;
            step[ch] = ((l - b) << 16) / 255;
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

static void top_bar(ft_canvas *c, const shell *s, const char *clock_text)
{
    ft_lang L = s->cfg.lang;
    int parent = s->cfg.profile == PROFILE_PARENT;
    const char *name = tr(L, parent ? STR_PARENT : STR_CHILD);
    const char *clock = clock_text ? clock_text : "--:--";
    int clock_w = text_width(&font_body, clock);
    int chip_w = 44 + text_width(&font_caption, name) + 16;
    ft_rect chip = {VIEW_W - VIEW_MARGIN - clock_w - 20 - chip_w, 26, chip_w, 36};
    char initial[2] = {name[0], '\0'};
    ft_rect avatar_text = {chip.x + 5, chip.y + 5, 26, 26};

    canvas_text_right(c, &font_body, VIEW_W - VIEW_MARGIN, 32, clock, COL_TEXT);
    canvas_round(c, chip, 18, COL_ROW);
    canvas_circle(c, chip.x + 18, chip.y + 18, 13, art_accent(parent ? ART_LIVE_TV : ART_PROFILE));
    canvas_text_center(c, &font_caption, avatar_text, initial, COL_TEXT);
    canvas_text(c, &font_caption, chip.x + 40, chip.y + 7, name, COL_TEXT_2);
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

/* ---- home --------------------------------------------------------------- */

static art_id tile_art(int tile)
{
    switch ((shell_tile)tile) {
    case TILE_LIVE_TV: return ART_LIVE_TV;
    case TILE_YOUTUBE: return ART_YOUTUBE;
    case TILE_FOOTBALL: return ART_FOOTBALL;
    case TILE_GAMES: return ART_GAMES;
    case TILE_NETFLIX: return ART_NETFLIX;
    case TILE_MORE:
    case TILE_COUNT: break;
    }
    return ART_MORE;
}

static const char *tile_text(const shell *s, int tile)
{
    /* Apps we do not have artwork for show their name, never a logo. */
    return (tile == TILE_YOUTUBE || tile == TILE_NETFLIX) ? shell_tile_label(s, tile) : NULL;
}

ft_rect view_home_tile_rect(const shell *s, int index)
{
    int n = s->ntiles > 0 ? s->ntiles : 1;
    int w = (VIEW_W - 2 * VIEW_MARGIN - (n - 1) * APP_GAP) / n;
    ft_rect r;
    if (w > APP_MAX_W)
        w = APP_MAX_W;
    r.x = VIEW_MARGIN + index * (w + APP_GAP);
    r.y = APP_TOP;
    r.w = w;
    r.h = w * 3 / 5;
    return r;
}

typedef struct {
    art_id art;
    const char *caption;
    const char *text; /* drawn on the card instead of a glyph */
} shelf_card;

static int shelf_cards(const shell *s, int tile, shelf_card *out, int max)
{
    ft_lang L = s->cfg.lang;
    int n = 0, i;
    switch ((shell_tile)tile) {
    case TILE_LIVE_TV:
        out[n++] = (shelf_card){ART_LIVE_TV, tr(L, STR_WATCH_LIVE), NULL};
        out[n++] = (shelf_card){ART_CHANNELS, tr(L, STR_ALL_CHANNELS), NULL};
        break;
    case TILE_FOOTBALL:
        for (i = 0; i < s->cfg.nfav && n < max - 1; i++)
            if (s->cfg.favs[i].received == 1)
                out[n++] = (shelf_card){ART_FOOTBALL, s->cfg.favs[i].name, s->cfg.favs[i].name};
        out[n++] = (shelf_card){ART_CHANNELS, tr(L, STR_ALL_CHANNELS), NULL};
        break;
    case TILE_GAMES: {
        static const art_id arts[GAME_COUNT] = {ART_RACER, ART_PENALTY, ART_GARDEN};
        for (i = 0; i < GAME_COUNT && n < max; i++)
            if (s->cfg.profile == PROFILE_PARENT || (s->cfg.child_games & (1u << i)))
                out[n++] = (shelf_card){arts[i], shell_game_name(L, i), NULL};
        break;
    }
    case TILE_MORE:
        if (s->cfg.profile == PROFILE_PARENT) {
            out[n++] = (shelf_card){ART_SETTINGS, tr(L, STR_PARENT_SETTINGS), NULL};
            out[n++] = (shelf_card){ART_PROFILE, tr(L, STR_TO_CHILD), NULL};
            out[n++] = (shelf_card){ART_SEARCH, tr(L, STR_WEB_SEARCH), NULL};
        } else {
            out[n++] = (shelf_card){ART_PROFILE, tr(L, STR_TO_PARENT), NULL};
        }
        break;
    case TILE_YOUTUBE:
    case TILE_NETFLIX:
    case TILE_COUNT:
        break;
    }
    return n;
}

static ft_str shelf_line(int tile)
{
    switch ((shell_tile)tile) {
    case TILE_LIVE_TV: return STR_SHELF_LIVE;
    case TILE_FOOTBALL: return STR_SHELF_FOOTBALL;
    case TILE_GAMES: return STR_SHELF_GAMES;
    case TILE_MORE: return STR_SHELF_MORE;
    case TILE_YOUTUBE:
    case TILE_NETFLIX:
    case TILE_COUNT: break;
    }
    return STR_SHELF_APP;
}

static void shelf(ft_canvas *c, const shell *s, int tile)
{
    shelf_card cards[5];
    int n = shelf_cards(s, tile, cards, 5), i, received = 0;
    for (i = 0; i < n; i++) {
        ft_rect r = {VIEW_MARGIN + i * (SHELF_W + 24), SHELF_TOP, SHELF_W, SHELF_H};
        ft_rect sh = {r.x + 4, r.y + 8, r.w - 8, r.h - 4};
        canvas_shadow(c, sh, 16, 0x70000000u);
        art_tile(c, r, 14, cards[i].art, 0, cards[i].text);
        {
            ft_rect cap = {r.x, r.y + r.h + 10, r.w, 2 * font_caption.line_height};
            canvas_text_wrap(c, &font_caption, cap, cards[i].caption, COL_TEXT_2);
        }
        if (cards[i].art == ART_FOOTBALL)
            received++;
    }
    if (tile == TILE_FOOTBALL && received == 0) {
        ft_rect note = {VIEW_MARGIN + n * (SHELF_W + 24) + 8, SHELF_TOP + 20, 420, 3 * font_body.line_height};
        canvas_text_wrap(c, &font_body, note, tr(s->cfg.lang, STR_SHELF_FAV_HINT), COL_TEXT_3);
    }
}

static void home(ft_canvas *c, const shell *s, const char *clock_text)
{
    int focus = s->ntiles > 0 ? s->focus : 0, tile = s->ntiles > 0 ? s->tiles[focus] : TILE_MORE, i;
    art_id accent = tile_art(tile);
    ft_rect sub = {VIEW_MARGIN, 154, 760, 2 * font_body.line_height};

    backdrop(c, art_accent(accent), 980, 40, 1);
    canvas_text_fit(c, &font_display, VIEW_MARGIN, 92, 760, shell_tile_label(s, tile), COL_TEXT);
    canvas_text_wrap(c, &font_body, sub, tr(s->cfg.lang, shelf_line(tile)), COL_TEXT_2);
    shelf(c, s, tile);

    for (i = 0; i < s->ntiles; i++) {
        ft_rect r = view_home_tile_rect(s, i);
        if (i == focus)
            continue;
        art_tile(c, r, 16, tile_art(s->tiles[i]), 0, tile_text(s, s->tiles[i]));
        {
            ft_rect label = {r.x - 10, r.y + r.h + 18, r.w + 20, font_body.line_height};
            canvas_text_center(c, &font_body, label, shell_tile_label(s, s->tiles[i]), COL_TEXT_2);
        }
    }
    if (s->ntiles > 0) {
        ft_rect r = view_home_tile_rect(s, focus);
        ft_rect big = lift(r, r.w / 20, r.h / 20);
        ft_rect sh = {big.x + 8, big.y + 22, big.w - 16, big.h - 8};
        ft_rect label = {big.x - 20, big.y + big.h + 14, big.w + 40, font_label.line_height};
        canvas_shadow(c, sh, 28, 0xB0000000u);
        art_tile(c, big, 18, accent, 1, tile_text(s, tile));
        canvas_text_center(c, &font_label, label, shell_tile_label(s, tile), COL_TEXT);
    }
    top_bar(c, s, clock_text);
    toast(c, s->status, 24);
    hint(c, tr(s->cfg.lang, STR_HELP_HOME));
}

/* ---- lists (Football, Games, More, Parent settings) -------------------- */

ft_rect view_list_row_rect(int visible_row)
{
    ft_rect r = {LIST_X, LIST_TOP + visible_row * LIST_STEP, LIST_W, LIST_ROW_H};
    return r;
}

static art_id screen_art(shell_screen scr)
{
    switch (scr) {
    case SCR_FOOTBALL: return ART_FOOTBALL;
    case SCR_GAMES: return ART_GAMES;
    case SCR_SETTINGS:
    case SCR_PIN: return ART_SETTINGS;
    case SCR_MORE:
    case SCR_HOME: break;
    }
    return ART_MORE;
}

static ft_str screen_desc(shell_screen scr)
{
    switch (scr) {
    case SCR_FOOTBALL: return STR_SHELF_FOOTBALL;
    case SCR_GAMES: return STR_SHELF_GAMES;
    case SCR_SETTINGS: return STR_DESC_SETTINGS;
    case SCR_MORE:
    case SCR_HOME:
    case SCR_PIN: break;
    }
    return STR_SHELF_MORE;
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
    art_id art = screen_art(s->screen);
    ft_rect title = {0, 34, VIEW_W, font_title.line_height};
    ft_rect hero = {96, 190, 360, 216};
    ft_rect hero_sh = {hero.x + 8, hero.y + 20, hero.w - 16, hero.h - 8};
    ft_rect desc = {hero.x, hero.y + hero.h + 26, hero.w, 3 * font_body.line_height};

    backdrop(c, art_accent(art), 260, 200, 0);
    canvas_text_center(c, &font_title, title, shell_screen_title(s), COL_TEXT);
    top_bar(c, s, clock_text);
    canvas_shadow(c, hero_sh, 30, 0x99000000u);
    art_tile(c, hero, 24, art, 0, NULL);
    canvas_text_wrap(c, &font_body, desc, tr(L, screen_desc(s->screen)), COL_TEXT_2);

    if (s->screen == SCR_GAMES && s->cfg.profile == PROFILE_CHILD && s->cfg.child_limit_s != 0) {
        char line[96];
        ft_rect chip;
        snprintf(line, sizeof line, "%s: %u %s", tr(L, STR_TIME_LEFT),
                 (unsigned)((s->cfg.play_remaining_s + 59) / 60), tr(L, STR_MINUTES));
        chip.x = hero.x;
        chip.y = desc.y + desc.h + 12;
        chip.w = text_width(&font_body, line) + 36;
        chip.h = 40;
        canvas_round(c, chip, 20, COL_ROW);
        canvas_text(c, &font_body, chip.x + 18, chip.y + (chip.h - font_body.line_height) / 2, line, COL_GOLD);
    }

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
    top_bar(c, s, clock_text);
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
