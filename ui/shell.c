#include "shell.h"

#include <stdio.h>
#include <string.h>

static const ft_str TILE_LABEL[TILE_COUNT] = {
    STR_LIVE_TV, STR_YOUTUBE, STR_FOOTBALL, STR_GAMES, STR_NETFLIX, STR_MORE
};
static const ft_str TILE_HINT[TILE_COUNT] = {
    STR_LIVE_TV_HINT, STR_APP_HINT, STR_FOOTBALL_HINT, STR_GAMES_HINT, STR_APP_HINT, STR_MORE_HINT
};
static const ft_str GAME_NAME[GAME_COUNT] = {STR_GAME_RACER, STR_GAME_PENALTY, STR_GAME_GARDEN};
static const ft_str QUALITY_LABEL[] = {STR_QUALITY_360, STR_QUALITY_480, STR_QUALITY_720};
#define QUALITY_LABELS ((int)(sizeof QUALITY_LABEL / sizeof QUALITY_LABEL[0]))

typedef struct {
    char label[SHELL_LABEL_MAX];
    shell_act act;
    int arg;
} item;

const char *shell_tile_label(const shell *s, int tile)
{
    return tile >= 0 && tile < TILE_COUNT ? tr(s->cfg.lang, TILE_LABEL[tile]) : "?";
}

const char *shell_tile_hint(const shell *s, int tile)
{
    return tile >= 0 && tile < TILE_COUNT ? tr(s->cfg.lang, TILE_HINT[tile]) : "";
}

const char *shell_game_name(ft_lang lang, int game)
{
    return game >= 0 && game < GAME_COUNT ? tr(lang, GAME_NAME[game]) : "?";
}

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

void shell_clear_pin(shell *s)
{
    memset(s->pin_digits, '0', SHELL_PIN_DIGITS);
    s->pin_digits[SHELL_PIN_DIGITS] = '\0';
    s->pin_pos = 0;
}

void shell_init(shell *s, const shell_config *cfg)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    s->screen = SCR_HOME;
    shell_clear_pin(s);
    layout(s);
}

void shell_set_profile(shell *s, profile_role p)
{
    s->cfg.profile = p;
    s->screen = SCR_HOME;
    s->focus = 0;
    s->item = 0;
    shell_clear_pin(s);
    layout(s);
}

void shell_open_pin(shell *s, pin_purpose why)
{
    if (s->screen != SCR_PIN)
        s->pin_return = s->screen;
    s->screen = SCR_PIN;
    s->pin_for = why;
    shell_clear_pin(s);
}

void shell_open_settings(shell *s)
{
    s->screen = SCR_SETTINGS;
    s->item = 0;
    shell_clear_pin(s);
}

static int add(item *out, int n, int max, const char *label, shell_act act, int arg)
{
    if (n < max) {
        snprintf(out[n].label, sizeof out[n].label, "%s", label);
        out[n].act = act;
        out[n].arg = arg;
        n++;
    }
    return n;
}

static int settings_items(const shell *s, item *out, int max)
{
    ft_lang L = s->cfg.lang;
    char buf[SHELL_LABEL_MAX];
    int n = 0, g;

    snprintf(buf, sizeof buf, "%s: %s", tr(L, STR_LANGUAGE), lang_name(L));
    n = add(out, n, max, buf, ACT_TOGGLE_LANGUAGE, 0);
    if (s->cfg.child_limit_s == 0)
        snprintf(buf, sizeof buf, "%s: %s", tr(L, STR_CHILD_LIMIT), tr(L, STR_LIMIT_OFF));
    else
        snprintf(buf, sizeof buf, "%s: %u %s", tr(L, STR_CHILD_LIMIT),
                 (unsigned)(s->cfg.child_limit_s / 60), tr(L, STR_MINUTES));
    n = add(out, n, max, buf, ACT_CYCLE_LIMIT, 0);
    n = add(out, n, max, tr(L, STR_GRANT_15), ACT_GRANT_15, 0);
    n = add(out, n, max, tr(L, STR_RESET_TODAY), ACT_RESET_TODAY, 0);
    for (g = 0; g < GAME_COUNT; g++) {
        snprintf(buf, sizeof buf, "%s: %s", shell_game_name(L, g),
                 tr(L, (s->cfg.child_games & (1u << g)) ? STR_ALLOWED : STR_BLOCKED));
        n = add(out, n, max, buf, ACT_TOGGLE_GAME, g);
    }
    n = add(out, n, max, tr(L, STR_CHANGE_PIN), ACT_CHANGE_PIN, 0);
    snprintf(buf, sizeof buf, "%s: %s", tr(L, STR_QUALITY),
             tr(L, QUALITY_LABEL[s->cfg.quality >= 0 && s->cfg.quality < QUALITY_LABELS ? s->cfg.quality : 1]));
    return add(out, n, max, buf, ACT_CYCLE_QUALITY, 0);
}

static int items(const shell *s, item *out, int max)
{
    int n = 0, i;
    int parent = s->cfg.profile == PROFILE_PARENT;
    ft_lang L = s->cfg.lang;

    switch (s->screen) {
    case SCR_FOOTBALL:
        for (i = 0; i < s->cfg.nfav && n < max - 1; i++)
            if (s->cfg.favs[i].received == 1)
                n = add(out, n, max, s->cfg.favs[i].name, ACT_TUNE_FAVOURITE, i);
        n = add(out, n, max, tr(L, STR_ALL_CHANNELS), ACT_OPEN_CHANNEL_LIST, 0);
        break;
    case SCR_GAMES:
        for (i = 0; i < GAME_COUNT; i++)
            if (parent || (s->cfg.child_games & (1u << i)))
                n = add(out, n, max, shell_game_name(L, i), ACT_LAUNCH_GAME, i);
        break;
    case SCR_MORE:
        if (parent) {
            n = add(out, n, max, tr(L, STR_WEB_SEARCH), ACT_WEB_SEARCH, 0);
            n = add(out, n, max, tr(L, STR_TO_CHILD), ACT_SWITCH_PROFILE, 0);
            n = add(out, n, max, tr(L, STR_PARENT_SETTINGS), ACT_OPEN_PARENT_SETTINGS, 0);
            n = add(out, n, max, tr(L, STR_APP_STATUS), ACT_SHOW_SUPPORT, 0);
        } else {
            n = add(out, n, max, tr(L, STR_TO_PARENT), ACT_SWITCH_PROFILE, 0);
        }
        break;
    case SCR_SETTINGS:
        n = settings_items(s, out, max);
        break;
    case SCR_HOME:
    case SCR_PIN:
        break;
    }
    return n;
}

int shell_list(const shell *s, char labels[][SHELL_LABEL_MAX], int max)
{
    item it[SHELL_MAX_ITEMS];
    int n = items(s, it, SHELL_MAX_ITEMS), i;
    if (n > max)
        n = max;
    for (i = 0; i < n; i++)
        memcpy(labels[i], it[i].label, SHELL_LABEL_MAX);
    return n;
}

const char *shell_screen_title(const shell *s)
{
    ft_lang L = s->cfg.lang;
    switch (s->screen) {
    case SCR_FOOTBALL: return tr(L, STR_FOOTBALL);
    case SCR_GAMES: return tr(L, STR_GAMES);
    case SCR_MORE: return tr(L, STR_MORE);
    case SCR_SETTINGS: return tr(L, STR_PARENT_SETTINGS);
    case SCR_PIN:
        switch (s->pin_for) {
        case PIN_NEW_FIRST: return tr(L, STR_PIN_NEW);
        case PIN_NEW_CONFIRM: return tr(L, STR_PIN_CONFIRM);
        case PIN_FOR_PARENT:
        case PIN_FOR_SETTINGS: break;
        }
        return tr(L, STR_PIN_ENTER);
    case SCR_HOME: break;
    }
    return tr(L, STR_FAMILY_TV);
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

static shell_action pin_key(shell *s, ft_key k)
{
    shell_action a = {ACT_NONE, 0};
    char *d = &s->pin_digits[s->pin_pos];

    if (k == FT_UP)
        *d = (char)('0' + (*d - '0' + 1) % 10);
    else if (k == FT_DOWN)
        *d = (char)('0' + (*d - '0' + 9) % 10);
    else if (k == FT_LEFT && s->pin_pos > 0)
        s->pin_pos--;
    else if (k == FT_RIGHT && s->pin_pos < SHELL_PIN_DIGITS - 1)
        s->pin_pos++;
    else if (k == FT_OK) {
        if (s->pin_pos < SHELL_PIN_DIGITS - 1) {
            s->pin_pos++;
        } else {
            a.act = ACT_PIN_SUBMIT;
            a.arg = (int)s->pin_for;
        }
    } else if (k == FT_BACK) {
        s->screen = s->pin_return;
        shell_clear_pin(s);
        a.act = ACT_PIN_CANCEL;
    }
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
    if (s->screen == SCR_PIN)
        return pin_key(s, k);

    n = items(s, it, SHELL_MAX_ITEMS);
    if (k == FT_UP && s->item > 0) {
        s->item--;
    } else if (k == FT_DOWN && s->item < n - 1) {
        s->item++;
    } else if (k == FT_BACK) {
        if (s->screen == SCR_SETTINGS) {
            open_list(s, SCR_MORE);
            a.act = ACT_SETTINGS_CLOSED;
        } else {
            s->screen = SCR_HOME;
        }
    } else if (k == FT_OK && n > 0) {
        a.act = it[s->item].act;
        a.arg = it[s->item].arg;
    }
    return a;
}
