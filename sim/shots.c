/* Renders every screen from fixed, repeatable states and writes PPM images
 * for review (docs/screens). Host tool only; not device evidence. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "view.h"

static uint32_t pixels[VIEW_W * VIEW_H];
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
    fprintf(f, "P6\n%d %d\n255\n", VIEW_W, VIEW_H);
    for (i = 0; i < VIEW_W * VIEW_H; i++) {
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
    static const char *const names[] = {"Citizen TV (demo)", "KTN (demo)", "SportyTV (demo)"};
    int i;
    memset(c, 0, sizeof *c);
    c->lang = lang;
    c->profile = role;
    c->child_games = 0x7;
    for (i = 0; i < 3; i++) {
        snprintf(c->favs[i].name, sizeof c->favs[i].name, "%s", names[i]);
        c->favs[i].received = 1;
    }
    c->nfav = 3;
}

static void shell_shots(void)
{
    shell_config c;
    static shell s;

    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);
    view_shell(&canvas, &s, "20:01");
    save("01-home");

    s.focus = 1;
    s.screen = SCR_FOOTBALL;
    s.item = 2;
    snprintf(s.status, sizeof s.status, "[SIMULATOR] Direct tuning not verified: TV channel list opened, choose SportyTV (demo)");
    view_shell(&canvas, &s, "20:01");
    save("02-football");

    config(&c, LANG_EN, PROFILE_CHILD);
    c.child_games = 0x3;
    c.child_limit_s = 3600;
    c.play_remaining_s = 25 * 60;
    shell_init(&s, &c);
    s.screen = SCR_GAMES;
    view_shell(&canvas, &s, "20:01");
    save("03-games-child");

    config(&c, LANG_EN, PROFILE_PARENT);
    shell_init(&s, &c);
    s.screen = SCR_MORE;
    shell_open_pin(&s, PIN_FOR_SETTINGS);
    s.pin_pos = 1;
    s.pin_digits[1] = '7';
    view_shell(&canvas, &s, "20:01");
    save("04-pin");

    c.child_limit_s = 3600;
    shell_init(&s, &c);
    shell_open_settings(&s);
    s.item = 1;
    view_shell(&canvas, &s, "20:01");
    save("05-parent-settings");

    config(&c, LANG_SW, PROFILE_PARENT);
    shell_init(&s, &c);
    view_shell(&canvas, &s, "20:01");
    save("06-home-kiswahili");
}

static void racer_shots(void)
{
    static const uint32_t best[RACER_LEVELS] = {412, 260, 0};
    racer r;
    int i;

    racer_init(&r, 7, RACER_MEDIUM, 0);
    view_racer(&canvas, &r, best, LANG_EN, "");
    save("07-racer-start");

    racer_key(&r, FT_OK);
    for (i = 0; i < 900; i++) {
        uint8_t next = r.rows[RACER_CAR_ROW - 1];
        if (next & (1u << r.lane))
            racer_key(&r, (r.lane > 0 && !(next & (1u << (r.lane - 1)))) ? FT_LEFT : FT_RIGHT);
        racer_tick(&r);
    }
    view_racer(&canvas, &r, best, LANG_EN, "");
    save("08-racer-running");
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
    save("09-penalty-aim");

    take(&p, 1);
    view_penalty(&canvas, &p, LANG_EN, "");
    save("10-penalty-goal");
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
    save("11-garden");

    g.mode = GM_MENU;
    g.menu = 2;
    view_garden(&canvas, &g, LANG_SW, "");
    save("12-garden-menu-kiswahili");
}

int main(int argc, char **argv)
{
    out_dir = argc > 1 ? argv[1] : "out/screens";
    mkdir(out_dir, 0755);
    canvas_init(&canvas, pixels, VIEW_W, VIEW_H, VIEW_W);
    shell_shots();
    racer_shots();
    penalty_shots();
    garden_shots();
    return failures ? 1 : 0;
}
