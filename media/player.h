#ifndef FT_PLAYER_H
#define FT_PLAYER_H

/* Live stream player. The real one plays through libmpv, with yt-dlp
 * turning a channel's YouTube address into a stream; the stub stands in
 * for tests, screenshots and computers without libmpv. Call everything
 * from the main thread; only cfg.wakeup may run on another thread. */

#include <stdint.h>

typedef enum { PLAYER_IDLE, PLAYER_LOADING, PLAYER_PLAYING, PLAYER_BUFFERING, PLAYER_ERROR } player_state;

typedef enum {
    PERR_NONE,
    PERR_OFFLINE,       /* no internet connection */
    PERR_NOT_LIVE,      /* the channel is not streaming right now */
    PERR_NO_RESOLVER,   /* yt-dlp is missing */
    PERR_NO_JS_RUNTIME, /* yt-dlp cannot find deno */
    PERR_BLOCKED,       /* YouTube bot check or too many requests */
    PERR_BAD_CHANNEL,   /* wrong address, or the channel is gone */
    PERR_TIMEOUT,       /* nothing arrived in time */
    PERR_PLAYBACK,      /* anything else */
    PERR_NO_ENGINE,     /* built without libmpv */
    PERR_COUNT
} player_error;

typedef struct {
    player_state state;
    player_error error;
    char detail[160]; /* ASCII; for the log, never shown as the main message */
    int volume;       /* 0-100 */
    int video_h;      /* picture height being played; 0 = not known yet */
} player_status;

typedef struct {
    const char *ytdl_path; /* yt-dlp program; NULL = search PATH */
    const char *js_dir;    /* folder holding deno; NULL = search PATH */
    const char *log_path;  /* engine log file; NULL = none */
    int max_height;        /* 360, 480 or 720 */
    int volume;            /* 0-100 */
    int verbose;           /* also print engine messages to stderr */
    /* Called (possibly from another thread) when player_pump or
     * player_render has something new. Must be thread-safe. */
    void (*wakeup)(void *ctx);
    void *wakeup_ctx;
} player_config;

typedef struct player player;

/* Loads after LOADING_MS, then plays a moving test card. Addresses of the
 * form https://stub.invalid/<error name> fail with that error instead. */
player *player_create_stub(const player_config *cfg);
/* NULL when built without libmpv or when libmpv cannot start. */
player *player_create_mpv(const player_config *cfg);
/* Fails every channel with PERR_NO_ENGINE, so the family sees why. */
player *player_create_unavailable(const player_config *cfg);
void player_destroy(player *p);

/* Starts a channel (stopping any other). Returns at once; watch the state. */
void player_open(player *p, const char *url);
/* Stops playback and network use. */
void player_stop(player *p);
void player_set_volume(player *p, int volume);
/* Applies from the next player_open. */
void player_set_max_height(player *p, int height);
/* Handles pending engine events. now_ms: the caller's monotonic clock. */
void player_pump(player *p, uint64_t now_ms);
const player_status *player_get(const player *p);
/* Draws the newest picture into px (0xAARRGGBB, w x h, stride in pixels).
 * Returns 1 if px now holds a picture, 0 if there is nothing to show. */
int player_render(player *p, uint32_t *px, int w, int h, int stride);

/* Reads engine and yt-dlp messages and says what went wrong. */
player_error player_classify(const char *log_text);
/* Short stable name, e.g. "not-live" (stub addresses, logs). */
const char *player_error_name(player_error e);

/* yt-dlp format choice for a height cap: at most 30 frames a second below
 * 720p, and the smallest stream rather than the largest when nothing fits. */
void player_format(int max_height, char *out, unsigned n);

#endif
