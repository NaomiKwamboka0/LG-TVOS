#ifndef FT_FRONTEND_H
#define FT_FRONTEND_H

/* Simulator frontends: the text terminal, or a desktop window showing the
 * real pixel views. Both drive the same shell, session and games. Host
 * only; the TV gets its own display/input backend after gate I05. */

#include <stddef.h>
#include <stdint.h>

#include "garden.h"
#include "penalty.h"
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
} sim_frontend;

extern const sim_frontend frontend_text;
extern const sim_frontend frontend_window;

/* Optional: the window frontend writes its last frame here (PPM) on close. */
extern const char *sim_dump_path;

/* Nairobi wall-clock time as HH:MM (UTC+3, no DST). */
void sim_clock_text(char *out, size_t n);

#endif
