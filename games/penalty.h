#ifndef FT_PENALTY_H
#define FT_PENALTY_H

/* Penalty Cup (plan p.23): five shots per player, optional sudden death,
 * arrows select one of five targets, OK shoots, seeded keeper, one remote
 * passed between two players. */

#include <stddef.h>
#include "common.h"

#define PEN_TARGETS 5
#define PEN_REGULATION 5
#define PEN_MAX_SHOTS 20
#define PEN_RESULT_COOLDOWN 10 /* ticks; absorbs rapid repeated OK presses */
#define PEN_PACKED_SIZE 22

typedef enum { PEN_AIMING, PEN_RESULT, PEN_OVER, PEN_PAUSED, PEN_EXITED } pen_phase;
enum { PEN_NO_WINNER = -1, PEN_TIE = 2 };

typedef struct {
    pen_phase phase;
    pen_phase resume_phase;
    int sudden_death;
    uint32_t seed;
    ft_rng rng;
    int shots[2];
    int goals[2];
    uint32_t history[2]; /* bit n set = shot n scored */
    int aim;
    int keeper;
    int scored;
    int cooldown;
    int winner;
} penalty;

void penalty_init(penalty *p, uint32_t seed, int sudden_death);
void penalty_key(penalty *p, ft_key k);
void penalty_tick(penalty *p);
int penalty_turn(const penalty *p);
int penalty_peek_keeper(const penalty *p);
size_t penalty_pack(const penalty *p, uint8_t *out, size_t cap);
int penalty_unpack(penalty *p, const uint8_t *in, size_t n);

#endif
