#ifndef FT_PREFS_H
#define FT_PREFS_H

/* Viewer preferences that change often and protect nothing: the last
 * channel, the volume and full screen. Kept in their own file (player.sav)
 * so damage here can never touch the parental settings. */

#define PREFS_PACKED_SIZE 8

typedef struct {
    int last_channel; /* channel number, 0 = none yet */
    int volume;       /* 0-100 */
    int fullscreen;
} player_prefs;

void prefs_defaults(player_prefs *p);
/* Returns an FT_SAVE_* status; on any failure *p holds defaults. */
int prefs_load(player_prefs *p, const char *path);
int prefs_save(const player_prefs *p, const char *path);

#endif
