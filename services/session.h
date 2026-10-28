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
    uint64_t (*now_ms)(void);
    int64_t (*wall_utc)(void);
} session;

/* Loads settings into the session and applies them to an initialised shell.
 * Returns the FT_SAVE_* load status. */
int session_init(session *ss, shell *s, const char *settings_path);
int session_init_with_clock(session *ss, shell *s, const char *settings_path,
                            uint64_t (*now_ms)(void), int64_t (*wall_utc)(void));

/* session_dispatch result: start watching ss->watch_channel. */
#define SESSION_WATCH GAME_COUNT

/* Writes a plain-language result into s->status. Returns the game to
 * launch, SESSION_WATCH, or -1. */
int session_dispatch(session *ss, shell *s, shell_action a);

/* Call while a game runs. Returns 0 once a child's play time has run out. */
int session_play_account(session *ss, shell *s, uint32_t seconds);
void session_play_end(session *ss, shell *s);

#endif
