#ifndef FT_GARDEN_H
#define FT_GARDEN_H

/* Block Garden (plan p.23): original 2D tile-building sandbox. Arrows move a
 * cursor, OK places the selected tile, Back opens the pause menu. Not
 * Minecraft and makes no claim of Minecraft compatibility. */

#include <stddef.h>
#include "common.h"

#define GARDEN_W 64
#define GARDEN_H 64
#define GARDEN_TILE_TYPES 8 /* 0 = erase/empty */
#define GARDEN_PACKED_SIZE (4 + GARDEN_W * GARDEN_H * 2) /* 8 KiB tiles + header */

typedef enum { GM_EDIT, GM_PALETTE, GM_MENU, GM_EXITED } garden_mode;
typedef enum { GA_NONE, GA_SAVE, GA_SAVE_EXIT, GA_EXIT } garden_action;
typedef enum {
    GMI_RESUME, GMI_TILE, GMI_SAVE, GMI_SAVE_EXIT, GMI_EXIT, GMI_COUNT
} garden_menu_item;

typedef struct {
    uint16_t tiles[GARDEN_W * GARDEN_H];
    int cx, cy;
    int selected;
    garden_mode mode;
    int menu;
    int dirty;
} garden;

extern const char *const garden_tile_names[GARDEN_TILE_TYPES];
extern const char *const garden_menu_names[GMI_COUNT];

void garden_init(garden *g);
garden_action garden_key(garden *g, ft_key k);
uint16_t garden_get(const garden *g, int x, int y);
size_t garden_pack(const garden *g, uint8_t *out, size_t cap);
int garden_unpack(garden *g, const uint8_t *in, size_t n);

#endif
