#ifndef FT_FRONTEND_H
#define FT_FRONTEND_H

/* Frontends: the desktop window showing the pixel views (the app), or a
 * text terminal (a development tool). Both drive the same shell, session,
 * games and live TV. */

#include <stddef.h>
#include <stdint.h>

#include "garden.h"
#include "live.h"
#include "penalty.h"
#include "player.h"
#include "racer.h"
#include "shell.h"

typedef struct {
    const char *name;
    /* scripted: keys come from --keys, so no interactive input setup. */
    int (*open)(int scripted);
    void (*close)(void);
    /* Waits up to ms for a key (0 = until a key arrives). Sets *quit when the user quits. */
    ft_key (*poll)(int ms, int *quit);
    void (*shell)(const shell *s);
    void (*racer)(const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang, const char *status);
    void (*penalty)(const penalty *p, ft_lang lang, const char *status);
    void (*garden)(const garden *g, ft_lang lang, const char *status);
    /* Draws the newest picture from p with the live TV overlays. */
    void (*live)(const live *l, player *p, const shell *s);
    /* Thread-safe: makes a waiting poll() return FT_NONE now. May be NULL. */
    void (*wake)(void *ctx);
} sim_frontend;

extern const sim_frontend frontend_text;
extern const sim_frontend frontend_window;

/* Optional: the window frontend writes its last frame here (PPM) on close. */
extern const char *sim_dump_path;

/* Nairobi wall-clock time as HH:MM (UTC+3, no DST). */
void sim_clock_text(char *out, size_t n);

#endif
