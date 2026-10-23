#include "view.h"

#include <stdio.h>
#include <string.h>

/* ---- shared pieces --------------------------------------------------- */

#define HOME_TOP 140
#define HOME_TILE_H 190
#define HOME_GAP 28
#define LIST_TOP 150
#define LIST_ROW_H 56
#define LIST_STEP 64
#define LIST_VISIBLE 7
#define STATUS_Y 606
#define FOOTER_Y 666

static void header(ft_canvas *c, const char *title, const char *right)
{
    canvas_text_fit(c, &font_32x64, VIEW_MARGIN, 36, VIEW_W - 2 * VIEW_MARGIN - 360, title, COL_TEXT_LIGHT);
    if (right && right[0])
        canvas_text(c, &font_12x24, VIEW_W - VIEW_MARGIN - text_width(&font_12x24, right), 58, right,
                    COL_TEXT_SOFT);
}

static void status_line(ft_canvas *c, const char *status, int y)
{
    ft_rect r = {VIEW_MARGIN, y, VIEW_W - 2 * VIEW_MARGIN, 40};
    if (!status || !status[0])
        return;
    canvas_fill_round(c, r, 10, COL_STATUS);
    canvas_text_fit(c, &font_12x24, r.x + 16, r.y + 8, r.w - 32, status, COL_TEXT_LIGHT);
}

static void footer(ft_canvas *c, const char *help)
{
    canvas_text_fit(c, &font_12x24, VIEW_MARGIN, FOOTER_Y, VIEW_W - 2 * VIEW_MARGIN, help, COL_TEXT_SOFT);
}

/* Centred modal panel with a big title and a help line. */
static void overlay(ft_canvas *c, const char *title, const char *line, const char *help)
{
    ft_rect r = {320, 220, 640, 280};
    ft_rect t = {r.x, r.y + 36, r.w, 64};
    ft_rect l = {r.x + 24, r.y + 120, r.w - 48, 32};
    ft_rect h = {r.x + 24, r.y + 200, r.w - 48, 48};
    canvas_panel(c, r, 24, 6, COL_FOCUS, COL_OVERLAY);
    canvas_text_center(c, &font_32x64, t, title, COL_TEXT_LIGHT);
    if (line && line[0])
        canvas_text_center(c, &font_16x32, l, line, COL_TEXT_SOFT);
    if (help && help[0])
        canvas_text_center(c, &font_12x24, h, help, COL_TEXT_SOFT);
}

/* ---- shell ------------------------------------------------------------ */

ft_rect view_home_tile_rect(int index)
{
    int w = (VIEW_W - 2 * VIEW_MARGIN - (SHELL_COLS - 1) * HOME_GAP) / SHELL_COLS;
    ft_rect r;
    r.x = VIEW_MARGIN + (index % SHELL_COLS) * (w + HOME_GAP);
    r.y = HOME_TOP + (index / SHELL_COLS) * (HOME_TILE_H + HOME_GAP);
    r.w = w;
    r.h = HOME_TILE_H;
    return r;
}

ft_rect view_list_row_rect(int visible_row)
{
    ft_rect r = {VIEW_MARGIN, LIST_TOP + visible_row * LIST_STEP, VIEW_W - 2 * VIEW_MARGIN, LIST_ROW_H};
    return r;
}

ft_rect view_pin_box_rect(int digit)
{
    ft_rect r;
    int w = 130, gap = 36, total = SHELL_PIN_DIGITS * w + (SHELL_PIN_DIGITS - 1) * gap;
    r.x = (VIEW_W - total) / 2 + digit * (w + gap);
    r.y = 240;
    r.w = w;
    r.h = 170;
    return r;
}

static void home(ft_canvas *c, const shell *s)
{
    int i;
    for (i = 0; i < s->ntiles; i++) {
        ft_rect r = view_home_tile_rect(i);
        int on = i == s->focus;
        if (on)
            canvas_panel(c, r, 18, 6, COL_FOCUS, COL_TILE_FOCUS);
        else
            canvas_fill_round(c, r, 18, COL_TILE);
        canvas_text_fit(c, &font_16x32, r.x + 28, r.y + 30, r.w - 56, shell_tile_label(s, s->tiles[i]),
                        on ? COL_FOCUS : COL_TEXT_DARK);
        if (on) {
            ft_rect bar = {r.x + 28, r.y + 70, 48, 6};
            canvas_fill(c, bar, COL_FOCUS);
        }
        canvas_text_fit(c, &font_12x24, r.x + 28, r.y + r.h - 52, r.w - 56, shell_tile_hint(s, s->tiles[i]),
                        COL_TEXT_MUTED);
    }
    status_line(c, s->status, STATUS_Y);
    footer(c, tr(s->cfg.lang, STR_HELP_HOME));
}

static void list(ft_canvas *c, const shell *s)
{
    char labels[SHELL_MAX_ITEMS][SHELL_LABEL_MAX];
    int n = shell_list(s, labels, SHELL_MAX_ITEMS), first = 0, i;
    ft_lang L = s->cfg.lang;

    if (s->screen == SCR_GAMES && s->cfg.profile == PROFILE_CHILD && s->cfg.child_limit_s != 0) {
        char line[96];
        snprintf(line, sizeof line, "%s: %u %s", tr(L, STR_TIME_LEFT),
                 (unsigned)((s->cfg.play_remaining_s + 59) / 60), tr(L, STR_MINUTES));
        canvas_text(c, &font_12x24, VIEW_MARGIN, 112, line, COL_GOLD);
    }
    if (s->item >= LIST_VISIBLE)
        first = s->item - LIST_VISIBLE + 1;
    for (i = first; i < n && i < first + LIST_VISIBLE; i++) {
        ft_rect r = view_list_row_rect(i - first);
        int on = i == s->item;
        if (on) {
            ft_rect mark = {r.x + 14, r.y + 14, 8, r.h - 28};
            canvas_panel(c, r, 12, 4, COL_FOCUS, COL_TILE_FOCUS);
            canvas_fill(c, mark, COL_FOCUS);
        } else {
            canvas_fill_round(c, r, 12, COL_ROW);
        }
        canvas_text_fit(c, &font_16x32, r.x + 36, r.y + 12, r.w - 72, labels[i],
                        on ? COL_TEXT_DARK : COL_TEXT_LIGHT);
    }
    if (first > 0)
        canvas_triangle(c, VIEW_W - VIEW_MARGIN - 20, LIST_TOP - 26, 12, 12, 1, COL_TEXT_SOFT);
    if (first + LIST_VISIBLE < n)
        canvas_triangle(c, VIEW_W - VIEW_MARGIN - 20, LIST_TOP + LIST_VISIBLE * LIST_STEP + 4, 12, 12, 0,
                        COL_TEXT_SOFT);
    status_line(c, s->status, STATUS_Y);
    footer(c, tr(L, STR_HELP_LIST));
}

/* Only the digit being changed is drawn; the others always show '*', so the
 * picture is the same whatever the rest of the PIN is. */
static void pin(ft_canvas *c, const shell *s)
{
    int i;
    for (i = 0; i < SHELL_PIN_DIGITS; i++) {
        ft_rect r = view_pin_box_rect(i);
        char d[2] = {'*', '\0'};
        if (i == s->pin_pos) {
            d[0] = s->pin_digits[i];
            canvas_panel(c, r, 18, 6, COL_FOCUS, COL_TILE_FOCUS);
            canvas_text_center(c, &font_32x64, r, d, COL_TEXT_DARK);
            canvas_triangle(c, r.x + r.w / 2, r.y - 46, 22, 22, 1, COL_TEXT_LIGHT);
            canvas_triangle(c, r.x + r.w / 2, r.y + r.h + 46, 22, 22, 0, COL_TEXT_LIGHT);
        } else {
            canvas_fill_round(c, r, 18, COL_TILE);
            canvas_text_center(c, &font_32x64, r, d, COL_TEXT_MUTED);
        }
    }
    status_line(c, s->status, STATUS_Y);
    footer(c, tr(s->cfg.lang, STR_HELP_PIN));
}

void view_shell(ft_canvas *c, const shell *s, const char *clock_text)
{
    char right[96];
    ft_lang L = s->cfg.lang;
    canvas_clear(c, COL_BG);
    snprintf(right, sizeof right, "Nairobi %s | %s", clock_text ? clock_text : "--:--",
             tr(L, s->cfg.profile == PROFILE_PARENT ? STR_PARENT : STR_CHILD));
    header(c, shell_screen_title(s), right);
    if (s->screen == SCR_HOME)
        home(c, s);
    else if (s->screen == SCR_PIN)
        pin(c, s);
    else
        list(c, s);
}

/* ---- Family Racer ----------------------------------------------------- */

#define ROAD_LANE_W 130
#define ROAD_W (ROAD_LANE_W * RACER_LANES)
#define ROAD_X ((VIEW_W - ROAD_W) / 2)
#define ROW_H (VIEW_H / RACER_ROWS)

static void car(ft_canvas *c, int lane, int row, ft_color body, ft_color glass)
{
    ft_rect b = {ROAD_X + lane * ROAD_LANE_W + 25, row * ROW_H + 4, ROAD_LANE_W - 50, ROW_H - 8};
    ft_rect w = {b.x + 12, b.y + 10, b.w - 24, 12};
    ft_rect wl = {b.x - 4, b.y + 8, 6, 14}, wr = {b.x + b.w - 2, b.y + 8, 6, 14};
    ft_rect bl = {b.x - 4, b.y + b.h - 22, 6, 14}, br = {b.x + b.w - 2, b.y + b.h - 22, 6, 14};
    canvas_fill(c, wl, 0xFF111111u);
    canvas_fill(c, wr, 0xFF111111u);
    canvas_fill(c, bl, 0xFF111111u);
    canvas_fill(c, br, 0xFF111111u);
    canvas_fill_round(c, b, 12, body);
    canvas_fill_round(c, w, 4, glass);
}

static const char *level_name(ft_lang L, racer_level level)
{
    static const ft_str names[RACER_LEVELS] = {STR_LEVEL_EASY, STR_LEVEL_MEDIUM, STR_LEVEL_HARD};
    return tr(L, names[level < RACER_LEVELS ? level : RACER_EASY]);
}

void view_racer(ft_canvas *c, const racer *r, const uint32_t best[RACER_LEVELS], ft_lang L, const char *status)
{
    ft_rect road = {ROAD_X, 0, ROAD_W, VIEW_H};
    ft_rect edge_l = {ROAD_X - 8, 0, 8, VIEW_H}, edge_r = {ROAD_X + ROAD_W, 0, 8, VIEW_H};
    uint32_t left = r->tick >= RACER_DURATION_TICKS ? 0 : (RACER_DURATION_TICKS - r->tick) / FT_TICK_HZ;
    int offset = (int)(r->tick * 6 % 60), row, lane, i, y;
    char line[64];

    canvas_clear(c, 0xFF2F6B3Fu);
    canvas_fill(c, road, 0xFF3B4654u);
    canvas_fill(c, edge_l, COL_TEXT_LIGHT);
    canvas_fill(c, edge_r, COL_TEXT_LIGHT);
    for (lane = 1; lane < RACER_LANES; lane++)
        for (y = offset - 60; y < VIEW_H; y += 60) {
            ft_rect dash = {ROAD_X + lane * ROAD_LANE_W - 3, y, 6, 30};
            canvas_fill(c, dash, 0xFFE9EEF2u);
        }
    for (row = 0; row < RACER_ROWS; row++)
        for (lane = 0; lane < RACER_LANES; lane++)
            if (r->rows[row] & (1u << lane))
                car(c, lane, row, 0xFFE8833Au, 0xFF5A2E10u);
    car(c, r->lane, RACER_CAR_ROW, 0xFF19B3A9u, COL_TEXT_LIGHT);

    /* Heads-up display on the grass, left of the road. */
    y = 60;
    canvas_text(c, &font_16x32, 48, y, tr(L, STR_GAME_RACER), COL_TEXT_LIGHT);
    canvas_text(c, &font_12x24, 48, y + 40, level_name(L, r->level), COL_TEXT_SOFT);
    snprintf(line, sizeof line, "%s %u", tr(L, STR_SCORE), (unsigned)r->score);
    canvas_text(c, &font_16x32, 48, y + 100, line, COL_TEXT_LIGHT);
    snprintf(line, sizeof line, "%s %u", tr(L, STR_BEST), (unsigned)best[r->level]);
    canvas_text(c, &font_16x32, 48, y + 140, line, COL_TEXT_LIGHT);
    snprintf(line, sizeof line, "%s %u:%02u", tr(L, STR_TIME), (unsigned)(left / 60), (unsigned)(left % 60));
    canvas_text(c, &font_16x32, 48, y + 180, line, COL_TEXT_LIGHT);
    if (r->practice) {
        canvas_text_fit(c, &font_12x24, 48, y + 230, ROAD_X - 80, tr(L, STR_PRACTICE), COL_GOLD);
    } else {
        canvas_text(c, &font_16x32, 48, y + 220, tr(L, STR_LIVES), COL_TEXT_LIGHT);
        for (i = 0; i < RACER_LIVES; i++)
            canvas_circle(c, 64 + i * 36, y + 280, 12, i < r->lives ? COL_BAD : 0xFF24553Au);
    }
    {
        ft_rect help = {ROAD_X + ROAD_W + 40, 60, VIEW_W - (ROAD_X + ROAD_W + 40) - 40, 200};
        canvas_text_wrap(c, &font_12x24, help, tr(L, r->state == RACER_START ? STR_RACER_START_HELP : STR_RACER_HELP),
                         COL_TEXT_LIGHT);
    }

    switch (r->state) {
    case RACER_START:
        overlay(c, tr(L, STR_READY), r->practice ? tr(L, STR_PRACTICE) : level_name(L, r->level),
                tr(L, STR_RACER_START_HELP));
        break;
    case RACER_PAUSED:
        overlay(c, tr(L, STR_PAUSED), NULL, tr(L, STR_PAUSED_HELP));
        break;
    case RACER_CRASHED:
    case RACER_FINISHED:
        snprintf(line, sizeof line, "%s %u", tr(L, STR_SCORE), (unsigned)r->score);
        overlay(c, tr(L, r->state == RACER_FINISHED ? STR_FINISHED : STR_RACE_OVER), line, tr(L, STR_AGAIN_HELP));
        break;
    case RACER_RUNNING:
    case RACER_EXITED:
        break;
    }
    status_line(c, status, STATUS_Y);
}

/* ---- Penalty Cup ------------------------------------------------------ */

#define GOAL_X 340
#define GOAL_Y 100
#define GOAL_W 600
#define GOAL_H 300

static void target_pos(int t, int *x, int *y)
{
    static const int tx[PEN_TARGETS] = {430, 430, 640, 850, 850};
    static const int ty[PEN_TARGETS] = {170, 330, 250, 330, 170};
    *x = tx[t];
    *y = ty[t];
}

static void ball(ft_canvas *c, int x, int y)
{
    canvas_circle(c, x, y, 22, COL_TEXT_LIGHT);
    canvas_circle(c, x, y, 7, 0xFF222222u);
    canvas_circle(c, x - 13, y - 9, 4, 0xFF222222u);
    canvas_circle(c, x + 13, y - 9, 4, 0xFF222222u);
    canvas_circle(c, x, y + 15, 4, 0xFF222222u);
}

static void keeper(ft_canvas *c, int x, int y)
{
    ft_rect body = {x - 34, y - 40, 68, 96};
    ft_rect arm_l = {x - 74, y - 34, 40, 18}, arm_r = {x + 34, y - 34, 40, 18};
    canvas_fill_round(c, arm_l, 8, COL_GOLD);
    canvas_fill_round(c, arm_r, 8, COL_GOLD);
    canvas_fill_round(c, body, 14, COL_GOLD);
    canvas_circle(c, x, y - 62, 22, 0xFF6B4226u);
}

static void shot_marks(ft_canvas *c, const penalty *p, int player, int x, int y, ft_lang L)
{
    char line[48];
    int shots = p->shots[player], shown = shots > PEN_REGULATION ? shots : PEN_REGULATION;
    int start = shown > 6 ? shown - 6 : 0, i;
    snprintf(line, sizeof line, "%s %d   %d", tr(L, STR_PLAYER), player + 1, p->goals[player]);
    canvas_text(c, &font_16x32, x, y, line, penalty_turn(p) == player && p->phase == PEN_AIMING ? COL_GOLD
                                                                                                 : COL_TEXT_LIGHT);
    for (i = start; i < shown; i++) {
        int cx = x + 18 + (i - start) * 42, cy = y + 58;
        char mark[2] = {0, 0};
        if (i >= shots) {
            canvas_circle(c, cx, cy, 16, COL_TEXT_SOFT);
            canvas_circle(c, cx, cy, 12, 0xFF2E7D46u);
            continue;
        }
        mark[0] = ((p->history[player] >> i) & 1u) ? 'O' : 'X';
        canvas_circle(c, cx, cy, 16, mark[0] == 'O' ? COL_GOOD : COL_BAD);
        canvas_text(c, &font_12x24, cx - 6, cy - 12, mark, COL_TEXT_LIGHT);
    }
}

void view_penalty(ft_canvas *c, const penalty *p, ft_lang L, const char *status)
{
    ft_rect goal = {GOAL_X, GOAL_Y, GOAL_W, GOAL_H};
    ft_rect msg = {GOAL_X - 100, 520, GOAL_W + 200, 64};
    int i, x, y, kx = 640, ky = 300;
    char line[96];

    canvas_clear(c, 0xFF2E7D46u);
    for (x = 0; x < VIEW_W; x += 160) {
        ft_rect stripe = {x, 0, 80, VIEW_H};
        canvas_fill(c, stripe, 0xFF2A7340u);
    }
    canvas_fill(c, goal, 0xFF245F37u);
    for (x = GOAL_X; x < GOAL_X + GOAL_W; x += 40) {
        ft_rect v = {x, GOAL_Y, 2, GOAL_H};
        canvas_fill(c, v, 0xFF8FB49Bu);
    }
    for (y = GOAL_Y; y < GOAL_Y + GOAL_H; y += 40) {
        ft_rect h = {GOAL_X, y, GOAL_W, 2};
        canvas_fill(c, h, 0xFF8FB49Bu);
    }
    {
        ft_rect bar = {GOAL_X - 12, GOAL_Y - 12, GOAL_W + 24, 12};
        ft_rect post_l = {GOAL_X - 12, GOAL_Y - 12, 12, GOAL_H + 12};
        ft_rect post_r = {GOAL_X + GOAL_W, GOAL_Y - 12, 12, GOAL_H + 12};
        ft_rect line_r = {0, GOAL_Y + GOAL_H, VIEW_W, 4};
        canvas_fill(c, line_r, 0xFFDDE8DFu);
        canvas_fill(c, bar, COL_TEXT_LIGHT);
        canvas_fill(c, post_l, COL_TEXT_LIGHT);
        canvas_fill(c, post_r, COL_TEXT_LIGHT);
    }

    for (i = 0; i < PEN_TARGETS; i++) {
        int on = p->phase == PEN_AIMING && i == p->aim;
        target_pos(i, &x, &y);
        canvas_circle(c, x, y, on ? 44 : 34, on ? COL_FOCUS : COL_TEXT_LIGHT);
        canvas_circle(c, x, y, on ? 32 : 28, on ? COL_TILE_FOCUS : 0xFF245F37u);
        if (on) {
            ft_rect hx = {x - 20, y - 3, 40, 6}, vy = {x - 3, y - 20, 6, 40};
            canvas_fill(c, hx, COL_FOCUS);
            canvas_fill(c, vy, COL_FOCUS);
        }
    }

    if (p->phase == PEN_RESULT || p->phase == PEN_OVER)
        target_pos(p->keeper, &kx, &ky);
    keeper(c, kx, ky + 20);
    if (p->phase == PEN_RESULT || p->phase == PEN_OVER) {
        target_pos(p->aim, &x, &y);
        if (!p->scored) {
            x = kx + 60;
            y = ky + 70;
        }
        ball(c, x, y);
    } else {
        ball(c, 640, 460);
    }

    shot_marks(c, p, 0, 40, 120, L);
    shot_marks(c, p, 1, 40, 250, L);
    if (p->sudden_death)
        canvas_text_fit(c, &font_12x24, 40, 348, GOAL_X - 70, tr(L, STR_SUDDEN_DEATH), COL_TEXT_SOFT);

    switch (p->phase) {
    case PEN_AIMING:
        snprintf(line, sizeof line, "%s %d %s", tr(L, STR_PLAYER), penalty_turn(p) + 1, tr(L, STR_TO_SHOOT));
        canvas_text_center(c, &font_16x32, msg, line, COL_TEXT_LIGHT);
        footer(c, tr(L, STR_PEN_HELP));
        break;
    case PEN_RESULT:
        canvas_text_center(c, &font_32x64, msg, tr(L, p->scored ? STR_GOAL : STR_SAVED),
                           p->scored ? COL_GOLD : COL_TEXT_LIGHT);
        if (p->winner == PEN_NO_WINNER) {
            ft_rect sub = {msg.x, msg.y + 64, msg.w, 32};
            snprintf(line, sizeof line, "%s %s %d", tr(L, STR_PASS_REMOTE), tr(L, STR_PLAYER), penalty_turn(p) + 1);
            canvas_text_center(c, &font_12x24, sub, line, COL_TEXT_SOFT);
        }
        footer(c, tr(L, STR_CONTINUE_HELP));
        break;
    case PEN_OVER:
        if (p->winner == PEN_TIE)
            snprintf(line, sizeof line, "%s", tr(L, STR_DRAW));
        else
            snprintf(line, sizeof line, "%s %d %s", tr(L, STR_PLAYER), p->winner + 1, tr(L, STR_WINS));
        overlay(c, tr(L, STR_GAME_PENALTY), line, tr(L, STR_NEW_MATCH_HELP));
        break;
    case PEN_PAUSED:
        overlay(c, tr(L, STR_PAUSED), NULL, tr(L, STR_PAUSED_HELP));
        break;
    case PEN_EXITED:
        break;
    }
    status_line(c, status, 620);
}

/* ---- Block Garden ----------------------------------------------------- */

#define CELL 18
#define MAP_X ((VIEW_W - GARDEN_W * CELL) / 2)
#define MAP_Y 72
#define MAP_ROWS 27

static const ft_str TILE_NAMES[GARDEN_TILE_TYPES] = {
    STR_TILE_ERASE, STR_TILE_GRASS, STR_TILE_DIRT, STR_TILE_STONE,
    STR_TILE_WOOD, STR_TILE_WATER, STR_TILE_FLOWER, STR_TILE_BRICK
};

/* Each tile has a pattern as well as a colour, so tiles differ by shape too. */
static void tile(ft_canvas *c, int x, int y, int size, int t)
{
    ft_rect r = {x, y, size, size};
    int q = size / 4;
    switch (t) {
    case 1: {
        ft_rect top = {x, y, size, q};
        canvas_fill(c, r, 0xFF4CAF50u);
        canvas_fill(c, top, 0xFF7CCB62u);
        break;
    }
    case 2:
        canvas_fill(c, r, 0xFF8B5E3Cu);
        canvas_circle(c, x + q, y + q, 1, 0xFF5E3E26u);
        canvas_circle(c, x + 3 * q, y + 2 * q, 1, 0xFF5E3E26u);
        canvas_circle(c, x + q + 1, y + 3 * q, 1, 0xFF5E3E26u);
        break;
    case 3: {
        ft_rect crack = {x + q, y + 2 * q, 2 * q, 2};
        canvas_fill(c, r, 0xFF9AA5B1u);
        canvas_fill(c, crack, 0xFF6C7682u);
        canvas_circle(c, x + 3 * q, y + q, 1, 0xFF6C7682u);
        break;
    }
    case 4: {
        ft_rect a = {x + q, y, 2, size}, b = {x + 3 * q, y, 2, size};
        canvas_fill(c, r, 0xFFB9853Fu);
        canvas_fill(c, a, 0xFF8A5F28u);
        canvas_fill(c, b, 0xFF8A5F28u);
        break;
    }
    case 5: {
        ft_rect w1 = {x + 2, y + q, size / 2, 2}, w2 = {x + size / 2 - 2, y + 3 * q, size / 2, 2};
        canvas_fill(c, r, 0xFF2F80D8u);
        canvas_fill(c, w1, 0xFF9CC9F5u);
        canvas_fill(c, w2, 0xFF9CC9F5u);
        break;
    }
    case 6:
        canvas_fill(c, r, 0xFF4CAF50u);
        canvas_circle(c, x + size / 2, y + size / 2, size / 3, 0xFFF06292u);
        canvas_circle(c, x + size / 2, y + size / 2, size / 8 + 1, 0xFFFFE066u);
        break;
    case 7: {
        ft_rect m1 = {x, y + size / 2 - 1, size, 2}, m2 = {x + size / 2 - 1, y, 2, size / 2},
                m3 = {x + q - 1, y + size / 2, 2, size / 2};
        canvas_fill(c, r, 0xFFB4513Au);
        canvas_fill(c, m1, 0xFFE8D6C8u);
        canvas_fill(c, m2, 0xFFE8D6C8u);
        canvas_fill(c, m3, 0xFFE8D6C8u);
        break;
    }
    default:
        canvas_fill(c, r, 0xFFCFE8F7u);
        break;
    }
}

void view_garden(ft_canvas *c, const garden *g, ft_lang L, const char *status)
{
    static const ft_str MENU[GMI_COUNT] = {
        STR_MENU_RESUME, STR_MENU_TILE, STR_MENU_SAVE, STR_MENU_SAVE_EXIT, STR_MENU_EXIT
    };
    int y0 = g->cy - MAP_ROWS / 2, x, y, i;
    ft_rect frame = {MAP_X - 6, MAP_Y - 6, GARDEN_W * CELL + 12, MAP_ROWS * CELL + 12};
    const char *state = tr(L, g->dirty ? STR_UNSAVED : STR_SAVED_STATE);

    if (y0 < 0)
        y0 = 0;
    if (y0 > GARDEN_H - MAP_ROWS)
        y0 = GARDEN_H - MAP_ROWS;

    canvas_clear(c, COL_BG);
    canvas_text(c, &font_16x32, MAP_X, 20, tr(L, STR_GAME_GARDEN), COL_TEXT_LIGHT);
    if (status && status[0]) {
        int w = text_width(&font_12x24, status), max_w = GARDEN_W * CELL - 360;
        if (w > max_w)
            w = max_w;
        canvas_text_fit(c, &font_12x24, VIEW_W - MAP_X - w, 26, max_w, status, COL_GOLD);
    } else {
        canvas_text(c, &font_12x24, VIEW_W - MAP_X - text_width(&font_12x24, state), 26, state,
                    g->dirty ? COL_GOLD : COL_TEXT_SOFT);
    }
    canvas_fill_round(c, frame, 8, COL_ROW);
    for (y = 0; y < MAP_ROWS; y++)
        for (x = 0; x < GARDEN_W; x++)
            tile(c, MAP_X + x * CELL, MAP_Y + y * CELL, CELL, garden_get(g, x, y0 + y));
    {
        int cx = MAP_X + g->cx * CELL, cy = MAP_Y + (g->cy - y0) * CELL;
        ft_rect outer = {cx - 4, cy - 4, CELL + 8, CELL + 8}, inner = {cx - 1, cy - 1, CELL + 2, CELL + 2};
        canvas_fill(c, outer, COL_TEXT_LIGHT);
        canvas_fill(c, inner, 0xFF111111u);
        tile(c, cx + 1, cy + 1, CELL - 2, g->selected);
    }

    /* Palette is always visible (plan p.23). */
    for (i = 0; i < GARDEN_TILE_TYPES; i++) {
        ft_rect r = {MAP_X + i * 144, 586, 136, 64};
        int on = i == g->selected;
        if (on)
            canvas_panel(c, r, 10, g->mode == GM_PALETTE ? 6 : 4, COL_FOCUS, COL_TILE_FOCUS);
        else
            canvas_fill_round(c, r, 10, COL_ROW);
        tile(c, r.x + 10, r.y + 14, 36, i);
        canvas_text_fit(c, &font_12x24, r.x + 54, r.y + 20, r.w - 60, tr(L, TILE_NAMES[i]),
                        on ? COL_TEXT_DARK : COL_TEXT_LIGHT);
    }

    if (g->mode == GM_MENU) {
        ft_rect panel = {380, 120, 520, 420};
        canvas_panel(c, panel, 20, 6, COL_FOCUS, COL_OVERLAY);
        canvas_text(c, &font_16x32, panel.x + 32, panel.y + 24, tr(L, STR_MENU), COL_TEXT_LIGHT);
        for (i = 0; i < GMI_COUNT; i++) {
            ft_rect row = {panel.x + 24, panel.y + 80 + i * 64, panel.w - 48, 52};
            int on = i == g->menu;
            if (on)
                canvas_panel(c, row, 10, 4, COL_FOCUS, COL_TILE_FOCUS);
            canvas_text_fit(c, &font_16x32, row.x + 20, row.y + 10, row.w - 40, tr(L, MENU[i]),
                            on ? COL_TEXT_DARK : COL_TEXT_LIGHT);
        }
        footer(c, tr(L, STR_HELP_LIST));
    } else {
        footer(c, tr(L, g->mode == GM_PALETTE ? STR_PALETTE_HELP : STR_GARDEN_HELP));
    }
}
