#ifndef FT_SHELL_H
#define FT_SHELL_H

/* Family home screen (plan p.14, Prompt 4): Live TV, YouTube, Football,
 * Games, Netflix, More. Remote-only: arrows, OK, Back. Pure state machine;
 * rendering, PIN checks and platform calls live elsewhere. */

#include <stdint.h>

#include "common.h"
#include "i18n.h"

typedef enum {
    TILE_LIVE_TV, TILE_YOUTUBE, TILE_FOOTBALL, TILE_GAMES, TILE_NETFLIX, TILE_MORE, TILE_COUNT
} shell_tile;

typedef enum { SCR_HOME, SCR_FOOTBALL, SCR_GAMES, SCR_MORE, SCR_SETTINGS, SCR_PIN } shell_screen;
typedef enum { GAME_RACER, GAME_PENALTY, GAME_GARDEN, GAME_COUNT } game_id;
typedef enum { PROFILE_PARENT, PROFILE_CHILD } profile_role;
typedef enum { PIN_FOR_PARENT, PIN_FOR_SETTINGS, PIN_NEW_FIRST, PIN_NEW_CONFIRM } pin_purpose;

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
    ACT_SHOW_SUPPORT,
    ACT_OPEN_PARENT_SETTINGS,
    ACT_PIN_SUBMIT,
    ACT_PIN_CANCEL,
    ACT_TOGGLE_LANGUAGE,
    ACT_CYCLE_LIMIT,
    ACT_GRANT_15,
    ACT_RESET_TODAY,
    ACT_TOGGLE_GAME,
    ACT_CHANGE_PIN,
    ACT_SETTINGS_CLOSED
} shell_act;

typedef struct {
    shell_act act;
    int arg;
} shell_action;

#define SHELL_MAX_FAVS 8
#define SHELL_MAX_ITEMS 12
#define SHELL_LABEL_MAX 80
#define SHELL_COLS 3
#define SHELL_PIN_DIGITS 4

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
    ft_lang lang;
    unsigned child_games;      /* bit per game_id */
    uint32_t child_limit_s;    /* 0 = no limit */
    uint32_t play_remaining_s; /* for the child's Games screen */
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
    char pin_digits[SHELL_PIN_DIGITS + 1];
    int pin_pos;
    pin_purpose pin_for;
    shell_screen pin_return;
    char status[160];
} shell;

void shell_init(shell *s, const shell_config *cfg);
void shell_set_profile(shell *s, profile_role p);
void shell_open_pin(shell *s, pin_purpose why);
void shell_clear_pin(shell *s);
void shell_open_settings(shell *s);
shell_action shell_key(shell *s, ft_key k);
int shell_list(const shell *s, char labels[][SHELL_LABEL_MAX], int max);
const char *shell_screen_title(const shell *s);
const char *shell_tile_label(const shell *s, int tile);
const char *shell_tile_hint(const shell *s, int tile);
const char *shell_game_name(ft_lang lang, int game);

#endif
