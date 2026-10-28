#include "live.h"

#include <string.h>

void live_init(live *l, int nch, int channel, int volume)
{
    memset(l, 0, sizeof *l);
    l->nch = nch;
    l->current = l->shown = channel;
    l->banner_ms = LIVE_BANNER_MS;
    l->volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
}

static live_action act(live_act a, int arg)
{
    live_action out;
    out.act = a;
    out.arg = arg;
    return out;
}

static live_action tune_now(live *l, int ch)
{
    l->current = l->shown = ch;
    l->tune_wait_ms = 0;
    l->banner_ms = LIVE_BANNER_MS;
    return act(LIVE_ACT_TUNE, ch);
}

static live_action panel_key(live *l, ft_key k)
{
    if (k == FT_UP && l->panel_item > 0)
        l->panel_item--;
    else if (k == FT_DOWN && l->panel_item < l->nch - 1)
        l->panel_item++;
    else if (k == FT_BACK || k == FT_LEFT)
        l->panel_open = 0;
    else if (k == FT_OK) {
        l->panel_open = 0;
        if (l->panel_item != l->current || l->tune_wait_ms > 0)
            return tune_now(l, l->panel_item);
        l->banner_ms = LIVE_BANNER_MS;
    }
    return act(LIVE_ACT_NONE, 0);
}

live_action live_key(live *l, ft_key k, int error_card)
{
    if (l->nch <= 0)
        return act(k == FT_BACK ? LIVE_ACT_EXIT : LIVE_ACT_NONE, 0);
    if (l->panel_open)
        return panel_key(l, k);

    switch (k) {
    case FT_UP:
    case FT_DOWN:
        /* Up is channel up: the next number, wrapping round like a TV. */
        l->shown = (l->shown + (k == FT_UP ? 1 : l->nch - 1)) % l->nch;
        l->tune_wait_ms = LIVE_TUNE_DELAY_MS;
        l->banner_ms = LIVE_BANNER_MS;
        break;
    case FT_LEFT:
    case FT_RIGHT:
        l->volume += k == FT_RIGHT ? LIVE_VOLUME_STEP : -LIVE_VOLUME_STEP;
        l->volume = l->volume < 0 ? 0 : l->volume > 100 ? 100 : l->volume;
        l->volume_ms = LIVE_VOLUME_MS;
        return act(LIVE_ACT_VOLUME, l->volume);
    case FT_OK:
        if (error_card && l->tune_wait_ms == 0)
            return tune_now(l, l->current);
        l->panel_open = 1;
        l->panel_item = l->shown;
        break;
    case FT_BACK:
        return act(LIVE_ACT_EXIT, 0);
    case FT_NONE:
        break;
    }
    return act(LIVE_ACT_NONE, 0);
}

static int count_down(int *t, int ms)
{
    if (*t <= 0)
        return 0;
    *t = *t > ms ? *t - ms : 0;
    return *t == 0;
}

live_action live_tick(live *l, int elapsed_ms)
{
    if (elapsed_ms < 0)
        elapsed_ms = 0;
    l->anim_ms += (uint32_t)elapsed_ms;
    count_down(&l->banner_ms, elapsed_ms);
    count_down(&l->volume_ms, elapsed_ms);
    if (count_down(&l->tune_wait_ms, elapsed_ms)) {
        l->current = l->shown;
        return act(LIVE_ACT_TUNE, l->current);
    }
    return act(LIVE_ACT_NONE, 0);
}
