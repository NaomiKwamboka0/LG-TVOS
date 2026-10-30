#ifndef FT_MOTION_H
#define FT_MOTION_H

/* Smooth TV motion for the home screen. The layout says where things
 * should be (ui_targets); the motion eases the drawn positions towards
 * that, frame by frame, so rows slide, the page scrolls, the tab
 * highlight glides and the focused card grows instead of jumping. Also
 * keeps the clock of the moving background and leans the focused card
 * toward the mouse pointer. Integer maths, no state in
 * the views: they draw whatever the motion says. */

#include <stdint.h>

#include "shell.h"

#define MOTION_ZOOM_FULL 256

typedef struct {
    int page_y;                /* vertical scroll of the rows, logical px */
    int row_x[SHELL_MAX_ROWS]; /* sideways scroll of each row, logical px */
    int tab_x, tab_w;          /* the tab highlight */
    int focus_key;             /* which card the focus is on (tab, row, column) */
    /* The focused card leans toward the mouse pointer: where the pointer is
     * across it, -256 (left/top edge) .. 256 (right/bottom); 0 = upright. */
    int tilt_x, tilt_y;
} ui_targets;

typedef struct {
    ui_targets now;   /* drawn values */
    int zoom;         /* focused card growth, 0..MOTION_ZOOM_FULL */
    int tab;          /* tab the rows belong to */
    uint32_t ambient; /* moving background clock, ms */
    int valid;
} ui_motion;

/* Moves towards t by elapsed_ms. A new tab jumps the rows into place; the
 * tab highlight still glides. Returns 1 while anything is still moving. */
int motion_step(ui_motion *m, const ui_targets *t, int tab, int elapsed_ms);

#endif
