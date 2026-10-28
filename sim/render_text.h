#ifndef FT_RENDER_TEXT_H
#define FT_RENDER_TEXT_H

/* Text renderer for the terminal frontend (development and smoke tests). */

#include "garden.h"
#include "live.h"
#include "penalty.h"
#include "player.h"
#include "racer.h"
#include "shell.h"

void render_shell(const shell *s);
void render_racer(const racer *r, const uint32_t best[RACER_LEVELS], const char *status);
void render_penalty(const penalty *p, const char *status);
void render_garden(const garden *g, const char *status);
void render_live(const live *l, const player_status *ps, const shell *s, const channel_list *chs);

#endif
