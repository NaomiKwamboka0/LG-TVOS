/* Host simulator for the family TV experience. Runs on the development
 * computer only; it is not device evidence (plan p.25). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "garden.h"
#include "penalty.h"
#include "platform.h"
#include "playtime.h"
#include "racer.h"
#include "frontend.h"
#include "save.h"
#include "session.h"
#include "shell.h"

#define DEFAULT_DATA_DIR "out/simdata"
#define FRAME_MS (1000 / FT_TICK_HZ)

static const char *data_dir = DEFAULT_DATA_DIR;
static int quit_req;
static int script_mode;
static char *script_tokens[1024];
static int script_len, script_pos, script_wait;

/* ---- input ---------------------------------------------------------- */

static ft_key script_next(void)
{
    const char *t;
    if (script_wait > 0) {
        script_wait--;
        return FT_NONE;
    }
    if (script_pos >= script_len) {
        quit_req = 1;
        return FT_NONE;
    }
    t = script_tokens[script_pos++];
    if (!strcmp(t, "L")) return FT_LEFT;
    if (!strcmp(t, "R")) return FT_RIGHT;
    if (!strcmp(t, "U")) return FT_UP;
    if (!strcmp(t, "D")) return FT_DOWN;
    if (!strcmp(t, "OK")) return FT_OK;
    if (!strcmp(t, "BACK")) return FT_BACK;
    if (t[0] == 'W') { /* W or W<n>: wait n ticks (default one second) */
        script_wait = t[1] ? atoi(t + 1) - 1 : FT_TICK_HZ - 1;
        return FT_NONE;
    }
    fprintf(stderr, "unknown script key: %s\n", t);
    quit_req = 1;
    return FT_NONE;
}

#ifdef FT_WINDOW
static const sim_frontend *fe = &frontend_window;
#else
static const sim_frontend *fe = &frontend_text;
#endif
const char *sim_dump_path;

/* frame_ms > 0: game frame pacing; 0: block until a key (menus). */
static ft_key next_key(int frame_ms)
{
    if (script_mode)
        return script_next();
    return fe->poll(frame_ms, &quit_req);
}

/* ---- play-time meter ------------------------------------------------ */

/* Scripted runs use a simulated clock (one frame per loop) so they are repeatable. */
static uint64_t meter_now(void)
{
    static uint64_t fake;
    if (script_mode)
        return fake += FRAME_MS;
    return plat_monotonic_ms();
}

static void meter_start(play_meter *m)
{
    play_meter_start(m, meter_now());
}

/* Counts time only while the game is actively played (not paused).
 * Returns 0 once a child's allowance has run out. */
static int meter_step(play_meter *m, int active, session *ss, shell *s)
{
    uint32_t secs = play_meter_step(m, meter_now(), active);
    return secs ? session_play_account(ss, s, secs) : 1;
}

static void time_up(session *ss, shell *s)
{
    snprintf(s->status, sizeof s->status, "[%s] %s", plat_name(),
             tr((ft_lang)ss->settings.lang, STR_TIME_UP));
}

/* ---- saves ------------------------------------------------------------ */

static const char *data_path(char *out, size_t n, const char *file)
{
    snprintf(out, n, "%s/%s", data_dir, file);
    return out;
}

static void save_note(char *status, size_t n, int r, const char *what)
{
    if (r == FT_SAVE_RESTORED_PREV)
        snprintf(status, n, "%s: latest save was damaged; restored the previous one", what);
    else if (r == FT_SAVE_CORRUPT)
        snprintf(status, n, "%s: save was damaged; starting fresh (damaged file kept as .bad)", what);
    else if (r < 0 && r != FT_SAVE_NONE)
        snprintf(status, n, "%s: could not read save", what);
}

static void put32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o[2] = (uint8_t)(v >> 16); o[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *i)
{
    return (uint32_t)i[0] | ((uint32_t)i[1] << 8) | ((uint32_t)i[2] << 16) | ((uint32_t)i[3] << 24);
}

/* ---- games ------------------------------------------------------------ */

static uint32_t next_seed(void)
{
    return script_mode ? 1u : (uint32_t)time(NULL);
}

static void run_racer(session *ss, shell *s)
{
    char path_buf[512];
    const char *path = data_path(path_buf, sizeof path_buf, "racer.sav");
    uint8_t buf[4 * RACER_LEVELS];
    uint32_t best[RACER_LEVELS] = {0}, len = 0;
    char status[128] = "";
    play_meter meter;
    racer r;
    int i, recorded = 0, r_load;

    r_load = ft_save_read(path, "RCSV", 1, buf, sizeof buf, &len);
    if (r_load >= 0 && len == sizeof buf)
        for (i = 0; i < RACER_LEVELS; i++)
            best[i] = get32(buf + 4 * i);
    save_note(status, sizeof status, r_load, "High scores");

    racer_init(&r, next_seed(), RACER_EASY, 0);
    meter_start(&meter);
    while (r.state != RACER_EXITED && !quit_req) {
        ft_key k = next_key(FRAME_MS);
        if (k != FT_NONE)
            racer_key(&r, k);
        racer_tick(&r);
        if (!meter_step(&meter, r.state == RACER_RUNNING, ss, s)) {
            time_up(ss, s);
            r.state = RACER_EXITED;
            break;
        }
        if (r.state == RACER_RUNNING)
            recorded = 0;
        if ((r.state == RACER_FINISHED || r.state == RACER_CRASHED) && !recorded) {
            recorded = 1;
            if (!r.practice && r.score > best[r.level]) {
                best[r.level] = r.score;
                for (i = 0; i < RACER_LEVELS; i++)
                    put32(buf + 4 * i, best[i]);
                snprintf(status, sizeof status, ft_save_write(path, "RCSV", 1, buf, sizeof buf) == FT_SAVE_OK
                                                    ? "New best score saved"
                                                    : "New best score (could not save)");
            }
        }
        fe->racer(&r, best, (ft_lang)ss->settings.lang, status);
    }
    session_play_end(ss, s);
}

static void run_penalty(session *ss, shell *s)
{
    char path_buf[512];
    const char *path = data_path(path_buf, sizeof path_buf, "penalty.sav");
    uint8_t buf[PEN_PACKED_SIZE];
    uint32_t len = 0;
    char status[128] = "";
    play_meter meter;
    penalty p;
    int r_load, dirty = 1;

    penalty_init(&p, next_seed(), 1);
    r_load = ft_save_read(path, "PCSV", 1, buf, sizeof buf, &len);
    if (r_load >= 0) {
        penalty saved;
        if (penalty_unpack(&saved, buf, len) == 0 && saved.phase != PEN_OVER) {
            p = saved;
            snprintf(status, sizeof status, "Resumed saved match");
        }
    }
    save_note(status, sizeof status, r_load, "Match");

    meter_start(&meter);
    while (p.phase != PEN_EXITED && !quit_req) {
        ft_key k = next_key(FRAME_MS);
        if (k != FT_NONE) {
            penalty_key(&p, k);
            status[0] = '\0';
            dirty = 1;
        }
        penalty_tick(&p);
        if (!meter_step(&meter, p.phase != PEN_PAUSED, ss, s)) {
            time_up(ss, s);
            p.phase = PEN_EXITED;
            break;
        }
        if (dirty || p.cooldown > 0)
            fe->penalty(&p, (ft_lang)ss->settings.lang, status);
        dirty = 0;
    }
    if (penalty_pack(&p, buf, sizeof buf) == PEN_PACKED_SIZE)
        ft_save_write(path, "PCSV", 1, buf, PEN_PACKED_SIZE);
    session_play_end(ss, s);
}

static int save_garden(const garden *g, uint8_t *buf, size_t cap, const char *path)
{
    return ft_save_write(path, "BGSV", 1, buf, (uint32_t)garden_pack(g, buf, cap));
}

static void run_garden(session *ss, shell *s)
{
    char path_buf[512];
    const char *path = data_path(path_buf, sizeof path_buf, "garden1.sav");
    static uint8_t buf[GARDEN_PACKED_SIZE];
    static garden g;
    uint32_t len = 0;
    char status[128] = "";
    play_meter meter;
    int r_load;

    garden_init(&g);
    r_load = ft_save_read(path, "BGSV", 1, buf, sizeof buf, &len);
    if (r_load >= 0 && garden_unpack(&g, buf, len) != 0) {
        garden_init(&g);
        snprintf(status, sizeof status, "Saved garden could not be loaded; starting fresh");
    } else if (r_load >= 0) {
        snprintf(status, sizeof status, "Garden loaded (slot 1)");
    }
    save_note(status, sizeof status, r_load, "Garden");

    fe->garden(&g, (ft_lang)ss->settings.lang, status);
    meter_start(&meter);
    while (g.mode != GM_EXITED && !quit_req) {
        ft_key k = next_key(FRAME_MS);
        garden_action a;
        if (!meter_step(&meter, g.mode == GM_EDIT || g.mode == GM_PALETTE, ss, s)) {
            if (g.dirty)
                save_garden(&g, buf, sizeof buf, path); /* keep the child's work */
            time_up(ss, s);
            g.mode = GM_EXITED;
            break;
        }
        if (k == FT_NONE)
            continue;
        a = garden_key(&g, k);
        status[0] = '\0';
        if (a == GA_SAVE || a == GA_SAVE_EXIT) {
            int w = save_garden(&g, buf, sizeof buf, path);
            if (w == FT_SAVE_OK)
                g.dirty = 0;
            snprintf(status, sizeof status, w == FT_SAVE_OK ? "Saved to slot 1" : "Save failed; garden kept in memory");
        }
        fe->garden(&g, (ft_lang)ss->settings.lang, status);
    }
    session_play_end(ss, s);
}

/* ---- main --------------------------------------------------------------- */

static void base_config(shell_config *c, int demo_received)
{
    static const char *const names[] = {"Citizen TV", "KTN", "SportyTV"};
    int i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < 3; i++) {
        snprintf(c->favs[i].name, sizeof c->favs[i].name, "%s%s", names[i], demo_received ? " (demo)" : "");
        /* Reception is unknown until Work packet A; --demo-received fakes it, labelled. */
        c->favs[i].received = demo_received ? 1 : -1;
    }
    c->nfav = 3;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: family-tv-sim [--demo-received] [--data DIR] [--keys K,K,...] [--dump-frame FILE.ppm]\n"
            "  keys: L R U D OK BACK W (wait 1s) Wn (wait n ticks)\n");
}

int main(int argc, char **argv)
{
    shell_config cfg;
    static shell s;
    static session ss;
    int demo = 0, i, r_load;
    static char keybuf[4096];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--demo-received")) {
            demo = 1;
        } else if (!strcmp(argv[i], "--dump-frame") && i + 1 < argc) {
            sim_dump_path = argv[++i];
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--keys") && i + 1 < argc) {
            char *tok;
            snprintf(keybuf, sizeof keybuf, "%s", argv[++i]);
            script_mode = 1;
            for (tok = strtok(keybuf, ","); tok && script_len < 1024; tok = strtok(NULL, ","))
                script_tokens[script_len++] = tok;
        } else {
            usage();
            return 2;
        }
    }

    plat_mkdir("out");
    plat_mkdir(data_dir);
    base_config(&cfg, demo);
    shell_init(&s, &cfg);
    {
        char settings_path[512];
        r_load = session_init(&ss, &s, data_path(settings_path, sizeof settings_path, "settings.sav"));
    }
    save_note(s.status, sizeof s.status, r_load, "Settings");
    if (fe->open(script_mode) != 0)
        return 1;

    while (!quit_req) {
        ft_key k;
        int game;
        fe->shell(&s);
        k = next_key(0);
        if (k == FT_NONE)
            continue;
        game = session_dispatch(&ss, &s, shell_key(&s, k));
        if (game == GAME_RACER)
            run_racer(&ss, &s);
        else if (game == GAME_PENALTY)
            run_penalty(&ss, &s);
        else if (game == GAME_GARDEN)
            run_garden(&ss, &s);
    }
    fe->close();
    return 0;
}
