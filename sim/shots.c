/* Renders every screen from fixed, repeatable states and writes PPM images
 * for review (docs/screens). Live TV shots use the stub player's test card
 * in place of a real broadcast. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "channels.h"
#include "platform.h"
#include "player.h"
#include "view.h"

static uint32_t pixels[3840 * 2160]; /* up to 3x */
static ft_canvas canvas;
static const char *out_dir;
static int failures;

static void save(const char *name)
{
    char path[512];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/%s.ppm", out_dir, name);
    f = fopen(path, "wb");
    if (!f) {
        perror(path);
        failures++;
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", canvas.w, canvas.h);
    for (i = 0; i < canvas.w * canvas.h; i++) {
        unsigned char rgb[3];
        rgb[0] = (unsigned char)(pixels[i] >> 16);
        rgb[1] = (unsigned char)(pixels[i] >> 8);
        rgb[2] = (unsigned char)pixels[i];
        fwrite(rgb, 1, 3, f);
    }
    if (fclose(f) != 0)
        failures++;
    printf("%s\n", path);
}

static void config(shell_config *c, ft_lang lang, profile_role role)
{
    memset(c, 0, sizeof *c);
    c->lang = lang;
    c->profile = role;
    c->child_games = 0x7;
    c->quality = 1;
    {
        static channel_list all;
        channels_defaults(&all);
        channels_group(&all, CH_TV, &c->channels);
        channels_group(&all, CH_SPORTS, &c->sports);
    }
}

static void shell_shots(void)
{
    shell_config c;
    static shell s;

    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);
    view_shell(&canvas, &s, "20:01");
    save("01-for-you");

    s.row = 1;
    s.col[1] = 3;
    view_shell(&canvas, &s, "20:01");
    save("02-live-channels-row");

    shell_init(&s, &c);
    s.tab = TAB_LIVE;
    s.row = 2;
    view_shell(&canvas, &s, "20:01");
    save("03-channel-guide");

    config(&c, LANG_EN, PROFILE_CHILD);
    c.child_games = 0x3;
    c.child_limit_s = 3600;
    c.play_remaining_s = 25 * 60;
    shell_init(&s, &c);
    s.tab = TAB_APPS;
    s.row = 0;
    s.col[0] = 1;
    view_shell(&canvas, &s, "20:01");
    save("04-apps-child");

    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);
    s.row = -1;
    s.top = TOP_GEAR;
    s.screen = SCR_MORE;
    view_shell(&canvas, &s, "20:01");
    save("05-settings");

    shell_open_pin(&s, PIN_FOR_SETTINGS);
    s.pin_pos = 1;
    s.pin_digits[1] = '7';
    view_shell(&canvas, &s, "20:01");
    save("06-pin");

    c.child_limit_s = 3600;
    shell_init(&s, &c);
    shell_open_settings(&s);
    s.item = 8;
    view_shell(&canvas, &s, "20:01");
    save("07-parent-settings");

    config(&c, LANG_SW, PROFILE_PARENT);
    shell_init(&s, &c);
    s.row = -1;
    s.top = TAB_FOR_YOU;
    view_shell(&canvas, &s, "20:01");
    save("08-home-kiswahili");

    /* Sports: example answers (match titles as the channels wrote them). */
    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);
    s.cfg.sport[1].state = SPORT_LIVE;
    snprintf(s.cfg.sport[1].title, sizeof s.cfg.sport[1].title, "Australia v Hong Kong China | Rugby World Cup 2027");
    s.cfg.sport[7].state = SPORT_LIVE;
    snprintf(s.cfg.sport[7].title, sizeof s.cfg.sport[7].title, "T1 | Day 2 | WTT Feeder Chennai 2026");
    s.cfg.sport[2].state = SPORT_UPCOMING;
    snprintf(s.cfg.sport[2].title, sizeof s.cfg.sport[2].title, "Venezuela v Colombia | FIBA U16 Women's AmeriCup");
    s.cfg.sport[6].state = SPORT_UPCOMING;
    snprintf(s.cfg.sport[6].title, sizeof s.cfg.sport[6].title, "Charlton Athletic v Chelsea | Barclays WSL");
    s.cfg.sport[0].state = SPORT_CHECKING;
    s.row = 2;
    view_shell(&canvas, &s, "20:01");
    save("21-sports-banner");
    s.screen = SCR_SPORTS;
    s.item = 1;
    view_shell(&canvas, &s, "20:01");
    save("22-sports-page");
    s.screen = SCR_HOME;
    s.row = 3;
    s.col[3] = 1;
    view_shell(&canvas, &s, "20:01");
    save("23-netflix-tile");

    view_startup(&canvas, 450, LANG_EN);
    save("24-startup-glow");
    view_startup(&canvas, 1500, LANG_EN);
    save("25-startup-name");
}

/* Puts the stub's picture of channel ch on the canvas; returns 1. */
static int picture(player *p, const shell *s, int ch)
{
    player_open(p, s->cfg.channels.ch[ch].url);
    player_pump(p, 0);
    player_pump(p, 2000);
    return player_render(p, pixels, canvas.w, canvas.h, canvas.stride);
}

static void live_shots(void)
{
    shell_config c;
    static shell s;
    player_config pc;
    player *p;
    live l;

    memset(&pc, 0, sizeof pc);
    pc.max_height = 480;
    p = player_create_stub(&pc);
    if (!p) {
        failures++;
        return;
    }
    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);

    live_init(&l, c.channels.n, 0, 70);
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", picture(p, &s, 0));
    save("09-live-banner");

    l.banner_ms = 0;
    l.panel_open = 1;
    l.panel_item = 3;
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", picture(p, &s, 0));
    save("10-live-channel-list");

    live_init(&l, c.channels.n, 1, 70);
    l.banner_ms = 0;
    l.volume_ms = 1000;
    l.volume = 45;
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", picture(p, &s, 1));
    save("11-live-volume");

    live_init(&l, c.channels.n, 2, 70);
    l.anim_ms = 330;
    player_open(p, c.channels.ch[2].url);
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", 0);
    save("12-live-connecting");

    live_init(&l, c.channels.n, 4, 70);
    l.banner_ms = 0;
    player_open(p, "https://stub.invalid/not-live");
    player_pump(p, 0);
    player_pump(p, 2000);
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", 0);
    save("13-live-not-live");

    config(&c, LANG_SW, PROFILE_PARENT);
    shell_init(&s, &c);
    player_open(p, "https://stub.invalid/offline");
    player_pump(p, 0);
    player_pump(p, 2000);
    view_live(&canvas, &l, player_get(p), &s, &s.cfg.channels, "20:01", 0);
    save("14-live-offline-kiswahili");
    player_destroy(p);
}

static void racer_shots(void)
{
    static const uint32_t best[RACER_LEVELS] = {412, 260, 0};
    racer r;
    int i;

    racer_init(&r, 7, RACER_MEDIUM, 0);
    view_racer(&canvas, &r, best, LANG_EN, "");
    save("15-racer-start");

    racer_key(&r, FT_OK);
    for (i = 0; i < 900; i++) {
        uint8_t next = r.rows[RACER_CAR_ROW - 1];
        if (next & (1u << r.lane))
            racer_key(&r, (r.lane > 0 && !(next & (1u << (r.lane - 1)))) ? FT_LEFT : FT_RIGHT);
        racer_tick(&r);
    }
    view_racer(&canvas, &r, best, LANG_EN, "");
    save("16-racer-running");
}

static void take(penalty *p, int score)
{
    int keeper = penalty_peek_keeper(p), i;
    p->aim = score ? (keeper + 2) % PEN_TARGETS : keeper;
    penalty_key(p, FT_OK);
    for (i = 0; i < PEN_RESULT_COOLDOWN; i++)
        penalty_tick(p);
}

static void penalty_shots(void)
{
    penalty p;
    penalty_init(&p, 5, 1);
    take(&p, 1);
    penalty_key(&p, FT_OK);
    take(&p, 0);
    penalty_key(&p, FT_OK);
    take(&p, 1);
    penalty_key(&p, FT_OK);
    p.aim = 4;
    view_penalty(&canvas, &p, LANG_EN, "");
    save("17-penalty-aim");

    take(&p, 1);
    view_penalty(&canvas, &p, LANG_EN, "");
    save("18-penalty-goal");
}

static void garden_shots(void)
{
    static garden g;
    int x, y;
    garden_init(&g);
    for (y = 0; y < GARDEN_H; y++)
        for (x = 0; x < GARDEN_W; x++) {
            uint16_t t = 0;
            if (y > 44)
                t = y == 45 ? 1 : (y < 52 ? 2 : 3);
            if (y >= 40 && y <= 44 && x >= 20 && x <= 30)
                t = (x == 20 || x == 30 || y == 40) ? 7 : 0;
            if (y >= 36 && y <= 39 && x >= 19 + (39 - y) && x <= 31 - (39 - y))
                t = 4;
            if (y == 44 && (x == 10 || x == 13 || x == 40))
                t = 6;
            if (y >= 45 && y <= 47 && x >= 44 && x <= 56)
                t = 5;
            g.tiles[y * GARDEN_W + x] = t;
        }
    g.cx = 25;
    g.cy = 42;
    g.selected = 7;
    g.dirty = 1;
    view_garden(&canvas, &g, LANG_EN, "");
    save("19-garden");

    g.mode = GM_MENU;
    g.menu = 2;
    view_garden(&canvas, &g, LANG_SW, "");
    save("20-garden-menu-kiswahili");
}

int main(int argc, char **argv)
{
    /* familytv-shots [DIR [SCALE]]: SCALE 2 = 720p (default), 3 = 1080p, 6 = 4K */
    int sc = argc > 2 ? atoi(argv[2]) : CANVAS_1X;
    out_dir = argc > 1 ? argv[1] : "out/screens";
    plat_mkdir(out_dir);
    if (sc != CANVAS_1_5X && sc != CANVAS_2X && sc != CANVAS_3X)
        sc = CANVAS_1X;
    if (sc != CANVAS_1X && font_scale_init("assets/fonts/inter/") != 0)
        fprintf(stderr, "fonts not found: text drawn from the 1x fonts\n");
    canvas_init(&canvas, pixels, VIEW_W * sc / 2, VIEW_H * sc / 2, VIEW_W * sc / 2);
    canvas_set_scale(&canvas, sc);
    shell_shots();
    live_shots();
    racer_shots();
    penalty_shots();
    garden_shots();
    return failures ? 1 : 0;
}
