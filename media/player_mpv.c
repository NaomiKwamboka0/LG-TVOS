/* Live channels through libmpv. mpv's ytdl hook asks yt-dlp (helped by
 * deno) for the stream behind a channel's YouTube "live" address; mpv
 * downloads, decodes and plays the sound itself; pictures come back
 * through the software render API straight into our 1280x720 buffer, so
 * the TV overlays draw on top exactly as in the screenshots. */
#include "player_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpv/client.h>
#include <mpv/render.h>

#define LOAD_TIMEOUT_MS 45000
#define CH_LINE 400
#define LOG_KEEP 6000 /* recent messages, for player_classify */

enum { OBS_CACHE_PAUSE = 1, OBS_VIDEO_H };
#define PROBE_REPLY_BASE 1000 /* reply_userdata of probes: base + tag */

typedef struct {
    player base;
    player_config cfg;
    mpv_handle *mpv;
    mpv_render_context *render;
    FILE *log;
    char recent[LOG_KEEP]; /* everything */
    size_t recent_len;
    char ytdl[LOG_KEEP];   /* yt-dlp's own words: these explain a failure best */
    size_t ytdl_len;
    uint64_t now_ms, opened_ms;
    int opening;       /* set by open, cleared by the first pump */
    int awaiting_start; /* old file may still report its end */
    int64_t entry;     /* playlist entry of the channel we asked for */
    int have_frame;
    int paused_for_cache;
} mpv_player;

static void wake(void *ctx)
{
    mpv_player *m = (mpv_player *)ctx;
    if (m->cfg.wakeup)
        m->cfg.wakeup(m->cfg.wakeup_ctx);
}

static void append(char *buf, size_t *len, const char *prefix, const char *text)
{
    size_t n = strlen(prefix) + strlen(text) + 3;
    if (n >= LOG_KEEP)
        return;
    if (*len + n >= LOG_KEEP) { /* drop the oldest half */
        size_t half = *len / 2;
        memmove(buf, buf + half, *len - half);
        *len -= half;
    }
    *len += (size_t)snprintf(buf + *len, LOG_KEEP - *len, "[%s] %s", prefix, text);
}

static void keep_message(mpv_player *m, const char *prefix, const char *text)
{
    if (m->log) {
        fprintf(m->log, "[%s] %s", prefix, text);
        fflush(m->log);
    }
    if (m->cfg.verbose)
        fprintf(stderr, "[%s] %s", prefix, text);
    append(m->recent, &m->recent_len, prefix, text);
    if (!strcmp(prefix, "ytdl_hook"))
        append(m->ytdl, &m->ytdl_len, prefix, text);
}

/* When yt-dlp fails, mpv retries the address itself and reports a 404 for
 * the YouTube page, so yt-dlp's messages decide first. */
static player_error classify(const mpv_player *m)
{
    player_error e = m->ytdl_len ? player_classify(m->ytdl) : PERR_PLAYBACK;
    return e != PERR_PLAYBACK ? e : player_classify(m->recent);
}

static void fail(mpv_player *m, player_error e)
{
    const char *tail = m->recent_len > 150 ? m->recent + m->recent_len - 150 : m->recent;
    mpv_command_string(m->mpv, "stop");
    m->have_frame = 0;
    player_fail(&m->base, e, tail);
}

static void mpv_destroy_player(player *p)
{
    mpv_player *m = (mpv_player *)p;
    if (m->render)
        mpv_render_context_free(m->render); /* before the core, as libmpv requires */
    if (m->mpv)
        mpv_terminate_destroy(m->mpv);       /* also ends any running yt-dlp */
    if (m->log)
        fclose(m->log);
    free(m);
}

static void mpv_open(player *p, const char *url)
{
    mpv_player *m = (mpv_player *)p;
    const char *cmd[] = {"loadfile", url, "replace", NULL};
    char format[160];
    player_format(p->max_height, format, sizeof format);
    mpv_set_property_string(m->mpv, "ytdl-format", format);
    m->recent_len = m->ytdl_len = 0;
    m->recent[0] = m->ytdl[0] = '\0';
    m->have_frame = 0;
    m->paused_for_cache = 0;
    m->opening = 1;
    m->awaiting_start = 1;
    p->st.state = PLAYER_LOADING;
    {
        char line[CH_LINE];
        snprintf(line, sizeof line, "open %s (%s)\n", url, format);
        keep_message(m, "familytv", line);
    }
    if (mpv_command(m->mpv, cmd) < 0)
        fail(m, PERR_PLAYBACK);
}

static void mpv_stop(player *p)
{
    mpv_player *m = (mpv_player *)p;
    mpv_command_string(m->mpv, "stop");
    m->have_frame = 0;
    m->opening = 0;
    m->awaiting_start = 0;
}

static void mpv_volume(player *p, int volume)
{
    double v = volume;
    mpv_set_property(((mpv_player *)p)->mpv, "volume", MPV_FORMAT_DOUBLE, &v);
}

static void on_end(mpv_player *m, const mpv_event_end_file *e)
{
    player *p = &m->base;
    if (m->awaiting_start || e->playlist_entry_id != m->entry || p->st.state == PLAYER_IDLE)
        return; /* the previous channel finishing, or our own stop */
    if (e->reason == MPV_END_FILE_REASON_ERROR) {
        fail(m, classify(m));
    } else if (e->reason == MPV_END_FILE_REASON_EOF) {
        /* A live stream that ends has stopped broadcasting. */
        player_error kind = classify(m);
        fail(m, kind == PERR_PLAYBACK ? PERR_NOT_LIVE : kind);
    }
}

/* yt-dlp through mpv's own "subprocess" command: no process code of our
 * own, and on Windows mpv starts it without a console window. */
static int mpv_probe(player *p, int tag, const char *page_url, const char *print_format, int items)
{
    mpv_player *m = (mpv_player *)p;
    char range[16];
    const char *argv[10];
    mpv_node args[10], vals[4], cmd;
    mpv_node_list arg_list, map;
    char *keys[4] = {"name", "args", "capture_stdout", "playback_only"};
    int n = 0, i;
    snprintf(range, sizeof range, "1:%d", items);
    argv[n++] = m->cfg.ytdl_path ? m->cfg.ytdl_path : "yt-dlp";
    argv[n++] = "--ignore-config";
    argv[n++] = "--flat-playlist";
    argv[n++] = "--playlist-items";
    argv[n++] = range;
    argv[n++] = "--socket-timeout";
    argv[n++] = "15";
    argv[n++] = "--print";
    argv[n++] = print_format;
    argv[n++] = page_url;
    for (i = 0; i < n; i++) {
        args[i].format = MPV_FORMAT_STRING;
        args[i].u.string = (char *)argv[i];
    }
    arg_list.num = n;
    arg_list.values = args;
    arg_list.keys = NULL;
    vals[0].format = MPV_FORMAT_STRING;
    vals[0].u.string = "subprocess";
    vals[1].format = MPV_FORMAT_NODE_ARRAY;
    vals[1].u.list = &arg_list;
    vals[2].format = MPV_FORMAT_FLAG;
    vals[2].u.flag = 1;
    vals[3].format = MPV_FORMAT_FLAG;
    vals[3].u.flag = 0;
    map.num = 4;
    map.values = vals;
    map.keys = keys;
    cmd.format = MPV_FORMAT_NODE_MAP;
    cmd.u.list = &map;
    return mpv_command_node_async(m->mpv, PROBE_REPLY_BASE + (uint64_t)tag, &cmd) < 0 ? -1 : 0;
}

static void on_probe_reply(mpv_player *m, mpv_event *e)
{
    int tag = (int)(e->reply_userdata - PROBE_REPLY_BASE);
    mpv_node *res = &((mpv_event_command *)e->data)->result;
    int i;
    if (e->error >= 0 && res->format == MPV_FORMAT_NODE_MAP)
        for (i = 0; i < res->u.list->num; i++) {
            mpv_node *v = &res->u.list->values[i];
            if (strcmp(res->u.list->keys[i], "stdout") != 0)
                continue;
            if (v->format == MPV_FORMAT_BYTE_ARRAY) {
                player_probe_done(&m->base, tag, v->u.ba->size > 0, (const char *)v->u.ba->data, v->u.ba->size);
                return;
            }
            if (v->format == MPV_FORMAT_STRING) {
                player_probe_done(&m->base, tag, v->u.string[0] != '\0', v->u.string, strlen(v->u.string));
                return;
            }
        }
    player_probe_done(&m->base, tag, 0, "", 0);
}

static void mpv_pump(player *p, uint64_t now_ms)
{
    mpv_player *m = (mpv_player *)p;
    m->now_ms = now_ms;
    if (m->opening) {
        m->opening = 0;
        m->opened_ms = now_ms;
    }
    for (;;) {
        mpv_event *e = mpv_wait_event(m->mpv, 0);
        if (e->event_id == MPV_EVENT_NONE)
            break;
        switch (e->event_id) {
        case MPV_EVENT_LOG_MESSAGE: {
            mpv_event_log_message *msg = (mpv_event_log_message *)e->data;
            keep_message(m, msg->prefix, msg->text);
            break;
        }
        case MPV_EVENT_START_FILE:
            m->entry = ((mpv_event_start_file *)e->data)->playlist_entry_id;
            m->awaiting_start = 0;
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            if (p->st.state == PLAYER_LOADING || p->st.state == PLAYER_BUFFERING)
                p->st.state = m->paused_for_cache ? PLAYER_BUFFERING : PLAYER_PLAYING;
            break;
        case MPV_EVENT_END_FILE:
            on_end(m, (mpv_event_end_file *)e->data);
            break;
        case MPV_EVENT_COMMAND_REPLY:
            if (e->reply_userdata >= PROBE_REPLY_BASE)
                on_probe_reply(m, e);
            break;
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = (mpv_event_property *)e->data;
            if (e->reply_userdata == OBS_CACHE_PAUSE && prop->format == MPV_FORMAT_FLAG) {
                m->paused_for_cache = *(int *)prop->data;
                if (p->st.state == PLAYER_PLAYING && m->paused_for_cache)
                    p->st.state = PLAYER_BUFFERING;
                else if (p->st.state == PLAYER_BUFFERING && !m->paused_for_cache)
                    p->st.state = PLAYER_PLAYING;
            } else if (e->reply_userdata == OBS_VIDEO_H) {
                p->st.video_h = prop->format == MPV_FORMAT_INT64 ? (int)*(int64_t *)prop->data : 0;
            }
            break;
        }
        default:
            break;
        }
    }
    if (p->st.state == PLAYER_LOADING && !m->opening && now_ms - m->opened_ms > LOAD_TIMEOUT_MS) {
        player_error kind = classify(m);
        fail(m, kind == PERR_PLAYBACK ? PERR_TIMEOUT : kind);
    }
}

static int mpv_render(player *p, uint32_t *px, int w, int h, int stride)
{
    mpv_player *m = (mpv_player *)p;
    int fresh = 0;
    if (p->st.state != PLAYER_PLAYING && p->st.state != PLAYER_BUFFERING) {
        m->have_frame = 0;
        return 0;
    }
    if (mpv_render_context_update(m->render) & MPV_RENDER_UPDATE_FRAME) {
        int size[2];
        size_t bytes = (size_t)stride * 4;
        long i, n = (long)stride * h;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_SW_SIZE, size},
            {MPV_RENDER_PARAM_SW_FORMAT, (void *)"bgr0"}, /* 0xAARRGGBB words, little-endian */
            {MPV_RENDER_PARAM_SW_STRIDE, &bytes},
            {MPV_RENDER_PARAM_SW_POINTER, px},
            {MPV_RENDER_PARAM_INVALID, NULL},
        };
        size[0] = w;
        size[1] = h;
        if (mpv_render_context_render(m->render, params) >= 0) {
            for (i = 0; i < n; i++) /* the "0" byte is undefined; make it opaque */
                px[i] |= 0xFF000000u;
            m->have_frame = 1;
            fresh = 1;
        }
    }
    return fresh ? PLAYER_NEW_PICTURE : m->have_frame ? PLAYER_SAME_PICTURE : 0;
}

static const player_ops MPV_OPS = {mpv_destroy_player, mpv_open, mpv_stop, mpv_volume, mpv_pump, mpv_render,
                                   mpv_probe};

static void set(mpv_handle *mpv, const char *name, const char *value)
{
    if (mpv_set_option_string(mpv, name, value) < 0)
        fprintf(stderr, "familytv: mpv option %s=%s not accepted\n", name, value);
}

/* Key/value list options as a map, so paths (C:\..., commas) need no escaping. */
static void set_map(mpv_handle *mpv, const char *name, char **keys, const char **values, int n)
{
    mpv_node items[4], map;
    mpv_node_list list;
    int i;
    for (i = 0; i < n && i < 4; i++) {
        items[i].format = MPV_FORMAT_STRING;
        items[i].u.string = (char *)values[i];
    }
    list.num = n < 4 ? n : 4;
    list.values = items;
    list.keys = keys;
    map.format = MPV_FORMAT_NODE_MAP;
    map.u.list = &list;
    if (mpv_set_option(mpv, name, MPV_FORMAT_NODE, &map) < 0)
        fprintf(stderr, "familytv: mpv option %s not accepted\n", name);
}

player *player_create_mpv(const player_config *cfg)
{
    mpv_player *m = (mpv_player *)calloc(1, sizeof *m);
    char vol[16], js[600];
    char *script_keys[1] = {"ytdl_hook-ytdl_path"};
    char *raw_keys[2] = {"ignore-config", "js-runtimes"};
    const char *raw_values[2], *script_values[1];
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_SW},
        {MPV_RENDER_PARAM_INVALID, NULL},
    };
    if (!m)
        return NULL;
    m->base.ops = &MPV_OPS;
    m->base.max_height = cfg->max_height;
    m->base.st.volume = cfg->volume;
    m->cfg = *cfg;
    m->mpv = mpv_create();
    if (!m->mpv) {
        free(m);
        return NULL;
    }
    if (cfg->log_path)
        m->log = fopen(cfg->log_path, "w");

    set(m->mpv, "vo", "libmpv");
    set(m->mpv, "ytdl", "yes"); /* libmpv turns the YouTube hook off by default */
    script_values[0] = cfg->ytdl_path;
    if (cfg->ytdl_path)
        set_map(m->mpv, "script-opts", script_keys, script_values, 1);
    /* yt-dlp: ignore any personal config; find deno next to the program. */
    raw_values[0] = "";
    snprintf(js, sizeof js, "deno:%s", cfg->js_dir ? cfg->js_dir : "");
    raw_values[1] = js;
    set_map(m->mpv, "ytdl-raw-options", raw_keys, raw_values, cfg->js_dir ? 2 : 1);
    set(m->mpv, "hwdec", "auto-copy");      /* decode on the GPU where it can, copy back */
    set(m->mpv, "zimg-scaler", "bilinear"); /* scaling runs on the CPU: keep it cheap */
    set(m->mpv, "sws-scaler", "fast-bilinear");
    set(m->mpv, "osd-level", "0");
    set(m->mpv, "network-timeout", "20");
    set(m->mpv, "demuxer-max-bytes", "32MiB");
    set(m->mpv, "volume-max", "100");
    snprintf(vol, sizeof vol, "%d", cfg->volume);
    set(m->mpv, "volume", vol);
    set(m->mpv, "audio-client-name", "Family TV");
    set(m->mpv, "keep-open", "no");
    /* Hand each picture over when it is due instead of rendering early and
     * blocking our thread (and the remote keys) until then. */
    set(m->mpv, "video-timing-offset", "0");
    {
        /* For diagnosing a laptop: FAMILYTV_MPV="hwdec=no;zimg-scaler=point" */
        const char *extra = getenv("FAMILYTV_MPV");
        char opt[256], *eq, *next, *cur = opt;
        snprintf(opt, sizeof opt, "%s", extra ? extra : "");
        for (; cur && *cur; cur = next) {
            next = strchr(cur, ';');
            if (next)
                *next++ = '\0';
            eq = strchr(cur, '=');
            if (eq) {
                *eq = '\0';
                set(m->mpv, cur, eq + 1);
            }
        }
    }

    if (mpv_initialize(m->mpv) < 0 ||
        mpv_render_context_create(&m->render, m->mpv, params) < 0) {
        mpv_destroy_player(&m->base);
        return NULL;
    }
    mpv_request_log_messages(m->mpv, "info");
    mpv_observe_property(m->mpv, OBS_CACHE_PAUSE, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(m->mpv, OBS_VIDEO_H, "video-params/h", MPV_FORMAT_INT64);
    mpv_set_wakeup_callback(m->mpv, wake, m);
    mpv_render_context_set_update_callback(m->render, wake, m);
    return &m->base;
}
