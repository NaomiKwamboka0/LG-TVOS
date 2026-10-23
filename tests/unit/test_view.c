/* Host tests for the software canvas and pixel views. Host results are not
 * TV evidence: real colours, geometry and timing need gates I05 and P01. */
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
    ft_canvas c;
    ft_rect huge = {-100, -100, 1000, 1000}, edge = {GW - 3, GH - 3, 10, 10};
    int i, x, y, guard_ok = 1;
    for (i = 0; i < (GW + 2) * (GH + 2); i++)
        buf[i] = GUARD;
    canvas_init(&c, buf + (GW + 2) + 1, GW, GH, GW + 2);
    canvas_fill(&c, huge, 0xFF000001u);
    canvas_fill(&c, edge, 0xFF000002u);
    canvas_fill_round(&c, huge, 30, 0xFF000003u);
    canvas_circle(&c, 0, 0, 25, 0xFF000004u);
    canvas_circle(&c, GW, GH, 25, 0xFF000004u);
    canvas_triangle(&c, GW - 1, -5, 30, 40, 1, 0xFF000005u);
    canvas_text(&c, &font_32x64, -20, -30, "Overflow test", 0xFF000006u);
    canvas_text(&c, &font_16x32, GW - 10, GH - 10, "edge", 0xFF000007u);
    canvas_text_wrap(&c, &font_12x24, huge, "wrapping text that is much longer than the canvas", 0xFF000008u);
    for (y = 0; y < GH + 2; y++)
        for (x = 0; x < GW + 2; x++) {
            int inside = x >= 1 && x <= GW && y >= 1 && y <= GH;
            if (!inside && buf[y * (GW + 2) + x] != GUARD)
                guard_ok = 0;
        }
    CHECK(guard_ok);
    CHECK(canvas_get(&c, -1, 0) == 0 && canvas_get(&c, GW, 0) == 0);
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

static void test_text_fit_and_wrap(void)
{
    ft_canvas c;
    ft_rect narrow = {0, 0, 12 * 10, 24 * 3};
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);

    CHECK(text_width(&font_16x32, "Games") == 5 * 16);
    canvas_clear(&c, 0);
    canvas_text_fit(&c, &font_12x24, 0, 0, 120, "Muda wa kucheza wa watoto kwa siku", 0xFFFFFFFFu);
    CHECK(has_ink(&c, 0, 120, 0, 24, 0));
    CHECK(!has_ink(&c, 120, 400, 0, 24, 0));

    canvas_clear(&c, 0);
    CHECK(canvas_text_wrap(&c, &font_12x24, narrow, "Chaneli za antena", 0xFFFFFFFFu) == 2);
    CHECK(!has_ink(&c, 120, 400, 0, 72, 0));
    canvas_clear(&c, 0);
    CHECK(canvas_text_wrap(&c, &font_12x24, narrow, "one two three four five six seven eight nine ten", 0xFFFFFFFFu) == 3);
    CHECK(!has_ink(&c, 0, 400, 72, 200, 0));
}

static void base(shell_config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    strcpy(cfg->favs[0].name, "SportyTV");
    cfg->favs[0].received = 1;
    cfg->nfav = 1;
    cfg->child_games = 0x7;
}

static void test_home_focus_is_visible(void)
{
    ft_canvas c;
    shell_config cfg;
    static shell s;
    ft_rect t0 = view_home_tile_rect(0), t1 = view_home_tile_rect(1);
    uint32_t h0, h1;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    base(&cfg);
    shell_init(&s, &cfg);
    view_shell(&c, &s, "20:01");
    CHECK(canvas_get(&c, t0.x + 2, t0.y + t0.h / 2) == COL_FOCUS);
    CHECK(canvas_get(&c, t1.x + 2, t1.y + t1.h / 2) == COL_TILE);
    CHECK(t0.x >= VIEW_MARGIN && view_home_tile_rect(5).x + view_home_tile_rect(5).w <= VIEW_W - VIEW_MARGIN);
    h0 = canvas_hash(&c);
    view_shell(&c, &s, "20:01");
    CHECK(canvas_hash(&c) == h0); /* deterministic */
    s.focus = 1;
    view_shell(&c, &s, "20:01");
    h1 = canvas_hash(&c);
    CHECK(h1 != h0);
    CHECK(canvas_get(&c, t1.x + 2, t1.y + t1.h / 2) == COL_FOCUS);
}

static void test_pin_screen_hides_other_digits(void)
{
    ft_canvas c;
    shell_config cfg;
    static shell a, b;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    base(&cfg);
    shell_init(&a, &cfg);
    shell_open_pin(&a, PIN_FOR_PARENT);
    b = a;
    memcpy(a.pin_digits, "1234", 4);
    memcpy(b.pin_digits, "9874", 4);
    a.pin_pos = b.pin_pos = 3;
    view_shell(&c, &a, "20:01");
    {
        uint32_t ha = canvas_hash(&c);
        view_shell(&c, &b, "20:01");
        CHECK(canvas_hash(&c) == ha);
    }
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
    CHECK(canvas_get(&c, last.x + 1, last.y + last.h / 2) == COL_FOCUS);
    CHECK(last.y + last.h < 606); /* rows stay clear of the status line */
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
    int lang, scr, st, ph, mode;
    canvas_init(&c, screen, VIEW_W, VIEW_H, VIEW_W);
    for (lang = 0; lang < LANG_COUNT; lang++) {
        base(&cfg);
        cfg.lang = (ft_lang)lang;
        for (scr = SCR_HOME; scr <= SCR_PIN; scr++) {
            shell_init(&s, &cfg);
            s.screen = (shell_screen)scr;
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
    test_text_fit_and_wrap();
    test_home_focus_is_visible();
    test_pin_screen_hides_other_digits();
    test_list_scrolls_to_focus();
    test_all_views_all_states();
}
