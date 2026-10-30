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
#include "sports.h"
#include "youtube.h"
#include "font.h"
#include "view.h"
#include "frontend.h"
#include "save.h"
#include "session.h"
#include "shell.h"

#define FRAME_MS (1000 / FT_TICK_HZ)

#ifdef _WIN32
#define EXE ".exe"
#else
#define EXE ""
#endif

static char data_dir[512] = "out/simdata";
static char base_dir[512]; /* "" = current folder; else ends in a separator */
static player *pl;
static player_prefs prefs;
static int quit_req;
static int script_mode;
static int script_realtime; /* --keys with an explicit --player: waits take real time */
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
int sim_show_stats;

/* frame_ms > 0: wait at most this long; 0: block until a key. */
static ft_key next_key(int frame_ms)
{
    if (script_mode) {
        if (script_realtime)
            fe->poll(frame_ms > 0 && frame_ms < FRAME_MS ? frame_ms : FRAME_MS, &quit_req);
        return script_next();
    }
    return fe->poll(frame_ms, &quit_req);
}

/* The games know the remote's keys: a click is OK, the rest of the mouse
 * is ignored. */
static ft_key game_key(ft_key k)
{
    return k == FT_CLICK ? FT_OK : k == FT_POINT || k == FT_WHEEL_UP || k == FT_WHEEL_DOWN || k == FT_TEXT ? FT_NONE : k;
}

/* Home screen and lists: hovering moves the focus to what the pointer is
 * on, a click chooses it (a tab or a PIN digit is only selected). Returns
 * the key for shell_key, or FT_NONE after a hover (which still redraws). */
static ft_key pointer_key(shell *s, ft_key k)
{
    int x, y;
    view_hit h;
    if (k != FT_POINT && k != FT_CLICK)
        return k;
    if (!fe->pointer || !fe->pointer(&x, &y))
        return FT_NONE;
    h = view_shell_hit(s, x, y);
    if (h.kind == HIT_NONE || (k == FT_POINT && !h.whole))
        return FT_NONE;
    if (h.kind == HIT_TOP)
        shell_focus_top(s, h.col);
    else if (h.kind == HIT_CARD)
        shell_focus_card(s, h.row, h.col);
    else
        shell_focus_item(s, h.col);
    if (k == FT_POINT || h.kind == HIT_PIN || (h.kind == HIT_TOP && h.col < TAB_COUNT))
        return FT_NONE;
    return FT_OK;
}

/* ---- clocks ----------------------------------------------------------- */

/* Scripted runs use a simulated clock (one frame per loop) so they are repeatable. */
static uint64_t loop_now(void)
{
    static uint64_t fake;
    if (script_mode && !script_realtime)
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

/* A path that does not fit comes back empty, so opening it simply fails
 * instead of touching some other, shortened path. */
static const char *data_path(char *out, size_t n, const char *file)
{
    size_t len = strlen(data_dir);
    int sep = len > 0 && (data_dir[len - 1] == '/' || data_dir[len - 1] == '\\');
    if (snprintf(out, n, "%s%s%s", data_dir, sep ? "" : "/", file) >= (int)n)
        out[0] = '\0';
    return out;
}

static const char *base_path(char *out, size_t n, const char *file)
{
    if (snprintf(out, n, "%s%s", base_dir, file) >= (int)n)
        out[0] = '\0';
    return out;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f)
        fclose(f);
    return f != NULL;
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
        ft_key k = game_key(next_key(FRAME_MS));
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
        ft_key k = game_key(next_key(FRAME_MS));
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
        ft_key k = game_key(next_key(FRAME_MS));
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
    return quality == QUALITY_360 ? 360 : quality == QUALITY_720 ? 720 : quality == QUALITY_1080 ? 1080 : 480;
}

static void save_prefs(void)
{
    char path[512];
    prefs_save(&prefs, data_path(path, sizeof path, "player.sav"));
}

/* "Live football now" as a channel list, so zapping works as on any list. */
static channel_list found_list;

static const channel_list *found_channels(const shell *s)
{
    int i;
    memset(&found_list, 0, sizeof found_list);
    for (i = 0; i < s->cfg.nfound && i < CH_MAX; i++) {
        channel *ch = &found_list.ch[found_list.n++];
        ch->number = i + 1;
        snprintf(ch->name, sizeof ch->name, "%s", s->cfg.found[i].by);
        snprintf(ch->url, sizeof ch->url, "https://www.youtube.com/watch?v=%s", s->cfg.found[i].video_id);
        ch->accent = FT_RGB(0x1C7A3C);
        ch->group = CH_SPORTS;
    }
    return &found_list;
}

/* A YouTube row as a channel list: Up and Down go to the next video. */
static const channel_list *video_channels(const shell *s, int index, int *start)
{
    int feed = index / YT_FEED_MAX, i;
    memset(&found_list, 0, sizeof found_list);
    for (i = 0; i < s->cfg.nvideos[feed]; i++) {
        const yt_video *v = &s->cfg.videos[feed * YT_FEED_MAX + i];
        channel *ch = &found_list.ch[found_list.n++];
        ch->number = i + 1;
        snprintf(ch->name, sizeof ch->name, "%s", v->title);
        yt_watch_url(v, ch->url, sizeof ch->url);
        ch->accent = FT_RGB(0xC4122F);
    }
    *start = index % YT_FEED_MAX;
    return &found_list;
}

/* Sports channels play their live match itself when the Sports tab found
 * one; the channel lists remember the last channel for the banner. */
static void tune(shell *s, const channel_list *chs, int index)
{
    const channel *ch = &chs->ch[index];
    if (chs == &found_list) { /* found by search: the address is the stream itself */
        player_open(pl, ch->url);
        return;
    }
    if (chs == &s->cfg.sports) {
        char url[CH_URL_MAX];
        player_open(pl, sport_watch_url(&s->cfg.sport[index], ch->url, url, sizeof url));
        return;
    }
    player_open(pl, ch->url);
    s->cfg.last_channel = index;
    prefs.last_channel = ch->number;
}

static int live_apply(shell *s, const channel_list *chs, live_action a)
{
    if (a.act == LIVE_ACT_TUNE)
        tune(s, chs, a.arg);
    else if (a.act == LIVE_ACT_VOLUME)
        player_set_volume(pl, a.arg);
    return a.act == LIVE_ACT_EXIT;
}

/* Watches until Back. The stream stops on the way out, so no data is
 * spent while the family is on the home screen. */
static void run_live(session *ss, shell *s, const channel_list *chs, int index)
{
    live l;
    uint64_t last = loop_now();
    int done = 0, was_playing = 0;

    live_init(&l, chs->n, index, prefs.volume);
    player_set_max_height(pl, quality_height(ss->settings.quality));
    player_set_volume(pl, l.volume);
    tune(s, chs, index);
    save_prefs();
    while (!quit_req && !done) {
        ft_key k = next_key(FRAME_MS);
        uint64_t now = loop_now();
        done = live_apply(s, chs, live_tick(&l, (int)(now - last)));
        last = now;
        if (k == FT_CLICK && l.panel_open) { /* a click on the channel list plays that channel */
            int x, y, row = fe->pointer && fe->pointer(&x, &y) ? view_live_hit(&l, chs->n, x, y) : -1;
            if (row < 0)
                k = FT_NONE;
            else
                l.panel_item = row;
        } else if (k == FT_POINT && l.panel_open) { /* hovering the list moves its focus */
            int x, y, row = fe->pointer && fe->pointer(&x, &y) ? view_live_hit(&l, chs->n, x, y) : -1;
            if (row >= 0)
                l.panel_item = row;
            k = FT_NONE;
        }
        if (k != FT_NONE)
            done = live_apply(s, chs, live_key(&l, k, player_get(pl)->state == PLAYER_ERROR)) || done;
        player_pump(pl, now);
        if (player_get(pl)->state == PLAYER_PLAYING && !was_playing)
            live_started(&l);
        was_playing = player_get(pl)->state == PLAYER_PLAYING;
        fe->live(&l, pl, s, chs);
    }
    player_stop(pl);
    prefs.volume = l.volume;
    save_prefs();
}

/* yt-dlp and deno ship next to the program on Windows; on a development
 * Mac they come from Homebrew, which a Finder launch leaves off PATH. */
static const char *find_tool(char *out, size_t n, const char *name)
{
    static const char *const dirs[] = {"/opt/homebrew/bin/", "/usr/local/bin/"};
    size_t i;
    char file[64];
    snprintf(file, sizeof file, "%s%s", name, EXE);
    if (file_exists(base_path(out, n, file)))
        return out;
    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        snprintf(out, n, "%s%s", dirs[i], file);
        if (file_exists(out))
            return out;
    }
    return NULL;
}

static player *make_player(int want_stub, const char *ytdl_path, int verbose)
{
    static char ytdl[512], deno[512], js_dir[512];
    player_config pc;
    player *p = NULL;
    char log_path[512];
    memset(&pc, 0, sizeof pc);
    pc.ytdl_path = ytdl_path ? ytdl_path : find_tool(ytdl, sizeof ytdl, "yt-dlp");
    if (find_tool(deno, sizeof deno, "deno")) {
        char *slash = strrchr(deno, '/'), *bslash = strrchr(deno, '\\');
        if (bslash > slash)
            slash = bslash;
        snprintf(js_dir, sizeof js_dir, "%.*s", slash ? (int)(slash - deno) : 0, deno);
        pc.js_dir = js_dir[0] ? js_dir : ".";
    }
    pc.verbose = verbose;
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

/* ---- Sports tab ------------------------------------------------------------ */

#define PROBES_AT_ONCE 4

static uint64_t sports_checked_ms;
static int probe_next, probes_running, searches_left;
static sport_found found_next[SPORT_FOUND_MAX]; /* this round's search results, shown when all are in */
static int nfound_next;

/* The live football searches go first: they take longest. */
static void start_probes(shell *s)
{
    while (probes_running < PROBES_AT_ONCE && probe_next < SPORT_SEARCHES + s->cfg.sports.n) {
        int next = probe_next++, i = next - SPORT_SEARCHES;
        char page[CH_URL_MAX + 8];
        if (next < SPORT_SEARCHES) {
            if (player_probe(pl, PLAYER_SEARCH_TAG + next, sport_search_url(next, page, sizeof page),
                             SPORT_SEARCH_FORMAT, SPORT_SEARCH_ITEMS) == 0)
                probes_running++;
            else
                searches_left--;
            continue;
        }
        if (!sport_streams_url(s->cfg.sports.ch[i].url, page, sizeof page) ||
            player_probe(pl, i, page, SPORT_PRINT_FORMAT, 4) != 0) {
            s->cfg.sport[i].state = SPORT_UNKNOWN;
            continue;
        }
        s->cfg.sport[i].state = SPORT_CHECKING;
        probes_running++;
    }
}

/* Asks every sports channel what it is streaming and searches for live
 * football, unless that was done within the last few minutes. */
static void sports_refresh(shell *s)
{
    uint64_t now = loop_now();
    int i;
    if (probes_running > 0 || (sports_checked_ms && now - sports_checked_ms < SPORT_FRESH_MS))
        return;
    sports_checked_ms = now;
    probe_next = 0;
    searches_left = SPORT_SEARCHES;
    nfound_next = 0;
    s->cfg.searching = 1;
    for (i = 0; i < s->cfg.sports.n; i++)
        s->cfg.sport[i].state = SPORT_CHECKING;
    start_probes(s);
}

/* ---- YouTube tab ----------------------------------------------------------- */

#define YT_TAG 800 /* probe tags YT_TAG + feed */

static uint64_t yt_checked_ms;
static int yt_running;

static void yt_lookup(shell *s, int feed)
{
    char url[128];
    if (!yt_feed_url(feed, s->cfg.yt_query, url, sizeof url) ||
        player_probe(pl, YT_TAG + feed, url, YT_PRINT_FORMAT, YT_FEED_MAX) != 0) {
        s->cfg.yt_loading &= ~(1u << feed);
        return;
    }
    s->cfg.yt_loading |= 1u << feed;
    yt_running++;
}

/* The ready-made rows, when the tab opens and every so often after. */
static void yt_refresh(shell *s)
{
    uint64_t now = loop_now();
    int f;
    if (s->cfg.profile != PROFILE_PARENT || (yt_checked_ms && now - yt_checked_ms < YT_FRESH_MS))
        return;
    yt_checked_ms = now;
    for (f = 1; f < YT_FEEDS; f++)
        yt_lookup(s, f);
}

/* Returns 1 if anything on the Sports or YouTube tab changed. */
static int sports_collect(shell *s)
{
    player_probe_result r;
    int changed = 0;
    while (player_probe_next(pl, &r)) {
        if (r.tag >= YT_TAG && r.tag < YT_TAG + YT_FEEDS) {
            int f = r.tag - YT_TAG;
            s->cfg.nvideos[f] = r.ok ? yt_parse(r.out, &s->cfg.videos[f * YT_FEED_MAX], YT_FEED_MAX) : 0;
            s->cfg.yt_loading &= ~(1u << f);
            yt_running = yt_running > 0 ? yt_running - 1 : 0;
            changed = 1;
            continue;
        }
        if (r.tag >= PLAYER_SEARCH_TAG) {
            if (r.ok)
                nfound_next = sport_add_found(found_next, nfound_next, r.out);
            searches_left--;
        } else if (r.tag >= 0 && r.tag < s->cfg.sports.n) {
            if (r.ok)
                sport_parse(r.out, &s->cfg.sport[r.tag]);
            else
                s->cfg.sport[r.tag].state = SPORT_UNKNOWN;
        }
        probes_running = probes_running > 0 ? probes_running - 1 : 0;
        changed = 1;
    }
    if (changed)
        start_probes(s);
    if (s->cfg.searching && searches_left <= 0) { /* all searches in: show them at once */
        memcpy(s->cfg.found, found_next, sizeof found_next);
        s->cfg.nfound = nfound_next;
        s->cfg.searching = 0;
        changed = 1;
    }
    return changed;
}

/* ---- main --------------------------------------------------------------- */

static void load_config(shell_config *c, char *warn, size_t n)
{
    char data_ch[512];
    char base_ch[512];
    const char *paths[3];
    static channel_list all;
    int last;
    memset(c, 0, sizeof *c);
    paths[0] = data_path(data_ch, sizeof data_ch, "channels.txt");
    paths[1] = base_path(base_ch, sizeof base_ch, "channels.txt");
    paths[2] = "assets/channels.txt"; /* running from the source folder */
    channels_load(&all, paths, 3, warn, n);
    channels_group(&all, CH_TV, &c->channels);
    channels_group(&all, CH_SPORTS, &c->sports);
    last = channels_find_number(&c->channels, prefs.last_channel);
    c->last_channel = last >= 0 ? last : 0;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: familytv [--data DIR] [--fullscreen] [--no-intro] [--player stub|mpv] [--ytdl PATH]\n"
            "                [--verbose] [--stats]\n"
            "                [--keys K,K,...] [--dump-frame FILE.ppm]\n"
            "  keys: L R U D OK BACK W (wait 1s) Wn (wait n ticks); scripted runs use the stub player\n"
            "  unless --player mpv is given; with either --player the waits take real time\n");
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
    int i, r_load, want_stub = 0, want_mpv = 0, verbose = 0, fullscreen = 0, intro = 1;
    const char *data_arg = NULL;
    const char *ytdl_path = NULL;
    char warn[160] = "";
    static char keybuf[4096];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump-frame") && i + 1 < argc) {
            sim_dump_path = argv[++i];
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_arg = argv[++i];
        } else if (!strcmp(argv[i], "--fullscreen")) {
            fullscreen = 1;
        } else if (!strcmp(argv[i], "--verbose")) {
            verbose = 1;
        } else if (!strcmp(argv[i], "--stats")) {
            sim_show_stats = 1;
        } else if (!strcmp(argv[i], "--no-intro")) {
            intro = 0;
        } else if (!strcmp(argv[i], "--player") && i + 1 < argc) {
            want_stub = !strcmp(argv[++i], "stub");
            want_mpv = !strcmp(argv[i], "mpv");
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

    fe->folders(data_dir, sizeof data_dir, base_dir, sizeof base_dir);
    if (data_arg || !data_dir[0]) {
        plat_mkdir("out");
        snprintf(data_dir, sizeof data_dir, "%s", data_arg ? data_arg : "out/simdata");
    }
    plat_mkdir(data_dir);
    {
        char path[512];
        prefs_load(&prefs, data_path(path, sizeof path, "player.sav"));
    }
    {
        /* Sharp text on big screens: the Inter TTFs ship next to the
         * program; in the source folder they are under assets/. */
        char fonts[600];
        if (font_scale_init(base_path(fonts, sizeof fonts, "fonts/")) != 0)
            font_scale_init("assets/fonts/inter/");
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
    if (fe->open(script_mode, (fullscreen || prefs.fullscreen) && !script_mode) != 0)
        return 1;
    script_realtime = script_mode && (want_mpv || want_stub);
    pl = make_player(want_stub || (script_mode && !want_mpv), ytdl_path, verbose);
    if (!pl) {
        fe->close();
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if (intro && !script_mode && fe->startup) { /* a TV switching on; any key skips it */
        uint64_t start = plat_monotonic_ms();
        for (;;) {
            uint64_t t = plat_monotonic_ms() - start;
            if (t >= VIEW_STARTUP_MS || quit_req)
                break;
            fe->startup((uint32_t)t, s.cfg.lang);
            if (fe->poll(16, &quit_req) != FT_NONE)
                break;
        }
    }

    while (!quit_req) {
        ft_key k;
        int r, again, wait;
        if (s.screen == SCR_HOME && s.tab == TAB_SPORTS)
            sports_refresh(&s);
        if (s.screen == SCR_HOME && s.tab == TAB_YOUTUBE)
            yt_refresh(&s);
        if (fe->text_mode)
            fe->text_mode(s.screen == SCR_KEYBOARD ? 2 : s.screen == SCR_HOME && s.tab == TAB_YOUTUBE ? 1 : 0);
        again = fe->shell(&s);
        /* Sports answers wake the loop through the player (the stub needs a nudge). */
        wait = probes_running > 0 || yt_running > 0 ? 100 : ms_to_next_minute();
        k = next_key(again > 0 && again < wait ? again : wait);
        player_pump(pl, loop_now());
        sports_collect(&s);
        k = pointer_key(&s, k);
        if (k == FT_NONE)
            continue; /* redraw: the clock moved on, sports news, or the pointer moved */
        r = session_dispatch(&ss, &s, k == FT_TEXT && fe->typed ? shell_type(&s, fe->typed()) : shell_key(&s, k));
        if (r == GAME_RACER)
            run_racer(&ss, &s);
        else if (r == GAME_PENALTY)
            run_penalty(&ss, &s);
        else if (r == GAME_GARDEN)
            run_garden(&ss, &s);
        else if (r == SESSION_YT_SEARCH)
            yt_lookup(&s, 0);
        else if (r == SESSION_WATCH && ss.watch_list == WATCH_VIDEO) {
            int start;
            const channel_list *row = video_channels(&s, ss.watch_channel, &start);
            run_live(&ss, &s, row, start);
        } else if (r == SESSION_WATCH)
            run_live(&ss, &s,
                     ss.watch_list == WATCH_FOUND    ? found_channels(&s)
                     : ss.watch_list == WATCH_SPORTS ? &s.cfg.sports
                                                     : &s.cfg.channels,
                     ss.watch_channel);
        else if (r == SESSION_OPEN_APP) {
            /* the real site inside the window; without a web view, its own window */
            if (!fe->browse || fe->browse(ss.open_app, shell_app_url(ss.open_app), data_dir, &quit_req) != 0)
                snprintf(s.status, sizeof s.status, "%s",
                         tr(s.cfg.lang, fe->open_url(shell_app_url(ss.open_app)) == 0 ? STR_APP_OPENED : STR_UNAVAILABLE));
        }
    }
    player_destroy(pl);
    if (!script_mode) {
        prefs.fullscreen = fe->is_fullscreen();
        save_prefs();
    }
    fe->close();
    return 0;
}
