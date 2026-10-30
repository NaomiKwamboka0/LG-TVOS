#include "shell.h"

#include <stdio.h>
#include <string.h>

static const ft_str GAME_NAME[GAME_COUNT] = {STR_GAME_RACER, STR_GAME_PENALTY, STR_GAME_GARDEN};
static const ft_str TAB_NAME[TAB_COUNT] = {STR_FOR_YOU, STR_LIVE_TV, STR_SPORTS, STR_YOUTUBE, STR_APPS};
static const char KEY_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789";
static const ft_str QUALITY_LABEL[] = {STR_QUALITY_360, STR_QUALITY_480, STR_QUALITY_720, STR_QUALITY_1080};
static const char *const APP_NAME[APP_COUNT] = {"Netflix", "YouTube", "Google"};
static const char *const APP_URL[APP_COUNT] = {"https://www.netflix.com/browse", "https://www.youtube.com/",
                                               "https://www.google.com/"};
#define QUALITY_LABELS ((int)(sizeof QUALITY_LABEL / sizeof QUALITY_LABEL[0]))

typedef struct {
    char label[SHELL_LABEL_MAX];
    shell_act act;
    int arg;
} item;

const char *shell_game_name(ft_lang lang, int game)
{
    return game >= 0 && game < GAME_COUNT ? tr(lang, GAME_NAME[game]) : "?";
}

const char *shell_tab_label(ft_lang lang, int tab)
{
    return tab >= 0 && tab < TAB_COUNT ? tr(lang, TAB_NAME[tab]) : "?";
}

const char *shell_app_name(int app)
{
    return app >= 0 && app < APP_COUNT ? APP_NAME[app] : "?";
}

const char *shell_app_url(int app)
{
    return app >= 0 && app < APP_COUNT ? APP_URL[app] : "";
}

int shell_sports_order(const shell *s, int *order)
{
    return sports_order(s->cfg.sport, s->cfg.sports.n, order);
}

const char *shell_card_label(const shell *s, shell_card c)
{
    ft_lang L = s->cfg.lang;
    switch (c.kind) {
    case CARD_CHANNEL:
        return c.arg >= 0 && c.arg < s->cfg.channels.n ? s->cfg.channels.ch[c.arg].name : "?";
    case CARD_GAME: return shell_game_name(L, c.arg);
    case CARD_SETTINGS: return tr(L, STR_PARENT_SETTINGS);
    case CARD_PROFILE: return tr(L, s->cfg.profile == PROFILE_PARENT ? STR_TO_CHILD : STR_TO_PARENT);
    case CARD_ABOUT: return tr(L, STR_ABOUT);
    case CARD_NO_CHANNELS: return tr(L, STR_NO_CHANNELS);
    case CARD_SPORTS: return tr(L, STR_SPORTS);
    case CARD_APP: return shell_app_name(c.arg);
    case CARD_SPORT: return c.arg >= 0 && c.arg < s->cfg.sports.n ? s->cfg.sports.ch[c.arg].name : "?";
    case CARD_FOUND: return c.arg >= 0 && c.arg < s->cfg.nfound ? s->cfg.found[c.arg].title : "?";
    case CARD_SEARCHING: return tr(L, s->cfg.searching ? STR_SEARCHING : STR_NONE_FOUND);
    case CARD_YT_SEARCH: return s->cfg.yt_query[0] ? s->cfg.yt_query : tr(L, STR_YT_SEARCH);
    case CARD_VIDEO:
        return c.arg >= 0 && c.arg < YT_FEEDS * YT_FEED_MAX ? s->cfg.videos[c.arg].title : "?";
    case CARD_YT_LOADING:
        return tr(L, c.arg >= 0 && c.arg < YT_FEEDS && (s->cfg.yt_loading & (1u << c.arg)) ? STR_YT_LOADING
                                                                                             : STR_YT_NOTHING);
    case CARD_ASK_PARENT: return tr(L, STR_YT_ASK);
    }
    return "?";
}

void shell_clear_pin(shell *s)
{
    memset(s->pin_digits, '0', SHELL_PIN_DIGITS);
    s->pin_digits[SHELL_PIN_DIGITS] = '\0';
    s->pin_pos = 0;
}

static void go_home(shell *s, shell_tab tab)
{
    s->screen = SCR_HOME;
    s->tab = tab;
    s->row = 0;
    s->top = tab;
    memset(s->col, 0, sizeof s->col);
}

void shell_init(shell *s, const shell_config *cfg)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    go_home(s, TAB_FOR_YOU);
    shell_clear_pin(s);
}

void shell_set_profile(shell *s, profile_role p)
{
    s->cfg.profile = p;
    go_home(s, TAB_FOR_YOU);
    s->item = 0;
    shell_clear_pin(s);
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
    /* Back returns to wherever Parent settings was opened from (Home or the
     * Settings list); changing the PIN from inside keeps that origin. */
    shell_screen from = s->screen == SCR_PIN ? s->pin_return : s->screen;
    if (from == SCR_HOME || from == SCR_MORE)
        s->settings_return = from;
    else if (from != SCR_SETTINGS)
        s->settings_return = SCR_MORE;
    s->screen = SCR_SETTINGS;
    s->item = 0;
    shell_clear_pin(s);
}

/* ---- home rows ---------------------------------------------------------- */

static int allowed_game(const shell *s, int g)
{
    return s->cfg.profile == PROFILE_PARENT || (s->cfg.child_games & (1u << g));
}

static shell_row *new_row(shell_row *out, int *n, int max, row_style style, ft_str title)
{
    shell_row *r;
    if (*n >= max)
        return NULL;
    r = &out[(*n)++];
    r->style = style;
    r->title = title;
    r->n = 0;
    return r;
}

static void put(shell_row *r, card_kind kind, int arg)
{
    if (r && r->n < SHELL_ROW_MAX) {
        r->cards[r->n].kind = kind;
        r->cards[r->n].arg = arg;
        r->n++;
    }
}

/* Streaming services open a web browser: parents only. */
static void apps_row(const shell *s, shell_row *out, int *n, int max, int with_sports)
{
    shell_row *r;
    int a;
    if (s->cfg.profile != PROFILE_PARENT && !(with_sports && s->cfg.sports.n > 0))
        return;
    r = new_row(out, n, max, ROW_CARDS, STR_WATCH_MORE);
    if (with_sports && s->cfg.sports.n > 0)
        put(r, CARD_SPORTS, 0);
    if (s->cfg.profile == PROFILE_PARENT)
        for (a = 0; a < APP_COUNT; a++)
            put(r, CARD_APP, a);
}

static void games_row(const shell *s, shell_row *out, int *n, int max)
{
    shell_row *r = new_row(out, n, max, ROW_CARDS, STR_GAMES);
    int g;
    for (g = 0; g < GAME_COUNT; g++)
        if (allowed_game(s, g))
            put(r, CARD_GAME, g);
    if (r && r->n == 0)
        (*n)--; /* a child with every game blocked sees no empty row */
}

/* Live official matches, the live football search (parents only: the
 * streams are not vetted), what is coming up, then every sports channel. */
static void sports_rows(const shell *s, shell_row *out, int *n, int max)
{
    int order[CH_MAX], k = shell_sports_order(s, order), i;
    shell_row *r = NULL;
    for (i = 0; i < k && s->cfg.sport[order[i]].state == SPORT_LIVE; i++) {
        if (!r)
            r = new_row(out, n, max, ROW_CARDS, STR_ON_NOW);
        put(r, CARD_SPORT, order[i]);
    }
    if (s->cfg.profile == PROFILE_PARENT) {
        r = new_row(out, n, max, ROW_CARDS, STR_LIVE_FOOTBALL);
        for (i = 0; i < s->cfg.nfound; i++)
            put(r, CARD_FOUND, i);
        if (s->cfg.nfound == 0)
            put(r, CARD_SEARCHING, 0);
    }
    r = NULL;
    for (i = 0; i < k; i++)
        if (s->cfg.sport[order[i]].state == SPORT_UPCOMING) {
            if (!r)
                r = new_row(out, n, max, ROW_CARDS, STR_COMING_UP);
            put(r, CARD_SPORT, order[i]);
        }
    if (k > 0) {
        r = new_row(out, n, max, ROW_CARDS, STR_SPORTS_CHANNELS);
        for (i = 0; i < k; i++)
            put(r, CARD_SPORT, order[i]);
    }
}

/* YouTube, Netflix and Google are the real sites shown inside Family TV, so
 * this tab is only the way to them. They reach everything: parents only. */
static void youtube_rows(const shell *s, shell_row *out, int *n, int max)
{
    shell_row *r = new_row(out, n, max, ROW_CARDS, STR_YOUTUBE);
    int a;
    if (s->cfg.profile != PROFILE_PARENT) {
        put(r, CARD_ASK_PARENT, 0);
        return;
    }
    for (a = 0; a < APP_COUNT; a++)
        put(r, CARD_APP, a);
}

int shell_home_rows(const shell *s, shell_row *out, int max)
{
    const channel_list *chs = &s->cfg.channels;
    shell_row *r;
    int n = 0, i;

    switch (s->tab) {
    case TAB_FOR_YOU:
        r = new_row(out, &n, max, ROW_HERO, STR_COUNT);
        if (chs->n > 0) {
            int last = s->cfg.last_channel;
            put(r, CARD_CHANNEL, last >= 0 && last < chs->n ? last : 0);
            r = new_row(out, &n, max, ROW_CARDS, STR_LIVE_CHANNELS);
            for (i = 0; i < chs->n; i++)
                put(r, CARD_CHANNEL, i);
        } else {
            put(r, CARD_NO_CHANNELS, 0);
        }
        if (s->cfg.sports.n > 0) {
            r = new_row(out, &n, max, ROW_BANNER, STR_COUNT);
            put(r, CARD_SPORTS, 0);
        }
        apps_row(s, out, &n, max, 0);
        games_row(s, out, &n, max);
        break;
    case TAB_LIVE:
        if (chs->n == 0) {
            r = new_row(out, &n, max, ROW_CARDS, STR_ALL_CHANNELS);
            put(r, CARD_NO_CHANNELS, 0);
        }
        for (i = 0; i < chs->n; i++) {
            r = new_row(out, &n, max, ROW_GUIDE, i == 0 ? STR_ALL_CHANNELS : STR_COUNT);
            put(r, CARD_CHANNEL, i);
        }
        break;
    case TAB_SPORTS:
        sports_rows(s, out, &n, max);
        break;
    case TAB_YOUTUBE:
        youtube_rows(s, out, &n, max);
        break;
    case TAB_APPS:
        games_row(s, out, &n, max);
        apps_row(s, out, &n, max, 1);
        r = new_row(out, &n, max, ROW_CARDS, STR_FAMILY);
        if (s->cfg.profile == PROFILE_PARENT)
            put(r, CARD_SETTINGS, 0);
        put(r, CARD_PROFILE, 0);
        put(r, CARD_ABOUT, 0);
        break;
    case TAB_COUNT:
        break;
    }
    return n;
}

/* ---- lists (Settings, Parent settings) ---------------------------------- */

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
    int n = 0;
    ft_lang L = s->cfg.lang;

    switch (s->screen) {
    case SCR_MORE:
        if (s->cfg.profile == PROFILE_PARENT) {
            n = add(out, n, max, tr(L, STR_TO_CHILD), ACT_SWITCH_PROFILE, 0);
            n = add(out, n, max, tr(L, STR_PARENT_SETTINGS), ACT_OPEN_PARENT_SETTINGS, 0);
        } else {
            n = add(out, n, max, tr(L, STR_TO_PARENT), ACT_SWITCH_PROFILE, 0);
        }
        n = add(out, n, max, tr(L, STR_ABOUT), ACT_ABOUT, 0);
        break;
    case SCR_SETTINGS:
        n = settings_items(s, out, max);
        break;
    case SCR_HOME:
    case SCR_PIN:
    case SCR_KEYBOARD:
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
    case SCR_MORE: return tr(L, STR_SETTINGS);
    case SCR_SETTINGS: return tr(L, STR_PARENT_SETTINGS);
    case SCR_PIN:
        switch (s->pin_for) {
        case PIN_NEW_FIRST: return tr(L, STR_PIN_NEW);
        case PIN_NEW_CONFIRM: return tr(L, STR_PIN_CONFIRM);
        case PIN_FOR_PARENT:
        case PIN_FOR_SETTINGS: break;
        }
        return tr(L, STR_PIN_ENTER);
    case SCR_KEYBOARD: return tr(L, STR_YT_SEARCH);
    case SCR_HOME: break;
    }
    return tr(L, STR_FAMILY_TV);
}

/* ---- keys --------------------------------------------------------------- */

static void open_list(shell *s, shell_screen scr)
{
    s->screen = scr;
    s->item = 0;
}

static void select_tab(shell *s, int tab)
{
    if (tab != (int)s->tab) {
        s->tab = (shell_tab)tab;
        memset(s->col, 0, sizeof s->col);
    }
}

static shell_action card_action(shell_card c)
{
    shell_action a = {ACT_NONE, 0};
    switch (c.kind) {
    case CARD_CHANNEL: a.act = ACT_PLAY_CHANNEL; break;
    case CARD_GAME: a.act = ACT_LAUNCH_GAME; break;
    case CARD_SETTINGS: a.act = ACT_OPEN_PARENT_SETTINGS; break;
    case CARD_PROFILE: a.act = ACT_SWITCH_PROFILE; break;
    case CARD_ABOUT: a.act = ACT_ABOUT; break;
    case CARD_APP: a.act = ACT_OPEN_APP; break;
    case CARD_SPORT: a.act = ACT_PLAY_SPORT; break;
    case CARD_FOUND: a.act = ACT_PLAY_FOUND; break;
    case CARD_VIDEO: a.act = ACT_PLAY_VIDEO; break;
    case CARD_NO_CHANNELS:
    case CARD_SPORTS:
    case CARD_SEARCHING:
    case CARD_YT_SEARCH:
    case CARD_YT_LOADING:
    case CARD_ASK_PARENT: break;
    }
    a.arg = c.arg;
    return a;
}

/* Tabs switch as soon as they are focused, like on a TV launcher. */
static shell_action top_key(shell *s, ft_key k, int nrows)
{
    shell_action a = {ACT_NONE, 0};
    if (k == FT_LEFT && s->top > 0) {
        s->top--;
        if (s->top < TAB_COUNT)
            select_tab(s, s->top);
    } else if (k == FT_RIGHT && s->top < TOP_COUNT - 1) {
        s->top++;
        if (s->top < TAB_COUNT)
            select_tab(s, s->top);
    } else if (k == FT_DOWN || (k == FT_OK && s->top < TAB_COUNT)) {
        if (nrows > 0)
            s->row = 0;
    } else if (k == FT_OK && s->top == TOP_GEAR) {
        open_list(s, SCR_MORE);
    } else if (k == FT_OK && s->top == TOP_PROFILE) {
        a.act = ACT_SWITCH_PROFILE;
    } else if (k == FT_BACK && s->tab != TAB_FOR_YOU) {
        select_tab(s, TAB_FOR_YOU);
        s->top = TAB_FOR_YOU;
    }
    return a;
}

static shell_action home_key(shell *s, ft_key k)
{
    shell_row rows[SHELL_MAX_ROWS];
    int nrows = shell_home_rows(s, rows, SHELL_MAX_ROWS);
    shell_action a = {ACT_NONE, 0};
    int *col;
    const shell_row *r;

    if (s->row >= nrows)
        s->row = nrows - 1;
    if (s->row < 0)
        return top_key(s, k, nrows);
    r = &rows[s->row];
    col = &s->col[s->row];
    if (*col >= r->n)
        *col = r->n > 0 ? r->n - 1 : 0;

    if (k == FT_LEFT && *col > 0) {
        (*col)--;
    } else if (k == FT_RIGHT && *col < r->n - 1) {
        (*col)++;
    } else if (k == FT_UP) {
        s->row--;
        if (s->row < 0)
            s->top = s->tab;
    } else if (k == FT_DOWN && s->row < nrows - 1) {
        s->row++;
    } else if (k == FT_OK && r->n > 0 && r->cards[*col].kind == CARD_SPORTS) {
        select_tab(s, TAB_SPORTS); /* the banner and the Apps tile lead to the Sports tab */
        s->top = TAB_SPORTS;
        s->row = 0;
        } else if (k == FT_OK && r->n > 0 && r->cards[*col].kind == CARD_YT_SEARCH) {
        s->screen = SCR_KEYBOARD;
        s->item = 0;
    } else if (k == FT_OK && r->n > 0) {
        a = card_action(r->cards[*col]);
    } else if (k == FT_BACK) {
        s->row = -1;
        s->top = s->tab;
    }
    return a;
}

/* ---- search keyboard ---------------------------------------------------- */

const char *shell_key_label(const shell *s, int key)
{
    static char one[SHELL_KEYS][2];
    ft_lang L = s->cfg.lang;
    switch (key) {
    case KEY_SPACE: return tr(L, STR_KEY_SPACE);
    case KEY_DELETE: return tr(L, STR_KEY_DELETE);
    case KEY_CLEAR: return tr(L, STR_KEY_CLEAR);
    case KEY_SEARCH: return tr(L, STR_KEY_SEARCH);
    }
    if (key < 0 || key >= KEY_SPACE)
        return "";
    one[key][0] = KEY_CHARS[key];
    one[key][1] = '\0';
    return one[key];
}

/* Search: back to the YouTube tab, on the results row. */
static shell_action search(shell *s)
{
    shell_action a = {ACT_NONE, 0};
    char q[YT_QUERY_MAX];
    yt_clean_query(q, sizeof q, s->cfg.yt_query);
    if (!q[0])
        return a;
    memcpy(s->cfg.yt_query, q, sizeof q);
    s->screen = SCR_HOME;
    select_tab(s, TAB_YOUTUBE);
    s->top = TAB_YOUTUBE;
    s->row = 1;
    s->col[1] = 0;
    s->cfg.nvideos[0] = 0;
    s->cfg.yt_loading |= 1u;
    a.act = ACT_YT_SEARCH;
    return a;
}

static void type_char(shell *s, char c)
{
    size_t len = strlen(s->cfg.yt_query);
    if (len + 1 < sizeof s->cfg.yt_query && !(c == ' ' && (len == 0 || s->cfg.yt_query[len - 1] == ' '))) {
        s->cfg.yt_query[len] = c;
        s->cfg.yt_query[len + 1] = '\0';
    }
}

static shell_action press_key(shell *s, int key)
{
    size_t len = strlen(s->cfg.yt_query);
    shell_action a = {ACT_NONE, 0};
    if (key == KEY_SEARCH)
        return search(s);
    if (key == KEY_DELETE && len > 0)
        s->cfg.yt_query[len - 1] = '\0';
    else if (key == KEY_CLEAR)
        s->cfg.yt_query[0] = '\0';
    else if (key == KEY_SPACE)
        type_char(s, ' ');
    else if (key >= 0 && key < KEY_SPACE)
        type_char(s, KEY_CHARS[key]);
    return a;
}

static shell_action keyboard_key(shell *s, ft_key k)
{
    int col = s->item % SHELL_KEY_COLS, row = s->item / SHELL_KEY_COLS;
    shell_action a = {ACT_NONE, 0};
    if (k == FT_LEFT && col > 0)
        s->item--;
    else if (k == FT_RIGHT && col < SHELL_KEY_COLS - 1)
        s->item++;
    else if (k == FT_UP && row > 0)
        s->item -= SHELL_KEY_COLS;
    else if (k == FT_DOWN && row < SHELL_KEYS / SHELL_KEY_COLS - 1)
        s->item += SHELL_KEY_COLS;
    else if (k == FT_OK)
        a = press_key(s, s->item);
    else if (k == FT_BACK) {
        s->screen = SCR_HOME;
        s->row = 0;
    }
    return a;
}

shell_action shell_type(shell *s, int ch)
{
    shell_action a = {ACT_NONE, 0};
    if (0 && s->screen == SCR_HOME &&
        ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
        s->screen = SCR_KEYBOARD; /* typing on the YouTube tab starts a search */
        s->item = 0;
        s->cfg.yt_query[0] = '\0';
    }
    if (s->screen != SCR_KEYBOARD)
        return a;
    if (ch == '\n')
        return search(s);
    if (ch == '\b')
        return press_key(s, KEY_DELETE);
    if (ch == ' ' || (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z'))
        type_char(s, (char)ch);
    else if (ch >= 'A' && ch <= 'Z')
        type_char(s, (char)(ch - 'A' + 'a'));
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

void shell_focus_top(shell *s, int item)
{
    if (s->screen != SCR_HOME || item < 0 || item >= TOP_COUNT)
        return;
    s->row = -1;
    s->top = item;
    if (item < TAB_COUNT)
        select_tab(s, item);
}

void shell_focus_card(shell *s, int row, int col)
{
    shell_row rows[SHELL_MAX_ROWS];
    int n = shell_home_rows(s, rows, SHELL_MAX_ROWS);
    if (s->screen != SCR_HOME || row < 0 || row >= n || col < 0 || col >= rows[row].n)
        return;
    s->row = row;
    s->col[row] = col;
}

void shell_focus_item(shell *s, int item)
{
    if (s->screen == SCR_PIN) {
        if (item >= 0 && item < SHELL_PIN_DIGITS)
            s->pin_pos = item;
        return;
    }
    if (s->screen == SCR_KEYBOARD && item >= 0 && item < SHELL_KEYS)
        s->item = item;
    else if (s->screen != SCR_HOME && s->screen != SCR_KEYBOARD && item >= 0 && item < SHELL_MAX_ITEMS)
        s->item = item;
}

shell_action shell_key(shell *s, ft_key k)
{
    shell_action a = {ACT_NONE, 0};
    item it[SHELL_MAX_ITEMS];
    int n;

    if (k == FT_WHEEL_UP || k == FT_WHEEL_DOWN) /* rows, list rows, PIN digits: as Up and Down */
        k = k == FT_WHEEL_UP ? FT_UP : FT_DOWN;
    if (k != FT_LEFT && k != FT_RIGHT && k != FT_UP && k != FT_DOWN && k != FT_OK && k != FT_BACK)
        return a; /* pointer moves and clicks are placed by shell_focus_* first */
    s->status[0] = '\0';
    if (s->screen == SCR_HOME)
        return home_key(s, k);
    if (s->screen == SCR_PIN)
        return pin_key(s, k);
    if (s->screen == SCR_KEYBOARD)
        return keyboard_key(s, k);

    n = items(s, it, SHELL_MAX_ITEMS);
    if (k == FT_UP && s->item > 0) {
        s->item--;
    } else if (k == FT_DOWN && s->item < n - 1) {
        s->item++;
    } else if (k == FT_BACK) {
        if (s->screen == SCR_SETTINGS) {
            open_list(s, s->settings_return);
            a.act = ACT_SETTINGS_CLOSED;
        } else {
            s->screen = SCR_HOME; /* focus stays on the gear */
        }
    } else if (k == FT_OK && n > 0) {
        a.act = it[s->item].act;
        a.arg = it[s->item].arg;
    }
    return a;
}
