#include "common.h"

void ft_rng_seed(ft_rng *r, uint32_t seed)
{
    r->s = seed ? seed : 0x9E3779B9u;
}

uint32_t ft_rng_next(ft_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

uint32_t ft_rng_range(ft_rng *r, uint32_t n)
{
    return n ? ft_rng_next(r) % n : 0;
}
