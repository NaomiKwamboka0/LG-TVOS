#include "session.h"

#include <stdio.h>
#include <string.h>

#include "monocypher.h"
#include "platform.h"
#include "save.h"

typedef char pin_lengths_match[(SHELL_PIN_DIGITS == PIN_DIGITS) ? 1 : -1];

enum { AFTER_CHILD, AFTER_SETTINGS };

static const uint32_t LIMIT_STEPS_MIN[] = {0, 30, 60, 90, 120, 180};
#define LIMIT_STEPS (sizeof LIMIT_STEPS_MIN / sizeof LIMIT_STEPS_MIN[0])

static ft_lang lang(const session *ss)
{
    return (ft_lang)ss->settings.lang;
}

static void say(session *ss, shell *s, ft_str id)
{
    snprintf(s->status, sizeof s->status, "%s", tr(lang(ss), id));
}

static void say_with(session *ss, shell *s, ft_str id, const char *detail)
{
    snprintf(s->status, sizeof s->status, "%s %s", tr(lang(ss), id), detail);
}

static void sync(session *ss, shell *s)
{
    family_settings *f = &ss->settings;
    play_observe_clock(&f->play, ss->wall_utc());
    s->cfg.lang = (ft_lang)f->lang;
    s->cfg.child_games = f->child_games;
    s->cfg.child_limit_s = f->play.daily_limit_s;
    s->cfg.play_remaining_s = play_remaining(&f->play);
    s->cfg.quality = f->quality;
    if (s->cfg.profile != (profile_role)f->profile)
        shell_set_profile(s, (profile_role)f->profile);
}

static int persist(session *ss, shell *s)
{
    int r = settings_save(&ss->settings, ss->path);
    if (r != FT_SAVE_OK)
        say(ss, s, STR_SAVE_FAILED);
    return r;
}

int session_init_with_clock(session *ss, shell *s, const char *settings_path,
                            uint64_t (*now_ms)(void), int64_t (*wall_utc)(void))
{
    int r;
    memset(ss, 0, sizeof *ss);
    ss->now_ms = now_ms;
    ss->wall_utc = wall_utc;
    snprintf(ss->path, sizeof ss->path, "%s", settings_path);
    r = settings_load(&ss->settings, ss->path);
    ss->guard.failures = ss->settings.pin_failures;
    pin_guard_boot(&ss->guard, ss->now_ms());
    s->cfg.profile = PROFILE_PARENT;
    shell_set_profile(s, PROFILE_PARENT);
    sync(ss, s);
    return r;
}

int session_init(session *ss, shell *s, const char *settings_path)
{
    return session_init_with_clock(ss, s, settings_path, plat_monotonic_ms, plat_wall_utc);
}

static void wipe_pins(session *ss, shell *s)
{
    crypto_wipe(ss->pin_first, sizeof ss->pin_first);
    shell_clear_pin(s);
}

/* Opens the PIN screen unless attempts are currently locked out. */
static void ask_pin(session *ss, shell *s, pin_purpose why)
{
    uint32_t wait = pin_guard_wait_s(&ss->guard, ss->now_ms());
    char buf[16];
    if (wait) {
        snprintf(buf, sizeof buf, "%u", (unsigned)wait);
        say_with(ss, s, STR_PIN_LOCKED, buf);
        return;
    }
    shell_open_pin(s, why);
}

static void start_new_pin(session *ss, shell *s, int after)
{
    ss->after_new_pin = after;
    shell_open_pin(s, PIN_NEW_FIRST);
}

static void finish_new_pin(session *ss, shell *s)
{
    family_settings *f = &ss->settings;
    uint8_t salt[PIN_SALT_SIZE];
    pin_verifier v;

    if (strcmp(ss->pin_first, s->pin_digits) != 0) {
        wipe_pins(ss, s);
        shell_open_pin(s, PIN_NEW_FIRST);
        say(ss, s, STR_PIN_MISMATCH);
        return;
    }
    if (plat_random(salt, sizeof salt) != PLAT_OK ||
        pin_make(&v, s->pin_digits, salt, PIN_DEFAULT_BLOCKS, PIN_DEFAULT_PASSES) != 0) {
        wipe_pins(ss, s);
        s->screen = s->pin_return;
        say(ss, s, STR_UNAVAILABLE);
        return;
    }
    wipe_pins(ss, s);
    f->pin = v;
    f->pin_set = 1;
    f->pin_failures = 0;
    pin_guard_success(&ss->guard);
    crypto_wipe(&v, sizeof v);

    if (ss->after_new_pin == AFTER_CHILD) {
        f->profile = PROFILE_CHILD;
        if (persist(ss, s) == FT_SAVE_OK) {
            sync(ss, s);
            say(ss, s, STR_CHILD_ACTIVE);
        }
    } else {
        ss->unlocked = 1;
        shell_open_settings(s);
        if (persist(ss, s) == FT_SAVE_OK)
            say(ss, s, STR_PIN_SAVED);
    }
}

static void check_pin(session *ss, shell *s)
{
    family_settings *f = &ss->settings;
    uint64_t now = ss->now_ms();
    pin_purpose why = s->pin_for;
    uint32_t wait = pin_guard_wait_s(&ss->guard, now);
    char buf[16];
    int ok;

    if (wait) {
        shell_clear_pin(s);
        s->screen = s->pin_return;
        snprintf(buf, sizeof buf, "%u", (unsigned)wait);
        say_with(ss, s, STR_PIN_LOCKED, buf);
        return;
    }
    ok = f->pin_set ? pin_check(&f->pin, s->pin_digits) : -1;
    shell_clear_pin(s);

    if (ok == 1) {
        pin_guard_success(&ss->guard);
        f->pin_failures = 0;
        if (why == PIN_FOR_PARENT) {
            f->profile = PROFILE_PARENT;
            if (persist(ss, s) == FT_SAVE_OK) {
                sync(ss, s);
                say(ss, s, STR_PARENT_ACTIVE);
            }
        } else {
            ss->unlocked = 1;
            shell_open_settings(s);
            persist(ss, s);
        }
    } else if (ok == 0) {
        pin_guard_fail(&ss->guard, now);
        f->pin_failures = ss->guard.failures;
        persist(ss, s);
        wait = pin_guard_wait_s(&ss->guard, now);
        if (wait) {
            s->screen = s->pin_return;
            snprintf(buf, sizeof buf, "%u", (unsigned)wait);
            say_with(ss, s, STR_PIN_LOCKED, buf);
        } else {
            say(ss, s, STR_PIN_WRONG);
        }
    } else {
        s->screen = s->pin_return;
        say(ss, s, STR_UNAVAILABLE);
    }
}

static int settings_open(session *ss, shell *s)
{
    if (ss->unlocked && s->screen == SCR_SETTINGS)
        return 1;
    say(ss, s, STR_SETTINGS_LOCKED);
    return 0;
}

static void cycle_limit(play_policy *p)
{
    size_t i;
    uint32_t current = p->daily_limit_s / 60;
    for (i = 0; i < LIMIT_STEPS; i++)
        if (LIMIT_STEPS_MIN[i] == current)
            break;
    p->daily_limit_s = LIMIT_STEPS_MIN[(i + 1) % LIMIT_STEPS] * 60;
}

int session_dispatch(session *ss, shell *s, shell_action a)
{
    family_settings *f = &ss->settings;

    switch (a.act) {
    case ACT_PLAY_CHANNEL:
        if (a.arg < 0 || a.arg >= s->cfg.channels.n)
            break;
        ss->watch_channel = a.arg;
        return SESSION_WATCH;
    case ACT_LAUNCH_GAME:
        if (a.arg < 0 || a.arg >= GAME_COUNT)
            break;
        if (s->cfg.profile == PROFILE_CHILD) {
            if (!(f->child_games & (1u << a.arg))) {
                say(ss, s, STR_UNAVAILABLE);
                break;
            }
            sync(ss, s);
            if (!play_allowed(&f->play)) {
                say(ss, s, STR_TIME_UP);
                break;
            }
        }
        return a.arg;
    case ACT_SWITCH_PROFILE:
        if (s->cfg.profile == PROFILE_CHILD) {
            ask_pin(ss, s, PIN_FOR_PARENT);
        } else if (!f->pin_set) {
            start_new_pin(ss, s, AFTER_CHILD);
            say(ss, s, STR_NEED_PIN_FIRST);
        } else {
            f->profile = PROFILE_CHILD;
            if (persist(ss, s) == FT_SAVE_OK) {
                sync(ss, s);
                say(ss, s, STR_CHILD_ACTIVE);
            }
        }
        break;
    case ACT_ABOUT:
        say(ss, s, STR_ABOUT_TEXT);
        break;
    case ACT_OPEN_PARENT_SETTINGS:
        if (s->cfg.profile != PROFILE_PARENT)
            break;
        if (!f->pin_set) {
            start_new_pin(ss, s, AFTER_SETTINGS);
            say(ss, s, STR_NEED_PIN_FIRST);
        } else {
            ask_pin(ss, s, PIN_FOR_SETTINGS);
        }
        break;
    case ACT_PIN_SUBMIT:
        if (s->screen != SCR_PIN)
            break;
        if (s->pin_for == PIN_NEW_FIRST) {
            memcpy(ss->pin_first, s->pin_digits, sizeof ss->pin_first);
            shell_open_pin(s, PIN_NEW_CONFIRM);
        } else if (s->pin_for == PIN_NEW_CONFIRM) {
            finish_new_pin(ss, s);
        } else {
            check_pin(ss, s);
        }
        break;
    case ACT_PIN_CANCEL:
        crypto_wipe(ss->pin_first, sizeof ss->pin_first);
        break;
    case ACT_TOGGLE_LANGUAGE:
        if (settings_open(ss, s)) {
            f->lang = f->lang == LANG_EN ? LANG_SW : LANG_EN;
            persist(ss, s);
            sync(ss, s);
        }
        break;
    case ACT_CYCLE_LIMIT:
        if (settings_open(ss, s)) {
            cycle_limit(&f->play);
            persist(ss, s);
            sync(ss, s);
        }
        break;
    case ACT_GRANT_15:
        if (settings_open(ss, s)) {
            play_grant(&f->play, 15 * 60);
            if (persist(ss, s) == FT_SAVE_OK)
                say(ss, s, STR_GRANTED);
            sync(ss, s);
        }
        break;
    case ACT_RESET_TODAY:
        if (settings_open(ss, s)) {
            play_reset_today(&f->play);
            if (persist(ss, s) == FT_SAVE_OK)
                say(ss, s, STR_RESET_DONE);
            sync(ss, s);
        }
        break;
    case ACT_TOGGLE_GAME:
        if (settings_open(ss, s) && a.arg >= 0 && a.arg < GAME_COUNT) {
            f->child_games ^= (uint8_t)(1u << a.arg);
            persist(ss, s);
            sync(ss, s);
        }
        break;
    case ACT_CHANGE_PIN:
        if (settings_open(ss, s))
            start_new_pin(ss, s, AFTER_SETTINGS);
        break;
    case ACT_CYCLE_QUALITY:
        if (settings_open(ss, s)) {
            f->quality = (uint8_t)((f->quality + 1) % QUALITY_COUNT);
            persist(ss, s);
            sync(ss, s);
        }
        break;
    case ACT_SETTINGS_CLOSED:
        ss->unlocked = 0;
        break;
    case ACT_NONE:
        break;
    }
    return -1;
}

int session_play_account(session *ss, shell *s, uint32_t seconds)
{
    family_settings *f = &ss->settings;
    int allowed;
    if (s->cfg.profile != PROFILE_CHILD)
        return 1;
    play_observe_clock(&f->play, ss->wall_utc());
    play_add(&f->play, seconds);
    ss->unsaved_play_s += seconds;
    allowed = play_allowed(&f->play);
    /* Checkpoint instead of writing every second (plan p.17 flash wear). */
    if (ss->unsaved_play_s >= 60 || !allowed) {
        settings_save(f, ss->path);
        ss->unsaved_play_s = 0;
    }
    s->cfg.play_remaining_s = play_remaining(&f->play);
    return allowed;
}

void session_play_end(session *ss, shell *s)
{
    if (ss->unsaved_play_s > 0) {
        settings_save(&ss->settings, ss->path);
        ss->unsaved_play_s = 0;
    }
    sync(ss, s);
}
