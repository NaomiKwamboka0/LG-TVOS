/* Stand-in player: no network, no decoding, fully repeatable. */
#include "player_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOADING_MS 1500
#define STUB_PREFIX "https://stub.invalid/"

typedef struct {
    player base;
    player_config cfg;
    uint64_t now_ms, opened_ms;
    int opening; /* waiting for the first pump after open */
    player_error fail_with;
    uint32_t seed; /* differs per address, so channels look different */
    int unavailable;
#define STUB_PROBES 16
    int pending[STUB_PROBES], npending;
    char pending_out[STUB_PROBES][256];
    uint32_t frame, drawn; /* drawn: frame + 1 last put in px, 0 = none */
} stub_player;

static void wake(stub_player *s)
{
    if (s->cfg.wakeup)
        s->cfg.wakeup(s->cfg.wakeup_ctx);
}

static void stub_destroy(player *p)
{
    free(p);
}

static void stub_open(player *p, const char *url)
{
    stub_player *s = (stub_player *)p;
    int e;
    p->st.state = PLAYER_LOADING;
    s->drawn = 0;
    s->opening = 1;
    s->fail_with = PERR_NONE;
    s->seed = 2166136261u;
    for (e = 0; url[e]; e++)
        s->seed = (s->seed ^ (unsigned char)url[e]) * 16777619u;
    if (strncmp(url, STUB_PREFIX, strlen(STUB_PREFIX)) == 0)
        for (e = PERR_OFFLINE; e < PERR_COUNT; e++)
            if (strcmp(url + strlen(STUB_PREFIX), player_error_name((player_error)e)) == 0)
                s->fail_with = (player_error)e;
    if (s->unavailable) {
        player_fail(p, PERR_NO_ENGINE, "built without libmpv, or libmpv failed to start");
        s->opening = 0;
    }
    wake(s);
}

static void stub_stop(player *p)
{
    ((stub_player *)p)->opening = 0;
}

static void stub_set_volume(player *p, int volume)
{
    (void)p;
    (void)volume;
}

static void stub_pump(player *p, uint64_t now_ms)
{
    stub_player *s = (stub_player *)p;
    int i;
    s->now_ms = now_ms;
    for (i = 0; i < s->npending; i++)
        player_probe_done(p, s->pending[i], 1, s->pending_out[i], strlen(s->pending_out[i]));
    s->npending = 0;
    if (s->opening) {
        s->opening = 0;
        s->opened_ms = now_ms;
    }
    if (p->st.state == PLAYER_LOADING && now_ms - s->opened_ms >= LOADING_MS) {
        if (s->fail_with != PERR_NONE) {
            player_fail(p, s->fail_with, "stub");
        } else {
            p->st.state = PLAYER_PLAYING;
            p->st.video_h = p->max_height;
        }
    }
    if (p->st.state == PLAYER_PLAYING)
        s->frame = (uint32_t)(now_ms / 40);
}

/* Colour bars in the channel's own order, with a bar sweeping across so
 * a frozen picture is obvious. */
static int stub_render(player *p, uint32_t *px, int w, int h, int stride)
{
    static const uint32_t BARS[7] = {
        0xFFBFBFBFu, 0xFFBFBF00u, 0xFF00BFBFu, 0xFF00BF00u, 0xFFBF00BFu, 0xFFBF0000u, 0xFF0000BFu
    };
    stub_player *s = (stub_player *)p;
    int x, y, sweep;
    if (p->st.state != PLAYER_PLAYING && p->st.state != PLAYER_BUFFERING) {
        s->drawn = 0;
        return 0;
    }
    if (s->drawn == s->frame + 1)
        return PLAYER_SAME_PICTURE;
    sweep = (int)(s->frame * 8u % (uint32_t)w);
    for (y = 0; y < h; y++) {
        uint32_t *row = px + (long)y * stride;
        for (x = 0; x < w; x++) {
            uint32_t c = BARS[(x * 7 / w + s->seed) % 7];
            if (y > h * 3 / 4)
                c = 0xFF101010u + (uint32_t)(x * 0x60 / w) * 0x010101u;
            if (x >= sweep && x < sweep + 6)
                c = 0xFFFFFFFFu;
            row[x] = c;
        }
    }
    s->drawn = s->frame + 1;
    return PLAYER_NEW_PICTURE;
}

/* Repeatable answers: tag 0 is live, tag 1 has a match coming up, the
 * rest stream nothing right now. They arrive on the next pump. */
static int stub_probe(player *p, int tag, const char *page_url, const char *print_format, int items)
{
    stub_player *s = (stub_player *)p;
    char out[256];
    (void)page_url;
    (void)print_format;
    (void)items;
    if (s->unavailable)
        return -1;
    if (tag == 0)
        snprintf(out, sizeof out, "is_live\tstubLive%02d\tLIVE | Home v Away | Stub Cup Final\n", tag);
    else if (tag == 1)
        snprintf(out, sizeof out, "is_upcoming\tstubNext%02d\tSemi-final | Stub League\n", tag);
    else
        snprintf(out, sizeof out, "was_live\tstubOld%02d\tLast week's game\n", tag);
    if (s->npending < STUB_PROBES)
        s->pending[s->npending++] = tag;
    snprintf(s->pending_out[s->npending - 1], sizeof s->pending_out[0], "%s", out);
    wake(s);
    return 0;
}

static const player_ops STUB_OPS = {
    stub_destroy, stub_open, stub_stop, stub_set_volume, stub_pump, stub_render, stub_probe
};

player *player_create_stub(const player_config *cfg)
{
    stub_player *s = (stub_player *)calloc(1, sizeof *s);
    if (!s)
        return NULL;
    s->base.ops = &STUB_OPS;
    s->cfg = *cfg;
    s->base.max_height = cfg->max_height;
    s->base.st.volume = cfg->volume;
    return &s->base;
}

player *player_create_unavailable(const player_config *cfg)
{
    player *p = player_create_stub(cfg);
    if (p)
        ((stub_player *)p)->unavailable = 1;
    return p;
}

#ifndef FT_HAVE_MPV
player *player_create_mpv(const player_config *cfg)
{
    (void)cfg;
    return NULL;
}
#endif
