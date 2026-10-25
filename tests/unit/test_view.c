/* Host tests for the software canvas, baked fonts and pixel views. Host
 * results are not TV evidence: real colours, geometry and timing need gates
 * I05 and P01. */
#include <string.h>

#include "check.h"
#include "view.h"

#define GW 64
#define GH 40
#define GUARD 0xDEADBEEFu

static uint32_t screen[VIEW_W * VIEW_H];

static void test_canvas_never_writes_outside(void)
{
    static uint32_t buf[(GW + 2) * (GH + 2)];
    static const int tri[6] = {-20, -20, 90, 10, 10, 70};
    ft_canvas c;
    ft_rect huge = {-100, -100, 1000, 1000}, edge = {GW - 3, GH - 3, 10, 10};
    int i, x, y, guard_ok = 1;
    for (i = 0; i < (GW + 2) * (GH + 2); i++)
        buf[i] = GUARD;
    canvas_init(&c, buf + (GW + 2) + 1, GW, GH, GW + 2);
    canvas_fill(&c, huge, 0xFF000001u);
    canvas_fill(&c, edge, 0x80000002u);
    canvas_gradient(&c, huge, 0xFF000003u, 0x40000004u);
    canvas_round(&c, huge, 30, 0xFF000005u);
    canvas_round_gradient(&c, edge, 8, CORNERS_TOP, 0xFF000006u, 0x00000006u);
    canvas_round_stroke(&c, huge, 40, 6, 0xFF000007u);
    canvas_circle(&c, 0, 0, 25, 0xFF000008u);
    canvas_ring(&c, GW, GH, 25, 4, 0xFF000009u);
    canvas_line(&c, -50, -50, GW + 50, GH + 50, 9, 0xFF00000Au);
    canvas_polygon(&c, tri, 3, 0xFF00000Bu);
    canvas_polygon_in_circle(&c, tri, 3, 0, 0, 30, 0xFF00000Cu);
    canvas_chevron(&c, GW, 0, 30, 6, 1, 0xFF00000Du);
    canvas_shadow(&c, huge, 40, 0x80000000u);
    canvas_glow(&c, GW, GH, 200, 200, 0x80FFFFFFu);
    canvas_text(&c, &font_display, -20, -30, "Overflow test", 0xFF00000Eu);
    canvas_text(&c, &font_label, GW - 10, GH - 10, "edge", 0xFF00000Fu);
    canvas_text_wrap(&c, &font_caption, huge, "wrapping text that is much longer than the canvas", 0xFF000010u);
    for (y = 0; y < GH + 2; y++)
        for (x = 0; x < GW + 2; x++) {
            int inside = x >= 1 && x <= GW && y >= 1 && y <= GH;
            if (!inside && buf[y * (GW + 2) + x] != GUARD)
                guard_ok = 0;
        }
    CHECK(guard_ok);
    CHECK(canvas_get(&c, -1, 0) == 0 && canvas_get(&c, GW, 0) == 0);
}

static void test_blending_and_colour(void)
{
    uint32_t px[4] = {0};
    ft_canvas c;
    ft_rect one = {0, 0, 1, 1};
    ft_color v;
    canvas_init(&c, px, 4, 1, 4);
    canvas_clear(&c, 0xFF000000u);
    canvas_fill(&c, one, 0x80FFFFFFu); /* 50% white over black */
    v = canvas_get(&c, 0, 0);
    CHECK((v >> 24) == 0xFF);
    CHECK(((v >> 16) & 0xFF) >= 0x7F && ((v >> 16) & 0xFF) <= 0x81);
    CHECK(color_mix(0xFF000000u, 0xFFFFFFFFu, 0) == 0xFF000000u);
    CHECK(color_mix(0xFF000000u, 0xFFFFFFFFu, 255) == 0xFFFFFFFFu);
    CHECK(color_alpha(0xFF123456u, 0x40) == 0x40123456u);
    canvas_fill(&c, one, 0x00FF0000u); /* fully transparent: no change */
    CHECK(canvas_get(&c, 0, 0) == v);
}

static int has_ink(const ft_canvas *c, int x0, int x1, int y0, int y1, ft_color bg)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (canvas_get(c, x, y) != bg)
                return 1;
    return 0;
}

static void test_fonts_and_kerning(void)
{
    static const ft_font *const fonts[] = {&font_display, &font_title, &font_label, &font_body, &font_caption};
    ft_canvas c;
    char all[96];
    int i, ok = 1;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    for (i = 0; i < 95; i++)
        all[i] = (char)(32 + i);
    all[95] = '\0';
    for (i = 0; i < 5; i++) {
        const ft_font *f = fonts[i];
        if (f->line_height < f->size || f->ascent <= 0 || f->count != 95)
            ok = 0;
        canvas_clear(&c, 0);
        canvas_text(&c, f, 0, 0, all, 0xFFFFFFFFu); /* every glyph, under ASan */
        canvas_text(&c, f, 0, 100, "\x01\x7f\xff", 0xFFFFFFFFu); /* out-of-range bytes draw as '?' */
    }
    CHECK(ok);
    CHECK(text_width(&font_label, "") == 0);
    CHECK(text_width(&font_label, "Games") > 0);
    /* Kerning from Inter's GPOS table pulls these pairs together. */
    CHECK(text_width(&font_label, "To") < text_width(&font_label, "T") + text_width(&font_label, "o"));
    CHECK(text_width(&font_display, "AV") < text_width(&font_display, "A") + text_width(&font_display, "V"));
}

static void test_text_fit_and_wrap(void)
{
    ft_canvas c;
    ft_rect narrow = {0, 0, 150, 3 * font_caption.line_height};
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);

    canvas_clear(&c, 0);
    canvas_text_fit(&c, &font_body, 0, 0, 150, "Muda wa kucheza wa watoto kwa siku", 0xFFFFFFFFu);
    CHECK(has_ink(&c, 0, 150, 0, font_body.line_height, 0xFF000000u));
    CHECK(!has_ink(&c, 152, 600, 0, font_body.line_height, 0xFF000000u));

    canvas_clear(&c, 0);
    CHECK(canvas_text_wrap(&c, &font_caption, narrow, "Chaneli zote (orodha ya TV)", 0xFFFFFFFFu) == 2);
    CHECK(!has_ink(&c, 152, 600, 0, 3 * font_caption.line_height, 0xFF000000u));
    canvas_clear(&c, 0);
    CHECK(canvas_text_wrap(&c, &font_caption, narrow, "one two three four five six seven eight nine ten eleven", 0xFFFFFFFFu) == 3);
    CHECK(!has_ink(&c, 0, 600, 3 * font_caption.line_height, 200, 0xFF000000u));
}

static void base(shell_config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    strcpy(cfg->favs[0].name, "SportyTV");
    cfg->favs[0].received = 1;
    cfg->nfav = 1;
    cfg->child_games = 0x7;
}

static void test_home_focus_lifts_tile(void)
{
    ft_canvas c;
    shell_config cfg;
    static shell s;
    ft_rect t0;
    ft_color with_focus, without_focus;
    uint32_t h0;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    base(&cfg);
    shell_init(&s, &cfg);
    t0 = view_home_tile_rect(&s, 0);
    CHECK(t0.x >= VIEW_MARGIN);
    CHECK(view_home_tile_rect(&s, s.ntiles - 1).x + t0.w <= VIEW_W - VIEW_MARGIN);

    view_shell(&c, &s, "20:01");
    with_focus = canvas_get(&c, t0.x - 3, t0.y + t0.h / 2); /* inside the lifted tile only */
    h0 = canvas_hash(&c);
    view_shell(&c, &s, "20:01");
    CHECK(canvas_hash(&c) == h0); /* deterministic */
    s.focus = 1;
    view_shell(&c, &s, "20:01");
    without_focus = canvas_get(&c, t0.x - 3, t0.y + t0.h / 2);
    CHECK(canvas_hash(&c) != h0);
    CHECK(with_focus != without_focus);
}

static void test_pin_screen_hides_other_digits(void)
{
    ft_canvas c;
    shell_config cfg;
    static shell a, b;
    uint32_t ha;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    base(&cfg);
    shell_init(&a, &cfg);
    shell_open_pin(&a, PIN_FOR_PARENT);
    b = a;
    memcpy(a.pin_digits, "1234", 4);
    memcpy(b.pin_digits, "9874", 4);
    a.pin_pos = b.pin_pos = 3;
    view_shell(&c, &a, "20:01");
    ha = canvas_hash(&c);
    view_shell(&c, &b, "20:01");
    CHECK(canvas_hash(&c) == ha);
}

static void test_list_scrolls_to_focus(void)
{
    ft_canvas c;
    shell_config cfg;
    static shell s;
    ft_rect last = view_list_row_rect(6);
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    base(&cfg);
    shell_init(&s, &cfg);
    shell_open_settings(&s);
    s.item = 7; /* eighth item: needs scrolling */
    view_shell(&c, &s, "20:01");
    CHECK(canvas_get(&c, last.x + last.w - 8, last.y + 4) == COL_FOCUS_BG);
    CHECK(last.y + last.h < 620); /* rows stay clear of the status toast */
}

static void test_all_views_all_states(void)
{
    static const uint32_t best[RACER_LEVELS] = {1, 2, 3};
    ft_canvas c;
    shell_config cfg;
    static shell s;
    static garden g;
    racer r;
    penalty p;
    int lang, scr, st, ph, mode, f;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    for (lang = 0; lang < LANG_COUNT; lang++) {
        base(&cfg);
        cfg.lang = (ft_lang)lang;
        cfg.youtube_supported = lang; /* also cover the six-tile layout */
        cfg.netflix_supported = lang;
        for (scr = SCR_HOME; scr <= SCR_PIN; scr++)
            for (f = 0; f < 6; f++) {
                shell_init(&s, &cfg);
                s.screen = (shell_screen)scr;
                s.focus = f % s.ntiles;
                snprintf(s.status, sizeof s.status, "A very long status message that has to be cut before it reaches the edge of the screen");
                view_shell(&c, &s, NULL);
            }
        for (st = RACER_START; st <= RACER_EXITED; st++) {
            racer_init(&r, 3, RACER_HARD, st & 1);
            r.state = (racer_state)st;
            view_racer(&c, &r, best, (ft_lang)lang, "status");
        }
        for (ph = PEN_AIMING; ph <= PEN_EXITED; ph++) {
            penalty_init(&p, 3, 1);
            p.shots[0] = 12;
            p.shots[1] = 11;
            p.phase = (pen_phase)ph;
            p.winner = ph == PEN_OVER ? PEN_TIE : PEN_NO_WINNER;
            view_penalty(&c, &p, (ft_lang)lang, "status");
        }
        for (mode = GM_EDIT; mode <= GM_EXITED; mode++) {
            garden_init(&g);
            g.mode = (garden_mode)mode;
            g.cx = GARDEN_W - 1;
            g.cy = (mode & 1) ? 0 : GARDEN_H - 1;
            view_garden(&c, &g, (ft_lang)lang, mode == GM_EDIT ? "Saved to slot 1" : "");
        }
    }
    CHECK(1); /* reaching here without sanitizer errors is the assertion */
}

void run_view_tests(void)
{
    test_canvas_never_writes_outside();
    test_blending_and_colour();
    test_fonts_and_kerning();
    test_text_fit_and_wrap();
    test_home_focus_lifts_tile();
    test_pin_screen_hides_other_digits();
    test_list_scrolls_to_focus();
    test_all_views_all_states();
}
