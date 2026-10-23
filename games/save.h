#ifndef FT_SAVE_H
#define FT_SAVE_H

#include <stdint.h>

/* Versioned, checksummed save files (plan p.16 persistence contract, p.22 save service).
 * Write: temp file, fsync, keep last-known-good as <path>.prev, atomic rename.
 * Read: on corruption, quarantine to <path>.bad and fall back to <path>.prev. */

#define FT_SAVE_MAX (256u * 1024u)

enum {
    FT_SAVE_OK = 0,
    FT_SAVE_RESTORED_PREV = 1,
    FT_SAVE_NONE = -1,
    FT_SAVE_CORRUPT = -2,
    FT_SAVE_IO = -3,
    FT_SAVE_TOO_BIG = -4
};

int ft_save_write(const char *path, const char magic[4], uint16_t schema,
                  const void *data, uint32_t len);
int ft_save_read(const char *path, const char magic[4], uint16_t schema,
                 void *buf, uint32_t cap, uint32_t *out_len);

#endif
