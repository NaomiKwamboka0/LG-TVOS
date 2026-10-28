/* Text-terminal frontend: a development tool (macOS and Linux terminals). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "frontend.h"
#include "render_text.h"

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

static void text_folders(char *data, size_t data_n, char *base, size_t base_n)
{
    snprintf(data, data_n, "out/simdata");
    snprintf(base, base_n, "%s", "");
}

static int text_open(int scripted, int fullscreen)
{
    struct termios t;
    (void)fullscreen;
    if (scripted || tcgetattr(0, &orig_term) != 0)
        return 0;
    t = orig_term;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    raw_on = 1;
    atexit(raw_off);
    printf("\033[?25l");
    return 0;
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

static ft_key read_key(int ms, int *quit)
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
        *quit = 1;
    return FT_NONE;
}

static ft_key text_poll(int ms, int *quit)
{
    ft_key k;
    if (ms > 0)
        return read_key(ms, quit);
    do {
        k = read_key(200, quit);
    } while (k == FT_NONE && !*quit);
    return k;
}

static void text_racer(const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang, const char *status)
{
    (void)lang;
    render_racer(r, best, status);
}

static void text_penalty(const penalty *p, ft_lang lang, const char *status)
{
    (void)lang;
    render_penalty(p, status);
}

static void text_garden(const garden *g, ft_lang lang, const char *status)
{
    (void)lang;
    render_garden(g, status);
}

static void text_live(const live *l, player *p, const shell *s, const channel_list *chs)
{
    render_live(l, player_get(p), s, chs);
}

static int text_open_url(const char *url)
{
    printf("\n  open in a browser: %s\n", url);
    return 0;
}

static int text_shell(const shell *s)
{
    render_shell(s);
    return 0;
}

static int text_is_fullscreen(void)
{
    return 0;
}

const sim_frontend frontend_text = {
    "text", text_folders, text_open, raw_off, text_poll, text_shell, text_racer, text_penalty, text_garden,
    text_live, NULL, text_is_fullscreen, text_open_url, NULL
};
