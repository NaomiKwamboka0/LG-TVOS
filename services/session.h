#ifndef FT_SESSION_H
#define FT_SESSION_H

/* Connects the shell to the family settings: checks the parent PIN,
 * enforces child play time, and decides what a choice starts (a game or a
 * channel). */

#include <stdint.h>

#include "settings.h"
#include "shell.h"

typedef struct {
    family_settings settings;
    pin_guard guard;
    char path[256];
    char pin_first[PIN_DIGITS + 1];
    int unlocked;      /* Parent settings open after a correct PIN */
    int after_new_pin; /* what to do once a new PIN is confirmed */
    uint32_t unsaved_play_s;
    int watch_channel; /* channel index, valid after SESSION_WATCH */
    int watch_list;    /* ... in which list: watch_list */
    int open_app;      /* app_id, valid after SESSION_OPEN_APP */
    uint64_t (*now_ms)(void);
    int64_t (*wall_utc)(void);
} session;

/* Where session.watch_channel points: cfg.channels, cfg.sports, cfg.found or cfg.videos. */
typedef enum { WATCH_TV, WATCH_SPORTS, WATCH_FOUND, WATCH_VIDEO } watch_list;

/* Loads settings into the session and applies them to an initialised shell.
 * Returns the FT_SAVE_* load status. */
int session_init(session *ss, shell *s, const char *settings_path);
int session_init_with_clock(session *ss, shell *s, const char *settings_path,
                            uint64_t (*now_ms)(void), int64_t (*wall_utc)(void));

/* session_dispatch results: start watching ss->watch_channel; open
 * ss->open_app in the web browser. */
#define SESSION_WATCH GAME_COUNT
#define SESSION_OPEN_APP (GAME_COUNT + 1)
#define SESSION_YT_SEARCH (GAME_COUNT + 2) /* look up shell cfg.yt_query */

/* Writes a plain-language result into s->status. Returns the game to
 * launch, SESSION_WATCH, or -1. */
int session_dispatch(session *ss, shell *s, shell_action a);

/* Call while a game runs. Returns 0 once a child's play time has run out. */
int session_play_account(session *ss, shell *s, uint32_t seconds);
void session_play_end(session *ss, shell *s);

#endif
