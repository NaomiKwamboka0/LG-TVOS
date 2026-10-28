/* Stand-in player: no network, no decoding, fully repeatable. */
#include "player_impl.h"

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
    uint32_t frame;
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
    s->now_ms = now_ms;
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
    if (p->st.state != PLAYER_PLAYING && p->st.state != PLAYER_BUFFERING)
        return 0;
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
    return 1;
}

static const player_ops STUB_OPS = {
    stub_destroy, stub_open, stub_stop, stub_set_volume, stub_pump, stub_render
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
