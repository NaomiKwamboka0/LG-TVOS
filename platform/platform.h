#ifndef FT_PLATFORM_H
#define FT_PLATFORM_H

/* Platform adapter: our own interface over the desktop OS (macOS, Windows). */

#include <stdint.h>

typedef enum { PLAT_OK = 0, PLAT_UNSUPPORTED = -1, PLAT_FAILED = -2 } plat_status;

/* Cryptographic random bytes; callers must fail closed on error. */
plat_status plat_random(void *buf, unsigned n);
/* Wall clock in UTC seconds. May be wrong; never treat it as trusted by itself. */
int64_t plat_wall_utc(void);
/* Monotonic milliseconds from an arbitrary origin; never goes backwards. */
uint64_t plat_monotonic_ms(void);

/* 0 if the folder exists afterwards. */
int plat_mkdir(const char *path);
/* Renames from over to, replacing to if it exists. 0 on success. */
int plat_replace(const char *from, const char *to);

/* Opens a web address as an app: full screen in a browser window of its
 * own, without tabs or an address bar, signed in as the family's browser
 * is (Edge on Windows; Chrome, Edge or Brave on a Mac). PLAT_UNSUPPORTED
 * when no such browser is installed; the caller then uses the normal one. */
plat_status plat_open_app_window(const char *url);

#endif
