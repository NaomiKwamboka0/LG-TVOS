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
#include "motion.h"
#include "penalty.h"
#include "player.h"
#include "racer.h"
#include "shell.h"
#include "theme.h"

/* clock_text: Nairobi time already formatted by the caller, e.g. "20:01". */
void view_shell(ft_canvas *c, const shell *s, const char *clock_text);
/* Same, with the home screen drawn where the motion has got to (m may be NULL). */
void view_shell_motion(ft_canvas *c, const shell *s, const char *clock_text, const ui_motion *m);
/* Where the home screen's moving parts should end up for this state. */
void view_home_targets(const shell *s, ui_targets *t);
/* The start-up animation at t_ms (0..VIEW_STARTUP_MS). */
#define VIEW_STARTUP_MS 2600
void view_startup(ft_canvas *c, uint32_t t_ms, ft_lang lang);
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

/* The mouse: what is under a point of the 1280x720 layout. CARD: row,
 * col; TOP: col is the tab, TOP_GEAR or TOP_PROFILE; ITEM: col is the
 * list row; PIN: col is the digit. whole: the card is fully on screen
 * (hovering a card that only peeks in would slide the row under the
 * pointer, so only a click takes it). */
typedef enum { HIT_NONE, HIT_TOP, HIT_CARD, HIT_ITEM, HIT_PIN } view_hit_kind;
typedef struct {
    view_hit_kind kind;
    int row, col, whole;
} view_hit;
view_hit view_shell_hit(const shell *s, int x, int y);
/* The channel list row (channel index) under a point while watching, or -1. */
int view_live_hit(const live *l, int nch, int x, int y);

/* Layout helpers, exposed for tests (resting positions, before focus lift). */
ft_rect view_top_rect(const shell *s, int item); /* a tab, TOP_GEAR or TOP_PROFILE */
ft_rect view_home_card_rect(const shell *s, int row, int col);
ft_rect view_list_row_rect(int visible_row);
ft_rect view_pin_box_rect(int digit);
ft_rect view_live_panel_row_rect(int visible_row);
ft_rect view_key_rect(int key); /* the search keyboard */

#endif
