#ifndef FT_COMMON_H
#define FT_COMMON_H

#include <stdint.h>

/* Logical remote keys (plan p.22 input mapper). The window maps keyboard
 * keys onto these (arrows, Enter = OK, Backspace/Esc = Back). */
/* The six remote keys, then the mouse: FT_POINT (the pointer moved; the
 * frontend says where), FT_CLICK (left button) and the wheel. The right
 * button arrives as FT_BACK. FT_TEXT: a character typed on the laptop's
 * keyboard while typing is wanted (the frontend says which). */
typedef enum {
    FT_NONE = 0, FT_LEFT, FT_RIGHT, FT_UP, FT_DOWN, FT_OK, FT_BACK, FT_POINT, FT_CLICK, FT_WHEEL_UP, FT_WHEEL_DOWN,
    FT_TEXT
} ft_key;

#define FT_TICK_HZ 30

/* Deterministic seeded generator (xorshift32) so games can be replayed. */
typedef struct { uint32_t s; } ft_rng;

void ft_rng_seed(ft_rng *r, uint32_t seed);
uint32_t ft_rng_next(ft_rng *r);
uint32_t ft_rng_range(ft_rng *r, uint32_t n);

#endif
