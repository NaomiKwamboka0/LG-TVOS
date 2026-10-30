/* Host unit tests for parent PIN, play-time policy, translations, settings
 * and the session flows (plan p.16, p.20; tests S01, U07-U09). Host results
 * are not TV evidence. */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "i18n.h"
#include "pin.h"
#include "playtime.h"
#include "prefs.h"
#include "save.h"
#include "session.h"
#include "settings.h"
#include "shell.h"

#define T_2026_10_23_1600Z 1792771200 /* 19:00 in Nairobi */
#define T_2026_10_23_2100Z 1792789200 /* 00:00 on 24 Oct in Nairobi */

static uint64_t clock_ms = 1000;
static int64_t clock_wall = T_2026_10_23_1600Z;
static uint64_t test_ms(void) { return clock_ms; }
static int64_t test_wall(void) { return clock_wall; }

/* ---- PIN ------------------------------------------------------------ */

static void test_pin_verifier(void)
{
    static const uint8_t salt_a[PIN_SALT_SIZE] = {1, 2, 3};
    static const uint8_t salt_b[PIN_SALT_SIZE] = {9, 9, 9};
    pin_verifier a, b;
    CHECK(pin_make(&a, "4821", salt_a, 64, 1) == 0);
    CHECK(pin_check(&a, "4821") == 1);
    CHECK(pin_check(&a, "4820") == 0);
    CHECK(pin_check(&a, "482") == -1);
    CHECK(pin_check(&a, "48a1") == -1);
    CHECK(pin_make(&b, "4821", salt_b, 64, 1) == 0);
    CHECK(memcmp(a.hash, b.hash, PIN_HASH_SIZE) != 0); /* salted */
    CHECK(pin_make(&b, "12345", salt_b, 64, 1) != 0);
    CHECK(pin_make(&b, "1234", salt_b, 4, 1) != 0); /* cost below Argon2 minimum */
}

static void test_pin_guard_backoff_and_restart(void)
{
    pin_guard g = {0, 0};
    CHECK(pin_guard_wait_s(&g, 0) == 0);
    pin_guard_fail(&g, 0);
    pin_guard_fail(&g, 0);
    CHECK(pin_guard_wait_s(&g, 0) == 0); /* two free mistakes */
    pin_guard_fail(&g, 0);
    CHECK(pin_guard_wait_s(&g, 0) == 30);
    CHECK(pin_guard_wait_s(&g, 29500) == 1);
    CHECK(pin_guard_wait_s(&g, 30000) == 0);
    pin_guard_fail(&g, 30000);
    CHECK(pin_guard_wait_s(&g, 30000) == 60);
    pin_guard_fail(&g, 0);
    pin_guard_fail(&g, 0);
    pin_guard_fail(&g, 0);
    CHECK(pin_guard_wait_s(&g, 0) == 300); /* capped */

    /* Restart: persisted failures re-apply the lockout from boot time. */
    g.locked_until_ms = 0;
    pin_guard_boot(&g, 5000);
    CHECK(pin_guard_wait_s(&g, 5000) == 300);
    pin_guard_success(&g);
    CHECK(g.failures == 0 && pin_guard_wait_s(&g, 5000) == 0);
}

/* ---- Play time ------------------------------------------------------ */

static void test_play_nairobi_day(void)
{
    CHECK(play_nairobi_day(T_2026_10_23_1600Z) == 20261023);
    CHECK(play_nairobi_day(T_2026_10_23_2100Z - 1) == 20261023);
    CHECK(play_nairobi_day(T_2026_10_23_2100Z) == 20261024);
    CHECK(play_nairobi_day(1767225600 - 3 * 3600) == 20260101); /* 2025-12-31T21:00Z */
    CHECK(play_nairobi_day(951782400) == 20000229);             /* leap day */
}

static void test_play_limits(void)
{
    play_policy p;
    memset(&p, 0, sizeof p);
    CHECK(play_remaining(&p) == PLAY_UNLIMITED && play_allowed(&p));
    p.daily_limit_s = 1800;
    play_observe_clock(&p, T_2026_10_23_1600Z);
    play_add(&p, 1799);
    CHECK(play_remaining(&p) == 1 && play_allowed(&p));
    play_add(&p, 1);
    CHECK(!play_allowed(&p));
    play_grant(&p, 900);
    CHECK(play_remaining(&p) == 900);
    play_add(&p, UINT32_MAX); /* saturates, does not wrap */
    CHECK(!play_allowed(&p));
    play_reset_today(&p);
    CHECK(play_remaining(&p) == 1800);
}

static void test_play_clock_rules(void)
{
    play_policy p;
    memset(&p, 0, sizeof p);
    p.daily_limit_s = 600;
    play_observe_clock(&p, T_2026_10_23_1600Z);
    play_add(&p, 600);
    CHECK(!play_allowed(&p));

    play_observe_clock(&p, T_2026_10_23_1600Z - 86400); /* clock moved back a day */
    CHECK(!play_allowed(&p) && p.day == 20261023);
    play_observe_clock(&p, 1400000000); /* 2014 default clock after power loss */
    CHECK(!play_allowed(&p));
    play_observe_clock(&p, T_2026_10_23_2100Z - 60); /* same Nairobi day */
    CHECK(!play_allowed(&p));
    play_observe_clock(&p, T_2026_10_23_2100Z + 60); /* after Nairobi midnight */
    CHECK(play_allowed(&p) && p.day == 20261024 && p.used_s == 0);
}

static void test_play_pack(void)
{
    play_policy p, q;
    uint8_t buf[PLAY_PACKED_SIZE];
    memset(&p, 0, sizeof p);
    p.daily_limit_s = 3600;
    p.used_s = 120;
    p.bonus_s = 900;
    play_observe_clock(&p, T_2026_10_23_1600Z);
    CHECK(play_pack(&p, buf, sizeof buf) == PLAY_PACKED_SIZE);
    CHECK(play_unpack(&q, buf, sizeof buf) == 0);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    buf[12] = 0x45; /* invalid day */
    CHECK(play_unpack(&q, buf, sizeof buf) != 0);
}

static void test_play_meter_clamps_sleep(void)
{
    play_meter m;
    uint32_t total = 0;
    uint64_t t = 5000;
    int i;
    play_meter_start(&m, t);
    for (i = 0; i < 90; i++) /* 3 s of 30 Hz frames */
        total += play_meter_step(&m, t += 33 + (i % 3 == 0), 1);
    CHECK(total == 3);
    total += play_meter_step(&m, t += 3u * 3600u * 1000u, 1); /* lid closed for 3 hours */
    CHECK(total <= 4);
    CHECK(play_meter_step(&m, t += 5000, 0) == 0); /* paused: nothing counted */
    CHECK(play_meter_step(&m, t - 100, 1) == 0); /* clock never goes back, but stay safe */
}

/* ---- Translations and settings ------------------------------------- */

static void test_i18n_complete(void)
{
    int id, lang, ok = 1;
    for (id = 0; id < STR_COUNT; id++)
        for (lang = 0; lang < LANG_COUNT; lang++) {
            const char *t = tr((ft_lang)lang, (ft_str)id);
            if (!t || !t[0] || !strcmp(t, "?"))
                ok = 0;
        }
    CHECK(ok);
    CHECK(strcmp(tr(LANG_SW, STR_GAMES), "Michezo") == 0);
    CHECK(strcmp(tr((ft_lang)7, STR_GAMES), "Games") == 0);
}

static void test_settings_roundtrip_and_validation(void)
{
    const char *path = TMP "/settings-rt.sav";
    static const uint8_t salt[PIN_SALT_SIZE] = {7};
    family_settings f, g;
    uint8_t buf[SETTINGS_PACKED_SIZE];
    test_clean_save(path);

    CHECK(settings_load(&f, path) == FT_SAVE_NONE);
    CHECK(f.profile == 0 && f.pin_set == 0 && f.child_games == SETTINGS_ALL_GAMES);
    f.lang = LANG_SW;
    f.profile = 1;
    f.pin_set = 1;
    CHECK(pin_make(&f.pin, "2468", salt, 64, 1) == 0);
    f.play.daily_limit_s = 3600;
    CHECK(settings_save(&f, path) == FT_SAVE_OK);
    CHECK(settings_load(&g, path) == FT_SAVE_OK);
    CHECK(g.lang == LANG_SW && g.profile == 1 && pin_check(&g.pin, "2468") == 1);
    CHECK(g.play.daily_limit_s == 3600);

    CHECK(settings_pack(&f, buf, sizeof buf) == SETTINGS_PACKED_SIZE);
    buf[3] = 0; /* child profile without a PIN is inconsistent */
    CHECK(settings_unpack(&g, buf, sizeof buf) != 0);
    buf[3] = 1;
    buf[0] = 9; /* unknown language */
    CHECK(settings_unpack(&g, buf, sizeof buf) != 0);
    buf[0] = LANG_SW;
    buf[88] = QUALITY_COUNT; /* unknown quality */
    CHECK(settings_unpack(&g, buf, sizeof buf) != 0);
    buf[88] = QUALITY_720;
    CHECK(settings_unpack(&g, buf, sizeof buf) == 0 && g.quality == QUALITY_720);
    CHECK(settings_unpack(&g, buf, SETTINGS_PACKED_SIZE - 1) != 0);
    test_clean_save(path);
}

/* A settings file written before live TV (schema 1, 88 bytes) must load
 * with its PIN and child profile intact; dropping to defaults would hand a
 * child the parent profile. */
static void test_settings_v1_migrates(void)
{
    const char *path = TMP "/settings-v1.sav";
    static const uint8_t salt[PIN_SALT_SIZE] = {3};
    family_settings f, g;
    uint8_t buf[SETTINGS_PACKED_SIZE];
    uint32_t len = 0;
    uint16_t schema = 0;
    test_clean_save(path);

    settings_defaults(&f);
    f.profile = 1;
    f.pin_set = 1;
    CHECK(pin_make(&f.pin, "1357", salt, 64, 1) == 0);
    f.play.daily_limit_s = 1800;
    CHECK(settings_pack(&f, buf, sizeof buf) == SETTINGS_PACKED_SIZE);
    CHECK(ft_save_write(path, "STSV", 1, buf, SETTINGS_V1_SIZE) == FT_SAVE_OK);

    CHECK(settings_load(&g, path) == FT_SAVE_OK);
    CHECK(g.profile == 1 && g.pin_set == 1 && pin_check(&g.pin, "1357") == 1);
    CHECK(g.play.daily_limit_s == 1800 && g.quality == QUALITY_1080);

    /* Saving upgrades the file; the v1 copy becomes the last-known-good .prev. */
    g.quality = QUALITY_360;
    CHECK(settings_save(&g, path) == FT_SAVE_OK);
    CHECK(ft_save_read_range(path, "STSV", 1, 2, buf, sizeof buf, &len, &schema) == FT_SAVE_OK);
    CHECK(schema == 2 && len == SETTINGS_PACKED_SIZE);
    CHECK(settings_load(&f, path) == FT_SAVE_OK && f.quality == QUALITY_360);
    test_corrupt_byte(path, 30);
    CHECK(settings_load(&f, path) == FT_SAVE_RESTORED_PREV);
    CHECK(f.profile == 1 && pin_check(&f.pin, "1357") == 1 && f.quality == QUALITY_1080);

    /* A v1-sized payload labelled schema 2 is damage, not a migration. */
    test_clean_save(path);
    CHECK(ft_save_write(path, "STSV", 2, buf, SETTINGS_V1_SIZE) == FT_SAVE_OK);
    CHECK(settings_load(&f, path) == FT_SAVE_CORRUPT && f.profile == 0);
    test_clean_save(path);
}

static void test_prefs_roundtrip_and_validation(void)
{
    const char *path = TMP "/player.sav";
    player_prefs p, q;
    uint8_t bad[PREFS_PACKED_SIZE] = {5, 0, 101, 0};
    test_clean_save(path);
    CHECK(prefs_load(&p, path) == FT_SAVE_NONE);
    CHECK(p.last_channel == 0 && p.volume == 80 && p.fullscreen == 0);
    p.last_channel = 312;
    p.volume = 35;
    p.fullscreen = 1;
    CHECK(prefs_save(&p, path) == FT_SAVE_OK);
    CHECK(prefs_load(&q, path) == FT_SAVE_OK);
    CHECK(q.last_channel == 312 && q.volume == 35 && q.fullscreen == 1);
    p.volume = 140; /* clamped on save */
    CHECK(prefs_save(&p, path) == FT_SAVE_OK && prefs_load(&q, path) == FT_SAVE_OK && q.volume == 100);
    test_clean_save(path);
    CHECK(ft_save_write(path, "PLSV", 1, bad, sizeof bad) == FT_SAVE_OK); /* volume 101 */
    CHECK(prefs_load(&q, path) == FT_SAVE_CORRUPT && q.volume == 80 && q.last_channel == 0);
    test_clean_save(path);
}

/* ---- Session flows -------------------------------------------------- */

static void press(session *ss, shell *s, ft_key k)
{
    session_dispatch(ss, s, shell_key(s, k));
}

/* Enter a PIN on the remote: Up presses per digit, OK after each digit. */
static void enter_pin(session *ss, shell *s, const char *pin)
{
    int i, j;
    for (i = 0; i < PIN_DIGITS; i++) {
        for (j = 0; j < pin[i] - '0'; j++)
            press(ss, s, FT_UP);
        press(ss, s, FT_OK);
    }
}

/* Home, up to the top bar, across to the gear, OK: the Settings list. */
static void open_more(session *ss, shell *s)
{
    while (s->screen != SCR_HOME)
        press(ss, s, FT_BACK);
    if (s->row >= 0)
        press(ss, s, FT_BACK);
    while (s->top < TOP_GEAR)
        press(ss, s, FT_RIGHT);
    while (s->top > TOP_GEAR)
        press(ss, s, FT_LEFT);
    press(ss, s, FT_OK);
}

static void choose_item(session *ss, shell *s, int index)
{
    int i;
    for (i = 0; i < 12; i++)
        press(ss, s, FT_UP);
    for (i = 0; i < index; i++)
        press(ss, s, FT_DOWN);
    press(ss, s, FT_OK);
}

static void fresh(session *ss, shell *s, const char *path)
{
    shell_config c;
    memset(&c, 0, sizeof c);
    shell_init(s, &c);
    session_init_with_clock(ss, s, path, test_ms, test_wall);
}

static void test_session_pin_creation_and_child_switch(void)
{
    const char *path = TMP "/session-a.sav";
    static shell s;
    static session ss;
    test_clean_save(path);
    clock_ms = 1000;
    fresh(&ss, &s, path);

    open_more(&ss, &s);
    choose_item(&ss, &s, 0); /* Switch to child profile */
    CHECK(s.screen == SCR_PIN && s.pin_for == PIN_NEW_FIRST);
    enter_pin(&ss, &s, "1234");
    CHECK(s.screen == SCR_PIN && s.pin_for == PIN_NEW_CONFIRM);
    enter_pin(&ss, &s, "1299"); /* mismatch */
    CHECK(s.screen == SCR_PIN && s.pin_for == PIN_NEW_FIRST);
    CHECK(!ss.settings.pin_set);
    enter_pin(&ss, &s, "1234");
    enter_pin(&ss, &s, "1234");
    CHECK(ss.settings.pin_set);
    CHECK(s.cfg.profile == PROFILE_CHILD && s.screen == SCR_HOME);

    /* Restart (or crash) does not elevate the child profile (U09). */
    fresh(&ss, &s, path);
    CHECK(s.cfg.profile == PROFILE_CHILD);

    /* Child cannot reach settings actions even if one is injected. */
    session_dispatch(&ss, &s, (shell_action){ACT_CYCLE_LIMIT, 0});
    session_dispatch(&ss, &s, (shell_action){ACT_OPEN_PARENT_SETTINGS, 0});
    CHECK(ss.settings.play.daily_limit_s == 0 && s.screen != SCR_SETTINGS);
    session_dispatch(&ss, &s, (shell_action){ACT_CYCLE_QUALITY, 0});
    CHECK(ss.settings.quality == QUALITY_1080);

    /* Back to parent with the right PIN. */
    open_more(&ss, &s);
    choose_item(&ss, &s, 0);
    CHECK(s.screen == SCR_PIN && s.pin_for == PIN_FOR_PARENT);
    enter_pin(&ss, &s, "1234");
    CHECK(s.cfg.profile == PROFILE_PARENT);
    test_clean_save(path);
}

static void test_session_wrong_pin_lockout_persists(void)
{
    const char *path = TMP "/session-b.sav";
    static shell s;
    static session ss;
    int i;
    test_clean_save(path);
    clock_ms = 1000;
    fresh(&ss, &s, path);
    open_more(&ss, &s);
    choose_item(&ss, &s, 0);
    enter_pin(&ss, &s, "0007");
    enter_pin(&ss, &s, "0007");
    CHECK(s.cfg.profile == PROFILE_CHILD);

    open_more(&ss, &s);
    choose_item(&ss, &s, 0);
    for (i = 0; i < 3; i++) {
        CHECK(s.screen == SCR_PIN);
        enter_pin(&ss, &s, "1111");
    }
    CHECK(s.cfg.profile == PROFILE_CHILD);
    CHECK(s.screen == SCR_MORE && strstr(s.status, "Seconds to wait: 30") != NULL);
    CHECK(strstr(s.status, "0007") == NULL); /* PIN never shown */

    /* Locked: the PIN screen will not open, even for the right PIN. */
    choose_item(&ss, &s, 0);
    CHECK(s.screen != SCR_PIN);

    /* Restart does not clear the lockout. */
    fresh(&ss, &s, path);
    CHECK(ss.guard.failures == 3 && pin_guard_wait_s(&ss.guard, clock_ms) == 30);

    clock_ms += 30000;
    open_more(&ss, &s);
    choose_item(&ss, &s, 0);
    CHECK(s.screen == SCR_PIN);
    enter_pin(&ss, &s, "0007");
    CHECK(s.cfg.profile == PROFILE_PARENT && ss.settings.pin_failures == 0);
    test_clean_save(path);
}

static void test_session_play_time_and_settings(void)
{
    const char *path = TMP "/session-c.sav";
    static shell s;
    static session ss;
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int i;
    test_clean_save(path);
    clock_ms = 1000;
    clock_wall = T_2026_10_23_1600Z;
    fresh(&ss, &s, path);

    /* Parent settings: create PIN, set 30 minutes, switch language, block Block Garden. */
    open_more(&ss, &s);
    choose_item(&ss, &s, 1); /* Parent settings */
    enter_pin(&ss, &s, "5555");
    enter_pin(&ss, &s, "5555");
    CHECK(s.screen == SCR_SETTINGS && ss.unlocked);
    choose_item(&ss, &s, 1); /* 0 -> 30 min */
    CHECK(ss.settings.play.daily_limit_s == 1800);
    choose_item(&ss, &s, 6); /* Block Garden toggle */
    CHECK(!(ss.settings.child_games & (1u << GAME_GARDEN)));
    choose_item(&ss, &s, 0);
    CHECK(ss.settings.lang == LANG_SW && s.cfg.lang == LANG_SW);
    shell_list(&s, labels, SHELL_MAX_ITEMS);
    CHECK(strstr(labels[1], "dk") != NULL);
    choose_item(&ss, &s, 0);
    CHECK(ss.settings.lang == LANG_EN);
    CHECK(ss.settings.quality == QUALITY_1080);
    choose_item(&ss, &s, 8); /* live TV picture: 1080p -> 360p -> 480p -> 720p */
    CHECK(ss.settings.quality == QUALITY_360 && s.cfg.quality == QUALITY_360);
    choose_item(&ss, &s, 8);
    CHECK(ss.settings.quality == QUALITY_480);
    choose_item(&ss, &s, 8);
    shell_list(&s, labels, SHELL_MAX_ITEMS);
    CHECK(ss.settings.quality == QUALITY_720 && strstr(labels[8], "720p") != NULL);
    {
        family_settings saved;
        CHECK(settings_load(&saved, path) == FT_SAVE_OK && saved.quality == QUALITY_720);
    }
    press(&ss, &s, FT_BACK);
    CHECK(s.screen == SCR_MORE && !ss.unlocked);
    session_dispatch(&ss, &s, (shell_action){ACT_GRANT_15, 0}); /* locked again */
    CHECK(ss.settings.play.bonus_s == 0);
    session_dispatch(&ss, &s, (shell_action){ACT_CYCLE_QUALITY, 0});
    CHECK(ss.settings.quality == QUALITY_720);

    /* Child plays until the allowance runs out. */
    choose_item(&ss, &s, 0);
    CHECK(s.cfg.profile == PROFILE_CHILD);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == GAME_RACER);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_GARDEN}) == -1);
    for (i = 0; i < 29; i++)
        CHECK(session_play_account(&ss, &s, 60) == 1);
    CHECK(session_play_account(&ss, &s, 60) == 0);
    session_play_end(&ss, &s);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == -1);
    CHECK(strstr(s.status, "Play time is up") != NULL);

    /* Restart and clock tricks do not give extra time. */
    fresh(&ss, &s, path);
    clock_wall = T_2026_10_23_1600Z - 86400;
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == -1);
    clock_wall = 1400000000;
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == -1);

    /* Parent grants 15 minutes after PIN. */
    clock_wall = T_2026_10_23_1600Z + 600;
    open_more(&ss, &s);
    choose_item(&ss, &s, 0);
    enter_pin(&ss, &s, "5555");
    CHECK(s.cfg.profile == PROFILE_PARENT);
    open_more(&ss, &s);
    choose_item(&ss, &s, 1);
    enter_pin(&ss, &s, "5555");
    choose_item(&ss, &s, 2);
    CHECK(ss.settings.play.bonus_s == 900);
    press(&ss, &s, FT_BACK);
    choose_item(&ss, &s, 0);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == GAME_RACER);

    /* Next Nairobi day: fresh allowance. */
    for (i = 0; i < 15; i++)
        session_play_account(&ss, &s, 60);
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == -1);
    clock_wall = T_2026_10_23_2100Z + 60;
    CHECK(session_dispatch(&ss, &s, (shell_action){ACT_LAUNCH_GAME, GAME_RACER}) == GAME_RACER);
    test_clean_save(path);
}

void run_service_tests(void)
{
    test_pin_verifier();
    test_pin_guard_backoff_and_restart();
    test_play_nairobi_day();
    test_play_limits();
    test_play_clock_rules();
    test_play_pack();
    test_play_meter_clamps_sleep();
    test_i18n_complete();
    test_settings_roundtrip_and_validation();
    test_settings_v1_migrates();
    test_prefs_roundtrip_and_validation();
    test_session_pin_creation_and_child_switch();
    test_session_wrong_pin_lockout_persists();
    test_session_play_time_and_settings();
}
