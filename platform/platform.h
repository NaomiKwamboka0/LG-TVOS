#ifndef FT_PLATFORM_H
#define FT_PLATFORM_H

/* Platform adapter (plan p.7, p.9): our own interface, not an LG API.
 * Every operation returns PLAT_UNSUPPORTED until the matching device
 * interface has been identified and verified on this TV. */

#include <stdint.h>

typedef enum { PLAT_OK = 0, PLAT_UNSUPPORTED = -1, PLAT_FAILED = -2 } plat_status;

const char *plat_name(void);
plat_status plat_open_broadcast(void);
plat_status plat_tune_service(const char *service_name);
plat_status plat_open_channel_list(void);
plat_status plat_open_app(const char *app_id);
plat_status plat_web_search(void);

/* Cryptographic random bytes; callers must fail closed on error. */
plat_status plat_random(void *buf, unsigned n);
/* Wall clock in UTC seconds. May be wrong; never treat it as trusted by itself. */
int64_t plat_wall_utc(void);
/* Monotonic milliseconds from an arbitrary origin; never goes backwards. */
uint64_t plat_monotonic_ms(void);

#endif
