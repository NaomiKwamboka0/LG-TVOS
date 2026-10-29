/* Host unit tests for game rules, saves and the home shell.
 * Covers the logic parts of G01-G05, F03, U01, U07-U09 and R06 (plan pp.25-28).
 * Host results are not TV evidence. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "channels.h"
#include "garden.h"
#include "live.h"
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

void test_corrupt_byte(const char *path, long off)
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

    test_corrupt_byte(p, 22); /* payload byte */
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
    test_corrupt_byte(p, 22);
    CHECK(ft_save_read(p, "TEST", 1, data, sizeof data, &len) == FT_SAVE_RESTORED_PREV);
    CHECK(strcmp(data, "save 4") == 0);
    for (i = 0; i < 2; i++) { /* the second quarantine finds .bad already there */
        CHECK(ft_save_write(p, "TEST", 1, "again", 6) == FT_SAVE_OK);
        test_corrupt_byte(p, 22);
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
    static channel_list all;
    memset(c, 0, sizeof *c);
    c->child_games = 0x7;
    channels_defaults(&all);
    channels_group(&all, CH_TV, &c->channels);
    channels_group(&all, CH_SPORTS, &c->sports);
}

static int rows_of(const shell *s, shell_row *rows)
{
    return shell_home_rows(s, rows, SHELL_MAX_ROWS);
}

static void test_home_rows_per_tab(void)
{
    static shell_row rows[SHELL_MAX_ROWS];
    shell_config c;
    static shell s;
    int n, i, guide = 1;
    base_config(&c);
    c.last_channel = 3;
    shell_init(&s, &c);

    n = rows_of(&s, rows); /* For you: banner, channels, sports, watch more, games */
    CHECK(n == 5);
    CHECK(rows[0].style == ROW_HERO && rows[0].n == 1);
    CHECK(rows[0].cards[0].kind == CARD_CHANNEL && rows[0].cards[0].arg == 3); /* last watched */
    CHECK(rows[1].style == ROW_CARDS && rows[1].n == c.channels.n && rows[1].title == STR_LIVE_CHANNELS);
    CHECK(rows[2].style == ROW_BANNER && rows[2].cards[0].kind == CARD_SPORTS);
    CHECK(rows[3].n == APP_COUNT && rows[3].cards[0].kind == CARD_APP && rows[3].cards[0].arg == APP_NETFLIX);
    CHECK(rows[4].n == GAME_COUNT && rows[4].cards[0].kind == CARD_GAME);
    CHECK(c.channels.n == 14 && c.sports.n == 11); /* sports stay off the channel list */

    s.tab = TAB_LIVE; /* a guide row per channel */
    n = rows_of(&s, rows);
    CHECK(n == c.channels.n);
    for (i = 0; i < n; i++)
        if (rows[i].style != ROW_GUIDE || rows[i].n != 1 || rows[i].cards[0].arg != i)
            guide = 0;
    CHECK(guide && rows[0].title == STR_ALL_CHANNELS && rows[1].title == STR_COUNT);

    s.tab = TAB_APPS;
    n = rows_of(&s, rows);
    CHECK(n == 3 && rows[0].n == GAME_COUNT);
    CHECK(rows[1].n == 1 + APP_COUNT && rows[1].cards[0].kind == CARD_SPORTS);
    CHECK(rows[2].n == 3 && rows[2].cards[0].kind == CARD_SETTINGS && rows[2].cards[2].kind == CARD_ABOUT);

    c.last_channel = 99; /* stale: falls back to the first channel */
    c.channels.n = 0;    /* and with no channels at all, say so */
    c.sports.n = 0;
    shell_init(&s, &c);
    n = rows_of(&s, rows);
    CHECK(n == 3 && rows[0].cards[0].kind == CARD_NO_CHANNELS);
    CHECK(shell_key(&s, FT_OK).act == ACT_NONE);
    s.tab = TAB_LIVE;
    CHECK(rows_of(&s, rows) == 1 && rows[0].cards[0].kind == CARD_NO_CHANNELS);
    base_config(&c);
    c.last_channel = 99;
    shell_init(&s, &c);
    rows_of(&s, rows);
    CHECK(rows[0].cards[0].arg == 0);
}

static void test_shell_dad_reaches_channel_in_two_presses(void)
{
    const char *path = TMP "/dad-settings.sav";
    shell_config c;
    static shell s;
    static session ss;
    shell_action a;
    test_clean_save(path);
    base_config(&c);
    shell_init(&s, &c);
    session_init_with_clock(&ss, &s, path, fixed_ms, fixed_wall);

    a = shell_key(&s, FT_OK); /* one press: the banner channel */
    CHECK(a.act == ACT_PLAY_CHANNEL && a.arg == 0);
    CHECK(session_dispatch(&ss, &s, a) == SESSION_WATCH && ss.watch_channel == 0);

    shell_key(&s, FT_DOWN); /* any channel: down, across, OK */
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_RIGHT);
    a = shell_key(&s, FT_OK);
    CHECK(a.act == ACT_PLAY_CHANNEL && a.arg == 2);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_PLAY_CHANNEL, c.channels.n}) == -1); /* bad index */
    test_clean_save(path);
}

/* Sports: the banner leads to the Sports tab; live official matches come
 * first, then live football found by search (parents only), what is
 * coming up and every sports channel. */
static void test_sports_tab(void)
{
    const char *path = TMP "/sports-settings.sav";
    static shell_row rows[SHELL_MAX_ROWS];
    shell_config c;
    static shell s;
    static session ss;
    shell_action a;
    int n;
    test_clean_save(path);
    base_config(&c);
    shell_init(&s, &c);
    session_init_with_clock(&ss, &s, path, fixed_ms, fixed_wall);
    s.cfg.sport[4].state = SPORT_LIVE;
    s.cfg.sport[2].state = SPORT_UPCOMING;
    s.cfg.nfound = 2;
    snprintf(s.cfg.found[0].video_id, sizeof s.cfg.found[0].video_id, "abc");
    snprintf(s.cfg.found[0].title, sizeof s.cfg.found[0].title, "Home vs Away");
    shell_key(&s, FT_DOWN);
    shell_key(&s, FT_DOWN); /* the Sports banner */
    shell_key(&s, FT_OK);
    CHECK(s.screen == SCR_HOME && s.tab == TAB_SPORTS && s.top == TAB_SPORTS && s.row == 0);
    CHECK(strcmp(shell_tab_label(LANG_EN, TAB_SPORTS), "Sports") == 0);

    n = rows_of(&s, rows);
    CHECK(n == 4);
    CHECK(rows[0].title == STR_ON_NOW && rows[0].n == 1 && rows[0].cards[0].kind == CARD_SPORT && rows[0].cards[0].arg == 4);
    CHECK(rows[1].title == STR_LIVE_FOOTBALL && rows[1].n == 2 && rows[1].cards[1].kind == CARD_FOUND);
    CHECK(rows[2].title == STR_COMING_UP && rows[2].n == 1 && rows[2].cards[0].arg == 2);
    CHECK(rows[3].title == STR_SPORTS_CHANNELS && rows[3].n == c.sports.n && rows[3].cards[0].arg == 4);

    a = shell_key(&s, FT_OK);
    CHECK(a.act == ACT_PLAY_SPORT && a.arg == 4);
    CHECK(session_dispatch(&ss, &s, a) == SESSION_WATCH && ss.watch_list == WATCH_SPORTS && ss.watch_channel == 4);
    shell_key(&s, FT_DOWN);
    a = shell_key(&s, FT_OK);
    CHECK(a.act == ACT_PLAY_FOUND && a.arg == 0);
    CHECK(session_dispatch(&ss, &s, a) == SESSION_WATCH && ss.watch_list == WATCH_FOUND && ss.watch_channel == 0);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_PLAY_FOUND, 2}) == -1); /* bad index */

    s.cfg.nfound = 0; /* still searching: one card says so */
    s.cfg.searching = 1;
    rows_of(&s, rows);
    CHECK(rows[1].n == 1 && rows[1].cards[0].kind == CARD_SEARCHING);
    CHECK(strstr(shell_card_label(&s, rows[1].cards[0]), "Looking") != NULL);
    CHECK(shell_key(&s, FT_OK).act == ACT_NONE);

    s.cfg.profile = PROFILE_CHILD; /* search results are not vetted: parents only */
    s.cfg.nfound = 2;
    n = rows_of(&s, rows);
    CHECK(n == 3 && rows[1].title != STR_LIVE_FOOTBALL);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_PLAY_FOUND, 0}) == -1);
    CHECK(strstr(s.status, "Ask a parent") != NULL);
    test_clean_save(path);
}

/* YouTube inside Family TV: a search bar, rows of videos, the search
 * keyboard (remote, mouse or laptop keyboard), parents only. */
static void test_youtube_tab(void)
{
    const char *path = TMP "/youtube-settings.sav";
    static shell_row rows[SHELL_MAX_ROWS];
    shell_config c;
    static shell s;
    static session ss;
    shell_action a;
    int n;
    test_clean_save(path);
    base_config(&c);
    shell_init(&s, &c);
    session_init_with_clock(&ss, &s, path, fixed_ms, fixed_wall);
    shell_focus_top(&s, TAB_YOUTUBE);
    n = rows_of(&s, rows);
    CHECK(n == YT_FEEDS && rows[0].style == ROW_SEARCH && rows[1].title == STR_YT_FOOTBALL); /* no search yet */
    CHECK(rows[1].n == 1 && rows[1].cards[0].kind == CARD_YT_LOADING);
    s.cfg.nvideos[1] = 2;
    snprintf(s.cfg.videos[YT_FEED_MAX + 1].id, YT_ID_MAX, "abc");
    rows_of(&s, rows);
    CHECK(rows[1].n == 2 && rows[1].cards[1].kind == CARD_VIDEO && rows[1].cards[1].arg == YT_FEED_MAX + 1);

    shell_key(&s, FT_DOWN); /* the search bar */
    shell_key(&s, FT_OK);
    CHECK(s.screen == SCR_KEYBOARD && s.item == 0);
    shell_key(&s, FT_OK); /* a */
    shell_key(&s, FT_DOWN);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_OK); /* l */
    shell_type(&s, 'X');
    shell_type(&s, ' ');
    shell_type(&s, ' '); /* one space only */
    shell_type(&s, '\b');
    CHECK(strcmp(s.cfg.yt_query, "alx") == 0);
    a = shell_type(&s, '\n');
    CHECK(a.act == ACT_YT_SEARCH && s.screen == SCR_HOME && s.tab == TAB_YOUTUBE && s.row == 1);
    CHECK((s.cfg.yt_loading & 1u) && session_dispatch(&ss, &s, a) == SESSION_YT_SEARCH);
    rows_of(&s, rows);
    CHECK(rows[1].title == STR_YT_RESULTS);

    shell_focus_card(&s, 2, 1); /* a Football video */
    a = shell_key(&s, FT_OK);
    CHECK(a.act == ACT_PLAY_VIDEO && a.arg == YT_FEED_MAX + 1);
    CHECK(session_dispatch(&ss, &s, a) == SESSION_WATCH && ss.watch_list == WATCH_VIDEO);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_PLAY_VIDEO, YT_FEED_MAX + 5}) == -1); /* not loaded */

    s.screen = SCR_HOME; /* typing on the tab starts a new search */
    s.row = 0;
    shell_type(&s, 'g');
    CHECK(s.screen == SCR_KEYBOARD && strcmp(s.cfg.yt_query, "g") == 0);
    shell_key(&s, FT_BACK);
    CHECK(s.screen == SCR_HOME && s.row == 0);

    s.cfg.profile = PROFILE_CHILD;
    rows_of(&s, rows);
    CHECK(rows[0].cards[0].kind == CARD_ASK_PARENT);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_PLAY_VIDEO, YT_FEED_MAX}) == -1);
    shell_type(&s, 'g'); /* no keyboard for the child */
    CHECK(s.screen == SCR_HOME);
    s.cfg.profile = PROFILE_PARENT;
    shell_focus_top(&s, TAB_APPS); /* the YouTube tile leads to the tab, not the browser */
    rows_of(&s, rows);
    shell_focus_card(&s, 1, 2);
    CHECK(shell_key(&s, FT_OK).act == ACT_NONE && s.tab == TAB_YOUTUBE);
    test_clean_save(path);
}

/* The mouse moves the focus straight to what it points at. */
static void test_shell_mouse_focus(void)
{
    shell_config c;
    static shell s;
    base_config(&c);
    shell_init(&s, &c);
    shell_focus_top(&s, TAB_APPS);
    CHECK(s.row == -1 && s.top == TAB_APPS && s.tab == TAB_APPS);
    shell_focus_card(&s, 0, 1);
    CHECK(s.row == 0 && s.col[0] == 1);
    shell_focus_card(&s, 0, 99); /* not a card: nothing moves */
    CHECK(s.col[0] == 1);
    CHECK(shell_key(&s, FT_WHEEL_DOWN).act == ACT_NONE && s.row == 1); /* the wheel is Up and Down */
    CHECK(shell_key(&s, FT_POINT).act == ACT_NONE && shell_key(&s, FT_CLICK).act == ACT_NONE && s.row == 1);
    shell_open_pin(&s, PIN_FOR_SETTINGS);
    shell_focus_item(&s, 2);
    CHECK(s.pin_pos == 2);
    shell_key(&s, FT_WHEEL_UP);
    CHECK(s.pin_digits[2] == '1');
}

/* Netflix and YouTube open in the browser, for parents only. */
static void test_apps_parent_only(void)
{
    const char *path = TMP "/apps-settings.sav";
    shell_config c;
    static shell s;
    static session ss;
    test_clean_save(path);
    base_config(&c);
    shell_init(&s, &c);
    session_init_with_clock(&ss, &s, path, fixed_ms, fixed_wall);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_OPEN_APP, APP_NETFLIX}) == SESSION_OPEN_APP);
    CHECK(ss.open_app == APP_NETFLIX && strncmp(shell_app_url(APP_NETFLIX), "https://www.netflix.com", 23) == 0);
    s.cfg.profile = PROFILE_CHILD;
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_OPEN_APP, APP_NETFLIX}) == -1);
    CHECK(strstr(s.status, "Ask a parent") != NULL);
    test_clean_save(path);
}

static void test_shell_child_profile_limits(void)
{
    static shell_row rows[SHELL_MAX_ROWS];
    shell_config c;
    static shell s;
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int n, i, has_settings = 0;
    base_config(&c);
    c.profile = PROFILE_CHILD;
    c.child_games = 1u << GAME_RACER;
    shell_init(&s, &c);
    n = rows_of(&s, rows);
    CHECK(n == 4 && rows[3].n == 1 && rows[3].cards[0].arg == GAME_RACER); /* no app tiles */

    s.tab = TAB_APPS;
    n = rows_of(&s, rows);
    CHECK(n == 3 && rows[1].n == 1 && rows[1].cards[0].kind == CARD_SPORTS); /* Sports, no apps */
    CHECK(rows[2].n == 2); /* no Parent settings card */
    for (i = 0; i < rows[2].n; i++)
        if (rows[2].cards[i].kind == CARD_SETTINGS)
            has_settings = 1;
    CHECK(!has_settings);
    CHECK(strcmp(shell_card_label(&s, rows[2].cards[0]), "Switch to parent profile") == 0);

    s.screen = SCR_MORE;
    n = shell_list(&s, labels, SHELL_MAX_ITEMS);
    has_settings = 0;
    for (i = 0; i < n; i++)
        if (strstr(labels[i], "settings"))
            has_settings = 1;
    CHECK(n == 2 && !has_settings);

    c.child_games = 0; /* every game blocked: no empty Games row */
    shell_init(&s, &c);
    CHECK(rows_of(&s, rows) == 3);
}

/* Every card on every tab, and each top bar item, can be reached with the arrows. */
static void test_shell_every_card_reachable(void)
{
    static shell_row rows[SHELL_MAX_ROWS];
    shell_config c;
    static shell s;
    int tab, total = 0, seen = 0, n, r, i;
    base_config(&c);
    shell_init(&s, &c);
    shell_key(&s, FT_UP); /* banner -> top bar */
    CHECK(s.row == -1 && s.top == TAB_FOR_YOU);
    for (tab = 0; tab < TAB_COUNT; tab++) {
        while (s.top < tab)
            shell_key(&s, FT_RIGHT);
        CHECK(s.tab == (shell_tab)tab);
        n = rows_of(&s, rows);
        shell_key(&s, FT_DOWN);
        for (r = 0; r < n; r++) {
            total += rows[r].n;
            CHECK(s.row == r);
            for (i = 0; i < rows[r].n; i++) {
                if (s.col[r] == i)
                    seen++;
                shell_key(&s, FT_RIGHT);
            }
            shell_key(&s, FT_DOWN);
        }
        while (s.row >= 0)
            shell_key(&s, FT_UP);
    }
    CHECK(seen == total && total > 0);
    shell_key(&s, FT_RIGHT);
    CHECK(s.top == TOP_GEAR && s.tab == TAB_APPS); /* the tab stays */
    shell_key(&s, FT_RIGHT);
    CHECK(s.top == TOP_PROFILE);
    CHECK(shell_key(&s, FT_OK).act == ACT_SWITCH_PROFILE);
    shell_key(&s, FT_LEFT);
    shell_key(&s, FT_OK);
    CHECK(s.screen == SCR_MORE);
    shell_key(&s, FT_BACK);
    CHECK(s.screen == SCR_HOME && s.row == -1 && s.top == TOP_GEAR);
}

static void test_shell_back_tabs_and_columns(void)
{
    shell_config c;
    static shell s;
    base_config(&c);
    shell_init(&s, &c);
    shell_key(&s, FT_DOWN);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_RIGHT);
    shell_key(&s, FT_DOWN);
    shell_key(&s, FT_UP);
    CHECK(s.row == 1 && s.col[1] == 3); /* the row kept its place */
    shell_key(&s, FT_BACK);
    CHECK(s.row == -1 && s.top == TAB_FOR_YOU);
    shell_key(&s, FT_RIGHT);
    CHECK(s.tab == TAB_LIVE && s.col[1] == 0); /* a new tab starts at the left */
    shell_key(&s, FT_BACK);
    CHECK(s.tab == TAB_FOR_YOU && s.top == TAB_FOR_YOU);
    shell_key(&s, FT_OK); /* OK on a tab goes into it */
    CHECK(s.row == 0);

    /* Parent settings closes back to wherever it was opened from. */
    shell_open_settings(&s);
    CHECK(shell_key(&s, FT_BACK).act == ACT_SETTINGS_CLOSED && s.screen == SCR_HOME);
    s.screen = SCR_MORE;
    shell_open_pin(&s, PIN_FOR_SETTINGS);
    shell_open_settings(&s);
    shell_open_pin(&s, PIN_NEW_FIRST); /* change PIN from inside */
    shell_open_settings(&s);
    CHECK(shell_key(&s, FT_BACK).act == ACT_SETTINGS_CLOSED && s.screen == SCR_MORE);
}

static void test_shell_kiswahili_labels(void)
{
    shell_config c;
    shell s;
    base_config(&c);
    c.lang = LANG_SW;
    shell_init(&s, &c);
    CHECK(strcmp(shell_tab_label(LANG_SW, TAB_APPS), "Programu") == 0);
    CHECK(strcmp(shell_screen_title(&s), "TV ya Familia") == 0);
}

/* ---- Live TV -------------------------------------------------------- */

static void test_live_zapping_tunes_once(void)
{
    live l;
    live_action a;
    int i, tunes = 0;
    live_init(&l, 10, 0, 50);
    CHECK(l.current == 0 && l.banner_ms == LIVE_BANNER_MS);
    for (i = 0; i < 3; i++) { /* Up, Up, Up quickly */
        CHECK(live_key(&l, FT_UP, 0).act == LIVE_ACT_NONE);
        a = live_tick(&l, 200);
        tunes += a.act == LIVE_ACT_TUNE;
    }
    CHECK(tunes == 0 && l.shown == 3 && l.current == 0);
    a = live_tick(&l, LIVE_TUNE_DELAY_MS);
    CHECK(a.act == LIVE_ACT_TUNE && a.arg == 3 && l.current == 3);
    CHECK(live_tick(&l, 1000).act == LIVE_ACT_NONE); /* only once */
    live_key(&l, FT_DOWN, 0);
    live_key(&l, FT_DOWN, 0);
    live_key(&l, FT_DOWN, 0);
    live_key(&l, FT_DOWN, 0);
    CHECK(l.shown == 9); /* wraps below the first channel */
    live_key(&l, FT_UP, 0);
    CHECK(l.shown == 0);
    live_tick(&l, LIVE_BANNER_MS);
    CHECK(l.banner_ms == 0 && l.current == 0);
    live_started(&l); /* the picture arrived late: banner again */
    CHECK(l.banner_ms == LIVE_BANNER_MS);
}

static void test_live_volume_panel_and_errors(void)
{
    live l;
    live_action a;
    int i;
    live_init(&l, 4, 1, 98);
    a = live_key(&l, FT_RIGHT, 0);
    CHECK(a.act == LIVE_ACT_VOLUME && a.arg == 100 && l.volume_ms > 0);
    for (i = 0; i < 30; i++)
        a = live_key(&l, FT_LEFT, 0);
    CHECK(a.arg == 0 && l.volume == 0);

    CHECK(live_key(&l, FT_OK, 0).act == LIVE_ACT_NONE && l.panel_open && l.panel_item == 1);
    live_key(&l, FT_DOWN, 0);
    live_key(&l, FT_DOWN, 0);
    live_key(&l, FT_DOWN, 0);
    CHECK(l.panel_item == 3); /* the list stops at the end */
    a = live_key(&l, FT_OK, 0);
    CHECK(a.act == LIVE_ACT_TUNE && a.arg == 3 && !l.panel_open && l.current == 3);
    live_key(&l, FT_OK, 0);
    CHECK(live_key(&l, FT_BACK, 0).act == LIVE_ACT_NONE && !l.panel_open); /* Back closes the list first */
    live_key(&l, FT_OK, 0);
    CHECK(live_key(&l, FT_OK, 0).act == LIVE_ACT_NONE); /* same channel: nothing restarts */

    a = live_key(&l, FT_OK, 1); /* on the error card OK tries again */
    CHECK(a.act == LIVE_ACT_TUNE && a.arg == 3 && !l.panel_open);
    CHECK(live_key(&l, FT_BACK, 1).act == LIVE_ACT_EXIT);

    live_init(&l, 0, 0, 50); /* no channels: Back still leaves */
    CHECK(live_key(&l, FT_UP, 0).act == LIVE_ACT_NONE && live_key(&l, FT_BACK, 0).act == LIVE_ACT_EXIT);
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
    test_home_rows_per_tab();
    test_shell_dad_reaches_channel_in_two_presses();
    test_shell_child_profile_limits();
    test_sports_tab();
    test_shell_mouse_focus();
    test_youtube_tab();
    test_apps_parent_only();
    test_shell_every_card_reachable();
    test_shell_back_tabs_and_columns();
    test_shell_kiswahili_labels();
    test_live_zapping_tunes_once();
    test_live_volume_panel_and_errors();
    run_service_tests();
    run_channel_tests();
    run_player_tests();
    run_view_tests();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
