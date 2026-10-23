#ifndef FT_COMMON_H
#define FT_COMMON_H

#include <stdint.h>

/* Logical remote keys (plan p.22 input mapper). The platform adapter maps
 * real remote events onto these once the TV's key codes are known. */
typedef enum { FT_NONE = 0, FT_LEFT, FT_RIGHT, FT_UP, FT_DOWN, FT_OK, FT_BACK } ft_key;

#define FT_TICK_HZ 30

/* Deterministic seeded generator (xorshift32) so games can be replayed. */
typedef struct { uint32_t s; } ft_rng;

void ft_rng_seed(ft_rng *r, uint32_t seed);
uint32_t ft_rng_next(ft_rng *r);
uint32_t ft_rng_range(ft_rng *r, uint32_t n);

#endif
