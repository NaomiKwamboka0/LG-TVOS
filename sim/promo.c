/* Renders the 30-second Family TV advert from the app's own views and
 * state machines: the home screen with its real motion, live TV (the stub
 * player's test card stands in for broadcasts, which belong to the
 * stations), sports, the games and the parent controls, each framed on a
 * moving backdrop with captions, then an end card. Writes raw 1920x1080
 * RGB frames at 30 fps to stdout for ffmpeg (make promo). Every frame is a
 * function of the frame number, so the same build makes the same video.
 *
 * familytv-promo                  all frames to stdout
 * familytv-promo --frame N OUT    frame N only, as a PPM picture */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "channels.h"
#include "platform.h"
#include "player.h"
#include "view.h"

#define FPS 30
#define TOTAL_MS 30000
#define PW 1920 /* physical frame size */
#define PH 1080

/* Scene start times, ms. */
enum { T_HOME = 2600, T_LIVE = 8000, T_SPORTS = 12600, T_GAMES = 17000, T_PARENTS = 21600, T_END = 25800 };

#define KENYA_RED FT_RGB(0xE53935)
#define KENYA_GREEN FT_RGB(0x22A85A)
#define BRAND_BLUE FT_RGB(0x2A4BD0)
#define URL_TEXT "github.com/NaomiKwamboka0/family-tv"

static uint32_t frame_px[PW * PH], app_px[PW * PH];
static ft_canvas F; /* the advert frame, logical 1280x720 (1.5x) */
static ft_canvas H; /* the same pixels at 2x: bigger headlines */
static ft_canvas B; /* the same pixels at 3x: the end card title */
static ft_canvas A; /* the app's screen, drawn full size, then framed */

/* ---- easing (as the start-up animation in ui/view.c) ---------------------- */

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

static int ease_back(uint32_t t, uint32_t from, uint32_t len)
{
    long v;
    if (t <= from)
        return 0;
    if (t >= from + len)
        return 256;
    v = (long)((t - from) * 256 / len) - 256;
    return (int)(256 + 691 * v * v * v / (256L * 256 * 256) + 435 * v * v / (256L * 256));
}

static int ease_in_out(uint32_t t, uint32_t from, uint32_t len)
{
    int u;
    if (t <= from)
        return 0;
    if (t >= from + len)
        return 256;
    u = (int)((t - from) * 256 / len);
    return u < 128 ? 2 * u * u / 256 : 256 - 2 * (256 - u) * (256 - u) / 256;
}

static int lerp(int a, int b, int t256)
{
    return a + (b - a) * t256 / 256;
}

/* ---- app state, advanced one frame at a time ------------------------------- */

static shell home, sports, parents;
static ui_motion home_motion;
static live tv;
static player *stub;
static racer race;
static penalty pen;
static int tv_has_picture;

static void config(shell_config *c, profile_role role)
{
    static channel_list all;
    memset(c, 0, sizeof *c);
    c->lang = LANG_EN;
    c->profile = role;
    c->child_games = 0x7;
    c->quality = 1;
    channels_defaults(&all);
    channels_group(&all, CH_TV, &c->channels);
    channels_group(&all, CH_SPORTS, &c->sports);
}

static void set_sport(shell *s, int i, sport_state st, const char *title)
{
    s->cfg.sport[i].state = st;
    snprintf(s->cfg.sport[i].title, sizeof s->cfg.sport[i].title, "%s", title);
}

static void setup(void)
{
    shell_config c;
    player_config pc;
    int i;

    config(&c, PROFILE_PARENT);
    shell_init(&home, &c);
    shell_init(&sports, &c);
    set_sport(&sports, 1, SPORT_LIVE, "Australia v Hong Kong China | Rugby World Cup 2027");
    set_sport(&sports, 7, SPORT_LIVE, "T1 | Day 2 | WTT Feeder Chennai 2026");
    set_sport(&sports, 2, SPORT_UPCOMING, "Venezuela v Colombia | FIBA U16 Women's AmeriCup");
    set_sport(&sports, 6, SPORT_UPCOMING, "Charlton Athletic v Chelsea | Barclays WSL");
    set_sport(&sports, 0, SPORT_CHECKING, "");
    sports.row = 2;
    sports.cfg.nfound = 3; /* example "Live football now" results (made-up streams) */
    snprintf(sports.cfg.found[0].title, sizeof sports.cfg.found[0].title, "Gor Mahia vs AFC Leopards | Derby watch along");
    snprintf(sports.cfg.found[0].by, sizeof sports.cfg.found[0].by, "Kenyan Football Live");
    sports.cfg.found[0].viewers = 12400;
    snprintf(sports.cfg.found[1].title, sizeof sports.cfg.found[1].title, "Harambee Stars vs Uganda | Qualifier");
    snprintf(sports.cfg.found[1].by, sizeof sports.cfg.found[1].by, "East Africa Football");
    sports.cfg.found[1].viewers = 5300;
    snprintf(sports.cfg.found[2].title, sizeof sports.cfg.found[2].title, "Tusker vs Shabana | Premier League");
    snprintf(sports.cfg.found[2].by, sizeof sports.cfg.found[2].by, "Matchday Kenya");
    sports.cfg.found[2].viewers = 860;

    shell_init(&parents, &c);
    shell_open_pin(&parents, PIN_FOR_SETTINGS);

    memset(&pc, 0, sizeof pc);
    pc.max_height = 480;
    stub = player_create_stub(&pc);
    live_init(&tv, home.cfg.channels.n, 0, 70);

    racer_init(&race, 7, RACER_MEDIUM, 1);
    racer_key(&race, FT_OK);
    for (i = 0; i < 240; i++) /* already under way when the scene starts */
        racer_tick(&race);

    penalty_init(&pen, 5, 1);
}

/* Keys pressed at given times in a scene. */
typedef struct {
    uint32_t at;
    ft_key key;
} press;

static const press home_keys[] = {{3300, FT_DOWN}, {5000, FT_RIGHT}, {5700, FT_RIGHT}, {6400, FT_RIGHT}};
static const press tv_keys[] = {{10000, FT_OK}, {10700, FT_DOWN}, {11200, FT_DOWN}, {11700, FT_DOWN}};

static int pressed(const press *p, size_t n, uint32_t prev, uint32_t t, ft_key *k)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (p[i].at > prev && p[i].at <= t) {
            *k = p[i].key;
            return 1;
        }
    return 0;
}

/* Steers round the next blocked lane, as a child would. */
static void race_step(void)
{
    uint8_t next = race.rows[RACER_CAR_ROW - 1];
    if (next & (1u << race.lane))
        racer_key(&race, (race.lane > 0 && !(next & (1u << (race.lane - 1)))) ? FT_LEFT : FT_RIGHT);
    racer_tick(&race);
}

/* Two goals and a save, then the aim sweeps and the last kick goes in. */
static void pen_shot(int score)
{
    int keeper = penalty_peek_keeper(&pen);
    pen.aim = score ? (keeper + 2) % PEN_TARGETS : keeper;
    penalty_key(&pen, FT_OK);
}

static void step(uint32_t prev, uint32_t t)
{
    ft_key k;
    int dt = (int)(t - prev);

    if (t >= T_HOME && t < T_LIVE) {
        ui_targets tg;
        if (pressed(home_keys, sizeof home_keys / sizeof home_keys[0], prev, t, &k))
            shell_key(&home, k);
        view_home_targets(&home, &tg);
        motion_step(&home_motion, &tg, home.tab, dt);
    }
    if (t >= T_LIVE && t < T_SPORTS) {
        if (prev < T_LIVE) {
            player_open(stub, home.cfg.channels.ch[0].url);
            player_pump(stub, 0);
            player_pump(stub, 2000);
            live_started(&tv);
        }
        if (pressed(tv_keys, sizeof tv_keys / sizeof tv_keys[0], prev, t, &k))
            live_key(&tv, k, 0);
        live_tick(&tv, dt);
        player_pump(stub, 2000 + (t - T_LIVE));
    }
    if (t >= T_SPORTS && t < T_GAMES) {
        if (prev < T_SPORTS + 1700 && t >= T_SPORTS + 1700)
            shell_key(&sports, FT_OK); /* the banner leads to the Sports tab */
        if (prev < T_SPORTS + 2300 && t >= T_SPORTS + 2300)
            shell_key(&sports, FT_DOWN); /* to "Live football now" */
        if (prev < T_SPORTS + 3100 && t >= T_SPORTS + 3100)
            shell_key(&sports, FT_RIGHT);
    }
    if (t >= T_GAMES && t < T_PARENTS) {
        uint32_t g = t - T_GAMES;
        if (g < 2200)
            race_step();
        else {
            /* goal, save, then the aim sweeps and the last kick goes in */
            uint32_t pg = prev - T_GAMES;
            if (pg < 2200 && g >= 2200)
                pen_shot(1);
            if (pg < 2650 && g >= 2650) {
                penalty_key(&pen, FT_OK); /* next kick */
                pen_shot(0);
            }
            if (pg < 3100 && g >= 3100)
                penalty_key(&pen, FT_OK);
            if (g >= 3100 && g < 3800)
                pen.aim = (int)((g - 3100) / 175) % PEN_TARGETS;
            if (pg < 3800 && g >= 3800)
                pen_shot(1);
            penalty_tick(&pen);
        }
    }
    if (t >= T_PARENTS && t < T_END) {
        uint32_t p = t - T_PARENTS;
        if (p < 2200) {
            /* the remote way: Up turns a digit, Right moves on */
            static const ft_key seq[] = {FT_UP, FT_UP, FT_RIGHT, FT_UP, FT_UP, FT_UP, FT_UP, FT_RIGHT,
                                         FT_UP, FT_RIGHT, FT_UP, FT_UP, FT_UP};
            uint32_t pp = prev - T_PARENTS;
            size_t i;
            for (i = 0; i < sizeof seq / sizeof seq[0]; i++) {
                uint32_t at = 500 + (uint32_t)i * 120;
                if (pp < at && p >= at)
                    shell_key(&parents, seq[i]);
            }
        } else if (parents.screen == SCR_PIN) {
            shell_config c;
            config(&c, PROFILE_CHILD);
            c.child_games = 0x3;
            c.child_limit_s = 3600;
            c.play_remaining_s = 25 * 60;
            shell_init(&parents, &c);
            parents.tab = TAB_APPS;
            parents.row = 0;
            parents.col[0] = 1;
        }
    }
}

/* ---- drawing -------------------------------------------------------------- */

/* The app's screen at time t, into A. */
static void draw_app(uint32_t t)
{
    if (t < T_LIVE)
        view_shell_motion(&A, &home, "20:01", &home_motion);
    else if (t < T_SPORTS) {
        tv_has_picture = player_render(stub, app_px, A.w, A.h, A.stride);
        view_live(&A, &tv, player_get(stub), &home, &home.cfg.channels, "20:01", tv_has_picture);
    } else if (t < T_GAMES)
        view_shell(&A, &sports, "20:01");
    else if (t < T_GAMES + 2200)
        view_racer(&A, &race, (const uint32_t[RACER_LEVELS]){412, 260, 0}, LANG_EN, "");
    else if (t < T_PARENTS)
        view_penalty(&A, &pen, LANG_EN, "");
    else
        view_shell(&A, &parents, "20:01");
}

/* Deep night blue with slowly drifting glows in the Kenyan flag's colours. */
static void backdrop(uint32_t t)
{
    double s = t / 1000.0;
    canvas_gradient(&F, (ft_rect){0, 0, VIEW_W, VIEW_H}, FT_RGB(0x121A3C), FT_RGB(0x05060B));
    canvas_glow(&F, (int)(260 + 90 * sin(s * 0.55)), (int)(170 + 50 * cos(s * 0.4)), 620, 420, color_alpha(BRAND_BLUE, 0x8C));
    canvas_glow(&F, (int)(1080 + 80 * cos(s * 0.5)), (int)(600 + 40 * sin(s * 0.7)), 560, 360, color_alpha(KENYA_RED, 0x4C));
    canvas_glow(&F, (int)(160 + 70 * cos(s * 0.35)), (int)(700 + 30 * sin(s * 0.6)), 480, 260, color_alpha(KENYA_GREEN, 0x38));
}

/* Black, red and green with white edges, as on the flag. w: 0..full. */
static void flag_bar(int x, int y, int w, int alpha)
{
    static const ft_color stripes[3] = {FT_RGB(0x000000), KENYA_RED, KENYA_GREEN};
    int i, seg = 30;
    for (i = 0; i < 3; i++) {
        int sx = x + i * (seg + 3), sw = w - i * (seg + 3);
        if (sw <= 0)
            break;
        if (sw > seg)
            sw = seg;
        canvas_round(&F, (ft_rect){sx - 1, y - 1, sw + 2, 8}, 4, color_alpha(FT_RGB(0xFFFFFF), (unsigned)alpha * 0xE0 / 256));
        canvas_round(&F, (ft_rect){sx, y, sw, 6}, 3, color_alpha(stripes[i], (unsigned)alpha * 255 / 256));
    }
}

/* Copies A into the physical rectangle r (logical F units, any size) with
 * rounded corners, 2x2 supersampled when shrinking; a: 0..256. */
static void blit_app(ft_rect r, int radius, int a)
{
    int x0 = r.x * 3 / 2, y0 = r.y * 3 / 2, w = r.w * 3 / 2, h = r.h * 3 / 2, rad = radius * 3 / 2;
    int taps = w < PW * 4 / 5 ? 2 : 1, x, y;
    if (a <= 0 || w <= 0 || h <= 0)
        return;
    for (y = 0; y < h; y++) {
        int py = y0 + y;
        if (py < 0 || py >= PH)
            continue;
        for (x = 0; x < w; x++) {
            int px = x0 + x, cov = 256, i, j;
            unsigned rr = 0, gg = 0, bb = 0;
            uint32_t *d;
            if (px < 0 || px >= PW)
                continue;
            /* corner rounding */
            {
                int cx = x < rad ? rad : x >= w - rad ? w - rad - 1 : -1;
                int cy = y < rad ? rad : y >= h - rad ? h - rad - 1 : -1;
                if (cx >= 0 && cy >= 0) {
                    double dist = hypot(x - cx, y - cy);
                    double c = rad - dist + 0.5;
                    if (c <= 0)
                        continue;
                    if (c < 1)
                        cov = (int)(c * 256);
                }
            }
            for (j = 0; j < taps; j++)
                for (i = 0; i < taps; i++) {
                    /* centre of sub-pixel (i, j) in source pixels, 16.16 fixed point */
                    long long sxf = ((long long)(x * taps + i) * 2 + 1) * ((long long)PW << 16) / (2LL * w * taps) - (1 << 15);
                    long long syf = ((long long)(y * taps + j) * 2 + 1) * ((long long)PH << 16) / (2LL * h * taps) - (1 << 15);
                    int sx, sy, fx, fy;
                    uint32_t p00, p10, p01, p11;
                    if (sxf < 0)
                        sxf = 0;
                    if (syf < 0)
                        syf = 0;
                    sx = (int)(sxf >> 16);
                    sy = (int)(syf >> 16);
                    fx = (int)((sxf >> 8) & 0xFF);
                    fy = (int)((syf >> 8) & 0xFF);
                    if (sx >= PW - 1) {
                        sx = PW - 2;
                        fx = 255;
                    }
                    if (sy >= PH - 1) {
                        sy = PH - 2;
                        fy = 255;
                    }
                    p00 = app_px[sy * PW + sx];
                    p10 = app_px[sy * PW + sx + 1];
                    p01 = app_px[(sy + 1) * PW + sx];
                    p11 = app_px[(sy + 1) * PW + sx + 1];
#define CH(p, s) (((p) >> (s)) & 0xFF)
#define BIL(s) ((CH(p00, s) * (256 - fx) + CH(p10, s) * fx) * (256 - fy) + (CH(p01, s) * (256 - fx) + CH(p11, s) * fx) * fy)
                    rr += BIL(16) >> 16;
                    gg += BIL(8) >> 16;
                    bb += BIL(0) >> 16;
#undef BIL
#undef CH
                }
            rr /= (unsigned)(taps * taps);
            gg /= (unsigned)(taps * taps);
            bb /= (unsigned)(taps * taps);
            {
                unsigned k = (unsigned)(cov * a / 256), dr, dg, db;
                d = &frame_px[py * PW + px];
                dr = (*d >> 16) & 0xFF;
                dg = (*d >> 8) & 0xFF;
                db = *d & 0xFF;
                *d = 0xFF000000u | ((dr + (rr - dr) * k / 256) & 0xFF) << 16 | ((dg + (gg - dg) * k / 256) & 0xFF) << 8 |
                     ((db + (bb - db) * k / 256) & 0xFF);
            }
        }
    }
}

/* The app framed like a screen: shadow, picture, thin bezel. */
static void screen_card(ft_rect r, int radius, int a)
{
    if (a <= 0)
        return;
    canvas_shadow(&F, (ft_rect){r.x + 10, r.y + 26, r.w - 20, r.h - 10}, 44, color_alpha(0xFF000000u, (unsigned)(0xA8 * a / 256)));
    blit_app(r, radius, a);
    if (radius > 0)
        canvas_round_stroke(&F, r, radius, 2, color_alpha(FT_RGB(0xFFFFFF), (unsigned)(0x30 * a * radius / 16 / 256)));
}

/* Where the screen sits during a scene [start, end): slides in from the
 * right, drifts slowly closer, fades out to the left. Returns the opacity. */
#define CARD_X 492
#define CARD_Y 150
#define CARD_W 728
#define CARD_H 410
static int card_place(uint32_t t, uint32_t start, uint32_t end, ft_rect *r)
{
    int in = ease_out(t, start, 520), out = ease_in_out(t, end - 380, 380);
    int grow = (int)((t - start) * 256 / (end - start)); /* slow push-in */
    int w = CARD_W * (968 + 32 * grow / 256) / 1000, h = CARD_H * (968 + 32 * grow / 256) / 1000;
    r->w = w;
    r->h = h;
    r->x = CARD_X + (CARD_W - w) / 2 + 70 * (256 - in) / 256 - 50 * out / 256;
    r->y = CARD_Y + (CARD_H - h) / 2;
    return in * (256 - out) / 256;
}

/* Text helpers: canvas c, alpha 0..256. */
static void text_a(ft_canvas *c, const ft_font *f, int x, int y, const char *s, ft_color col, int a)
{
    canvas_text(c, f, x, y, s, color_alpha(col, (unsigned)((col >> 24) * (unsigned)a / 256)));
}

/* Left-hand caption: kicker with the flag bar, a big two-line headline and
 * a short line under it; each part rises in a little after the last. */
static void caption(uint32_t t, uint32_t start, uint32_t end, const char *kicker, const char *head1, const char *head2,
                    const char *sub)
{
    int out = ease_in_out(t, end - 380, 380);
    int k = ease_out(t, start + 200, 500) * (256 - out) / 256;
    int h1 = ease_out(t, start + 320, 600) * (256 - out) / 256;
    int h2 = ease_out(t, start + 420, 600) * (256 - out) / 256;
    int sb = ease_out(t, start + 620, 600) * (256 - out) / 256;
    int x = 72, colw = CARD_X - 72 - 40;
    const ft_font *big = &font_display;
    ft_canvas *hc = &H;
    int hx, hy, lh;

    if (k <= 0)
        return;
    flag_bar(x, 214, 99 * ease_out(t, start + 150, 700) / 256, k);
    text_a(&F, &font_label, x, 232 + 12 * (256 - k) / 256, kicker, FT_RGB(0xFFD24A), k);

    /* Headline at 2x when it fits the column, else at the frame's own scale. */
    if (text_width(big, head1) * 4 / 3 > colw || text_width(big, head2) * 4 / 3 > colw)
        hc = &F;
    hx = hc == &H ? x * 3 / 4 : x;
    hy = hc == &H ? 272 * 3 / 4 : 272;
    lh = big->size + (hc == &H ? 4 : 6); /* tighter than body text, as in a poster */
    text_a(hc, big, hx, hy + 18 * (256 - h1) / 256, head1, FT_RGB(0xFFFFFF), h1);
    text_a(hc, big, hx, hy + lh + 18 * (256 - h2) / 256, head2, FT_RGB(0xFFFFFF), h2);
    {
        int sub_y = (hc == &H ? (hy + 2 * lh) * 4 / 3 : hy + 2 * lh) + 18;
        canvas_text_wrap(&F, &font_body, (ft_rect){x, sub_y + 12 * (256 - sb) / 256, colw, 120}, sub,
                         color_alpha(FT_RGB(0xE8EAED), (unsigned)(0xD0 * sb / 256)));
    }
}

/* Small Family TV mark, top left, during the scenes. */
static void corner_mark(int a)
{
    if (a <= 0)
        return;
    art_tile(&F, (ft_rect){72, 46, 34, 34}, 8, ART_LIVE_TV, 1, NULL);
    if (a < 256)
        canvas_round(&F, (ft_rect){70, 44, 38, 38}, 9, color_alpha(FT_RGB(0x0A0E1E), (unsigned)(255 * (256 - a) / 256)));
    text_a(&F, &font_label, 118, 50, "Family TV", FT_RGB(0xFFFFFF), a);
}

static void chip(int x, int y, const char *s, ft_color dot, int a)
{
    int w = text_width(&font_label, s) + (dot ? 58 : 40);
    canvas_round(&F, (ft_rect){x, y, w, 46}, 23, color_alpha(FT_RGB(0xFFFFFF), (unsigned)(0x1E * a / 256)));
    canvas_round_stroke(&F, (ft_rect){x, y, w, 46}, 23, 1, color_alpha(FT_RGB(0xFFFFFF), (unsigned)(0x40 * a / 256)));
    if (dot)
        canvas_circle(&F, x + 24, y + 23, 6, color_alpha(dot, (unsigned)(255 * a / 256)));
    text_a(&F, &font_label, x + (dot ? 40 : 20), y + 9, s, FT_RGB(0xFFFFFF), a);
}

static int chip_w(const char *s, int dot)
{
    return text_width(&font_label, s) + (dot ? 58 : 40);
}

static void end_card(uint32_t t)
{
    uint32_t e = t - T_END;
    int pop = ease_back(e, 250, 700), ring = ease_out(e, 600, 1100), title = ease_out(e, 700, 600);
    int line = ease_out(e, 950, 600), chips = ease_out(e, 1250, 600), url = ease_out(e, 1600, 600);
    int size = 132 * pop / 256, cx = VIEW_W / 2, cy = 196, gap = 14, total, x;
    static const char *names[3] = {"Windows", "Mac", "Free & open source"};
    static const ft_color dots[3] = {FT_RGB(0x4C8DFF), FT_RGB(0xE8EAED), KENYA_GREEN};
    int i;

    canvas_glow(&F, cx, cy, 120 + 380 * pop / 256, 90 + 230 * pop / 256, color_alpha(BRAND_BLUE, (unsigned)(0x90 * pop / 256)));
    if (ring > 0 && ring < 256)
        canvas_ring(&F, cx, cy, 80 + 220 * ring / 256, 3, color_alpha(FT_RGB(0xFFFFFF), (unsigned)(0x80 * (256 - ring) / 256)));
    if (size > 0) {
        ft_rect tile = {cx - size / 2, cy - size / 2, size, size};
        canvas_shadow(&F, (ft_rect){tile.x + 8, tile.y + 22, tile.w - 16, tile.h - 8}, 30, 0x99000000u);
        art_tile(&F, tile, size / 5, ART_LIVE_TV, 1, NULL);
    }
    if (title > 0) /* 3x canvas: logical 640x360 */
        canvas_text_center(&B, &font_display, (ft_rect){0, 140 + 10 * (256 - title) / 256, 640, font_display.line_height},
                           "Family TV", color_alpha(FT_RGB(0xFFFFFF), (unsigned)(255 * title / 256)));
    if (line > 0)
        canvas_text_center(&H, &font_title, (ft_rect){0, 318 + 8 * (256 - line) / 256, 960, font_title.line_height},
                           "Kenyan TV for the whole family.", color_alpha(FT_RGB(0xE8EAED), (unsigned)(0xE0 * line / 256)));
    flag_bar(cx - 49, 486, 99 * line / 256, line);
    if (chips > 0) {
        total = 0;
        for (i = 0; i < 3; i++)
            total += chip_w(names[i], 1) + (i ? gap : 0);
        x = cx - total / 2;
        for (i = 0; i < 3; i++) {
            chip(x, 510 + 10 * (256 - chips) / 256, names[i], dots[i], chips);
            x += chip_w(names[i], 1) + gap;
        }
    }
    if (url > 0)
        canvas_text_center(&F, &font_body, (ft_rect){0, 590, VIEW_W, font_body.line_height},
                           "Download free at  " URL_TEXT, color_alpha(FT_RGB(0xFFD24A), (unsigned)(255 * url / 256)));
}

static void scene_card(uint32_t t, uint32_t start, uint32_t end)
{
    ft_rect r;
    int a = card_place(t, start, end, &r);
    screen_card(r, 16, a);
    corner_mark(256);
}

static void draw(uint32_t t)
{
    ft_rect r;
    int a;

    if (t < T_HOME) { /* the app's own start-up animation, full frame */
        view_startup(&F, t, LANG_EN);
        return;
    }
    backdrop(t);
    draw_app(t);
    if (t < T_LIVE) {
        /* home screen full frame, then pulls back into its place */
        int back = ease_in_out(t, T_HOME + 1200, 900), radius = 16;
        a = card_place(t, T_HOME, T_LIVE, &r);
        if (back < 256) {
            a = 256;
            r.x = lerp(0, CARD_X, back);
            r.y = lerp(0, CARD_Y, back);
            r.w = lerp(VIEW_W, CARD_W * 968 / 1000, back);
            r.h = lerp(VIEW_H, CARD_H * 968 / 1000, back);
            radius = 16 * back / 256;
        }
        screen_card(r, radius, a);
        corner_mark(ease_out(t, T_HOME + 1700, 600));
        caption(t, T_HOME + 1500, T_LIVE, "LIVE TV", "Kenyan TV,", "live.",
                "Citizen, NTV, KTN, K24, KBC and more, straight from each station's own stream.");
    } else if (t < T_SPORTS) {
        scene_card(t, T_LIVE, T_SPORTS);
        caption(t, T_LIVE, T_SPORTS, "WATCH", "Zap like", "a real TV.",
                "Up and down to change channel, OK for the full list. Just like a remote.");
    } else if (t < T_GAMES) {
        scene_card(t, T_SPORTS, T_GAMES);
        caption(t, T_SPORTS, T_GAMES, "SPORTS", "See what's", "live now.",
                "Official channels and live football from YouTube, all in one Sports tab.");
    } else if (t < T_PARENTS) {
        scene_card(t, T_GAMES, T_PARENTS);
        caption(t, T_GAMES, T_PARENTS, "GAMES", "Play time,", "built in.",
                "Penalty Cup, Racer and Garden. No ads, no sign-up, no internet needed.");
    } else if (t < T_END) {
        scene_card(t, T_PARENTS, T_END);
        caption(t, T_PARENTS, T_END, "PARENTS", "You stay", "in charge.",
                "PIN-locked settings, a child profile and a daily play-time limit.");
    } else {
        corner_mark(256 - ease_out(t, T_END, 300));
        end_card(t);
    }
}

static void write_rgb(FILE *f)
{
    static unsigned char row[PW * 3];
    int x, y;
    for (y = 0; y < PH; y++) {
        for (x = 0; x < PW; x++) {
            uint32_t p = frame_px[y * PW + x];
            row[x * 3] = (unsigned char)(p >> 16);
            row[x * 3 + 1] = (unsigned char)(p >> 8);
            row[x * 3 + 2] = (unsigned char)p;
        }
        fwrite(row, 1, sizeof row, f);
    }
}

int main(int argc, char **argv)
{
    int only = -1, n, frames = TOTAL_MS * FPS / 1000;
    const char *out_path = NULL;
    uint32_t prev = 0;
    FILE *out = stdout;

    if (argc == 4 && !strcmp(argv[1], "--frame")) {
        only = atoi(argv[2]);
        out_path = argv[3];
    } else if (argc != 1) {
        fprintf(stderr, "usage: familytv-promo [--frame N OUT.ppm]\n");
        return 2;
    }
    if (font_scale_init("assets/fonts/inter/") != 0) {
        fprintf(stderr, "familytv-promo: run from the source folder (needs assets/fonts/inter)\n");
        return 1;
    }
    canvas_init(&F, frame_px, PW, PH, PW);
    canvas_set_scale(&F, CANVAS_1_5X);
    canvas_init(&H, frame_px, PW, PH, PW);
    canvas_set_scale(&H, CANVAS_2X);
    canvas_init(&B, frame_px, PW, PH, PW);
    canvas_set_scale(&B, CANVAS_3X);
    canvas_init(&A, app_px, PW, PH, PW);
    canvas_set_scale(&A, CANVAS_1_5X);
    setup();
    if (!stub)
        return 1;

    for (n = 0; n < frames && (only < 0 || n <= only); n++) {
        uint32_t t = (uint32_t)n * 1000 / FPS;
        step(prev, t);
        prev = t;
        if (only >= 0 && n != only)
            continue;
        draw(t);
        if (only >= 0) {
            out = fopen(out_path, "wb");
            if (!out) {
                perror(out_path);
                return 1;
            }
            fprintf(out, "P6\n%d %d\n255\n", PW, PH);
        }
        write_rgb(out);
        if (only >= 0)
            return fclose(out) != 0;
    }
    player_destroy(stub);
    return fflush(out) != 0;
}
