#ifndef FT_SHELL_H
#define FT_SHELL_H

/* Family home screen (plan p.14, Prompt 4): Live TV, YouTube, Football,
 * Games, Netflix, More. Remote-only: arrows, OK, Back. Pure state machine;
 * rendering and platform calls live elsewhere. */

#include "common.h"

typedef enum {
    TILE_LIVE_TV, TILE_YOUTUBE, TILE_FOOTBALL, TILE_GAMES, TILE_NETFLIX, TILE_MORE, TILE_COUNT
} shell_tile;

typedef enum { SCR_HOME, SCR_FOOTBALL, SCR_GAMES, SCR_MORE } shell_screen;
typedef enum { GAME_RACER, GAME_PENALTY, GAME_GARDEN, GAME_COUNT } game_id;
typedef enum { PROFILE_PARENT, PROFILE_CHILD } profile_role;

typedef enum {
    ACT_NONE,
    ACT_OPEN_BROADCAST,
    ACT_TUNE_FAVOURITE,
    ACT_OPEN_CHANNEL_LIST,
    ACT_OPEN_YOUTUBE,
    ACT_OPEN_NETFLIX,
    ACT_LAUNCH_GAME,
    ACT_WEB_SEARCH,
    ACT_SWITCH_PROFILE,
    ACT_SHOW_SUPPORT
} shell_act;

typedef struct {
    shell_act act;
    int arg;
} shell_action;

#define SHELL_MAX_FAVS 8
#define SHELL_MAX_ITEMS 12
#define SHELL_COLS 3

/* received: 1 = confirmed in the household baseline, 0 = not received,
 * -1 = not yet checked. Only confirmed services become shortcuts. */
typedef struct {
    char name[24];
    int received;
} shell_fav;

typedef struct {
    int youtube_supported; /* set only after an exact-device playback test */
    int netflix_supported;
    profile_role profile;
    unsigned child_games; /* bit per game_id */
    shell_fav favs[SHELL_MAX_FAVS];
    int nfav;
} shell_config;

typedef struct {
    shell_config cfg;
    shell_screen screen;
    int focus;
    int item;
    int tiles[TILE_COUNT];
    int ntiles;
    char status[128];
} shell;

extern const char *const shell_tile_labels[TILE_COUNT];
extern const char *const shell_tile_hints[TILE_COUNT];
extern const char *const game_names[GAME_COUNT];

void shell_init(shell *s, const shell_config *cfg);
void shell_set_profile(shell *s, profile_role p);
shell_action shell_key(shell *s, ft_key k);
int shell_list(const shell *s, const char **labels, int max);
const char *shell_screen_title(const shell *s);

#endif
