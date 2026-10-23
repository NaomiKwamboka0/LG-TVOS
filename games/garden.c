#include "garden.h"

#include <string.h>

const char *const garden_tile_names[GARDEN_TILE_TYPES] = {
    "Erase", "Grass", "Dirt", "Stone", "Wood", "Water", "Flower", "Brick"
};

const char *const garden_menu_names[GMI_COUNT] = {
    "Resume", "Choose tile", "Save", "Save and exit", "Exit without saving"
};

void garden_init(garden *g)
{
    memset(g, 0, sizeof *g);
    g->cx = GARDEN_W / 2;
    g->cy = GARDEN_H / 2;
    g->selected = 1;
    g->mode = GM_EDIT;
}

uint16_t garden_get(const garden *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= GARDEN_W || y >= GARDEN_H)
        return 0;
    return g->tiles[y * GARDEN_W + x];
}

static garden_action menu_choose(garden *g)
{
    switch ((garden_menu_item)g->menu) {
    case GMI_RESUME:
        g->mode = GM_EDIT;
        return GA_NONE;
    case GMI_TILE:
        g->mode = GM_PALETTE;
        return GA_NONE;
    case GMI_SAVE:
        g->mode = GM_EDIT;
        return GA_SAVE;
    case GMI_SAVE_EXIT:
        g->mode = GM_EXITED;
        return GA_SAVE_EXIT;
    case GMI_EXIT:
    case GMI_COUNT:
        break;
    }
    g->mode = GM_EXITED;
    return GA_EXIT;
}

garden_action garden_key(garden *g, ft_key k)
{
    switch (g->mode) {
    case GM_EDIT:
        if (k == FT_LEFT && g->cx > 0)
            g->cx--;
        else if (k == FT_RIGHT && g->cx < GARDEN_W - 1)
            g->cx++;
        else if (k == FT_UP && g->cy > 0)
            g->cy--;
        else if (k == FT_DOWN && g->cy < GARDEN_H - 1)
            g->cy++;
        else if (k == FT_OK) {
            uint16_t *t = &g->tiles[g->cy * GARDEN_W + g->cx];
            if (*t != (uint16_t)g->selected) {
                *t = (uint16_t)g->selected;
                g->dirty = 1;
            }
        } else if (k == FT_BACK) {
            g->mode = GM_MENU;
            g->menu = GMI_RESUME;
        }
        break;
    case GM_PALETTE:
        if (k == FT_LEFT)
            g->selected = (g->selected + GARDEN_TILE_TYPES - 1) % GARDEN_TILE_TYPES;
        else if (k == FT_RIGHT)
            g->selected = (g->selected + 1) % GARDEN_TILE_TYPES;
        else if (k == FT_OK || k == FT_BACK)
            g->mode = GM_EDIT;
        break;
    case GM_MENU:
        if (k == FT_UP && g->menu > 0)
            g->menu--;
        else if (k == FT_DOWN && g->menu < GMI_COUNT - 1)
            g->menu++;
        else if (k == FT_OK)
            return menu_choose(g);
        else if (k == FT_BACK)
            g->mode = GM_EDIT;
        break;
    case GM_EXITED:
        break;
    }
    return GA_NONE;
}

size_t garden_pack(const garden *g, uint8_t *o, size_t cap)
{
    size_t i;
    if (cap < GARDEN_PACKED_SIZE)
        return 0;
    o[0] = GARDEN_W;
    o[1] = 0;
    o[2] = GARDEN_H;
    o[3] = 0;
    for (i = 0; i < GARDEN_W * GARDEN_H; i++) {
        o[4 + 2 * i] = (uint8_t)g->tiles[i];
        o[5 + 2 * i] = (uint8_t)(g->tiles[i] >> 8);
    }
    return GARDEN_PACKED_SIZE;
}

int garden_unpack(garden *g, const uint8_t *in, size_t n)
{
    size_t i;
    if (n != GARDEN_PACKED_SIZE || in[0] != GARDEN_W || in[1] || in[2] != GARDEN_H || in[3])
        return -1;
    for (i = 0; i < GARDEN_W * GARDEN_H; i++) {
        uint16_t t = (uint16_t)(in[4 + 2 * i] | (in[5 + 2 * i] << 8));
        if (t >= GARDEN_TILE_TYPES)
            return -1;
    }
    garden_init(g);
    for (i = 0; i < GARDEN_W * GARDEN_H; i++)
        g->tiles[i] = (uint16_t)(in[4 + 2 * i] | (in[5 + 2 * i] << 8));
    return 0;
}
