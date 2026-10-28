/* Channel line-up parsing: what a parent might type into channels.txt. */
#include <stdio.h>
#include <string.h>

#include "channels.h"

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

void run_channel_tests(void)
{
    test_channels_basic_and_sorted();
    test_channels_notepad_bom_and_crlf();
    test_channels_bad_lines_skipped();
    test_channels_names_ascii_only();
    test_channels_limit_and_empty();
    test_channels_load_order_and_fallback();
    test_channels_asset_matches_defaults();
}
