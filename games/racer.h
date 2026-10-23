#ifndef FT_RACER_H
#define FT_RACER_H

/* Family Racer (plan p.23): top-down three-lane driving, auto-acceleration,
 * LEFT/RIGHT change lane, OK starts, Back pauses. */

#include "common.h"

#define RACER_LANES 3
#define RACER_ROWS 12 /* row 0 is the far end; the car sits in the last row */
#define RACER_CAR_ROW (RACER_ROWS - 1)
#define RACER_DURATION_TICKS (180 * FT_TICK_HZ) /* 3-minute race */
#define RACER_LIVES 3

typedef enum { RACER_EASY, RACER_MEDIUM, RACER_HARD, RACER_LEVELS } racer_level;
typedef enum {
    RACER_START, RACER_RUNNING, RACER_PAUSED, RACER_CRASHED, RACER_FINISHED, RACER_EXITED
} racer_state;

typedef struct {
    racer_state state;
    racer_level level;
    int practice; /* no-fail mode */
    uint32_t seed;
    ft_rng rng;
    uint8_t rows[RACER_ROWS]; /* bit n set = lane n blocked */
    int lane;
    int lives;
    int gap_left;
    uint32_t tick;
    uint32_t row_timer;
    uint32_t score;
    uint32_t hits;
} racer;

void racer_init(racer *r, uint32_t seed, racer_level level, int practice);
void racer_key(racer *r, ft_key k);
void racer_tick(racer *r);
uint8_t racer_next_row(racer *r);
const char *racer_level_name(racer_level level);

#endif
