/* Player interface: the stub's states, and reading engine messages. */
#include <stdint.h>
#include <string.h>

#include "player.h"

#include "check.h"

static int wakes;
static void count_wake(void *ctx)
{
    (void)ctx;
    wakes++;
}

static player *make_stub(void)
{
    player_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.max_height = 480;
    cfg.volume = 60;
    cfg.wakeup = count_wake;
    return player_create_stub(&cfg);
}

static void test_stub_loads_then_plays(void)
{
    static uint32_t px[64 * 36];
    player *p = make_stub();
    CHECK(p != NULL);
    if (!p)
        return;
    CHECK(player_get(p)->state == PLAYER_IDLE && player_get(p)->volume == 60);
    CHECK(player_render(p, px, 64, 36, 64) == 0);
    wakes = 0;
    player_open(p, "https://www.youtube.com/@citizentvkenya/live");
    CHECK(wakes == 1 && player_get(p)->state == PLAYER_LOADING);
    player_pump(p, 10000);
    player_pump(p, 11000);
    CHECK(player_get(p)->state == PLAYER_LOADING);
    player_pump(p, 11500);
    CHECK(player_get(p)->state == PLAYER_PLAYING && player_get(p)->video_h == 480);
    px[0] = 0;
    CHECK(player_render(p, px, 64, 36, 64) == PLAYER_NEW_PICTURE && px[0] >> 24 == 0xFF);
    CHECK(player_render(p, px, 64, 36, 64) == PLAYER_SAME_PICTURE); /* nothing new yet */
    player_pump(p, 11600);
    CHECK(player_render(p, px, 64, 36, 64) == PLAYER_NEW_PICTURE);
    player_set_volume(p, 130);
    CHECK(player_get(p)->volume == 100);
    player_stop(p);
    CHECK(player_get(p)->state == PLAYER_IDLE && player_render(p, px, 64, 36, 64) == 0);
    player_destroy(p);
}

static void test_stub_error_addresses(void)
{
    player *p = make_stub();
    int e;
    if (!p)
        return;
    for (e = PERR_OFFLINE; e < PERR_COUNT; e++) {
        char url[64] = "https://stub.invalid/";
        strcat(url, player_error_name((player_error)e));
        player_open(p, url);
        player_pump(p, 0);
        player_pump(p, 2000);
        CHECK(player_get(p)->state == PLAYER_ERROR && player_get(p)->error == (player_error)e);
    }
    player_open(p, "https://stub.invalid/no-such-error"); /* unknown name: plays */
    player_pump(p, 0);
    player_pump(p, 2000);
    CHECK(player_get(p)->state == PLAYER_PLAYING && player_get(p)->error == PERR_NONE);
    player_destroy(p);
}

static void test_classify_messages(void)
{
    CHECK(player_classify("[ytdl_hook] youtube-dl failed: not found or not enough permissions") == PERR_NO_RESOLVER);
    CHECK(player_classify("WARNING: [youtube] No supported JavaScript runtime could be found.") == PERR_NO_JS_RUNTIME);
    /* Captured from yt-dlp 2026.10.26 on 2026-10-28. */
    CHECK(player_classify("ERROR: [youtube:tab] @KBCChannel1: The channel is not currently live") == PERR_NOT_LIVE);
    CHECK(player_classify("ERROR: [youtube:tab] @ntvkenya/live: Unable to download API page: HTTP Error 404: "
                          "Not Found (caused by <HTTPError 404: Not Found>)") == PERR_BAD_CHANNEL);
    CHECK(player_classify("ERROR: [youtube] abc: This live event will begin in 3 hours.") == PERR_NOT_LIVE);
    CHECK(player_classify("ERROR: [youtube] abc: Sign in to confirm you're not a bot.") == PERR_BLOCKED);
    CHECK(player_classify("ERROR: Unable to download webpage: HTTP Error 429: Too Many Requests") == PERR_BLOCKED);
    CHECK(player_classify("ERROR: Unable to download webpage: HTTP Error 404: Not Found") == PERR_BAD_CHANNEL);
    CHECK(player_classify("ERROR: [youtube] abc: Video unavailable") == PERR_BAD_CHANNEL);
    CHECK(player_classify("ERROR: Unable to download webpage: <urlopen error [Errno 8] nodename nor servname provided, or not known>") == PERR_OFFLINE);
    CHECK(player_classify("[ffmpeg] tcp: Failed to resolve hostname www.youtube.com: Temporary failure in name resolution") == PERR_OFFLINE);
    CHECK(player_classify("Failed to recognize file format.") == PERR_PLAYBACK);
    /* mpv with libmpv 2.5, 2026-10-28: yt-dlp missing, then mpv's own retry 404s. */
    CHECK(player_classify("[ytdl_hook] Subprocess failed: init\n"
                          "[ytdl_hook] youtube-dl failed: not found or not enough permissions\n"
                          "[ffmpeg] https: HTTP error 404 Not Found\n") == PERR_NO_RESOLVER);
    /* No way out to the internet (a dead proxy stood in for Wi-Fi off). */
    CHECK(player_classify("[ytdl_hook] ERROR: [youtube:tab] @ntvkenya/live: Unable to download API page: "
                          "('Unable to connect to proxy', NewConnectionError(\"HTTPSConnection(host='127.0.0.1', "
                          "port=9): Failed to establish a new connection: [Errno 61] Connection refused\"))\n"
                          "[ytdl_hook] youtube-dl failed: unexpected error occurred\n") == PERR_OFFLINE);
    CHECK(player_classify("") == PERR_PLAYBACK && player_classify(NULL) == PERR_PLAYBACK);
}

static void test_format_never_upscales_data(void)
{
    char f[160];
    player_format(360, f, sizeof f);
    CHECK(strcmp(f, "bv*[height<=?360][fps<=?30]+ba/b[height<=?360][fps<=?30]/"
                    "bv*[height<=?360]+ba/b[height<=?360]/wv*+ba/w") == 0);
    player_format(480, f, sizeof f);
    CHECK(strstr(f, "height<=?480") != NULL && strstr(f, "720") == NULL);
    CHECK(strcmp(f + strlen(f) - 9, "/wv*+ba/w") == 0); /* never "best" as the last resort */
    player_format(720, f, sizeof f);
    CHECK(strcmp(f, "bv*[height<=?720]+ba/b[height<=?720]/wv*+ba/w") == 0);
}

void run_player_tests(void)
{
    test_stub_loads_then_plays();
    test_stub_error_addresses();
    test_classify_messages();
    test_format_never_upscales_data();
}
