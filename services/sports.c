#include "sports.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *sport_streams_url(const char *live_url, char *out, size_t n)
{
    size_t len = strlen(live_url);
    if (strncmp(live_url, "https://www.youtube.com/", 24) != 0 || len < 5 ||
        strcmp(live_url + len - 5, "/live") != 0 || len + 3 >= n)
        return NULL;
    snprintf(out, n, "%.*s/streams", (int)(len - 5), live_url);
    return out;
}

static int starts_word(const char *p, const char *w)
{
    size_t n = strlen(w), i;
    for (i = 0; i < n; i++)
        if ((p[i] | 0x20) != w[i])
            return 0;
    return !((p[n] >= 'a' && p[n] <= 'z') || (p[n] >= 'A' && p[n] <= 'Z'));
}

void sport_clean_title(char *out, size_t n, const char *in)
{
    char tmp[256];
    size_t k = 0;
    const char *p;
    int space = 1;
    if (n == 0)
        return;
    for (p = in; *p && *p != '\n' && k + 1 < sizeof tmp; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= 0x80 || c < 0x20) /* emoji, flags, fancy letters */
            c = ' ';
        if (c == ' ' && space)
            continue;
        space = c == ' ';
        tmp[k++] = (char)c;
    }
    while (k > 0 && tmp[k - 1] == ' ')
        k--;
    tmp[k] = '\0';
    /* "LIVE | ", "LIVE! ", "LIVE NOW - ", "| " at the start say nothing new. */
    for (p = tmp;;) {
        if (*p == '|' || *p == '-' || *p == ':' || *p == '!' || *p == ' ')
            p++;
        else if (starts_word(p, "live"))
            p += 4;
        else if (starts_word(p, "now") && p > tmp)
            p += 3;
        else
            break;
    }
    snprintf(out, n, "%s", *p ? p : tmp);
}

void sport_parse(const char *lines, sport_status *st)
{
    const char *line = lines;
    int found_upcoming = 0;
    memset(st, 0, sizeof *st);
    st->state = SPORT_OFF;
    while (line && *line) {
        const char *tab1 = strchr(line, '\t'), *nl = strchr(line, '\n');
        const char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
        if (tab1 && tab2 && (!nl || tab2 < nl)) {
            int live = (size_t)(tab1 - line) == 7 && !strncmp(line, "is_live", 7);
            int upcoming = (size_t)(tab1 - line) == 11 && !strncmp(line, "is_upcoming", 11);
            if (live || (upcoming && !found_upcoming)) {
                size_t idn = (size_t)(tab2 - tab1 - 1);
                st->state = live ? SPORT_LIVE : SPORT_UPCOMING;
                snprintf(st->video_id, sizeof st->video_id, "%.*s", (int)(idn < SPORT_ID_MAX ? idn : 0), tab1 + 1);
                sport_clean_title(st->title, sizeof st->title, tab2 + 1);
                if (live)
                    return;
                found_upcoming = 1;
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

static int rank(int state)
{
    return state == SPORT_LIVE ? 0 : state == SPORT_UPCOMING ? 1 : 2;
}

int sports_order(const sport_status *st, int n, int *order)
{
    int r, i, k = 0;
    for (r = 0; r < 3; r++)
        for (i = 0; i < n; i++)
            if (rank(st[i].state) == r)
                order[k++] = i;
    return k;
}

const char *sport_watch_url(const sport_status *st, const char *channel_url, char *out, size_t n)
{
    if (st && st->state == SPORT_LIVE && st->video_id[0])
        snprintf(out, n, "https://www.youtube.com/watch?v=%s", st->video_id);
    else
        snprintf(out, n, "%s", channel_url);
    return out;
}

/* ---- live football search ------------------------------------------------ */

const char *sport_search_url(int i, char *out, size_t n)
{
    /* sp=EgJAAQ== is YouTube's "Live" filter. */
    static const char *const QUERY[SPORT_SEARCHES] = {"soccer+live+match", "live+football+watchalong"};
    snprintf(out, n, "https://www.youtube.com/results?search_query=%s&sp=EgJAAQ%%253D%%253D",
             QUERY[i >= 0 && i < SPORT_SEARCHES ? i : 0]);
    return out;
}

/* Case-insensitive substring search. */
static int contains(const char *text, const char *needle)
{
    size_t i, j, n = strlen(needle);
    for (i = 0; text[i]; i++) {
        for (j = 0; j < n && text[i + j] && ((text[i + j] | 0x20) == (needle[j] | 0x20) ||
                                            (needle[j] == ' ' && text[i + j] == ' '));
             j++)
            ;
        if (j == n)
            return 1;
    }
    return 0;
}

static int contains_any(const char *text, const char *const *words)
{
    for (; *words; words++)
        if (contains(text, *words))
            return 1;
    return 0;
}

/* "ASDSportSavvyLive 38", "PAWSportDayScoreboard": names made up in bulk
 * by channels that stream other people's school games. */
static int bulk_channel_name(const char *by)
{
    size_t len = strlen(by);
    while (len > 0 && by[len - 1] == ' ')
        len--;
    if (len > 0 && by[len - 1] >= '0' && by[len - 1] <= '9')
        return 1;
    return len >= 5 && by[0] >= 'A' && by[0] <= 'Z' && by[1] >= 'A' && by[1] <= 'Z' && by[2] >= 'A' &&
           by[2] <= 'Z' && by[3] >= 'A' && by[3] <= 'Z' && by[4] >= 'a' && by[4] <= 'z';
}

int sport_is_football(const char *title, const char *by)
{
    static const char *const NOT_SOCCER[] = {"nfl", "ncaa", "high school", "hs football", "college football",
                                             "night football", "american football", "touchdown", "flag football",
                                             "fantasy football", "cricket", "basketball", "baseball", NULL};
    static const char *const NOT_REAL[] = {"simulation", "efootball", "e-football", "gameplay", "video game",
                                           "game play", "pes 20", "fifa 2", "fc 2", "ea fc", "ea sports",
                                           "football manager", "career mode", "ultimate team", "ps5", "xbox",
                                           "cartoon", "prediction", NULL};
    static const char *const NOT_A_MATCH[] = {"24/7", "radio", "podcast", "preview", "post-match", "post match",
                                              "reaction", "press conference", "highlights", "replay", "auction",
                                              "draw ceremony", "ballon", "transfer", NULL};
    static const char *const MATCH[] = {" vs ", " vs. ", " v ", " vs.", "watch along", "watchalong", NULL};
    return !bulk_channel_name(by) && !contains_any(title, NOT_SOCCER) && !contains_any(title, NOT_REAL) &&
           !contains_any(title, NOT_A_MATCH) && contains_any(title, MATCH);
}

static int busier(const sport_found *a, const sport_found *b)
{
    return a->viewers > b->viewers;
}

int sport_add_found(sport_found *found, int n, const char *lines)
{
    const char *line = lines;
    while (line && *line) {
        const char *start = line, *nl = strchr(line, '\n'), *f[5];
        char tmp[512];
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        int k = 1, i, dup = 0;
        char *p;
        sport_found x;
        line = nl ? nl + 1 : NULL;
        if (len >= sizeof tmp)
            continue;
        memcpy(tmp, start, len);
        tmp[len] = '\0';
        f[0] = tmp;
        for (p = tmp; *p && k < 5; p++)
            if (*p == '\t') {
                *p = '\0';
                f[k++] = p + 1;
            }
        if (k < 5 || strcmp(f[0], "is_live") != 0 || !f[1][0] || strlen(f[1]) >= SPORT_ID_MAX)
            continue;
        memset(&x, 0, sizeof x);
        snprintf(x.video_id, sizeof x.video_id, "%s", f[1]);
        x.viewers = f[2][0] >= '0' && f[2][0] <= '9' ? atoi(f[2]) : 0;
        sport_clean_title(x.by, sizeof x.by, f[3]);
        sport_clean_title(x.title, sizeof x.title, f[4]);
        if (!x.title[0] || !sport_is_football(f[4], f[3]))
            continue;
        for (i = 0; i < n; i++)
            dup |= !strcmp(found[i].video_id, x.video_id) || !strcmp(found[i].title, x.title);
        if (dup)
            continue;
        if (n < SPORT_FOUND_MAX)
            found[n++] = x;
        else if (busier(&x, &found[n - 1]))
            found[n - 1] = x;
        else
            continue;
        for (i = n - 1; i > 0 && busier(&found[i], &found[i - 1]); i--) { /* keep busiest first */
            sport_found t = found[i];
            found[i] = found[i - 1];
            found[i - 1] = t;
        }
    }
    return n;
}
