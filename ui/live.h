#ifndef FT_LIVE_H
#define FT_LIVE_H

/* Watching a channel, TV style, with the six remote keys:
 *   Up/Down      channel up/down (tunes after a short pause, so zapping
 *                past three channels starts one stream, not three)
 *   Left/Right   volume
 *   OK           channel list panel; on an error card, try again
 *   Back         close the panel, else go home (the stream stops)
 * Pure state machine: the caller owns the player and the clock. */

#include "common.h"

#define LIVE_TUNE_DELAY_MS 600
#define LIVE_BANNER_MS 5000
#define LIVE_VOLUME_MS 2000
#define LIVE_VOLUME_STEP 5

typedef enum { LIVE_ACT_NONE, LIVE_ACT_TUNE, LIVE_ACT_VOLUME, LIVE_ACT_EXIT } live_act;

typedef struct {
    live_act act;
    int arg; /* TUNE: channel index; VOLUME: 0-100 */
} live_action;

typedef struct {
    int nch;          /* channels in the line-up */
    int current;      /* channel playing, or being started */
    int shown;        /* channel on the banner; runs ahead of current while zapping */
    int tune_wait_ms; /* > 0: tune `shown` when this runs out */
    int banner_ms;    /* info banner visible while > 0 */
    int volume_ms;    /* volume bar visible while > 0 */
    int volume;       /* 0-100 */
    int panel_open;
    int panel_item;
    uint32_t anim_ms; /* free-running clock for the loading spinner */
} live;

/* The caller starts `channel` right away; the banner shows it. */
void live_init(live *l, int nch, int channel, int volume);
/* error_card: the player is showing an error, so OK means try again. */
live_action live_key(live *l, ft_key k, int error_card);
live_action live_tick(live *l, int elapsed_ms);
/* The picture has just appeared: show the banner over it. A YouTube
 * channel can take 10-15 s to start, longer than the banner lasts. */
void live_started(live *l);

#endif
