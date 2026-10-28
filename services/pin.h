#ifndef FT_PIN_H
#define FT_PIN_H

/* Parent PIN (plan p.16): salted Argon2id verifier (Monocypher) and
 * rate-limited attempts. The PIN itself is never stored or logged. This
 * deters normal child use; it does not protect against someone with
 * administrator or unrestricted physical access. */

#include <stdint.h>

#define PIN_DIGITS 4
#define PIN_SALT_SIZE 16
#define PIN_HASH_SIZE 32
/* Argon2id cost: well under a second on a budget laptop (plan p.16). */
#define PIN_DEFAULT_BLOCKS 4096u /* KiB of work memory */
#define PIN_DEFAULT_PASSES 3u
#define PIN_MAX_BLOCKS 16384u
#define PIN_MAX_PASSES 16u

#define PIN_FREE_ATTEMPTS 3
#define PIN_LOCK_BASE_MS 30000u
#define PIN_LOCK_MAX_MS 300000u
#define PIN_MAX_FAILURES 1000u

typedef struct {
    uint8_t salt[PIN_SALT_SIZE];
    uint8_t hash[PIN_HASH_SIZE];
    uint32_t blocks;
    uint32_t passes;
} pin_verifier;

typedef struct {
    uint32_t failures;
    uint64_t locked_until_ms; /* monotonic */
} pin_guard;

int pin_valid_digits(const char *digits);
int pin_make(pin_verifier *v, const char *digits, const uint8_t salt[PIN_SALT_SIZE],
             uint32_t blocks, uint32_t passes);
/* 1 = match, 0 = mismatch, -1 = error. */
int pin_check(const pin_verifier *v, const char *digits);

/* Failures are persisted, so a restart re-applies the lockout instead of clearing it. */
void pin_guard_boot(pin_guard *g, uint64_t now_ms);
uint32_t pin_guard_wait_s(const pin_guard *g, uint64_t now_ms);
void pin_guard_fail(pin_guard *g, uint64_t now_ms);
void pin_guard_success(pin_guard *g);

#endif
