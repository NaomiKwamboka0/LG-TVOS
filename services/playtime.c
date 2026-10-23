#include "playtime.h"

#include <string.h>

static uint32_t sat_add(uint32_t a, uint32_t b)
{
    return a > UINT32_MAX - b ? UINT32_MAX : a + b;
}

static int64_t floor_div(int64_t a, int64_t b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

/* Civil date from day count (Howard Hinnant's algorithm). */
uint32_t play_nairobi_day(int64_t utc)
{
    int64_t z = floor_div(utc + 3 * 3600, 86400) + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    int64_t y = (int64_t)yoe + era * 400 + (m <= 2);
    return (uint32_t)(y * 10000 + m * 100 + d);
}

int play_clock_trusted(const play_policy *p, int64_t wall)
{
    return wall >= PLAY_TRUSTED_FLOOR_UTC && wall + PLAY_CLOCK_SLACK_S >= p->last_wall;
}

void play_observe_clock(play_policy *p, int64_t wall)
{
    uint32_t today;
    if (!play_clock_trusted(p, wall))
        return;
    today = play_nairobi_day(wall);
    if (p->day == 0) {
        p->day = today;
    } else if (today > p->day) {
        p->used_s = 0;
        p->bonus_s = 0;
        p->day = today;
    }
    if (wall > p->last_wall)
        p->last_wall = wall;
}

void play_add(play_policy *p, uint32_t seconds)
{
    p->used_s = sat_add(p->used_s, seconds);
}

uint32_t play_remaining(const play_policy *p)
{
    uint32_t total;
    if (p->daily_limit_s == 0)
        return PLAY_UNLIMITED;
    total = sat_add(p->daily_limit_s, p->bonus_s);
    return p->used_s >= total ? 0 : total - p->used_s;
}

int play_allowed(const play_policy *p)
{
    return play_remaining(p) > 0;
}

void play_grant(play_policy *p, uint32_t seconds)
{
    p->bonus_s = sat_add(p->bonus_s, seconds);
}

void play_reset_today(play_policy *p)
{
    p->used_s = 0;
    p->bonus_s = 0;
}

static void put32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o[2] = (uint8_t)(v >> 16); o[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *i)
{
    return (uint32_t)i[0] | ((uint32_t)i[1] << 8) | ((uint32_t)i[2] << 16) | ((uint32_t)i[3] << 24);
}

static int valid_day(uint32_t day)
{
    uint32_t y = day / 10000, m = day / 100 % 100, d = day % 100;
    return day == 0 || (y >= 2000 && y <= 2200 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

size_t play_pack(const play_policy *p, uint8_t *o, size_t cap)
{
    uint64_t w = (uint64_t)p->last_wall;
    if (cap < PLAY_PACKED_SIZE)
        return 0;
    put32(o, p->daily_limit_s);
    put32(o + 4, p->used_s);
    put32(o + 8, p->bonus_s);
    put32(o + 12, p->day);
    put32(o + 16, (uint32_t)w);
    put32(o + 20, (uint32_t)(w >> 32));
    return PLAY_PACKED_SIZE;
}

int play_unpack(play_policy *p, const uint8_t *in, size_t n)
{
    play_policy q;
    uint64_t w;
    if (n != PLAY_PACKED_SIZE)
        return -1;
    q.daily_limit_s = get32(in);
    q.used_s = get32(in + 4);
    q.bonus_s = get32(in + 8);
    q.day = get32(in + 12);
    w = (uint64_t)get32(in + 16) | ((uint64_t)get32(in + 20) << 32);
    if (!valid_day(q.day) || w > (uint64_t)INT64_MAX)
        return -1;
    q.last_wall = (int64_t)w;
    *p = q;
    return 0;
}
