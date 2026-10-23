#ifndef FT_SESSION_H
#define FT_SESSION_H

#include "shell.h"

/* Turn a shell action into platform calls, applying the plan's fallbacks
 * (p.19: if direct tuning is unavailable, open the stock channel list).
 * Writes a plain-language result into s->status. Returns the game to
 * launch, or -1. */
int session_dispatch(shell *s, shell_action a);

#endif
