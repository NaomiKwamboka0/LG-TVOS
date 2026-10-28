#ifndef FT_SHELL_H
#define FT_SHELL_H

/* Family home screen in a TCL / Google TV-style layout: tabs along the top
 * (For you, Live TV, Apps) over rows of cards, plus the Settings list,
 * Parent settings and the PIN pad. Remote-only: arrows, OK, Back. Pure
 * state machine; rendering, PIN checks and playback live elsewhere. */

#include <stdint.h>

#include "channels.h"
#include "common.h"
#include "i18n.h"
#include "sports.h"

typedef enum { TAB_FOR_YOU, TAB_LIVE, TAB_APPS, TAB_COUNT } shell_tab;
/* Top bar focus positions: the tabs, then these two. */
enum { TOP_GEAR = TAB_COUNT, TOP_PROFILE, TOP_COUNT };

typedef enum { SCR_HOME, SCR_MORE, SCR_SETTINGS, SCR_PIN, SCR_SPORTS } shell_screen;
/* Streaming services opened in the web browser with the family's own account. */
typedef enum { APP_NETFLIX, APP_YOUTUBE, APP_COUNT } app_id;
typedef enum { GAME_RACER, GAME_PENALTY, GAME_GARDEN, GAME_COUNT } game_id;
typedef enum { PROFILE_PARENT, PROFILE_CHILD } profile_role;
typedef enum { PIN_FOR_PARENT, PIN_FOR_SETTINGS, PIN_NEW_FIRST, PIN_NEW_CONFIRM } pin_purpose;

typedef enum {
    ACT_NONE,
    ACT_PLAY_CHANNEL, /* arg: index into cfg.channels */
    ACT_PLAY_SPORT,   /* arg: index into cfg.sports */
    ACT_OPEN_APP,     /* arg: app_id */
    ACT_LAUNCH_GAME,
    ACT_SWITCH_PROFILE,
    ACT_ABOUT,
    ACT_OPEN_PARENT_SETTINGS,
    ACT_PIN_SUBMIT,
    ACT_PIN_CANCEL,
    ACT_TOGGLE_LANGUAGE,
    ACT_CYCLE_LIMIT,
    ACT_GRANT_15,
    ACT_RESET_TODAY,
    ACT_TOGGLE_GAME,
    ACT_CHANGE_PIN,
    ACT_CYCLE_QUALITY,
    ACT_SETTINGS_CLOSED
} shell_act;

typedef struct {
    shell_act act;
    int arg;
} shell_action;

#define SHELL_MAX_ITEMS 12
#define SHELL_LABEL_MAX 80
#define SHELL_PIN_DIGITS 4
#define SHELL_ROW_MAX CH_MAX
#define SHELL_MAX_ROWS (CH_MAX + 2)

/* Hero: one wide banner. Banner: a shorter wide tile (Sports). Cards: a
 * row that scrolls sideways. Guide: one channel per row, like a TV guide. */
typedef enum { ROW_HERO, ROW_BANNER, ROW_CARDS, ROW_GUIDE } row_style;
typedef enum {
    CARD_CHANNEL, CARD_GAME, CARD_SETTINGS, CARD_PROFILE, CARD_ABOUT, CARD_NO_CHANNELS, CARD_SPORTS, CARD_APP
} card_kind;

typedef struct {
    card_kind kind;
    int arg; /* channel index, game_id or app_id */
} shell_card;

typedef struct {
    row_style style;
    ft_str title; /* STR_COUNT: no title */
    int n;
    shell_card cards[SHELL_ROW_MAX];
} shell_row;

typedef struct {
    profile_role profile;
    ft_lang lang;
    unsigned child_games;      /* bit per game_id */
    uint32_t child_limit_s;    /* 0 = no limit */
    uint32_t play_remaining_s; /* for the child's Games row */
    int quality;               /* video_quality (settings.h) */
    channel_list channels;     /* the channel list (group tv) */
    int last_channel;          /* index shown in the For you banner */
    channel_list sports;       /* the Sports page (group sports) */
    sport_status sport[CH_MAX];
} shell_config;

typedef struct {
    shell_config cfg;
    shell_screen screen;
    shell_tab tab;
    int row;                 /* focused home row; -1 = top bar */
    int top;                 /* focused top bar item while row == -1 */
    int col[SHELL_MAX_ROWS]; /* each row remembers its column (per tab) */
    int item;                /* focused row of the Settings lists; on the
                              * Sports page, the focused channel's index */
    char pin_digits[SHELL_PIN_DIGITS + 1];
    int pin_pos;
    pin_purpose pin_for;
    shell_screen pin_return;
    shell_screen settings_return;
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
const char *shell_game_name(ft_lang lang, int game);
const char *shell_tab_label(ft_lang lang, int tab);
/* The rows of the current tab, top to bottom. */
int shell_home_rows(const shell *s, shell_row *out, int max);
const char *shell_card_label(const shell *s, shell_card c);
const char *shell_app_name(int app);
const char *shell_app_url(int app);
/* Sports channels in page order (live first); returns how many. */
int shell_sports_order(const shell *s, int *order);

#endif
