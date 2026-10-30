#include "prefs.h"

#include <stdint.h>

#include "channels.h"
#include "save.h"

static const char MAGIC[4] = {'P', 'L', 'S', 'V'};
#define SCHEMA 1

void prefs_defaults(player_prefs *p)
{
    p->last_channel = 0;
    p->volume = 80;
    p->fullscreen = 0;
}

/* Layout: last_channel u16, volume u8, fullscreen u8, reserved[4] = 0. */
int prefs_load(player_prefs *p, const char *path)
{
    uint8_t b[PREFS_PACKED_SIZE] = {0};
    uint32_t len = 0;
    int r = ft_save_read(path, MAGIC, SCHEMA, b, sizeof b, &len);
    int ch = b[0] | (b[1] << 8);
    if (r >= 0 && len == PREFS_PACKED_SIZE && ch <= CH_NUMBER_MAX && b[2] <= 100 && b[3] <= 1) {
        p->last_channel = ch;
        p->volume = b[2];
        p->fullscreen = b[3];
        return r;
    }
    prefs_defaults(p);
    return r < 0 ? r : FT_SAVE_CORRUPT;
}

int prefs_save(const player_prefs *p, const char *path)
{
    uint8_t b[PREFS_PACKED_SIZE] = {0};
    int ch = p->last_channel < 0 || p->last_channel > CH_NUMBER_MAX ? 0 : p->last_channel;
    int vol = p->volume < 0 ? 0 : p->volume > 100 ? 100 : p->volume;
    b[0] = (uint8_t)ch;
    b[1] = (uint8_t)(ch >> 8);
    b[2] = (uint8_t)vol;
    b[3] = p->fullscreen ? 1 : 0;
    return ft_save_write(path, MAGIC, SCHEMA, b, sizeof b);
}
