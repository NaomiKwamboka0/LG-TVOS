#ifndef FT_FRONTEND_H
#define FT_FRONTEND_H

/* Frontends: the desktop window showing the pixel views (the app), or a
 * text terminal (a development tool). Both drive the same shell, session,
 * games and live TV. */

#include <stddef.h>
#include <stdint.h>

#include "garden.h"
#include "live.h"
#include "penalty.h"
#include "player.h"
#include "racer.h"
#include "shell.h"

typedef struct {
    const char *name;
    /* Default folders: data (settings, saves, log) and base (the program's
     * own folder, holding channels.txt and the bundled yt-dlp and deno).
     * Either may come back empty; base then means the current folder. */
    void (*folders)(char *data, size_t data_n, char *base, size_t base_n);
    /* scripted: keys come from --keys, so no interactive input setup. */
    int (*open)(int scripted, int fullscreen);
    void (*close)(void);
    /* Waits up to ms for a key (0 = until a key arrives). Sets *quit when the user quits. */
    ft_key (*poll)(int ms, int *quit);
    /* Where the mouse pointer is, in the 1280x720 layout; 0 when there is
     * no pointer or it has not moved for a while. May be NULL. */
    int (*pointer)(int *x, int *y);
    /* Typing: 0 off; 1 letters and digits arrive as FT_TEXT (the YouTube
     * tab); 2 everything typed does, Backspace as '\b' and Return as '\n'
     * (the search keyboard). typed() is the character of the last FT_TEXT.
     * Both may be NULL. */
    void (*text_mode)(int mode);
    int (*typed)(void);
    /* Draws the shell; returns how soon (ms) it wants to draw again for
     * motion or the moving background, or 0 for not until something changes. */
    int (*shell)(const shell *s);
    void (*racer)(const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang, const char *status);
    void (*penalty)(const penalty *p, ft_lang lang, const char *status);
    void (*garden)(const garden *g, ft_lang lang, const char *status);
    /* Draws the newest picture from p with the live TV overlays; chs is
     * the list being watched (channels or sports). */
    void (*live)(const live *l, player *p, const shell *s, const channel_list *chs);
    /* Thread-safe: makes a waiting poll() return FT_NONE now. May be NULL. */
    void (*wake)(void *ctx);
    /* 1 while the window fills the screen (F11 toggles it). */
    int (*is_fullscreen)(void);
    /* Opens an address in the web browser (and gets out of its way). 0 = ok. */
    int (*open_url)(const char *url);
    /* Shows app's real site (YouTube, Netflix) inside the window under a
     * Family TV bar, until the family presses its Back button. 0 when done;
     * -1 when this computer has no web view (then open_url). May be NULL. */
    int (*browse)(int app, const char *url, const char *data_dir, int *quit);
    /* One frame of the start-up animation (t: 0..VIEW_STARTUP_MS). May be NULL. */
    void (*startup)(uint32_t t_ms, ft_lang lang);
} sim_frontend;

extern const sim_frontend frontend_text;
extern const sim_frontend frontend_window;

/* Optional: the window frontend writes its last frame here (PPM) on close. */
extern const char *sim_dump_path;
/* --stats: the window shows frames per second and drawing cost while watching. */
extern int sim_show_stats;

/* Nairobi wall-clock time as HH:MM (UTC+3, no DST). */
void sim_clock_text(char *out, size_t n);

#endif
