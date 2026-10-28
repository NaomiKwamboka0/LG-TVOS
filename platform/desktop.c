/* Desktop adapter (macOS, Windows). Broadcast and channel-list views are
 * simulated; direct tuning and online apps are reported unsupported. */
#define _POSIX_C_SOURCE 200809L
#include "platform.h"

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <errno.h>
#include <windows.h>
#include <bcrypt.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

const char *plat_name(void) { return "SIMULATOR"; }
plat_status plat_open_broadcast(void) { return PLAT_OK; }
plat_status plat_tune_service(const char *service_name) { (void)service_name; return PLAT_UNSUPPORTED; }
plat_status plat_open_channel_list(void) { return PLAT_OK; }
plat_status plat_open_app(const char *app_id) { (void)app_id; return PLAT_UNSUPPORTED; }
plat_status plat_web_search(void) { return PLAT_UNSUPPORTED; }

#ifdef _WIN32

plat_status plat_random(void *buf, unsigned n)
{
    return BCryptGenRandom(NULL, (PUCHAR)buf, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? PLAT_OK : PLAT_FAILED;
}

uint64_t plat_monotonic_ms(void)
{
    return (uint64_t)GetTickCount64();
}

int plat_mkdir(const char *path)
{
    return _mkdir(path) == 0 || errno == EEXIST ? 0 : -1;
}

/* rename() refuses to overwrite on Windows; this replaces atomically. */
int plat_replace(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

#else

plat_status plat_random(void *buf, unsigned n)
{
    size_t got;
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f)
        return PLAT_FAILED;
    got = fread(buf, 1, n, f);
    fclose(f);
    return got == n ? PLAT_OK : PLAT_FAILED;
}

uint64_t plat_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int plat_mkdir(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

int plat_replace(const char *from, const char *to)
{
    return rename(from, to);
}

#endif

int64_t plat_wall_utc(void)
{
    return (int64_t)time(NULL);
}
