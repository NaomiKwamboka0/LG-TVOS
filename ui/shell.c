#include "shell.h"

#include <string.h>

const char *const shell_tile_labels[TILE_COUNT] = {
    "Live TV", "YouTube", "Football", "Games", "Netflix", "More"
};
const char *const shell_tile_hints[TILE_COUNT] = {
    "Aerial channels", "Existing TV app", "Channels", "Race, kick, build",
    "Existing TV app", "Search and settings"
};
const char *const game_names[GAME_COUNT] = {"Family Racer", "Penalty Cup", "Block Garden"};

typedef struct {
    const char *label;
    shell_act act;
    int arg;
} item;

static void layout(shell *s)
{
    int i;
    s->ntiles = 0;
    for (i = 0; i < TILE_COUNT; i++) {
        if (i == TILE_YOUTUBE && !s->cfg.youtube_supported)
            continue;
        if (i == TILE_NETFLIX && !s->cfg.netflix_supported)
            continue;
        s->tiles[s->ntiles++] = i;
    }
    if (s->focus >= s->ntiles)
        s->focus = 0;
}

void shell_init(shell *s, const shell_config *cfg)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    s->screen = SCR_HOME;
    layout(s);
}

void shell_set_profile(shell *s, profile_role p)
{
    s->cfg.profile = p;
    s->screen = SCR_HOME;
    s->focus = 0;
    s->item = 0;
    layout(s);
}

static int add(item *out, int n, int max, const char *label, shell_act act, int arg)
{
    if (n < max) {
        out[n].label = label;
        out[n].act = act;
        out[n].arg = arg;
        n++;
    }
    return n;
}

static int items(const shell *s, item *out, int max)
{
    int n = 0, i;
    int parent = s->cfg.profile == PROFILE_PARENT;

    switch (s->screen) {
    case SCR_FOOTBALL:
        for (i = 0; i < s->cfg.nfav && n < max - 1; i++)
            if (s->cfg.favs[i].received == 1)
                n = add(out, n, max, s->cfg.favs[i].name, ACT_TUNE_FAVOURITE, i);
        n = add(out, n, max, "All channels (TV channel list)", ACT_OPEN_CHANNEL_LIST, 0);
        break;
    case SCR_GAMES:
        for (i = 0; i < GAME_COUNT; i++)
            if (parent || (s->cfg.child_games & (1u << i)))
                n = add(out, n, max, game_names[i], ACT_LAUNCH_GAME, i);
        break;
    case SCR_MORE:
        if (parent)
            n = add(out, n, max, "Web search", ACT_WEB_SEARCH, 0);
        n = add(out, n, max, parent ? "Switch to child profile" : "Switch to parent profile",
                ACT_SWITCH_PROFILE, 0);
        if (parent)
            n = add(out, n, max, "App support status", ACT_SHOW_SUPPORT, 0);
        break;
    case SCR_HOME:
        break;
    }
    return n;
}

int shell_list(const shell *s, const char **labels, int max)
{
    item it[SHELL_MAX_ITEMS];
    int n = items(s, it, SHELL_MAX_ITEMS), i;
    for (i = 0; i < n && i < max; i++)
        labels[i] = it[i].label;
    return n < max ? n : max;
}

const char *shell_screen_title(const shell *s)
{
    switch (s->screen) {
    case SCR_FOOTBALL: return "Football";
    case SCR_GAMES: return "Games";
    case SCR_MORE: return "More";
    case SCR_HOME: break;
    }
    return "Family TV";
}

static void open_list(shell *s, shell_screen scr)
{
    s->screen = scr;
    s->item = 0;
}

static shell_action home_key(shell *s, ft_key k)
{
    shell_action a = {ACT_NONE, 0};
    int f = s->focus;

    if (k == FT_LEFT && f % SHELL_COLS > 0)
        f--;
    else if (k == FT_RIGHT && f % SHELL_COLS < SHELL_COLS - 1 && f + 1 < s->ntiles)
        f++;
    else if (k == FT_UP && f >= SHELL_COLS)
        f -= SHELL_COLS;
    else if (k == FT_DOWN && (f / SHELL_COLS + 1) * SHELL_COLS < s->ntiles)
        f = f + SHELL_COLS < s->ntiles ? f + SHELL_COLS : s->ntiles - 1; /* no dead end on a short row */
    else if (k == FT_OK && s->ntiles > 0) {
        switch ((shell_tile)s->tiles[f]) {
        case TILE_LIVE_TV: a.act = ACT_OPEN_BROADCAST; break;
        case TILE_YOUTUBE: a.act = ACT_OPEN_YOUTUBE; break;
        case TILE_NETFLIX: a.act = ACT_OPEN_NETFLIX; break;
        case TILE_FOOTBALL: open_list(s, SCR_FOOTBALL); break;
        case TILE_GAMES: open_list(s, SCR_GAMES); break;
        case TILE_MORE: open_list(s, SCR_MORE); break;
        case TILE_COUNT: break;
        }
    }
    /* Back on Home is left to the vendor (plan p.14). */
    s->focus = f;
    return a;
}

shell_action shell_key(shell *s, ft_key k)
{
    shell_action a = {ACT_NONE, 0};
    item it[SHELL_MAX_ITEMS];
    int n;

    s->status[0] = '\0';
    if (s->screen == SCR_HOME)
        return home_key(s, k);

    n = items(s, it, SHELL_MAX_ITEMS);
    if (k == FT_UP && s->item > 0)
        s->item--;
    else if (k == FT_DOWN && s->item < n - 1)
        s->item++;
    else if (k == FT_BACK)
        s->screen = SCR_HOME;
    else if (k == FT_OK && n > 0) {
        a.act = it[s->item].act;
        a.arg = it[s->item].arg;
    }
    return a;
}
