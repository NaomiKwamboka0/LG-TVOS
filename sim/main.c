/* Family TV: home screen, live channels and games for a laptop. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "channels.h"
#include "garden.h"
#include "live.h"
#include "penalty.h"
#include "platform.h"
#include "player.h"
#include "playtime.h"
#include "prefs.h"
#include "racer.h"
#include "frontend.h"
#include "save.h"
#include "session.h"
#include "shell.h"

#define DEFAULT_DATA_DIR "out/simdata"
#define FRAME_MS (1000 / FT_TICK_HZ)

static const char *data_dir = DEFAULT_DATA_DIR;
static player *pl;
static player_prefs prefs;
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

/* frame_ms > 0: wait at most this long; 0: block until a key. */
static ft_key next_key(int frame_ms)
{
    if (script_mode)
        return script_next();
    return fe->poll(frame_ms, &quit_req);
}

/* ---- clocks ----------------------------------------------------------- */

/* Scripted runs use a simulated clock (one frame per loop) so they are repeatable. */
static uint64_t loop_now(void)
{
    static uint64_t fake;
    if (script_mode)
        return fake += FRAME_MS;
    return plat_monotonic_ms();
}

static void meter_start(play_meter *m)
{
    play_meter_start(m, loop_now());
}

/* Counts time only while the game is actively played (not paused).
 * Returns 0 once a child's allowance has run out. */
static int meter_step(play_meter *m, int active, session *ss, shell *s)
{
    uint32_t secs = play_meter_step(m, loop_now(), active);
    return secs ? session_play_account(ss, s, secs) : 1;
}

static void time_up(session *ss, shell *s)
{
    snprintf(s->status, sizeof s->status, "%s", tr((ft_lang)ss->settings.lang, STR_TIME_UP));
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

/* ---- live TV ----------------------------------------------------------- */

static int quality_height(int quality)
{
    return quality == QUALITY_360 ? 360 : quality == QUALITY_720 ? 720 : 480;
}

static void save_prefs(void)
{
    char path[512];
    prefs_save(&prefs, data_path(path, sizeof path, "player.sav"));
}

static void tune(shell *s, int index)
{
    const channel *ch = &s->cfg.channels.ch[index];
    player_open(pl, ch->url);
    s->cfg.last_channel = index;
    prefs.last_channel = ch->number;
}

static int live_apply(shell *s, live_action a)
{
    if (a.act == LIVE_ACT_TUNE)
        tune(s, a.arg);
    else if (a.act == LIVE_ACT_VOLUME)
        player_set_volume(pl, a.arg);
    return a.act == LIVE_ACT_EXIT;
}

/* Watches until Back. The stream stops on the way out, so no data is
 * spent while the family is on the home screen. */
static void run_live(session *ss, shell *s, int index)
{
    live l;
    uint64_t last = loop_now();
    int done = 0;

    live_init(&l, s->cfg.channels.n, index, prefs.volume);
    player_set_max_height(pl, quality_height(ss->settings.quality));
    player_set_volume(pl, l.volume);
    tune(s, index);
    save_prefs();
    while (!quit_req && !done) {
        ft_key k = next_key(FRAME_MS);
        uint64_t now = loop_now();
        done = live_apply(s, live_tick(&l, (int)(now - last)));
        last = now;
        if (k != FT_NONE)
            done = live_apply(s, live_key(&l, k, player_get(pl)->state == PLAYER_ERROR)) || done;
        player_pump(pl, now);
        fe->live(&l, pl, s);
    }
    player_stop(pl);
    prefs.volume = l.volume;
    save_prefs();
}

static player *make_player(int want_stub, const char *ytdl_path)
{
    player_config pc;
    player *p = NULL;
    char log_path[512];
    memset(&pc, 0, sizeof pc);
    pc.ytdl_path = ytdl_path;
    pc.log_path = data_path(log_path, sizeof log_path, "familytv.log");
    pc.max_height = 480;
    pc.volume = prefs.volume;
    pc.wakeup = fe->wake;
    if (want_stub)
        return player_create_stub(&pc);
#ifdef FT_WINDOW
    p = player_create_mpv(&pc);
#endif
    return p ? p : player_create_unavailable(&pc);
}

/* ---- main --------------------------------------------------------------- */

static void load_config(shell_config *c, char *warn, size_t n)
{
    char data_ch[512];
    const char *paths[2];
    int last;
    memset(c, 0, sizeof *c);
    paths[0] = data_path(data_ch, sizeof data_ch, "channels.txt");
    paths[1] = "assets/channels.txt"; /* running from the source folder */
    channels_load(&c->channels, paths, 2, warn, n);
    last = channels_find_number(&c->channels, prefs.last_channel);
    c->last_channel = last >= 0 ? last : 0;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: familytv [--data DIR] [--player stub|mpv] [--ytdl PATH]\n"
            "                [--keys K,K,...] [--dump-frame FILE.ppm]\n"
            "  keys: L R U D OK BACK W (wait 1s) Wn (wait n ticks); scripted runs use the stub player\n");
}

/* Milliseconds until the clock on screen next changes. */
static int ms_to_next_minute(void)
{
    return (int)(60 - plat_wall_utc() % 60) * 1000 + 50;
}

int main(int argc, char **argv)
{
    shell_config cfg;
    static shell s;
    static session ss;
    int i, r_load, want_stub = 0;
    const char *ytdl_path = NULL;
    char warn[160] = "";
    static char keybuf[4096];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump-frame") && i + 1 < argc) {
            sim_dump_path = argv[++i];
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--player") && i + 1 < argc) {
            want_stub = !strcmp(argv[++i], "stub");
        } else if (!strcmp(argv[i], "--ytdl") && i + 1 < argc) {
            ytdl_path = argv[++i];
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
    {
        char path[512];
        prefs_load(&prefs, data_path(path, sizeof path, "player.sav"));
    }
    load_config(&cfg, warn, sizeof warn);
    shell_init(&s, &cfg);
    {
        char settings_path[512];
        r_load = session_init(&ss, &s, data_path(settings_path, sizeof settings_path, "settings.sav"));
    }
    save_note(s.status, sizeof s.status, r_load, "Settings");
    if (warn[0] && !s.status[0])
        snprintf(s.status, sizeof s.status, "%s", warn);
    if (fe->open(script_mode) != 0)
        return 1;
    pl = make_player(want_stub || script_mode, ytdl_path);
    if (!pl) {
        fe->close();
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    while (!quit_req) {
        ft_key k;
        int r;
        fe->shell(&s);
        k = next_key(ms_to_next_minute());
        if (k == FT_NONE)
            continue; /* redraw: the clock moved on */
        r = session_dispatch(&ss, &s, shell_key(&s, k));
        if (r == GAME_RACER)
            run_racer(&ss, &s);
        else if (r == GAME_PENALTY)
            run_penalty(&ss, &s);
        else if (r == GAME_GARDEN)
            run_garden(&ss, &s);
        else if (r == SESSION_WATCH)
            run_live(&ss, &s, ss.watch_channel);
    }
    player_destroy(pl);
    fe->close();
    return 0;
}
