#include "render_text.h"

#include <stdio.h>
#include <time.h>

#define CLEAR "\033[H\033[2J"
#define INV "\033[7m"
#define RST "\033[0m"

/* UTC internally, Africa/Nairobi (UTC+3, no DST) on screen. */
static void nairobi_clock(char *out, size_t n)
{
    time_t t = time(NULL) + 3 * 3600;
    struct tm tmv;
    struct tm *g = gmtime(&t);
    if (!g) {
        snprintf(out, n, "--:--");
        return;
    }
    tmv = *g;
    strftime(out, n, "%H:%M", &tmv);
}

static void header(const char *title, const char *right)
{
    char clock[8];
    nairobi_clock(clock, sizeof clock);
    printf(CLEAR "  %-28s Nairobi %s | %s   [SIMULATOR]\n\n", title, clock, right);
}

static void footer(const char *keys, const char *status)
{
    printf("\n  %s\n", keys);
    printf("  (sim keys: arrows, Enter = OK, Backspace/Esc = Back, q = quit)\n");
    if (status && status[0])
        printf("\n  %s\n", status);
    fflush(stdout);
}

static void render_pin(const shell *s)
{
    int i;
    printf("      ");
    for (i = 0; i < SHELL_PIN_DIGITS; i++) {
        if (i == s->pin_pos)
            printf(" " INV "[ %c ]" RST, s->pin_digits[i]);
        else
            printf(" [ * ]");
    }
    printf("\n      ");
    for (i = 0; i < SHELL_PIN_DIGITS; i++)
        printf(i == s->pin_pos ? "   ^  " : "      ");
    printf("\n");
    footer(tr(s->cfg.lang, STR_HELP_PIN), s->status);
}

void render_shell(const shell *s)
{
    ft_lang L = s->cfg.lang;
    const char *profile = tr(L, s->cfg.profile == PROFILE_PARENT ? STR_PARENT : STR_CHILD);
    header(shell_screen_title(s), profile);

    if (s->screen == SCR_PIN) {
        render_pin(s);
        return;
    }

    if (s->screen == SCR_HOME) {
        int row, c;
        for (row = 0; row * SHELL_COLS < s->ntiles; row++) {
            for (c = 0; c < SHELL_COLS; c++) {
                int i = row * SHELL_COLS + c;
                if (i < s->ntiles)
                    printf("  +----------------------+");
            }
            printf("\n");
            for (c = 0; c < SHELL_COLS; c++) {
                int i = row * SHELL_COLS + c;
                if (i < s->ntiles) {
                    int on = i == s->focus;
                    printf("  |%s%s %-20s%s|", on ? INV : "", on ? ">" : " ",
                           shell_tile_label(s, s->tiles[i]), on ? RST : "");
                }
            }
            printf("\n");
            for (c = 0; c < SHELL_COLS; c++) {
                int i = row * SHELL_COLS + c;
                if (i < s->ntiles)
                    printf("  |  %-20.20s|", shell_tile_hint(s, s->tiles[i]));
            }
            printf("\n");
            for (c = 0; c < SHELL_COLS; c++) {
                int i = row * SHELL_COLS + c;
                if (i < s->ntiles)
                    printf("  +----------------------+");
            }
            printf("\n");
        }
        footer(tr(L, STR_HELP_HOME), s->status);
        return;
    }

    {
        char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
        int n = shell_list(s, labels, SHELL_MAX_ITEMS), i;
        if (s->screen == SCR_GAMES && s->cfg.profile == PROFILE_CHILD && s->cfg.child_limit_s != 0)
            printf("  %s: %u %s\n\n", tr(L, STR_TIME_LEFT),
                   (unsigned)((s->cfg.play_remaining_s + 59) / 60), tr(L, STR_MINUTES));
        for (i = 0; i < n; i++) {
            int on = i == s->item;
            printf("  %s%s %s%s\n", on ? INV : "", on ? ">" : " ", labels[i], on ? RST : "");
        }
        footer(tr(L, STR_HELP_LIST), s->status);
    }
}

static const char *racer_state_text(racer_state st)
{
    switch (st) {
    case RACER_START: return "Ready";
    case RACER_RUNNING: return "Racing";
    case RACER_PAUSED: return "PAUSED - OK: resume, Back: exit";
    case RACER_CRASHED: return "Race over - OK: race again, Back: exit";
    case RACER_FINISHED: return "FINISHED! - OK: race again, Back: exit";
    case RACER_EXITED: return "Exited";
    }
    return "";
}

void render_racer(const racer *r, const uint32_t best[RACER_LEVELS], const char *status)
{
    int row, lane;
    uint32_t left = r->tick >= RACER_DURATION_TICKS ? 0 : (RACER_DURATION_TICKS - r->tick) / FT_TICK_HZ;
    char right[64];
    snprintf(right, sizeof right, "%s%s", racer_level_name(r->level), r->practice ? " practice" : "");
    header("Family Racer", right);
    printf("  Score %-6u Best %-6u Time %u:%02u   Lives %s\n\n", r->score, best[r->level],
           left / 60, left % 60, r->practice ? "no-fail" : (r->lives >= 3 ? "***" : r->lives == 2 ? "** " : "*  "));
    for (row = 0; row < RACER_ROWS; row++) {
        printf("        |");
        for (lane = 0; lane < RACER_LANES; lane++) {
            int car = row == RACER_CAR_ROW && lane == r->lane;
            int block = r->rows[row] & (1u << lane);
            printf("%s|", car ? (block ? " XX " : " /\\ ") : block ? "[##]" : "    ");
        }
        printf("\n");
    }
    printf("\n  %s\n", racer_state_text(r->state));
    if (r->state == RACER_START)
        footer("OK: start   Left/Right: difficulty   Up/Down: practice on/off   Back: exit", status);
    else
        footer("Left/Right: change lane   Back: pause", status);
}

static void shot_row(const penalty *p, int player)
{
    int i, n = p->shots[player] > PEN_REGULATION ? p->shots[player] : PEN_REGULATION;
    printf("  Player %d  %d  ", player + 1, p->goals[player]);
    for (i = 0; i < n; i++) {
        if (i >= p->shots[player])
            printf(" . ");
        else
            printf((p->history[player] >> i) & 1u ? " O " : " X ");
    }
    printf("\n");
}

void render_penalty(const penalty *p, const char *status)
{
    static const char *const target_names[PEN_TARGETS] = {
        "Top L", "Low L", "Centre", "Low R", "Top R"
    };
    int i;
    header("Penalty Cup", p->sudden_death ? "sudden death on" : "sudden death off");
    shot_row(p, 0);
    shot_row(p, 1);
    printf("  (O = goal, X = saved)\n\n   ");
    for (i = 0; i < PEN_TARGETS; i++) {
        int on = p->phase == PEN_AIMING && i == p->aim;
        printf(" %s%s%-6s%s", on ? INV : "", on ? ">" : " ", target_names[i], on ? RST : "");
    }
    printf("\n");
    if (p->phase == PEN_RESULT || p->phase == PEN_OVER)
        printf("\n   Keeper dived: %s\n", target_names[p->keeper]);

    switch (p->phase) {
    case PEN_AIMING:
        printf("\n  Player %d to shoot.\n", penalty_turn(p) + 1);
        footer("Left/Right: pick target   OK: shoot   Back: pause", status);
        break;
    case PEN_RESULT:
        printf("\n  %s\n", p->scored ? "GOAL! (O)" : "SAVED! (X)");
        if (p->winner == PEN_NO_WINNER)
            printf("  Pass the remote to Player %d.\n", penalty_turn(p) + 1);
        footer("OK: continue   Back: pause", status);
        break;
    case PEN_OVER:
        if (p->winner == PEN_TIE)
            printf("\n  It's a draw!\n");
        else
            printf("\n  Player %d wins the cup!\n", p->winner + 1);
        footer("OK: new match   Back: exit", status);
        break;
    case PEN_PAUSED:
        printf("\n  PAUSED\n");
        footer("OK: resume   Back: save and exit", status);
        break;
    case PEN_EXITED:
        break;
    }
}

static const char TILE_CHARS[GARDEN_TILE_TYPES] = {' ', ',', ':', '#', '=', '~', '*', 'B'};

void render_garden(const garden *g, const char *status)
{
    enum { VW = 40, VH = 16 };
    int x0 = g->cx - VW / 2, y0 = g->cy - VH / 2, x, y, i;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 > GARDEN_W - VW) x0 = GARDEN_W - VW;
    if (y0 > GARDEN_H - VH) y0 = GARDEN_H - VH;

    header("Block Garden", g->dirty ? "unsaved changes" : "saved");
    printf("  +");
    for (x = 0; x < VW; x++) printf("-");
    printf("+  cursor %d,%d\n", g->cx, g->cy);
    for (y = y0; y < y0 + VH; y++) {
        printf("  |");
        for (x = x0; x < x0 + VW; x++) {
            char ch = TILE_CHARS[garden_get(g, x, y)];
            if (x == g->cx && y == g->cy)
                printf(INV "%c" RST, ch == ' ' ? '+' : ch);
            else
                putchar(ch);
        }
        printf("|\n");
    }
    printf("  +");
    for (x = 0; x < VW; x++) printf("-");
    printf("+\n  Tile: ");
    for (i = 0; i < GARDEN_TILE_TYPES; i++) {
        int on = i == g->selected;
        printf("%s%c%s'%c'%s ", on ? INV : "", on ? '[' : ' ', garden_tile_names[i], TILE_CHARS[i], on ? "]" RST : " ");
    }
    printf("\n");

    if (g->mode == GM_MENU) {
        printf("\n  MENU (paused)\n");
        for (i = 0; i < GMI_COUNT; i++)
            printf("  %s%s %s%s\n", i == g->menu ? INV : "", i == g->menu ? ">" : " ", garden_menu_names[i], i == g->menu ? RST : "");
        footer("Up/Down: move   OK: choose   Back: resume", status);
    } else if (g->mode == GM_PALETTE) {
        footer("Left/Right: choose tile   OK: done", status);
    } else {
        footer("Arrows: move   OK: place tile   Back: menu (tile, save, exit)", status);
    }
}
