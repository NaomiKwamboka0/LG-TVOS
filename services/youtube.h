#ifndef FT_YOUTUBE_H
#define FT_YOUTUBE_H

/* The YouTube tab: rows of videos found by YouTube's own search, played in
 * Family TV's player. yt-dlp answers each search (no account, no API key);
 * these pure helpers make the searches and read the answers. */

#include <stddef.h>

#define YT_FEEDS 5     /* 0: the family's own search; then the ready-made rows */
#define YT_FEED_MAX 10 /* videos per row */
#define YT_QUERY_MAX 48
#define YT_TITLE_MAX 96
#define YT_BY_MAX 32
#define YT_ID_MAX 16
#define YT_FRESH_MS (15u * 60u * 1000u) /* the ready-made rows are looked up again after this */
#define YT_PRINT_FORMAT "%(id)s\t%(duration)s\t%(live_status)s\t%(channel)s\t%(title)s"

typedef struct {
    char id[YT_ID_MAX];
    char title[YT_TITLE_MAX];
    char by[YT_BY_MAX]; /* the channel */
    int seconds;        /* 0 when not known (live, or YouTube did not say) */
    int live;
} yt_video;

/* What yt-dlp is asked for row feed: the query for row 0, a fixed one for
 * the others. NULL for row 0 with no usable query. */
const char *yt_feed_url(int feed, const char *query, char *out, size_t n);
/* Keeps letters, digits, spaces and a little punctuation, at most
 * YT_QUERY_MAX - 1 characters, without leading or doubled spaces. */
void yt_clean_query(char *out, size_t n, const char *in);
/* Reads lines of YT_PRINT_FORMAT into out; returns how many (at most max). */
int yt_parse(const char *lines, yt_video *out, int max);
/* "12:34", "1:02:03", or "" when not known. */
void yt_duration(char *out, size_t n, int seconds);
const char *yt_watch_url(const yt_video *v, char *out, size_t n);

#endif
