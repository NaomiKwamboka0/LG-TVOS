#ifndef FT_PLAYER_IMPL_H
#define FT_PLAYER_IMPL_H

/* Shared by the player implementations; not for callers. */

#include "player.h"

typedef struct {
    void (*destroy)(player *p);
    void (*open)(player *p, const char *url);
    void (*stop)(player *p);
    void (*set_volume)(player *p, int volume);
    void (*pump)(player *p, uint64_t now_ms);
    int (*render)(player *p, uint32_t *px, int w, int h, int stride);
} player_ops;

/* Every implementation starts with this. */
struct player {
    const player_ops *ops;
    player_status st;
    int max_height;
};

/* Sets ERROR with a kind and an ASCII-only detail. */
void player_fail(player *p, player_error e, const char *detail);

#endif
