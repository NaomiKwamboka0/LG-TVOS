#include "player_impl.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void player_destroy(player *p)
{
    if (p)
        p->ops->destroy(p);
}

void player_open(player *p, const char *url)
{
    p->st.error = PERR_NONE;
    p->st.detail[0] = '\0';
    p->st.video_h = 0;
    p->ops->open(p, url);
}

void player_stop(player *p)
{
    p->ops->stop(p);
    p->st.state = PLAYER_IDLE;
    p->st.error = PERR_NONE;
    p->st.video_h = 0;
}

void player_set_volume(player *p, int volume)
{
    volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    p->st.volume = volume;
    p->ops->set_volume(p, volume);
}

void player_set_max_height(player *p, int height)
{
    p->max_height = height;
}

void player_pump(player *p, uint64_t now_ms)
{
    p->ops->pump(p, now_ms);
}

const player_status *player_get(const player *p)
{
    return &p->st;
}

int player_render(player *p, uint32_t *px, int w, int h, int stride)
{
    return p->ops->render(p, px, w, h, stride);
}

int player_probe(player *p, int tag, const char *page_url, const char *print_format, int items)
{
    return p->ops->probe ? p->ops->probe(p, tag, page_url, print_format, items) : -1;
}

void player_probe_done(player *p, int tag, int ok, const char *out, size_t len)
{
    player_probe_result *r;
    if (p->ndone >= PROBE_QUEUE)
        return;
    r = &p->done[p->ndone++];
    r->tag = tag;
    r->ok = ok;
    if (len >= sizeof r->out)
        len = sizeof r->out - 1;
    memcpy(r->out, out, len);
    r->out[len] = '\0';
}

int player_probe_next(player *p, player_probe_result *r)
{
    if (p->ndone == 0)
        return 0;
    *r = p->done[0];
    memmove(p->done, p->done + 1, (size_t)(p->ndone - 1) * sizeof p->done[0]);
    p->ndone--;
    return 1;
}

void player_fail(player *p, player_error e, const char *detail)
{
    size_t i, n = 0;
    p->st.state = PLAYER_ERROR;
    p->st.error = e;
    for (i = 0; detail && detail[i] && n + 1 < sizeof p->st.detail; i++) {
        unsigned char c = (unsigned char)detail[i];
        p->st.detail[n++] = (char)(c >= 0x20 && c < 0x7F ? c : ' ');
    }
    p->st.detail[n] = '\0';
}

static const char *const ERROR_NAMES[PERR_COUNT] = {
    "none", "offline", "not-live", "no-resolver", "no-js", "blocked", "bad-channel", "timeout",
    "playback", "no-engine"
};

const char *player_error_name(player_error e)
{
    return e >= 0 && e < PERR_COUNT ? ERROR_NAMES[e] : "playback";
}

void player_format(int max_height, char *out, unsigned n)
{
    /* YouTube live sends video and audio as separate streams, so a plain
     * "best" finds nothing. 720p is only offered at 60 fps, so the HD
     * choice drops the 30 fps cap rather than quietly falling to 480p. */
    if (max_height >= 720)
        snprintf(out, n, "bv*[height<=?%d]+ba/b[height<=?%d]/wv*+ba/w", max_height, max_height);
    else
        snprintf(out, n, "bv*[height<=?%d][fps<=?30]+ba/b[height<=?%d][fps<=?30]/"
                         "bv*[height<=?%d]+ba/b[height<=?%d]/wv*+ba/w",
                 max_height, max_height, max_height, max_height);
}

/* Case-insensitive substring search. */
static int has(const char *text, const char *needle)
{
    size_t i, j, n = strlen(needle);
    for (i = 0; text[i]; i++) {
        for (j = 0; j < n && text[i + j]; j++)
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)needle[j]))
                break;
        if (j == n)
            return 1;
    }
    return 0;
}

static int has_any(const char *text, const char *const *needles)
{
    for (; *needles; needles++)
        if (has(text, *needles))
            return 1;
    return 0;
}

/* Order matters: a missing tool explains everything after it, a channel
 * that is simply not live should never read as "no internet", and yt-dlp
 * words a 404 as "unable to download webpage", like a network failure. */
player_error player_classify(const char *t)
{
    static const char *const no_resolver[] = {
        "youtube-dl failed: not found", "yt-dlp: not found", "ytdl_hook: not found",
        "could not find yt-dlp", "unable to start yt-dlp", NULL
    };
    static const char *const no_js[] = {
        "no supported javascript runtime", "javascript runtime", "challenge solving failed",
        "n challenge solving", "signature solving failed", NULL
    };
    static const char *const not_live[] = {
        "not currently live", "is not live", "will begin", "premieres in", "live event has ended",
        "this live event", NULL
    };
    static const char *const blocked[] = {
        "sign in to confirm", "not a bot", "http error 429", "too many requests", NULL
    };
    static const char *const bad_channel[] = {
        "video unavailable", "does not exist", "http error 404", "unsupported url", NULL
    };
    static const char *const offline[] = {
        "failed to resolve", "getaddrinfo", "nodename nor servname", "name or service not known",
        "temporary failure in name resolution", "no address associated", "network is unreachable",
        "connection refused", "urlopen error", "connection reset", "timed out",
        "failed to establish a new connection", "unable to connect to proxy", "network is down", NULL
    };
    if (!t || !t[0])
        return PERR_PLAYBACK;
    if (has_any(t, no_resolver))
        return PERR_NO_RESOLVER;
    if (has_any(t, no_js))
        return PERR_NO_JS_RUNTIME;
    if (has_any(t, not_live))
        return PERR_NOT_LIVE;
    if (has_any(t, blocked))
        return PERR_BLOCKED;
    if (has_any(t, bad_channel))
        return PERR_BAD_CHANNEL;
    if (has_any(t, offline))
        return PERR_OFFLINE;
    return PERR_PLAYBACK;
}
