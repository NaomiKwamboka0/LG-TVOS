#ifndef FT_SETTINGS_H
#define FT_SETTINGS_H

/* Family settings stored on the TV (plan p.16): language, last profile,
 * games allowed for children, parent PIN verifier, failed-attempt count and
 * the play-time policy. No PIN, account data or TV identifiers. */

#include <stddef.h>
#include <stdint.h>

#include "pin.h"
#include "playtime.h"

#define SETTINGS_PACKED_SIZE 88
#define SETTINGS_ALL_GAMES 0x7u

typedef struct {
    uint8_t lang;        /* ft_lang */
    uint8_t profile;     /* 0 parent, 1 child */
    uint8_t child_games; /* bit per game */
    uint8_t pin_set;
    pin_verifier pin;
    uint32_t pin_failures;
    play_policy play;
} family_settings;

void settings_defaults(family_settings *f);
size_t settings_pack(const family_settings *f, uint8_t *out, size_t cap);
int settings_unpack(family_settings *f, const uint8_t *in, size_t n);
/* Returns an FT_SAVE_* status; on any failure *f holds defaults. */
int settings_load(family_settings *f, const char *path);
int settings_save(const family_settings *f, const char *path);

#endif
