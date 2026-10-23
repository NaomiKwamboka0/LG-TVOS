#include "penalty.h"

#include <string.h>

void penalty_init(penalty *p, uint32_t seed, int sudden_death)
{
    memset(p, 0, sizeof *p);
    p->seed = seed;
    ft_rng_seed(&p->rng, seed);
    p->sudden_death = sudden_death;
    p->aim = PEN_TARGETS / 2;
    p->winner = PEN_NO_WINNER;
    p->phase = PEN_AIMING;
}

int penalty_turn(const penalty *p)
{
    return p->shots[0] > p->shots[1];
}

int penalty_peek_keeper(const penalty *p)
{
    ft_rng copy = p->rng;
    return (int)ft_rng_range(&copy, PEN_TARGETS);
}

static int decide(const penalty *p)
{
    int a = p->goals[0], b = p->goals[1];
    int sa = p->shots[0], sb = p->shots[1];

    if (sa < PEN_REGULATION || sb < PEN_REGULATION) {
        int left_a = PEN_REGULATION - sa, left_b = PEN_REGULATION - sb;
        if (a > b + left_b)
            return 0;
        if (b > a + left_a)
            return 1;
        return PEN_NO_WINNER;
    }
    if (sa != sb)
        return PEN_NO_WINNER;
    if (a != b)
        return a > b ? 0 : 1;
    if (!p->sudden_death || sa >= PEN_MAX_SHOTS)
        return PEN_TIE;
    return PEN_NO_WINNER;
}

static void shoot(penalty *p)
{
    int t = penalty_turn(p);
    p->keeper = (int)ft_rng_range(&p->rng, PEN_TARGETS);
    p->scored = p->keeper != p->aim;
    if (p->scored) {
        p->goals[t]++;
        p->history[t] |= 1u << p->shots[t];
    }
    p->shots[t]++;
    p->cooldown = PEN_RESULT_COOLDOWN;
    p->winner = decide(p);
    p->phase = PEN_RESULT;
}

static void pause_from(penalty *p, pen_phase from)
{
    p->resume_phase = from;
    p->phase = PEN_PAUSED;
}

void penalty_key(penalty *p, ft_key k)
{
    switch (p->phase) {
    case PEN_AIMING:
        if (k == FT_LEFT && p->aim > 0)
            p->aim--;
        else if (k == FT_RIGHT && p->aim < PEN_TARGETS - 1)
            p->aim++;
        else if (k == FT_OK)
            shoot(p);
        else if (k == FT_BACK)
            pause_from(p, PEN_AIMING);
        break;
    case PEN_RESULT:
        if (k == FT_OK && p->cooldown == 0) {
            p->phase = p->winner == PEN_NO_WINNER ? PEN_AIMING : PEN_OVER;
            p->aim = PEN_TARGETS / 2;
        } else if (k == FT_BACK) {
            pause_from(p, PEN_RESULT);
        }
        break;
    case PEN_OVER:
        if (k == FT_OK)
            penalty_init(p, p->seed + 1, p->sudden_death);
        else if (k == FT_BACK)
            p->phase = PEN_EXITED;
        break;
    case PEN_PAUSED:
        if (k == FT_OK)
            p->phase = p->resume_phase;
        else if (k == FT_BACK)
            p->phase = PEN_EXITED;
        break;
    case PEN_EXITED:
        break;
    }
}

void penalty_tick(penalty *p)
{
    if (p->phase == PEN_RESULT && p->cooldown > 0)
        p->cooldown--;
}

static void put32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o[2] = (uint8_t)(v >> 16); o[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *i)
{
    return (uint32_t)i[0] | ((uint32_t)i[1] << 8) | ((uint32_t)i[2] << 16) | ((uint32_t)i[3] << 24);
}

/* Layout: seed u32, rng u32, hist0 u32, hist1 u32, shots0..1 u8, goals0..1 u8, sd u8, winner i8 */
size_t penalty_pack(const penalty *p, uint8_t *o, size_t cap)
{
    if (cap < PEN_PACKED_SIZE)
        return 0;
    put32(o, p->seed);
    put32(o + 4, p->rng.s);
    put32(o + 8, p->history[0]);
    put32(o + 12, p->history[1]);
    o[16] = (uint8_t)p->shots[0];
    o[17] = (uint8_t)p->shots[1];
    o[18] = (uint8_t)p->goals[0];
    o[19] = (uint8_t)p->goals[1];
    o[20] = (uint8_t)(p->sudden_death ? 1 : 0);
    o[21] = (uint8_t)(int8_t)p->winner;
    return PEN_PACKED_SIZE;
}

int penalty_unpack(penalty *p, const uint8_t *in, size_t n)
{
    penalty q;
    int i;
    if (n != PEN_PACKED_SIZE || in[20] > 1)
        return -1;
    penalty_init(&q, get32(in), in[20]);
    q.rng.s = get32(in + 4);
    q.history[0] = get32(in + 8);
    q.history[1] = get32(in + 12);
    for (i = 0; i < 2; i++) {
        q.shots[i] = in[16 + i];
        q.goals[i] = in[18 + i];
        if (q.shots[i] > PEN_MAX_SHOTS || q.goals[i] > q.shots[i])
            return -1;
    }
    if (q.shots[0] - q.shots[1] < 0 || q.shots[0] - q.shots[1] > 1 || q.rng.s == 0)
        return -1;
    q.winner = (int8_t)in[21];
    if (q.winner < PEN_NO_WINNER || q.winner > PEN_TIE)
        return -1;
    q.phase = q.winner == PEN_NO_WINNER ? PEN_AIMING : PEN_OVER;
    *p = q;
    return 0;
}
