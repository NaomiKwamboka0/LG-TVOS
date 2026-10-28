#include "sports.h"

#include <stdio.h>
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
