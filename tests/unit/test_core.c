/* Host unit tests for game rules, saves and the home shell.
 * Covers the logic parts of G01-G05, F03, U01, U07-U09 and R06 (plan pp.25-28).
 * Host results are not TV evidence. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "garden.h"
#include "penalty.h"
#include "platform.h"
#include "racer.h"
#include "save.h"
#include "session.h"
#include "shell.h"

#include "check.h"

int failures, checks;

/* ---- Family Racer --------------------------------------------------- */

static int popcount3(uint8_t m) { return (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1); }

static void test_racer_patterns_always_safe(void)
{
    int level, ok = 1;
    uint32_t seed;
    for (level = 0; level < RACER_LEVELS; level++) {
        for (seed = 1; seed <= 300; seed++) {
            racer r;
            uint8_t prev = 0;
            int i;
            racer_init(&r, seed, (racer_level)level, 0);
            for (i = 0; i < 2000; i++) {
                uint8_t row = racer_next_row(&r);
                if (popcount3(row) > 2 || (row && prev))
                    ok = 0;
                prev = row;
            }
        }
    }
    CHECK(ok);
}

/* Bot: steer toward a free lane in the next row before it reaches the car. */
static void bot_step(racer *r)
{
    uint8_t next = r->rows[RACER_CAR_ROW - 1];
    if (next & (1u << r->lane)) {
        int target, best = -1;
        for (target = 0; target < RACER_LANES; target++)
            if (!(next & (1u << target)) && (best < 0 || abs(target - r->lane) < abs(best - r->lane)))
                best = target;
        racer_key(r, best < r->lane ? FT_LEFT : FT_RIGHT);
    }
}

static void test_racer_bot_finishes_every_level(void)
{
    int level;
    uint32_t seed;
    for (level = 0; level < RACER_LEVELS; level++) {
        for (seed = 1; seed <= 20; seed++) {
            racer r;
            racer_init(&r, seed, (racer_level)level, 0);
            racer_key(&r, FT_OK);
            while (r.state == RACER_RUNNING) {
                bot_step(&r);
                racer_tick(&r);
            }
            CHECK(r.state == RACER_FINISHED);
            CHECK(r.hits == 0);
            CHECK(r.tick == RACER_DURATION_TICKS);
        }
    }
}

static void test_racer_replay_deterministic(void)
{
    racer a, b;
    int i;
    racer_init(&a, 42, RACER_MEDIUM, 0);
    racer_init(&b, 42, RACER_MEDIUM, 0);
    racer_key(&a, FT_OK);
    racer_key(&b, FT_OK);
    for (i = 0; i < 3000; i++) {
        ft_key k = (i % 37 == 0) ? FT_LEFT : (i % 53 == 0) ? FT_RIGHT : FT_NONE;
        racer_key(&a, k);
        racer_key(&b, k);
        racer_tick(&a);
        racer_tick(&b);
    }
    CHECK(a.score == b.score && a.hits == b.hits && a.state == b.state && a.lane == b.lane);
    CHECK(memcmp(a.rows, b.rows, sizeof a.rows) == 0);
}

static void test_racer_crash_and_practice(void)
{
    racer r;
    int i;
    racer_init(&r, 7, RACER_HARD, 0);
    racer_key(&r, FT_OK);
    for (i = 0; i < RACER_DURATION_TICKS && r.state == RACER_RUNNING; i++)
        racer_tick(&r); /* never steer */
    CHECK(r.state == RACER_CRASHED);
    CHECK(r.hits == RACER_LIVES);

    racer_init(&r, 7, RACER_HARD, 1);
    racer_key(&r, FT_OK);
    while (r.state == RACER_RUNNING)
        racer_tick(&r);
    CHECK(r.state == RACER_FINISHED); /* no-fail practice */
    CHECK(r.hits > 0);
}

static void test_racer_pause_and_exit(void)
{
    racer r;
    uint32_t t;
    racer_init(&r, 1, RACER_EASY, 0);
    racer_key(&r, FT_OK);
    racer_tick(&r);
    racer_key(&r, FT_BACK);
    CHECK(r.state == RACER_PAUSED);
    t = r.tick;
    racer_tick(&r);
    CHECK(r.tick == t);
    racer_key(&r, FT_OK);
    CHECK(r.state == RACER_RUNNING);
    racer_key(&r, FT_BACK);
    racer_key(&r, FT_BACK);
    CHECK(r.state == RACER_EXITED);
}

/* ---- Penalty Cup ---------------------------------------------------- */

static void take_shot(penalty *p, int score)
{
    int keeper = penalty_peek_keeper(p), i;
    p->aim = score ? (keeper + 1) % PEN_TARGETS : keeper;
    penalty_key(p, FT_OK);
    for (i = 0; i < PEN_RESULT_COOLDOWN; i++)
        penalty_tick(p);
    penalty_key(p, FT_OK);
}

static void test_penalty_rapid_ok_counts_once(void)
{
    penalty p;
    int i;
    penalty_init(&p, 3, 1);
    for (i = 0; i < 25; i++)
        penalty_key(&p, FT_OK); /* no ticks: cooldown never expires */
    CHECK(p.shots[0] + p.shots[1] == 1);
    CHECK(p.phase == PEN_RESULT);
}

static void test_penalty_early_win(void)
{
    penalty p;
    int i;
    penalty_init(&p, 5, 1);
    for (i = 0; i < 3; i++) {
        take_shot(&p, 1);
        take_shot(&p, 0);
    }
    CHECK(p.goals[0] == 3 && p.goals[1] == 0);
    CHECK(p.winner == 0);
    CHECK(p.phase == PEN_OVER);
}

static void test_penalty_tie_without_sudden_death(void)
{
    penalty p;
    int i;
    penalty_init(&p, 9, 0);
    for (i = 0; i < PEN_REGULATION * 2; i++)
        take_shot(&p, 1);
    CHECK(p.winner == PEN_TIE);
    CHECK(p.phase == PEN_OVER);
}

static void test_penalty_sudden_death(void)
{
    penalty p;
    int i;
    penalty_init(&p, 11, 1);
    for (i = 0; i < PEN_REGULATION * 2; i++)
        take_shot(&p, 1);
    CHECK(p.winner == PEN_NO_WINNER && p.phase == PEN_AIMING);
    take_shot(&p, 0);
    CHECK(p.winner == PEN_NO_WINNER); /* player 2 still to shoot */
    take_shot(&p, 1);
    CHECK(p.winner == 1);
    CHECK(p.shots[0] == 6 && p.shots[1] == 6);
}

static void test_penalty_pack_roundtrip(void)
{
    penalty p, q;
    uint8_t buf[PEN_PACKED_SIZE];
    take_shot((penalty_init(&p, 21, 1), &p), 1);
    take_shot(&p, 0);
    take_shot(&p, 1);
    CHECK(penalty_pack(&p, buf, sizeof buf) == PEN_PACKED_SIZE);
    CHECK(penalty_unpack(&q, buf, sizeof buf) == 0);
    CHECK(q.shots[0] == p.shots[0] && q.shots[1] == p.shots[1]);
    CHECK(q.goals[0] == p.goals[0] && q.goals[1] == p.goals[1]);
    CHECK(q.history[0] == p.history[0] && q.rng.s == p.rng.s);
    CHECK(penalty_peek_keeper(&q) == penalty_peek_keeper(&p));
    buf[18] = 9; /* more goals than shots */
    CHECK(penalty_unpack(&q, buf, sizeof buf) != 0);
}

/* ---- Block Garden --------------------------------------------------- */

static void test_garden_roundtrip_all_tiles_and_edges(void)
{
    static garden g, h;
    static uint8_t buf[GARDEN_PACKED_SIZE];
    int t;
    garden_init(&g);
    for (t = 0; t < GARDEN_TILE_TYPES; t++)
        g.tiles[t * 9] = (uint16_t)t;
    g.tiles[0] = 3;
    g.tiles[GARDEN_W - 1] = 4;
    g.tiles[(GARDEN_H - 1) * GARDEN_W] = 5;
    g.tiles[GARDEN_W * GARDEN_H - 1] = 7;
    CHECK(garden_pack(&g, buf, sizeof buf) == GARDEN_PACKED_SIZE);
    CHECK(garden_unpack(&h, buf, sizeof buf) == 0);
    CHECK(memcmp(g.tiles, h.tiles, sizeof g.tiles) == 0);
    buf[4] = GARDEN_TILE_TYPES; /* invalid tile id */
    CHECK(garden_unpack(&h, buf, sizeof buf) != 0);
    CHECK(garden_unpack(&h, buf, sizeof buf - 1) != 0);
}

static void test_garden_cursor_clamps_and_places(void)
{
    static garden g;
    int i;
    garden_init(&g);
    for (i = 0; i < 200; i++) {
        garden_key(&g, FT_LEFT);
        garden_key(&g, FT_UP);
    }
    CHECK(g.cx == 0 && g.cy == 0);
    garden_key(&g, FT_OK);
    CHECK(garden_get(&g, 0, 0) == 1 && g.dirty);
    for (i = 0; i < 200; i++) {
        garden_key(&g, FT_RIGHT);
        garden_key(&g, FT_DOWN);
    }
    CHECK(g.cx == GARDEN_W - 1 && g.cy == GARDEN_H - 1);
}

static void test_garden_menu_flow(void)
{
    static garden g;
    garden_init(&g);
    garden_key(&g, FT_BACK);
    CHECK(g.mode == GM_MENU);
    garden_key(&g, FT_DOWN);
    garden_key(&g, FT_OK);
    CHECK(g.mode == GM_PALETTE);
    garden_key(&g, FT_LEFT);
    CHECK(g.selected == 0); /* Erase */
    garden_key(&g, FT_OK);
    garden_key(&g, FT_OK);
    CHECK(garden_get(&g, g.cx, g.cy) == 0);
    garden_key(&g, FT_BACK);
    garden_key(&g, FT_DOWN);
    garden_key(&g, FT_DOWN);
    garden_key(&g, FT_DOWN);
    CHECK(garden_key(&g, FT_OK) == GA_SAVE_EXIT);
    CHECK(g.mode == GM_EXITED);
}

/* ---- Save service (F03) --------------------------------------------- */

static void corrupt_byte(const char *path, long off)
{
    FILE *f = fopen(path, "r+b");
    int c;
    if (!f)
        return;
    fseek(f, off, SEEK_SET);
    c = fgetc(f);
    fseek(f, off, SEEK_SET);
    fputc(c ^ 0xFF, f);
    fclose(f);
}

void test_clean_save(const char *p)
{
    char b[256];
    unlink(p);
    snprintf(b, sizeof b, "%s.prev", p); unlink(b);
    snprintf(b, sizeof b, "%s.bad", p); unlink(b);
    snprintf(b, sizeof b, "%s.tmp", p); unlink(b);
}

static void test_save_roundtrip_and_recovery(void)
{
    const char *p = TMP "/t.sav";
    char data[16] = "", bad[64];
    uint32_t len = 0;
    test_clean_save(p);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_NONE);
    CHECK(ft_save_write(p, "TEST", 1, "first", 6) == FT_SAVE_OK);
    CHECK(ft_save_write(p, "TEST", 1, "second", 7) == FT_SAVE_OK);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_OK);
    CHECK(len == 7 && strcmp(data, "second") == 0);

    corrupt_byte(p, 22); /* payload byte */
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_RESTORED_PREV);
    CHECK(len == 6 && strcmp(data, "first") == 0);
    snprintf(bad, sizeof bad, "%s.bad", p);
    CHECK(access(bad, F_OK) == 0); /* quarantined */

    /* A later save must not demote anything damaged into .prev. */
    CHECK(ft_save_write(p, "TEST", 1, "third", 6) == FT_SAVE_OK);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_OK && strcmp(data, "third") == 0);
    test_clean_save(p);
}

/* Windows rename() refuses to overwrite, which used to break the 3rd save
 * (once .prev existed) and every save after a quarantine (once .bad existed). */
static void test_save_many_writes_and_repeat_quarantine(void)
{
    const char *p = TMP "/m.sav";
    char data[16] = "", want[16];
    uint32_t len = 0;
    int i;
    test_clean_save(p);
    for (i = 1; i <= 5; i++) {
        snprintf(want, sizeof want, "save %d", i);
        CHECK(ft_save_write(p, "TEST", 1, want, (uint32_t)strlen(want) + 1) == FT_SAVE_OK);
    }
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_OK && strcmp(data, "save 5") == 0);
    corrupt_byte(p, 22);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_RESTORED_PREV);
    CHECK(strcmp(data, "save 4") == 0);
    for (i = 0; i < 2; i++) { /* the second quarantine finds .bad already there */
        CHECK(ft_save_write(p, "TEST", 1, "again", 6) == FT_SAVE_OK);
        corrupt_byte(p, 22);
        CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_RESTORED_PREV);
    }
    CHECK(ft_save_write(p, "TEST", 1, "last", 5) == FT_SAVE_OK);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_OK && strcmp(data, "last") == 0);
    test_clean_save(p);
}

static void test_save_rejects_wrong_schema_truncation_and_size(void)
{
    const char *p = TMP "/s.sav";
    char data[16];
    uint32_t len;
    FILE *f;
    test_clean_save(p);
    CHECK(ft_save_write(p, "TEST", 1, "abc", 4) == FT_SAVE_OK);
    CHECK(ft_save_read(p, "TEST", 2, data, sizeof data, &len) == FT_SAVE_CORRUPT);
    test_clean_save(p);
    CHECK(ft_save_write(p, "TEST", 1, "abc", 4) == FT_SAVE_OK);
    f = fopen(p, "r+b");
    if (f) {
        CHECK(ftruncate(fileno(f), 21) == 0);
        fclose(f);
    }
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_CORRUPT);
    CHECK(ft_save_write(p, "TEST", 1, data, FT_SAVE_MAX + 1) == FT_SAVE_TOO_BIG);
    test_clean_save(p);
}

/* ---- Home shell ----------------------------------------------------- */

static int64_t fixed_wall(void) { return 1792771200; /* 2026-10-23T16:00:00Z */ }
static uint64_t fixed_ms(void) { return 1000; }

static void base_config(shell_config *c)
{
    memset(c, 0, sizeof *c);
    strcpy(c->favs[0].name, "Citizen TV");
    c->favs[0].received = -1;
    strcpy(c->favs[1].name, "SportyTV");
    c->favs[1].received = 1;
    c->nfav = 2;
}

static void test_shell_hides_untested_apps(void)
{
    shell_config c;
    shell s;
    base_config(&c);
    shell_init(&s, &c);
    CHECK(s.ntiles == 4);
    CHECK(s.tiles[0] == TILE_LIVE_TV && s.tiles[1] == TILE_FOOTBALL);
    c.youtube_supported = 1;
    shell_init(&s, &c);
    CHECK(s.ntiles == 5 && s.tiles[1] == TILE_YOUTUBE);
}

static void test_shell_dad_reaches_favourite_in_two_choices(void)
{
    const char *path = TMP "/dad-settings.sav";
    shell_config c;
    shell s;
    session ss;
    shell_action a;
    int ok_presses = 0;
    test_clean_save(path);
    base_config(&c);
    shell_init(&s, &c);
    session_init_with_clock(&ss, &s, path, fixed_ms, fixed_wall);
    shell_key(&s, FT_RIGHT); /* focus Football */
    a = shell_key(&s, FT_OK);
    ok_presses++;
    CHECK(s.screen == SCR_FOOTBALL && a.act == ACT_NONE);
    a = shell_key(&s, FT_OK);
    ok_presses++;
    CHECK(a.act == ACT_TUNE_FAVOURITE && a.arg == 1); /* SportyTV, the only received one */
    CHECK(ok_presses <= 2);

    session_dispatch(&ss, &s, a);
    CHECK(strstr(s.status, "channel list") != NULL); /* sim cannot tune directly: fallback */
}

static void test_shell_unreceived_channels_not_shortcuts(void)
{
    shell_config c;
    shell s;
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int n;
    base_config(&c);
    c.favs[1].received = 0;
    shell_init(&s, &c);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_OK);
    n = shell_list(&s, labels, SHELL_MAX_ITEMS);
    CHECK(n == 1);
    CHECK(strstr(labels[0], "channel list") != NULL);
}

static void test_shell_child_profile_limits(void)
{
    shell_config c;
    shell s;
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int n, i, has_search = 0, has_settings = 0;
    base_config(&c);
    c.profile = PROFILE_CHILD;
    c.child_games = 1u << GAME_RACER;
    shell_init(&s, &c);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_RIGHT); /* Games */
    shell_key(&s, FT_OK);
    n = shell_list(&s, labels, SHELL_MAX_ITEMS);
    CHECK(n == 1 && strcmp(labels[0], "Family Racer") == 0);
    shell_key(&s, FT_BACK);
    CHECK(s.screen == SCR_HOME);
    shell_key(&s, FT_DOWN); /* short last row: lands on More */
    shell_key(&s, FT_OK);
    CHECK(s.screen == SCR_MORE);
    n = shell_list(&s, labels, SHELL_MAX_ITEMS);
    for (i = 0; i < n; i++) {
        if (strstr(labels[i], "search"))
            has_search = 1;
        if (strstr(labels[i], "settings"))
            has_settings = 1;
    }
    CHECK(!has_search && !has_settings);
}

static void test_shell_every_tile_reachable(void)
{
    shell_config c;
    shell s;
    int seen[TILE_COUNT] = {0}, i, k;
    static const ft_key walk[] = {FT_RIGHT, FT_RIGHT, FT_DOWN, FT_LEFT, FT_LEFT, FT_UP};
    base_config(&c);
    c.youtube_supported = c.netflix_supported = 1;
    shell_init(&s, &c);
    seen[s.tiles[s.focus]] = 1;
    for (k = 0; k < 6; k++) {
        shell_key(&s, walk[k]);
        seen[s.tiles[s.focus]] = 1;
    }
    for (i = 0; i < TILE_COUNT; i++)
        CHECK(seen[i]);
}

static void test_shell_kiswahili_labels(void)
{
    shell_config c;
    shell s;
    base_config(&c);
    c.lang = LANG_SW;
    shell_init(&s, &c);
    CHECK(strcmp(shell_tile_label(&s, TILE_GAMES), "Michezo") == 0);
    CHECK(strcmp(shell_screen_title(&s), "TV ya Familia") == 0);
}

int main(void)
{
    plat_mkdir("out");
    plat_mkdir(TMP);

    test_racer_patterns_always_safe();
    test_racer_bot_finishes_every_level();
    test_racer_replay_deterministic();
    test_racer_crash_and_practice();
    test_racer_pause_and_exit();
    test_penalty_rapid_ok_counts_once();
    test_penalty_early_win();
    test_penalty_tie_without_sudden_death();
    test_penalty_sudden_death();
    test_penalty_pack_roundtrip();
    test_garden_roundtrip_all_tiles_and_edges();
    test_garden_cursor_clamps_and_places();
    test_garden_menu_flow();
    test_save_roundtrip_and_recovery();
    test_save_many_writes_and_repeat_quarantine();
    test_save_rejects_wrong_schema_truncation_and_size();
    test_shell_hides_untested_apps();
    test_shell_dad_reaches_favourite_in_two_choices();
    test_shell_unreceived_channels_not_shortcuts();
    test_shell_child_profile_limits();
    test_shell_every_tile_reachable();
    test_shell_kiswahili_labels();
    run_service_tests();
    run_channel_tests();
    run_view_tests();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
