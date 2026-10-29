#ifndef FT_SPORTS_H
#define FT_SPORTS_H

/* The Sports page: which official sports channels are streaming a real
 * match right now, and what is coming up. yt-dlp lists a channel's
 * streams; these pure helpers read that list and order the page. */

#include <stddef.h>

#define SPORT_TITLE_MAX 96
#define SPORT_ID_MAX 16
#define SPORT_FRESH_MS (3u * 60u * 1000u) /* look again after three minutes */

typedef enum { SPORT_UNKNOWN, SPORT_CHECKING, SPORT_LIVE, SPORT_UPCOMING, SPORT_OFF } sport_state;

typedef struct {
    int state;                       /* sport_state */
    char video_id[SPORT_ID_MAX];     /* the live (or next) stream */
    char title[SPORT_TITLE_MAX];     /* its title, cleaned to plain ASCII */
} sport_status;

/* The channel's streams page for a YouTube ".../live" address; NULL for
 * any other address (those are simply offered without a live check). */
const char *sport_streams_url(const char *live_url, char *out, size_t n);
/* yt-dlp's --print format matching sport_parse. */
#define SPORT_PRINT_FORMAT "%(live_status)s\t%(id)s\t%(title)s"
/* Reads lines of "live_status<TAB>id<TAB>title" (newest first). */
void sport_parse(const char *lines, sport_status *st);
/* Drops emoji and other non-ASCII, the "LIVE" and "|" decoration that
 * starts many titles, and repeated spaces. */
void sport_clean_title(char *out, size_t n, const char *in);
/* Channel indexes ordered live first, then coming up, then the rest
 * (stable within each). Returns n. */
int sports_order(const sport_status *st, int n, int *order);
/* Watch address for channel url: the live stream itself when known. */
const char *sport_watch_url(const sport_status *st, const char *channel_url, char *out, size_t n);

/* ---- "Live football now": YouTube's own live search ------------------------
 * Most live "football" results are not football matches: American football,
 * school games from look-alike channels, video-game "simulations", 24/7
 * talk and replays. Only streams that look like a match ("A vs B") or a
 * watch-along of one are kept, busiest first. They are not vetted, so the
 * shell shows them to parents only. */
#define SPORT_FOUND_MAX 12
#define SPORT_BY_MAX 32
#define SPORT_SEARCHES 2
#define SPORT_SEARCH_ITEMS 25
#define SPORT_SEARCH_FORMAT "%(live_status)s\t%(id)s\t%(concurrent_view_count)s\t%(channel)s\t%(title)s"

typedef struct {
    char video_id[SPORT_ID_MAX];
    char by[SPORT_BY_MAX];         /* the channel streaming it */
    char title[SPORT_TITLE_MAX];
    int viewers;                   /* watching now; 0 when YouTube does not say */
} sport_found;

/* The i-th search page (0..SPORT_SEARCHES-1): live videos only. */
const char *sport_search_url(int i, char *out, size_t n);
/* 1 if a search result (title, channel name) looks like live football. */
int sport_is_football(const char *title, const char *by);
/* Adds the football matches from lines of SPORT_SEARCH_FORMAT to found
 * (n so far), skipping repeats; keeps the busiest SPORT_FOUND_MAX.
 * Returns the new count. */
int sport_add_found(sport_found *found, int n, const char *lines);

#endif
