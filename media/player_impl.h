#ifndef FT_PLAYER_IMPL_H
#define FT_PLAYER_IMPL_H

/* Shared by the player implementations; not for callers. */

#include <stddef.h>

#include "player.h"

typedef struct {
    void (*destroy)(player *p);
    void (*open)(player *p, const char *url);
    void (*stop)(player *p);
    void (*set_volume)(player *p, int volume);
    void (*pump)(player *p, uint64_t now_ms);
    int (*render)(player *p, uint32_t *px, int w, int h, int stride);
    int (*probe)(player *p, int tag, const char *page_url, const char *print_format, int items);
} player_ops;

#define PROBE_QUEUE 16

/* Every implementation starts with this. */
struct player {
    const player_ops *ops;
    player_status st;
    int max_height;
    player_probe_result done[PROBE_QUEUE]; /* finished probes, oldest first */
    int ndone;
};

/* Queues a finished probe for player_probe_next (drops it if full). */
void player_probe_done(player *p, int tag, int ok, const char *out, size_t len);

/* Sets ERROR with a kind and an ASCII-only detail. */
void player_fail(player *p, player_error e, const char *detail);

#endif
