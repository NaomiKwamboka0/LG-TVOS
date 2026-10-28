#ifndef FT_VIEW_H
#define FT_VIEW_H

/* Pixel views of the shell, live TV and the three games at logical
 * 1280x720, in a TCL / Google TV-inspired style (see theme.h). Pure
 * functions of state: the same state always draws the same pixels, and
 * nothing reads clocks or files. */

#include <stdint.h>

#include "canvas.h"
#include "garden.h"
#include "live.h"
#include "penalty.h"
#include "player.h"
#include "racer.h"
#include "shell.h"
#include "theme.h"

/* clock_text: Nairobi time already formatted by the caller, e.g. "20:01". */
void view_shell(ft_canvas *c, const shell *s, const char *clock_text);
void view_racer(ft_canvas *c, const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang,
                const char *status);
void view_penalty(ft_canvas *c, const penalty *p, ft_lang lang, const char *status);
void view_garden(ft_canvas *c, const garden *g, ft_lang lang, const char *status);
/* Live TV overlays. The canvas already holds the newest picture when
 * has_picture is set; otherwise the view paints the background itself. */
void view_live(ft_canvas *c, const live *l, const player_status *ps, const shell *s, const channel_list *chs,
               const char *clock_text, int has_picture);
/* Changes whenever view_live would draw different overlays for the same
 * picture, so a caller can skip repainting while nothing moves. */
uint32_t view_live_signature(const live *l, const player_status *ps, const shell *s, const channel_list *chs,
                             const char *clock_text, int has_picture);

/* Layout helpers, exposed for tests (resting positions, before focus lift). */
ft_rect view_top_rect(const shell *s, int item); /* a tab, TOP_GEAR or TOP_PROFILE */
ft_rect view_home_card_rect(const shell *s, int row, int col);
ft_rect view_list_row_rect(int visible_row);
ft_rect view_pin_box_rect(int digit);
ft_rect view_live_panel_row_rect(int visible_row);

#endif
