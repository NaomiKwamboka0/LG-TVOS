/* Channel line-up parsing: what a parent might type into channels.txt. */
#include <stdio.h>
#include <string.h>

#include "channels.h"
#include "sports.h"
#include "youtube.h"

#include "check.h"

static int parse(channel_list *l, const char *text, char *warn, size_t n)
{
    return channels_parse(l, text, strlen(text), warn, n);
}

static void test_channels_basic_and_sorted(void)
{
    channel_list l;
    char warn[160];
    CHECK(parse(&l, "# note\n\n2|NTV|https://www.youtube.com/@ntv/live|#c62828\n"
                    " 1 | Citizen TV | https://www.youtube.com/@citizen/live \n", warn, sizeof warn) == 2);
    CHECK(warn[0] == '\0');
    CHECK(l.ch[0].number == 1 && strcmp(l.ch[0].name, "Citizen TV") == 0);
    CHECK(strcmp(l.ch[0].url, "https://www.youtube.com/@citizen/live") == 0);
    CHECK(l.ch[0].accent >> 24 == 0xFF); /* palette colour when none given */
    CHECK(l.ch[1].number == 2 && l.ch[1].accent == 0xFFC62828u);
    CHECK(channels_find_number(&l, 2) == 1 && channels_find_number(&l, 3) == -1);
}

static void test_channels_notepad_bom_and_crlf(void)
{
    channel_list l;
    char warn[160];
    CHECK(parse(&l, "\xEF\xBB\xBF" "1|KBC|https://youtube.com/@kbc/live|#0D47A1\r\n2|K24|https://youtube.com/@k24/live\r\n",
                warn, sizeof warn) == 2);
    CHECK(warn[0] == '\0');
    CHECK(strcmp(l.ch[0].url, "https://youtube.com/@kbc/live") == 0);
    CHECK(l.ch[0].accent == 0xFF0D47A1u);
    CHECK(strcmp(l.ch[1].url, "https://youtube.com/@k24/live") == 0);
}

static void test_channels_bad_lines_skipped(void)
{
    channel_list l;
    char warn[160];
    CHECK(parse(&l, "1|A|https://a.example/live\n"
                    "1|Dup|https://b.example/live\n"            /* duplicate number */
                    "x|Bad|https://c.example/live\n"            /* not a number */
                    "1000|Big|https://d.example/live\n"         /* out of range */
                    "4|File|file:///etc/passwd\n"               /* not http(s) */
                    "5|Opt|--ytdl-raw-options=exec=x\n"         /* option, not an address */
                    "6|Space|https://e.example/a b\n"           /* space in address */
                    "7||https://f.example/live\n"               /* empty name */
                    "8|Colour|https://g.example/live|red\n"     /* bad colour */
                    "9|Missing\n"                               /* too few fields */
                    "10|Ok|https://h.example/live|#00FF00\n",
                warn, sizeof warn) == 2);
    CHECK(l.ch[0].number == 1 && strcmp(l.ch[0].name, "A") == 0);
    CHECK(l.ch[1].number == 10);
    CHECK(strstr(warn, "line 2") != NULL);
    CHECK(strstr(warn, "9 lines skipped") != NULL);
}

static void test_channels_names_ascii_only(void)
{
    channel_list l;
    char warn[160];
    CHECK(parse(&l, "1|Caf\xC3\xA9 TV|https://a.example/live\n"
                    "2|A name that is far too long for the cards here|https://b.example/live\n", warn, sizeof warn) == 2);
    CHECK(strcmp(l.ch[0].name, "Caf? TV") == 0);
    CHECK(strlen(l.ch[1].name) == CH_NAME_MAX - 1);
    CHECK(strstr(warn, "line 1") != NULL);
}

static void test_channels_limit_and_empty(void)
{
    static char text[CH_MAX * 64 + 128];
    channel_list l;
    char warn[160];
    size_t n = 0;
    int i;
    for (i = 1; i <= CH_MAX + 1; i++)
        n += (size_t)snprintf(text + n, sizeof text - n, "%d|C%d|https://x.example/%d\n", i, i, i);
    CHECK(channels_parse(&l, text, n, warn, sizeof warn) == CH_MAX);
    CHECK(strstr(warn, "too many") != NULL);
    CHECK(parse(&l, "# only notes\n", warn, sizeof warn) == 0);
    CHECK(parse(&l, "", NULL, 0) == 0);
}

static void test_channels_load_order_and_fallback(void)
{
    const char *good = TMP "/ch-good.txt", *bad = TMP "/ch-bad.txt";
    const char *paths[3];
    channel_list l, d;
    char warn[160];
    FILE *f = fopen(good, "wb");
    if (f) {
        fputs("7|Mine|https://a.example/live\n", f);
        fclose(f);
    }
    f = fopen(bad, "wb");
    if (f) {
        fputs("not a channel\n", f);
        fclose(f);
    }
    paths[0] = TMP "/ch-missing.txt";
    paths[1] = bad;
    paths[2] = good;
    CHECK(channels_load(&l, paths, 3, warn, sizeof warn) == 2);
    CHECK(l.n == 1 && l.ch[0].number == 7);
    CHECK(channels_load(&l, paths, 2, warn, sizeof warn) == -1);
    channels_defaults(&d);
    CHECK(l.n == d.n && memcmp(&l, &d, sizeof l) == 0);
    CHECK(strstr(warn, "built-in") != NULL);
    remove(good);
    remove(bad);
}

/* The shipped channels.txt and the built-in list must not drift apart. */
static void test_channels_asset_matches_defaults(void)
{
    const char *paths[1] = {"assets/channels.txt"};
    channel_list a, d;
    char warn[160];
    CHECK(channels_load(&a, paths, 1, warn, sizeof warn) == 0);
    CHECK(warn[0] == '\0');
    channels_defaults(&d);
    CHECK(d.n > 0 && memcmp(&a, &d, sizeof a) == 0);
}

static void test_channels_groups(void)
{
    channel_list all, tv, sports;
    char warn[160];
    CHECK(parse(&all, "1|News|https://a.example/live\n"
                      "101|Rugby|https://b.example/live|#00FF00|sports\n"
                      "2|Odd|https://c.example/live||football\n", warn, sizeof warn) == 2);
    CHECK(strstr(warn, "line 3") != NULL);
    channels_group(&all, CH_TV, &tv);
    channels_group(&all, CH_SPORTS, &sports);
    CHECK(tv.n == 1 && tv.ch[0].number == 1);
    CHECK(sports.n == 1 && sports.ch[0].number == 101 && sports.ch[0].accent == 0xFF00FF00u);
    channels_defaults(&all);
    channels_group(&all, CH_SPORTS, &sports);
    CHECK(sports.n >= 10 && sports.ch[0].number == 101);
}

/* Lines captured from yt-dlp 2026.10.26 on 2026-10-29. */
static void test_sports_parse_and_order(void)
{
    sport_status st[3];
    int order[3];
    char url[128];
    sport_parse("was_live\tA1\tYesterday's game\n"
                "is_upcoming\tT3LgXSn5ihY\t\xF0\x9F\x94\xB4 LIVE | Australia v Hong Kong China | Rugby World Cup 2027\n"
                "is_upcoming\tB2\tLater game\n", &st[0]);
    CHECK(st[0].state == SPORT_UPCOMING && strcmp(st[0].video_id, "T3LgXSn5ihY") == 0);
    CHECK(strcmp(st[0].title, "Australia v Hong Kong China | Rugby World Cup 2027") == 0);
    sport_parse("is_upcoming\tU1\tSoon\nis_live\tgCNeDWCI0vo\tLIVE! | T1 | Day 2 | WTT Feeder\n", &st[1]);
    CHECK(st[1].state == SPORT_LIVE && strcmp(st[1].video_id, "gCNeDWCI0vo") == 0);
    CHECK(strcmp(st[1].title, "T1 | Day 2 | WTT Feeder") == 0);
    sport_parse("", &st[2]);
    CHECK(st[2].state == SPORT_OFF && st[2].title[0] == '\0');
    CHECK(sports_order(st, 3, order) == 3 && order[0] == 1 && order[1] == 0 && order[2] == 2);
    CHECK(strcmp(sport_watch_url(&st[1], "https://x/live", url, sizeof url),
                 "https://www.youtube.com/watch?v=gCNeDWCI0vo") == 0);
    CHECK(strcmp(sport_watch_url(&st[0], "https://x/live", url, sizeof url), "https://x/live") == 0);
    CHECK(sport_streams_url("https://www.youtube.com/channel/UCx/live", url, sizeof url) &&
          strcmp(url, "https://www.youtube.com/channel/UCx/streams") == 0);
    CHECK(sport_streams_url("https://example.com/match.m3u8", url, sizeof url) == NULL);
}

/* Live football search: matches and watch-alongs stay, busiest first;
 * American football, school games from look-alike channels, game-engine
 * fakes and talk shows go. */
static void test_sports_search_filter(void)
{
    sport_found f[SPORT_FOUND_MAX];
    char url[256];
    int n;
    CHECK(sport_is_football("Everton vs Newcastle LIVE Watch Along | Carabao Cup", "Matchday Unfiltered"));
    CHECK(sport_is_football("Al Nassr vs Al Kholood Live: Saudi Pro League | Watchalong & Live Score", "Blink Sports"));
    CHECK(!sport_is_football("Green Bay Packers VS Carolina Panthers | LIVE NFL 2026", "yuigot"));
    CHECK(!sport_is_football("Santa Fe Catholic vs Lakeland | LIVE Soccer High School @Florida", "ASDSportSavvyLive 38"));
    CHECK(!sport_is_football("Vinalhaven Vs. Telstar | LIVE Soccer", "PAWSportDayScoreboard"));
    CHECK(!sport_is_football("LIVE - Everton vs Newcastle | Full Match | Video Game Simulation", "BOB TUBEE"));
    CHECK(!sport_is_football("Nepal U15 vs New Zealand U15 | eFootball Gameplay", "Football Live Stand"));
    CHECK(!sport_is_football("LIVE: talkSPORT 24/7", "talkSPORT"));
    CHECK(!sport_is_football("CHELSEA v MANCHESTER UNITED PREVIEW", "CB1MILLI"));
    CHECK(!sport_is_football("Barcelona Training Session", "APT Sports")); /* no match in it */
    n = sport_add_found(f, 0,
                        "is_live\tA1\t99\tSmall Club\tHome FC vs Away FC\n"
                        "is_live\tB2\tNA\tQuiet Club\tNorth v South | League\n"
                        "was_live\tC3\t5000\tOld\tYesterday vs Today\n"
                        "is_live\tD4\t1200\tBig Club\t\xF0\x9F\x94\xB4 LIVE | Derby: Reds vs Blues watch along\n"
                        "is_live\tE5\t8000\tyuigot\tPackers vs Panthers | LIVE NFL\n");
    CHECK(n == 3 && strcmp(f[0].video_id, "D4") == 0 && f[0].viewers == 1200);
    CHECK(strcmp(f[0].title, "Derby: Reds vs Blues watch along") == 0 && strcmp(f[0].by, "Big Club") == 0);
    CHECK(strcmp(f[1].video_id, "A1") == 0 && strcmp(f[2].video_id, "B2") == 0 && f[2].viewers == 0);
    n = sport_add_found(f, n, "is_live\tA1\t99\tSmall Club\tHome FC vs Away FC\n"); /* the other search found it too */
    CHECK(n == 3);
    CHECK(strstr(sport_search_url(0, url, sizeof url), "youtube.com/results?search_query=") != NULL &&
          strstr(url, "sp=EgJAAQ") != NULL);
}

/* YouTube: searches, answers and durations. */
static void test_youtube_service(void)
{
    yt_video v[YT_FEED_MAX];
    char out[128];
    int n;
    yt_clean_query(out, sizeof out, "  Harambee   stars! --rm -rf ");
    CHECK(strcmp(out, "Harambee stars rm rf") == 0);
    CHECK(yt_feed_url(0, "  ", out, sizeof out) == NULL); /* nothing to search for */
    CHECK(strcmp(yt_feed_url(0, "gor mahia", out, sizeof out), "ytsearch10:gor mahia") == 0);
    CHECK(strncmp(yt_feed_url(2, NULL, out, sizeof out), "ytsearch10:kenya", 16) == 0);
    n = yt_parse("L8oG27lPVx4\t490\tNA\tMaisha TV Kenya\tKindiki rally\n"
                 "gyp8Onb4vmk\tNA\tis_live\tCitizen TV Kenya\tCitizen TV Live:\n"
                 "UPCOMING123\tNA\tis_upcoming\tSomeone\tLater\n"
                 "broken line\n"
                 "iRvdPxUzY_U\t4955\twas_live\tKenya News Channel\t\xF0\x9F\x94\xB4 LIVE - Rally\n",
                 v, YT_FEED_MAX);
    CHECK(n == 3 && v[0].seconds == 490 && !v[0].live && strcmp(v[0].by, "Maisha TV Kenya") == 0);
    CHECK(v[1].live && v[1].seconds == 0 && strcmp(v[2].title, "Rally") == 0);
    yt_duration(out, sizeof out, 490);
    CHECK(strcmp(out, "8:10") == 0);
    yt_duration(out, sizeof out, 4955);
    CHECK(strcmp(out, "1:22:35") == 0);
    yt_duration(out, sizeof out, 0);
    CHECK(out[0] == '\0');
    CHECK(strcmp(yt_watch_url(&v[0], out, sizeof out), "https://www.youtube.com/watch?v=L8oG27lPVx4") == 0);
}

void run_channel_tests(void)
{
    test_youtube_service();
    test_sports_search_filter();
    test_channels_groups();
    test_sports_parse_and_order();
    test_channels_basic_and_sorted();
    test_channels_notepad_bom_and_crlf();
    test_channels_bad_lines_skipped();
    test_channels_names_ascii_only();
    test_channels_limit_and_empty();
    test_channels_load_order_and_fallback();
    test_channels_asset_matches_defaults();
}
