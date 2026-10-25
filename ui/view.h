#ifndef FT_VIEW_H
#define FT_VIEW_H

/* Pixel views of the shell and the three games at logical 1280x720
 * (plan p.14), in an Apple TV-inspired style (see theme.h). Pure functions
 * of state: the same state always draws the same pixels, and nothing reads
 * clocks or files. */

#include <stdint.h>

#include "canvas.h"
#include "garden.h"
#include "penalty.h"
#include "racer.h"
#include "shell.h"
#include "theme.h"

/* clock_text: Nairobi time already formatted by the caller, e.g. "20:01". */
void view_shell(ft_canvas *c, const shell *s, const char *clock_text);
void view_racer(ft_canvas *c, const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang,
                const char *status);
void view_penalty(ft_canvas *c, const penalty *p, ft_lang lang, const char *status);
void view_garden(ft_canvas *c, const garden *g, ft_lang lang, const char *status);

/* Layout helpers, exposed for tests (resting positions, before focus lift). */
ft_rect view_home_tile_rect(const shell *s, int index);
ft_rect view_list_row_rect(int visible_row);
ft_rect view_pin_box_rect(int digit);

#endif
