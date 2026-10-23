#define _POSIX_C_SOURCE 200809L
#include "save.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HDR 20 /* magic[4] schema u16 reserved u16 seq u32 len u32 checksum u32 */
#define PATH_MAX_LEN 512

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t fnv(const uint8_t *p, uint32_t n, uint32_t h)
{
    while (n--) {
        h ^= *p++;
        h *= 16777619u;
    }
    return h;
}

static int read_one(const char *path, const char magic[4], uint16_t schema,
                    void *buf, uint32_t cap, uint32_t *len, uint32_t *seq)
{
    uint8_t h[HDR];
    uint32_t n;
    FILE *f = fopen(path, "rb");
    if (!f)
        return FT_SAVE_NONE;
    if (fread(h, 1, HDR, f) != HDR) {
        fclose(f);
        return FT_SAVE_CORRUPT;
    }
    n = get32(h + 12);
    if (memcmp(h, magic, 4) != 0 || get16(h + 4) != schema || n > cap || n > FT_SAVE_MAX) {
        fclose(f);
        return FT_SAVE_CORRUPT;
    }
    if (fread(buf, 1, n, f) != n || fgetc(f) != EOF) {
        fclose(f);
        return FT_SAVE_CORRUPT;
    }
    fclose(f);
    if (fnv((const uint8_t *)buf, n, fnv(h, 16, 2166136261u)) != get32(h + 16))
        return FT_SAVE_CORRUPT;
    if (len)
        *len = n;
    if (seq)
        *seq = get32(h + 8);
    return FT_SAVE_OK;
}

static int write_all(int fd, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0)
            return -1;
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

static void sync_parent_dir(const char *path)
{
    char dir[PATH_MAX_LEN];
    const char *slash = strrchr(path, '/');
    int fd;
    if (!slash) {
        strcpy(dir, ".");
    } else {
        size_t n = (size_t)(slash - path);
        if (n >= sizeof dir)
            return;
        memcpy(dir, path, n);
        dir[n] = '\0';
        if (n == 0)
            strcpy(dir, "/");
    }
    fd = open(dir, O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
}

static int with_suffix(char *out, const char *path, const char *suffix)
{
    return snprintf(out, PATH_MAX_LEN, "%s%s", path, suffix) < PATH_MAX_LEN ? 0 : -1;
}

int ft_save_write(const char *path, const char magic[4], uint16_t schema,
                  const void *data, uint32_t len)
{
    char tmp[PATH_MAX_LEN], prev[PATH_MAX_LEN];
    uint8_t h[HDR];
    uint32_t seq = 0;
    int current_valid, fd, ok;
    void *scratch;

    if (len > FT_SAVE_MAX)
        return FT_SAVE_TOO_BIG;
    if (with_suffix(tmp, path, ".tmp") || with_suffix(prev, path, ".prev"))
        return FT_SAVE_IO;

    /* Only a valid current file may become the last-known-good copy. */
    scratch = malloc(FT_SAVE_MAX);
    if (!scratch)
        return FT_SAVE_IO;
    current_valid = read_one(path, magic, schema, scratch, FT_SAVE_MAX, NULL, &seq) == FT_SAVE_OK;
    free(scratch);

    memcpy(h, magic, 4);
    put16(h + 4, schema);
    put16(h + 6, 0);
    put32(h + 8, seq + 1);
    put32(h + 12, len);
    put32(h + 16, fnv((const uint8_t *)data, len, fnv(h, 16, 2166136261u)));

    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return FT_SAVE_IO;
    ok = write_all(fd, h, HDR) == 0 && write_all(fd, data, len) == 0 && fsync(fd) == 0;
    if (close(fd) != 0)
        ok = 0;
    if (!ok) {
        unlink(tmp);
        return FT_SAVE_IO;
    }
    if (current_valid && rename(path, prev) != 0) {
        unlink(tmp);
        return FT_SAVE_IO;
    }
    if (rename(tmp, path) != 0)
        return FT_SAVE_IO;
    sync_parent_dir(path);
    return FT_SAVE_OK;
}

int ft_save_read(const char *path, const char magic[4], uint16_t schema,
                 void *buf, uint32_t cap, uint32_t *out_len)
{
    char prev[PATH_MAX_LEN], bad[PATH_MAX_LEN];
    int r = read_one(path, magic, schema, buf, cap, out_len, NULL);
    if (r == FT_SAVE_OK)
        return r;
    if (r == FT_SAVE_CORRUPT && with_suffix(bad, path, ".bad") == 0)
        rename(path, bad);
    if (with_suffix(prev, path, ".prev") == 0 &&
        read_one(prev, magic, schema, buf, cap, out_len, NULL) == FT_SAVE_OK)
        return FT_SAVE_RESTORED_PREV;
    return r;
}
