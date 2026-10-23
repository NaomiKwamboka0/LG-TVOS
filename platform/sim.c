/* Host simulator adapter. Broadcast and channel-list views are simulated;
 * direct tuning and online apps are reported unsupported, matching what is
 * currently verified on the real TV (nothing). */
#define _POSIX_C_SOURCE 200809L
#include "platform.h"

#include <stdio.h>
#include <time.h>

const char *plat_name(void) { return "SIMULATOR"; }
plat_status plat_open_broadcast(void) { return PLAT_OK; }
plat_status plat_tune_service(const char *service_name) { (void)service_name; return PLAT_UNSUPPORTED; }
plat_status plat_open_channel_list(void) { return PLAT_OK; }
plat_status plat_open_app(const char *app_id) { (void)app_id; return PLAT_UNSUPPORTED; }
plat_status plat_web_search(void) { return PLAT_UNSUPPORTED; }

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

int64_t plat_wall_utc(void)
{
    return (int64_t)time(NULL);
}

uint64_t plat_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
