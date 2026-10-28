#include "channels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built-in line-up, used when no channels.txt is found. Keep it identical
 * to assets/channels.txt (a unit test compares them).
 *
 * Addresses use the permanent channel ID, not the @handle: handles change,
 * and look-alike handles belong to other channels (@citizentvkenya is
 * Citizen Extra; @KBCChannel1 is a children's channel). Checked with
 * tools/verify-channels.sh on 2026-10-28 22:33 EAT; live then: Citizen,
 * NTV, KTN News, K24, TV47, Inooro. The others stream on a schedule. */
static const struct {
    int number;
    const char *name, *url;
    uint32_t accent;
} DEFAULTS[] = {
    {1, "Citizen TV", "https://www.youtube.com/channel/UChBQgieUidXV1CmDxSdRm3g/live", 0xFFD32F2Fu},
    {2, "NTV Kenya", "https://www.youtube.com/channel/UCqBJ47FjJcl61fmSbcadAVg/live", 0xFFC62828u},
    {3, "KTN News", "https://www.youtube.com/channel/UCKVsdeoHExltrWMuK0hOWmg/live", 0xFF1565C0u},
    {4, "K24 TV", "https://www.youtube.com/channel/UCt3SE-Mvs3WwP7UW-PiFdqQ/live", 0xFFE53935u},
    {5, "KBC Channel 1", "https://www.youtube.com/channel/UCypNjM5hP1qcUqQZe57jNfg/live", 0xFF0D47A1u},
    {6, "TV47", "https://www.youtube.com/channel/UC_zA9UIWE1fB-jfFk_DBSYw/live", 0xFFEF6C00u},
    {7, "Inooro TV", "https://www.youtube.com/channel/UCZlNMLTVNxzwB0axilK-rDw/live", 0xFF2E7D32u},
    {8, "Kameme TV", "https://www.youtube.com/channel/UCd9nkc2XA77NMxBvQz35I2Q/live", 0xFF6A1B9Au},
    {9, "Ramogi TV", "https://www.youtube.com/channel/UCmKXJ4C59JjVVJ-axXfpJcg/live", 0xFF00838Fu},
    {10, "Switch TV", "https://www.youtube.com/channel/UCUhrpGr_luwUzVaxiW5Jkhw/live", 0xFFAD1457u},
};

/* Card colours for lines that give none. */
static const uint32_t PALETTE[] = {0xFF1E88E5u, 0xFF43A047u, 0xFF8E24AAu, 0xFFF4511Eu, 0xFF00897Bu, 0xFF6D4C41u};

static void sort_by_number(channel_list *l)
{
    int i, j;
    for (i = 1; i < l->n; i++) {
        channel c = l->ch[i];
        for (j = i; j > 0 && l->ch[j - 1].number > c.number; j--)
            l->ch[j] = l->ch[j - 1];
        l->ch[j] = c;
    }
}

void channels_defaults(channel_list *l)
{
    int i;
    memset(l, 0, sizeof *l);
    for (i = 0; i < (int)(sizeof DEFAULTS / sizeof DEFAULTS[0]); i++) {
        channel *c = &l->ch[l->n++];
        c->number = DEFAULTS[i].number;
        snprintf(c->name, sizeof c->name, "%s", DEFAULTS[i].name);
        snprintf(c->url, sizeof c->url, "%s", DEFAULTS[i].url);
        c->accent = DEFAULTS[i].accent;
    }
    sort_by_number(l);
}

int channels_find_number(const channel_list *l, int number)
{
    int i;
    for (i = 0; i < l->n; i++)
        if (l->ch[i].number == number)
            return i;
    return -1;
}

/* [s, e) with spaces and tabs trimmed. */
static void trim(const char **s, const char **e)
{
    while (*s < *e && (**s == ' ' || **s == '\t'))
        (*s)++;
    while (*e > *s && ((*e)[-1] == ' ' || (*e)[-1] == '\t'))
        (*e)--;
}

/* Copies a name, turning each non-ASCII UTF-8 character into one '?'.
 * Returns 1 if anything was replaced or cut short. */
static int copy_name(char *out, size_t cap, const char *s, const char *e)
{
    size_t n = 0;
    int changed = 0;
    while (s < e) {
        unsigned char ch = (unsigned char)*s++;
        if (ch >= 0x80) {
            while (s < e && ((unsigned char)*s & 0xC0) == 0x80)
                s++;
            ch = '?';
            changed = 1;
        } else if (ch < 0x20 || ch == 0x7F) {
            ch = '?';
            changed = 1;
        }
        if (n + 1 >= cap) {
            changed = 1;
            break;
        }
        out[n++] = (char)ch;
    }
    out[n] = '\0';
    return changed;
}

static int parse_number(const char *s, const char *e, int *out)
{
    int v = 0;
    if (s == e || e - s > 3)
        return -1;
    for (; s < e; s++) {
        if (*s < '0' || *s > '9')
            return -1;
        v = v * 10 + (*s - '0');
    }
    if (v < 1 || v > CH_NUMBER_MAX)
        return -1;
    *out = v;
    return 0;
}

/* Only plain http(s) addresses: nothing that could reach a local file or
 * be read as a player option. */
static int valid_url(const char *s, const char *e)
{
    const char *p;
    size_t n = (size_t)(e - s);
    if (n >= CH_URL_MAX)
        return 0;
    if (!(n > 8 && strncmp(s, "https://", 8) == 0) && !(n > 7 && strncmp(s, "http://", 7) == 0))
        return 0;
    for (p = s; p < e; p++)
        if ((unsigned char)*p <= 0x20 || (unsigned char)*p >= 0x7F || *p == '|')
            return 0;
    return 1;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_colour(const char *s, const char *e, uint32_t *out)
{
    uint32_t v = 0;
    if (e - s != 7 || *s != '#')
        return -1;
    for (s++; s < e; s++) {
        int h = hexval(*s);
        if (h < 0)
            return -1;
        v = (v << 4) | (uint32_t)h;
    }
    *out = 0xFF000000u | v;
    return 0;
}

/* Returns NULL on success, or why the line was skipped. */
static const char *parse_line(channel_list *l, const char *s, const char *e, int *renamed)
{
    const char *f[5], *fe[5];
    int nf = 0, number;
    const char *p = s;
    channel c;

    while (nf < 5) {
        const char *bar = p;
        while (bar < e && *bar != '|')
            bar++;
        f[nf] = p;
        fe[nf] = bar;
        trim(&f[nf], &fe[nf]);
        nf++;
        if (bar == e)
            break;
        p = bar + 1;
    }
    if (nf < 3 || nf > 4)
        return "expected number|name|url|#RRGGBB";
    if (parse_number(f[0], fe[0], &number) != 0)
        return "channel number must be 1-999";
    if (channels_find_number(l, number) >= 0)
        return "channel number used twice";
    if (f[1] == fe[1])
        return "channel name is empty";
    if (!valid_url(f[2], fe[2]))
        return "address must start with https:// and have no spaces";
    if (l->n >= CH_MAX)
        return "too many channels (40 at most)";

    memset(&c, 0, sizeof c);
    c.number = number;
    *renamed = copy_name(c.name, sizeof c.name, f[1], fe[1]);
    memcpy(c.url, f[2], (size_t)(fe[2] - f[2]));
    c.url[fe[2] - f[2]] = '\0';
    if (nf == 4 && f[3] != fe[3]) {
        if (parse_colour(f[3], fe[3], &c.accent) != 0)
            return "colour must look like #1E88E5";
    } else {
        c.accent = PALETTE[l->n % (int)(sizeof PALETTE / sizeof PALETTE[0])];
    }
    l->ch[l->n++] = c;
    return NULL;
}

int channels_parse(channel_list *l, const char *text, size_t len, char *warn, size_t warnn)
{
    const char *p = text, *end = text + len;
    int line = 0, skipped = 0;
    char first[96] = "";

    memset(l, 0, sizeof *l);
    if (len >= 3 && memcmp(p, "\xEF\xBB\xBF", 3) == 0)
        p += 3;
    while (p < end) {
        const char *s = p, *e = p;
        const char *why;
        int renamed = 0;
        while (e < end && *e != '\n')
            e++;
        p = e < end ? e + 1 : e;
        line++;
        if (e > s && e[-1] == '\r')
            e--;
        trim(&s, &e);
        if (s == e || *s == '#')
            continue;
        why = parse_line(l, s, e, &renamed);
        if (why) {
            skipped++;
            if (!first[0])
                snprintf(first, sizeof first, "line %d: %s", line, why);
        } else if (renamed && !first[0]) {
            snprintf(first, sizeof first, "line %d: name shortened or letters replaced by ?", line);
        }
    }
    sort_by_number(l);
    if (warn && warnn) {
        if (!first[0])
            warn[0] = '\0';
        else if (skipped > 1)
            snprintf(warn, warnn, "channels.txt %s (%d lines skipped)", first, skipped);
        else
            snprintf(warn, warnn, "channels.txt %s", first);
    }
    return l->n;
}

int channels_load(channel_list *l, const char *const paths[], int npaths, char *warn, size_t warnn)
{
    static char text[CH_FILE_MAX];
    char note[160] = "";
    int i;

    for (i = 0; i < npaths; i++) {
        size_t n;
        FILE *f = paths[i] ? fopen(paths[i], "rb") : NULL;
        if (!f)
            continue;
        n = fread(text, 1, sizeof text, f);
        fclose(f);
        if (channels_parse(l, text, n, note, sizeof note) > 0) {
            if (warn && warnn)
                snprintf(warn, warnn, "%s", note);
            return i;
        }
        if (!note[0])
            snprintf(note, sizeof note, "channels.txt has no channels");
    }
    channels_defaults(l);
    if (warn && warnn)
        snprintf(warn, warnn, "%s%s", note, note[0] ? "; using the built-in channels" : "");
    return -1;
}
