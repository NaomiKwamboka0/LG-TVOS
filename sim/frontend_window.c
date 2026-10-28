/* Desktop window: shows the 1280x720 pixel views (ui/view.c) using SDL2,
 * scaled smoothly to the window or the whole screen. Keys: arrows,
 * Enter = OK, Backspace/Esc = Back, F11 or F = full screen, q = quit. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "frontend.h"
#include "view.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t pixels[VIEW_W * VIEW_H];
static uint32_t video[VIEW_W * VIEW_H]; /* newest picture, kept between frames */
static ft_canvas canvas;
static Uint32 wake_event = (Uint32)-1;
static SDL_atomic_t wake_pending;

static void present(void)
{
    SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * (int)sizeof pixels[0]);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void window_folders(char *data, size_t data_n, char *base, size_t base_n)
{
    char *pref = SDL_GetPrefPath("FamilyTV", "FamilyTV");
    char *exe = SDL_GetBasePath();
    snprintf(data, data_n, "%s", pref ? pref : "");
    snprintf(base, base_n, "%s", exe ? exe : "");
    SDL_free(pref);
    SDL_free(exe);
}

static void set_fullscreen(int on)
{
    SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}

static int window_is_fullscreen(void)
{
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
}

static int window_open(int scripted, int fullscreen)
{
    SDL_Rect usable;
    int w = VIEW_W, h = VIEW_H;
    (void)scripted;
    SDL_SetMainReady();
    /* Sharp on high-DPI Windows laptops instead of a blurry bitmap stretch. */
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    SDL_SetHint("SDL_WINDOWS_DPI_SCALING", "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1"); /* blocked only while watching */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); /* smooth scaling for pictures and fullscreen */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    /* A 1366x768 laptop has no room for 1280x720 plus title bar and taskbar. */
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0 && usable.w > 0 && usable.h > 0) {
        if (w > usable.w * 9 / 10)
            w = usable.w * 9 / 10;
        if (w * VIEW_H / VIEW_W > usable.h * 9 / 10)
            w = usable.h * 9 / 10 * VIEW_W / VIEW_H;
        h = w * VIEW_H / VIEW_W;
    }
    window = SDL_CreateWindow("Family TV", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
    wake_event = SDL_RegisterEvents(1);
    if (fullscreen)
        set_fullscreen(1);
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
    case SDLK_F11:
    case SDLK_f:
        if (!k->repeat)
            set_fullscreen(!window_is_fullscreen());
        return FT_NONE;
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
            if (e.type == wake_event) {
                SDL_AtomicSet(&wake_pending, 0);
                return FT_NONE;
            }
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
    SDL_EnableScreenSaver();
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

static void window_live(const live *l, player *p, const shell *s)
{
    char clock[8];
    int has_picture = player_render(p, video, VIEW_W, VIEW_H, VIEW_W);
    SDL_DisableScreenSaver(); /* nobody touches the keys while watching */
    if (has_picture)
        memcpy(pixels, video, sizeof pixels);
    sim_clock_text(clock, sizeof clock);
    view_live(&canvas, l, player_get(p), s, clock, has_picture);
    present();
}

/* Called by the player from its own threads: one pending wake-up is
 * enough, so a busy stream cannot flood the event queue. */
static void window_wake(void *ctx)
{
    (void)ctx;
    if (wake_event != (Uint32)-1 && SDL_AtomicCAS(&wake_pending, 0, 1)) {
        SDL_Event e;
        SDL_memset(&e, 0, sizeof e);
        e.type = wake_event;
        if (SDL_PushEvent(&e) != 1)
            SDL_AtomicSet(&wake_pending, 0);
    }
}

const sim_frontend frontend_window = {
    "window", window_folders, window_open, window_close, window_poll, window_shell, window_racer, window_penalty,
    window_garden, window_live, window_wake, window_is_fullscreen
};
