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

#endif
