#include "racer.h"

#include <string.h>

/* Difficulty alters traffic speed and gaps. min_gap >= 1 keeps an empty row
 * between obstacle rows, so a lane change never has to cross a blocked cell
 * and every pattern has a reachable safe path. */
static const struct {
    uint32_t ticks_per_row;
    int min_gap;
    int max_blocked;
} LEVELS[RACER_LEVELS] = {
    {10, 3, 1},
    {7, 2, 2},
    {5, 1, 2},
};

static const char *LEVEL_NAMES[RACER_LEVELS] = {"Easy", "Medium", "Hard"};

const char *racer_level_name(racer_level level)
{
    return level < RACER_LEVELS ? LEVEL_NAMES[level] : "?";
}

void racer_init(racer *r, uint32_t seed, racer_level level, int practice)
{
    memset(r, 0, sizeof *r);
    r->seed = seed;
    r->level = level < RACER_LEVELS ? level : RACER_EASY;
    r->practice = practice;
    ft_rng_seed(&r->rng, seed);
    r->lane = 1;
    r->lives = RACER_LIVES;
    r->gap_left = RACER_ROWS / 2; /* clear runway at the start */
    r->state = RACER_START;
}

uint8_t racer_next_row(racer *r)
{
    uint8_t mask = 0;
    int blocked, i;

    if (r->gap_left > 0) {
        r->gap_left--;
        return 0;
    }
    blocked = 1 + (int)ft_rng_range(&r->rng, (uint32_t)LEVELS[r->level].max_blocked);
    for (i = 0; i < blocked; i++) {
        uint32_t lane;
        do {
            lane = ft_rng_range(&r->rng, RACER_LANES);
        } while (mask & (1u << lane));
        mask |= (uint8_t)(1u << lane);
    }
    r->gap_left = LEVELS[r->level].min_gap;
    return mask;
}

static void hit(racer *r)
{
    r->rows[RACER_CAR_ROW] = 0;
    r->hits++;
    if (!r->practice && --r->lives <= 0)
        r->state = RACER_CRASHED;
}

static void restart(racer *r)
{
    racer_init(r, r->seed + 1, r->level, r->practice);
    r->state = RACER_RUNNING;
}

void racer_key(racer *r, ft_key k)
{
    switch (r->state) {
    case RACER_START:
        if (k == FT_OK) {
            r->state = RACER_RUNNING;
        } else if (k == FT_LEFT && r->level > RACER_EASY) {
            racer_init(r, r->seed, (racer_level)(r->level - 1), r->practice);
        } else if (k == FT_RIGHT && r->level < RACER_HARD) {
            racer_init(r, r->seed, (racer_level)(r->level + 1), r->practice);
        } else if (k == FT_UP || k == FT_DOWN) {
            r->practice = !r->practice;
        } else if (k == FT_BACK) {
            r->state = RACER_EXITED;
        }
        break;
    case RACER_RUNNING:
        if (k == FT_BACK) {
            r->state = RACER_PAUSED;
            break;
        }
        if (k == FT_LEFT && r->lane > 0)
            r->lane--;
        else if (k == FT_RIGHT && r->lane < RACER_LANES - 1)
            r->lane++;
        else
            break;
        if (r->rows[RACER_CAR_ROW] & (1u << r->lane))
            hit(r);
        break;
    case RACER_PAUSED:
        if (k == FT_OK)
            r->state = RACER_RUNNING;
        else if (k == FT_BACK)
            r->state = RACER_EXITED;
        break;
    case RACER_CRASHED:
    case RACER_FINISHED:
        if (k == FT_OK)
            restart(r);
        else if (k == FT_BACK)
            r->state = RACER_EXITED;
        break;
    case RACER_EXITED:
        break;
    }
}

void racer_tick(racer *r)
{
    if (r->state != RACER_RUNNING)
        return;
    r->tick++;
    if (++r->row_timer >= LEVELS[r->level].ticks_per_row) {
        r->row_timer = 0;
        if (r->rows[RACER_CAR_ROW])
            r->score += 5; /* obstacle row passed without a hit */
        memmove(r->rows + 1, r->rows, RACER_ROWS - 1);
        r->rows[0] = racer_next_row(r);
        r->score += 1;
        if (r->rows[RACER_CAR_ROW] & (1u << r->lane))
            hit(r);
    }
    if (r->state == RACER_RUNNING && r->tick >= RACER_DURATION_TICKS)
        r->state = RACER_FINISHED;
}
