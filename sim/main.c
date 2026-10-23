/* Host simulator for the family TV experience. Runs on the development
 * computer only; it is not device evidence (plan p.25). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "garden.h"
#include "penalty.h"
#include "racer.h"
#include "render_text.h"
#include "save.h"
#include "session.h"
#include "shell.h"

#define DATA_DIR "out/simdata"

static int quit_req;
static int script_mode;
static char *script_tokens[1024];
static int script_len, script_pos, script_wait;

/* ---- input ---------------------------------------------------------- */

static struct termios orig_term;
static int raw_on;

static void raw_off(void)
{
    if (raw_on) {
        tcsetattr(0, TCSANOW, &orig_term);
        raw_on = 0;
        printf("\033[?25h\n");
    }
}

static void raw_enable(void)
{
    struct termios t;
    if (tcgetattr(0, &orig_term) != 0)
        return;
    t = orig_term;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    raw_on = 1;
    atexit(raw_off);
    printf("\033[?25l");
}

static int wait_readable(int ms)
{
    fd_set fs;
    struct timeval tv;
    FD_ZERO(&fs);
    FD_SET(0, &fs);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select(1, &fs, NULL, NULL, &tv) > 0;
}

static ft_key poll_terminal(int ms)
{
    unsigned char c, seq[2];
    if (!wait_readable(ms) || read(0, &c, 1) != 1)
        return FT_NONE;
    if (c == 27) {
        if (!wait_readable(30) || read(0, &seq[0], 1) != 1 || seq[0] != '[')
            return FT_BACK;
        if (read(0, &seq[1], 1) != 1)
            return FT_NONE;
        switch (seq[1]) {
        case 'A': return FT_UP;
        case 'B': return FT_DOWN;
        case 'C': return FT_RIGHT;
        case 'D': return FT_LEFT;
        default: return FT_NONE;
        }
    }
    if (c == '\r' || c == '\n' || c == ' ')
        return FT_OK;
    if (c == 127 || c == 8 || c == 'b')
        return FT_BACK;
    if (c == 'q')
        quit_req = 1;
    return FT_NONE;
}

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

/* frame_ms > 0: game frame pacing; 0: block until a key (menus). */
static ft_key next_key(int frame_ms)
{
    ft_key k;
    if (script_mode)
        return script_next();
    if (frame_ms > 0)
        return poll_terminal(frame_ms);
    do {
        k = poll_terminal(200);
    } while (k == FT_NONE && !quit_req);
    return k;
}

/* Scripted runs print every frame too, so smoke tests can grep the output. */
static int show_frames(void)
{
    return 1;
}

/* ---- saves ------------------------------------------------------------ */

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

static void run_racer(void)
{
    static const char path[] = DATA_DIR "/racer.sav";
    uint8_t buf[4 * RACER_LEVELS];
    uint32_t best[RACER_LEVELS] = {0}, len = 0;
    char status[128] = "";
    racer r;
    int i, recorded = 0, r_load;

    r_load = ft_save_read(path, "RCSV", 1, buf, sizeof buf, &len);
    if (r_load >= 0 && len == sizeof buf)
        for (i = 0; i < RACER_LEVELS; i++)
            best[i] = get32(buf + 4 * i);
    save_note(status, sizeof status, r_load, "High scores");

    racer_init(&r, next_seed(), RACER_EASY, 0);
    while (r.state != RACER_EXITED && !quit_req) {
        ft_key k = next_key(1000 / FT_TICK_HZ);
        if (k != FT_NONE)
            racer_key(&r, k);
        racer_tick(&r);
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
        if (show_frames())
            render_racer(&r, best, status);
    }
}

static void run_penalty(void)
{
    static const char path[] = DATA_DIR "/penalty.sav";
    uint8_t buf[PEN_PACKED_SIZE];
    uint32_t len = 0;
    char status[128] = "";
    penalty p;
    int r_load;

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

    while (p.phase != PEN_EXITED && !quit_req) {
        ft_key k = next_key(1000 / FT_TICK_HZ);
        if (k != FT_NONE) {
            penalty_key(&p, k);
            status[0] = '\0';
        }
        penalty_tick(&p);
        if (show_frames() && (k != FT_NONE || p.cooldown > 0 || status[0]))
            render_penalty(&p, status);
    }
    if (penalty_pack(&p, buf, sizeof buf) == PEN_PACKED_SIZE)
        ft_save_write(path, "PCSV", 1, buf, PEN_PACKED_SIZE);
}

static void run_garden(void)
{
    static const char path[] = DATA_DIR "/garden1.sav";
    static uint8_t buf[GARDEN_PACKED_SIZE];
    static garden g;
    uint32_t len = 0;
    char status[128] = "";
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

    if (show_frames())
        render_garden(&g, status);
    while (g.mode != GM_EXITED && !quit_req) {
        ft_key k = next_key(0);
        garden_action a;
        if (k == FT_NONE)
            continue;
        a = garden_key(&g, k);
        status[0] = '\0';
        if (a == GA_SAVE || a == GA_SAVE_EXIT) {
            int w = ft_save_write(path, "BGSV", 1, buf, (uint32_t)garden_pack(&g, buf, sizeof buf));
            if (w == FT_SAVE_OK)
                g.dirty = 0;
            snprintf(status, sizeof status, w == FT_SAVE_OK ? "Saved to slot 1" : "Save failed; garden kept in memory");
        }
        if (show_frames())
            render_garden(&g, status);
    }
}

/* ---- main --------------------------------------------------------------- */

static void default_config(shell_config *c, int demo_received)
{
    static const char *const names[] = {"Citizen TV", "KTN", "SportyTV"};
    int i;
    memset(c, 0, sizeof *c);
    c->profile = PROFILE_PARENT;
    c->child_games = (1u << GAME_RACER) | (1u << GAME_PENALTY) | (1u << GAME_GARDEN);
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
            "usage: family-tv-sim [--demo-received] [--keys K,K,...]\n"
            "  keys: L R U D OK BACK W (wait 1s) Wn (wait n ticks)\n");
}

int main(int argc, char **argv)
{
    shell_config cfg;
    shell s;
    int demo = 0, i;
    static char keybuf[4096];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--demo-received")) {
            demo = 1;
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

    mkdir("out", 0755);
    mkdir(DATA_DIR, 0755);
    default_config(&cfg, demo);
    shell_init(&s, &cfg);
    if (!script_mode)
        raw_enable();

    while (!quit_req) {
        ft_key k;
        shell_action a;
        int game;
        if (show_frames())
            render_shell(&s);
        k = next_key(0);
        if (k == FT_NONE)
            continue;
        a = shell_key(&s, k);
        game = session_dispatch(&s, a);
        if (game == GAME_RACER)
            run_racer();
        else if (game == GAME_PENALTY)
            run_penalty();
        else if (game == GAME_GARDEN)
            run_garden();
    }
    return 0;
}
