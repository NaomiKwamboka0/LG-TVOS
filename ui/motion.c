#include "motion.h"

#include <string.h>

/* About two thirds of the way there every 60 ms: quick, but visibly smooth. */
static int ease(int *v, int target, int elapsed_ms)
{
    int d = target - *v, step;
    if (d == 0)
        return 0;
    step = (int)((long)d * (elapsed_ms > 120 ? 120 : elapsed_ms) / 60);
    if (step == 0 || (d > 0 ? step > d : step < d))
        step = d;
    *v += step;
    if (*v != target && (d > 0 ? target - *v : *v - target) <= 1)
        *v = target;
    return *v != target;
}

int motion_step(ui_motion *m, const ui_targets *t, int tab, int elapsed_ms)
{
    int moving = 0, i;
    if (elapsed_ms < 0)
        elapsed_ms = 0;
    m->ambient += (uint32_t)elapsed_ms;
    if (!m->valid || tab != m->tab) {
        int tx = m->now.tab_x, tw = m->now.tab_w, keep = m->valid;
        m->now = *t;
        if (keep) { /* the highlight still glides over to the new tab */
            m->now.tab_x = tx;
            m->now.tab_w = tw;
        }
        m->tab = tab;
        m->zoom = 0;
        m->valid = 1;
    }
    if (t->focus_key != m->now.focus_key) {
        m->now.focus_key = t->focus_key;
        m->zoom = 0;
    }
    moving |= ease(&m->now.page_y, t->page_y, elapsed_ms);
    for (i = 0; i < SHELL_MAX_ROWS; i++)
        moving |= ease(&m->now.row_x[i], t->row_x[i], elapsed_ms);
    moving |= ease(&m->now.tab_x, t->tab_x, elapsed_ms);
    moving |= ease(&m->now.tab_w, t->tab_w, elapsed_ms);
    if (m->zoom < MOTION_ZOOM_FULL) {
        m->zoom += elapsed_ms * MOTION_ZOOM_FULL / 140;
        if (m->zoom > MOTION_ZOOM_FULL)
            m->zoom = MOTION_ZOOM_FULL;
        moving = 1;
    }
    return moving;
}
