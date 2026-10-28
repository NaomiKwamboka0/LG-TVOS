#ifndef FT_PLAYTIME_H
#define FT_PLAYTIME_H

/* Children's daily play-time allowance (plan p.20, R07, test U08).
 * Play seconds come from a monotonic clock. The wall clock is used only to
 * find the Nairobi day, and only while it looks trustworthy.
 *
 * Declared policy:
 *  - the allowance resets at Nairobi midnight when the TV clock is trusted;
 *  - a clock earlier than 2026-01-01, or moved backwards, never resets it;
 *  - parents can give extra minutes or reset today after entering the PIN.
 * Known limit: moving the TV clock forward (TV settings access) can cause an
 * early reset. This is recorded, not hidden. */

#include <stddef.h>
#include <stdint.h>

#define PLAY_TRUSTED_FLOOR_UTC 1767225600 /* 2026-01-01T00:00:00Z */
#define PLAY_CLOCK_SLACK_S 300
#define PLAY_UNLIMITED UINT32_MAX
#define PLAY_PACKED_SIZE 24

typedef struct {
    uint32_t daily_limit_s; /* 0 = no limit */
    uint32_t used_s;
    uint32_t bonus_s;
    uint32_t day;      /* Nairobi date YYYYMMDD of used_s; 0 = not yet known */
    int64_t last_wall; /* latest trusted wall clock seen, UTC seconds */
} play_policy;

uint32_t play_nairobi_day(int64_t utc);
int play_clock_trusted(const play_policy *p, int64_t wall);
void play_observe_clock(play_policy *p, int64_t wall);
void play_add(play_policy *p, uint32_t seconds);
uint32_t play_remaining(const play_policy *p);
int play_allowed(const play_policy *p);
void play_grant(play_policy *p, uint32_t seconds);
void play_reset_today(play_policy *p);
size_t play_pack(const play_policy *p, uint8_t *out, size_t cap);
int play_unpack(play_policy *p, const uint8_t *in, size_t n);

/* Measures active play from a monotonic clock. A single step never counts
 * more than PLAY_METER_MAX_STEP_MS, so a laptop sleeping mid-game (lid
 * closed) does not charge the child for the time it slept. */
#define PLAY_METER_MAX_STEP_MS 1000u

typedef struct {
    uint64_t last_ms;
    uint32_t carry_ms;
} play_meter;

void play_meter_start(play_meter *m, uint64_t now_ms);
/* Returns the whole seconds of play to charge since the last step. */
uint32_t play_meter_step(play_meter *m, uint64_t now_ms, int active);

#endif
