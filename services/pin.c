#include "pin.h"

#include <stdlib.h>
#include <string.h>

#include "monocypher.h"

int pin_valid_digits(const char *digits)
{
    int i;
    for (i = 0; i < PIN_DIGITS; i++)
        if (digits[i] < '0' || digits[i] > '9')
            return 0;
    return digits[PIN_DIGITS] == '\0';
}

static int derive(uint8_t out[PIN_HASH_SIZE], const char *digits, const uint8_t *salt,
                  uint32_t blocks, uint32_t passes)
{
    crypto_argon2_config cfg;
    crypto_argon2_inputs in;
    void *work;

    if (!pin_valid_digits(digits) || blocks < 8 || blocks > PIN_MAX_BLOCKS ||
        passes < 1 || passes > PIN_MAX_PASSES)
        return -1;
    work = malloc((size_t)blocks * 1024u);
    if (!work)
        return -1;
    cfg.algorithm = CRYPTO_ARGON2_ID;
    cfg.nb_blocks = blocks;
    cfg.nb_passes = passes;
    cfg.nb_lanes = 1;
    in.pass = (const uint8_t *)digits;
    in.pass_size = PIN_DIGITS;
    in.salt = salt;
    in.salt_size = PIN_SALT_SIZE;
    crypto_argon2(out, PIN_HASH_SIZE, work, cfg, in, crypto_argon2_no_extras);
    crypto_wipe(work, (size_t)blocks * 1024u);
    free(work);
    return 0;
}

int pin_make(pin_verifier *v, const char *digits, const uint8_t salt[PIN_SALT_SIZE],
             uint32_t blocks, uint32_t passes)
{
    pin_verifier tmp;
    memcpy(tmp.salt, salt, PIN_SALT_SIZE);
    tmp.blocks = blocks;
    tmp.passes = passes;
    if (derive(tmp.hash, digits, tmp.salt, blocks, passes) != 0)
        return -1;
    *v = tmp;
    crypto_wipe(&tmp, sizeof tmp);
    return 0;
}

int pin_check(const pin_verifier *v, const char *digits)
{
    uint8_t candidate[PIN_HASH_SIZE];
    int match;
    if (derive(candidate, digits, v->salt, v->blocks, v->passes) != 0)
        return -1;
    match = crypto_verify32(candidate, v->hash) == 0;
    crypto_wipe(candidate, sizeof candidate);
    return match;
}

static uint64_t lock_ms(uint32_t failures)
{
    uint64_t d = PIN_LOCK_BASE_MS;
    uint32_t extra;
    if (failures < PIN_FREE_ATTEMPTS)
        return 0;
    for (extra = failures - PIN_FREE_ATTEMPTS; extra > 0 && d < PIN_LOCK_MAX_MS; extra--)
        d *= 2;
    return d > PIN_LOCK_MAX_MS ? PIN_LOCK_MAX_MS : d;
}

void pin_guard_boot(pin_guard *g, uint64_t now_ms)
{
    if (g->failures > PIN_MAX_FAILURES)
        g->failures = PIN_MAX_FAILURES;
    g->locked_until_ms = g->failures >= PIN_FREE_ATTEMPTS ? now_ms + lock_ms(g->failures) : 0;
}

uint32_t pin_guard_wait_s(const pin_guard *g, uint64_t now_ms)
{
    if (now_ms >= g->locked_until_ms)
        return 0;
    return (uint32_t)((g->locked_until_ms - now_ms + 999u) / 1000u);
}

void pin_guard_fail(pin_guard *g, uint64_t now_ms)
{
    if (g->failures < PIN_MAX_FAILURES)
        g->failures++;
    g->locked_until_ms = now_ms + lock_ms(g->failures);
}

void pin_guard_success(pin_guard *g)
{
    g->failures = 0;
    g->locked_until_ms = 0;
}
