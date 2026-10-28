#ifndef FT_CHANNELS_H
#define FT_CHANNELS_H

/* The family's channel line-up: Kenyan stations' YouTube live streams.
 * Parents can edit channels.txt (one channel per line):
 *
 *     number|name|url|#RRGGBB|group
 *
 * '#' starts a comment line; the colour and group are optional. The group
 * "sports" puts a channel on the Sports page instead of the channel list.
 * Lines that do not parse are skipped with a warning; the rest still load. */

#include <stddef.h>
#include <stdint.h>

#define CH_MAX 60
#define CH_NAME_MAX 32
#define CH_URL_MAX 200
#define CH_NUMBER_MAX 999
#define CH_FILE_MAX (64u * 1024u)

typedef struct {
    int number;
    char name[CH_NAME_MAX]; /* ASCII only: the fonts have no other glyphs */
    char url[CH_URL_MAX];   /* http(s) only */
    uint32_t accent;        /* 0xFFRRGGBB card colour */
    int group;              /* ch_group */
} channel;

typedef enum { CH_TV, CH_SPORTS } ch_group;

typedef struct {
    channel ch[CH_MAX]; /* sorted by number */
    int n;
} channel_list;

void channels_defaults(channel_list *l);
/* Parses channels.txt text (UTF-8, LF or CRLF, optional BOM). Returns the
 * number of channels kept. warn (may be NULL) gets the first problem and
 * how many lines were skipped, or "" when every line was fine. */
int channels_parse(channel_list *l, const char *text, size_t len, char *warn, size_t warnn);
/* Uses the first file in paths that holds at least one channel; otherwise
 * the built-in defaults. Returns the index of the file used, or -1. */
int channels_load(channel_list *l, const char *const paths[], int npaths, char *warn, size_t warnn);
/* Index of the channel with this number, or -1. */
int channels_find_number(const channel_list *l, int number);
/* Copies the channels of one group, keeping their order. */
void channels_group(const channel_list *all, ch_group g, channel_list *out);

#endif
