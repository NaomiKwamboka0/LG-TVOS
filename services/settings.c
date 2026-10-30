#include "settings.h"

#include <string.h>

#include "i18n.h"
#include "save.h"

static const char MAGIC[4] = {'S', 'T', 'S', 'V'};
#define SCHEMA 2

void settings_defaults(family_settings *f)
{
    memset(f, 0, sizeof *f);
    f->lang = LANG_EN;
    f->child_games = SETTINGS_ALL_GAMES;
    f->quality = QUALITY_1080; /* sport looks best at 1080p, 60 fps where offered */
}

static void put32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o[2] = (uint8_t)(v >> 16); o[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *i)
{
    return (uint32_t)i[0] | ((uint32_t)i[1] << 8) | ((uint32_t)i[2] << 16) | ((uint32_t)i[3] << 24);
}

/* Layout: lang, profile, child_games, pin_set, salt[16], hash[32],
 * blocks u32, passes u32, failures u32, play[24], then (schema 2)
 * quality, reserved[3] = 0. */
size_t settings_pack(const family_settings *f, uint8_t *o, size_t cap)
{
    if (cap < SETTINGS_PACKED_SIZE)
        return 0;
    o[0] = f->lang;
    o[1] = f->profile;
    o[2] = f->child_games;
    o[3] = f->pin_set;
    memcpy(o + 4, f->pin.salt, PIN_SALT_SIZE);
    memcpy(o + 20, f->pin.hash, PIN_HASH_SIZE);
    put32(o + 52, f->pin.blocks);
    put32(o + 56, f->pin.passes);
    put32(o + 60, f->pin_failures);
    if (play_pack(&f->play, o + 64, cap - 64) != PLAY_PACKED_SIZE)
        return 0;
    o[88] = f->quality;
    o[89] = o[90] = o[91] = 0;
    return SETTINGS_PACKED_SIZE;
}

int settings_unpack(family_settings *f, const uint8_t *in, size_t n)
{
    family_settings q;
    if (n != SETTINGS_PACKED_SIZE && n != SETTINGS_V1_SIZE)
        return -1;
    settings_defaults(&q);
    q.lang = in[0];
    q.profile = in[1];
    q.child_games = in[2];
    q.pin_set = in[3];
    memcpy(q.pin.salt, in + 4, PIN_SALT_SIZE);
    memcpy(q.pin.hash, in + 20, PIN_HASH_SIZE);
    q.pin.blocks = get32(in + 52);
    q.pin.passes = get32(in + 56);
    q.pin_failures = get32(in + 60);
    if (q.lang >= LANG_COUNT || q.profile > 1 || q.child_games > SETTINGS_ALL_GAMES || q.pin_set > 1)
        return -1;
    if (q.pin_set && (q.pin.blocks < 8 || q.pin.blocks > PIN_MAX_BLOCKS ||
                      q.pin.passes < 1 || q.pin.passes > PIN_MAX_PASSES))
        return -1;
    if (q.profile == 1 && !q.pin_set) /* a child profile always has a parent PIN behind it */
        return -1;
    if (q.pin_failures > PIN_MAX_FAILURES)
        q.pin_failures = PIN_MAX_FAILURES;
    if (play_unpack(&q.play, in + 64, PLAY_PACKED_SIZE) != 0)
        return -1;
    if (n == SETTINGS_PACKED_SIZE) {
        q.quality = in[88];
        if (q.quality >= QUALITY_COUNT)
            return -1;
    }
    *f = q;
    return 0;
}

int settings_load(family_settings *f, const char *path)
{
    uint8_t buf[SETTINGS_PACKED_SIZE];
    uint32_t len = 0;
    uint16_t schema = 0;
    /* Schema 1 files (before live TV) must keep their PIN and child profile:
     * falling back to defaults would quietly drop the parental controls. */
    int r = ft_save_read_range(path, MAGIC, 1, SCHEMA, buf, sizeof buf, &len, &schema);
    if (r >= 0 && len == (schema == 1 ? SETTINGS_V1_SIZE : SETTINGS_PACKED_SIZE) &&
        settings_unpack(f, buf, len) == 0)
        return r;
    settings_defaults(f);
    return r < 0 ? r : FT_SAVE_CORRUPT;
}

int settings_save(const family_settings *f, const char *path)
{
    uint8_t buf[SETTINGS_PACKED_SIZE];
    if (settings_pack(f, buf, sizeof buf) != SETTINGS_PACKED_SIZE)
        return FT_SAVE_IO;
    return ft_save_write(path, MAGIC, SCHEMA, buf, SETTINGS_PACKED_SIZE);
}
