/* Desktop-window frontend for the host simulator: shows the real 1280x720
 * pixel views (ui/view.c) using SDL2. Development computer only; SDL is not
 * part of the TV build. Keys: arrows, Enter = OK, Backspace/Esc = Back,
 * q = quit. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

#include "frontend.h"
#include "view.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t pixels[VIEW_W * VIEW_H];
static ft_canvas canvas;

static void present(void)
{
    SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * (int)sizeof pixels[0]);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static int window_open(int scripted)
{
    (void)scripted;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); /* keep the bitmap font crisp */
    window = SDL_CreateWindow("Family TV [SIMULATOR]", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              VIEW_W, VIEW_H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window)
        renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer)
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VIEW_W,
                                    VIEW_H);
    if (!texture) {
        fprintf(stderr, "SDL window setup failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    SDL_RenderSetLogicalSize(renderer, VIEW_W, VIEW_H);
    canvas_init(&canvas, pixels, VIEW_W, VIEW_H, VIEW_W);
    canvas_clear(&canvas, COL_BG);
    present();
    return 0;
}

static void dump_frame(const char *path)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", VIEW_W, VIEW_H);
    for (i = 0; i < VIEW_W * VIEW_H; i++) {
        unsigned char rgb[3];
        rgb[0] = (unsigned char)(pixels[i] >> 16);
        rgb[1] = (unsigned char)(pixels[i] >> 8);
        rgb[2] = (unsigned char)pixels[i];
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void window_close(void)
{
    if (sim_dump_path)
        dump_frame(sim_dump_path);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

static ft_key map_key(const SDL_KeyboardEvent *k, int *quit)
{
    switch (k->keysym.sym) {
    case SDLK_LEFT: return FT_LEFT;
    case SDLK_RIGHT: return FT_RIGHT;
    case SDLK_UP: return FT_UP;
    case SDLK_DOWN: return FT_DOWN;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
        /* A held OK must not activate twice (test U02). */
        return k->repeat ? FT_NONE : FT_OK;
    case SDLK_BACKSPACE:
    case SDLK_ESCAPE:
        return k->repeat ? FT_NONE : FT_BACK;
    case SDLK_q:
        *quit = 1;
        return FT_NONE;
    default:
        return FT_NONE;
    }
}

static ft_key window_poll(int ms, int *quit)
{
    Uint32 start = SDL_GetTicks();
    for (;;) {
        SDL_Event e;
        int wait = 200;
        if (ms > 0) {
            Uint32 spent = SDL_GetTicks() - start;
            if (spent >= (Uint32)ms)
                return FT_NONE;
            wait = ms - (int)spent;
        }
        if (SDL_WaitEventTimeout(&e, wait)) {
            if (e.type == SDL_QUIT) {
                *quit = 1;
                return FT_NONE;
            }
            if (e.type == SDL_WINDOWEVENT)
                present(); /* redraw after expose/resize */
            if (e.type == SDL_KEYDOWN) {
                ft_key k = map_key(&e.key, quit);
                if (k != FT_NONE || *quit)
                    return k;
            }
        }
        if (*quit)
            return FT_NONE;
    }
}

static void window_shell(const shell *s)
{
    char clock[8];
    sim_clock_text(clock, sizeof clock);
    view_shell(&canvas, s, clock);
    present();
}

static void window_racer(const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang, const char *status)
{
    view_racer(&canvas, r, best, lang, status);
    present();
}

static void window_penalty(const penalty *p, ft_lang lang, const char *status)
{
    view_penalty(&canvas, p, lang, status);
    present();
}

static void window_garden(const garden *g, ft_lang lang, const char *status)
{
    view_garden(&canvas, g, lang, status);
    present();
}

const sim_frontend frontend_window = {
    "window", window_open, window_close, window_poll, window_shell, window_racer, window_penalty, window_garden
};
