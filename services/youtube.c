#include "youtube.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sports.h"

const char *yt_feed_url(int feed, const char *query, char *out, size_t n)
{
    /* Kenyan first: the family's own news, music and comedy. */
    static const char *const FIXED[YT_FEEDS] = {NULL, "football highlights today", "kenya news today",
                                                "kenyan music hits", "kenyan comedy"};
    char q[YT_QUERY_MAX];
    if (feed < 0 || feed >= YT_FEEDS)
        return NULL;
    yt_clean_query(q, sizeof q, feed == 0 ? (query ? query : "") : FIXED[feed]);
    if (!q[0])
        return NULL;
    snprintf(out, n, "ytsearch%d:%s", YT_FEED_MAX, q);
    return out;
}

void yt_clean_query(char *out, size_t n, const char *in)
{
    size_t k = 0;
    int space = 1;
    if (n == 0)
        return;
    for (; *in && k + 1 < n && k + 1 < YT_QUERY_MAX; in++) {
        char c = *in;
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '\'' ||
                 c == '&' || c == '.' || c == ',';
        if (c == ' ' || !ok) {
            if (!space)
                out[k++] = ' ';
            space = 1;
            continue;
        }
        out[k++] = c;
        space = 0;
    }
    while (k > 0 && out[k - 1] == ' ')
        k--;
    out[k] = '\0';
}

int yt_parse(const char *lines, yt_video *out, int max)
{
    const char *line = lines;
    int n = 0;
    while (line && *line && n < max) {
        const char *start = line, *nl = strchr(line, '\n'), *f[5];
        char tmp[512], *p;
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        int k = 1;
        yt_video *v = &out[n];
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
        if (k < 5 || !f[0][0] || strlen(f[0]) >= YT_ID_MAX || !strcmp(f[2], "is_upcoming"))
            continue;
        memset(v, 0, sizeof *v);
        snprintf(v->id, sizeof v->id, "%s", f[0]);
        v->seconds = f[1][0] >= '0' && f[1][0] <= '9' ? atoi(f[1]) : 0;
        v->live = !strcmp(f[2], "is_live");
        sport_clean_title(v->by, sizeof v->by, f[3]);
        sport_clean_title(v->title, sizeof v->title, f[4]);
        if (v->title[0])
            n++;
    }
    return n;
}

void yt_duration(char *out, size_t n, int seconds)
{
    if (seconds <= 0)
        snprintf(out, n, "%s", "");
    else if (seconds >= 3600)
        snprintf(out, n, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        snprintf(out, n, "%d:%02d", seconds / 60, seconds % 60);
}

const char *yt_watch_url(const yt_video *v, char *out, size_t n)
{
    snprintf(out, n, "https://www.youtube.com/watch?v=%s", v->id);
    return out;
}
