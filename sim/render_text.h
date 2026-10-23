#ifndef FT_RENDER_TEXT_H
#define FT_RENDER_TEXT_H

/* Text renderer for the host simulator only. The TV renderer is chosen
 * after gate I05 identifies the display interface. */

#include "garden.h"
#include "penalty.h"
#include "racer.h"
#include "shell.h"

void render_shell(const shell *s);
void render_racer(const racer *r, const uint32_t best[RACER_LEVELS], const char *status);
void render_penalty(const penalty *p, const char *status);
void render_garden(const garden *g, const char *status);

#endif
