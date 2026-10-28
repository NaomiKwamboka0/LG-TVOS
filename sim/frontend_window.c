/* Desktop window: shows the pixel views (ui/view.c) using SDL2. The views
 * are drawn at the window's real resolution: 1x (720p), 1.5x (1080p),
 * 2x (1440p, Retina) or 3x (4K); live TV stops at 1.5x, as no channel is
 * sharper than 1080p. Keys: arrows, Enter = OK, Backspace/Esc = Back,
 * F11 or F = full screen, q = quit. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend.h"
#include "view.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *pixels;     /* the canvas: pw x ph physical pixels */
static int pw, ph, scale;    /* scale: CANVAS_* in use, 0 before the first */
/* Newest picture, kept between frames. The player's software scaler wants
 * 64-byte alignment, which malloc does not promise. */
static uint8_t *video_store;
static uint32_t *video;
static ft_canvas canvas;
static Uint32 wake_event = (Uint32)-1;
static int live_on_screen; /* pixels hold the live view; other screens clear it */
static SDL_atomic_t wake_pending;

static void present(void)
{
    if (!texture)
        return;
    SDL_UpdateTexture(texture, NULL, pixels, pw * (int)sizeof pixels[0]);
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

/* The biggest scale the window's real pixels can show (Retina and Windows
 * display scaling count), and that fonts can be made for. */
static int best_scale(void)
{
    int ow = 0, oh = 0;
    if (!font_scale_ready() || SDL_GetRendererOutputSize(renderer, &ow, &oh) != 0)
        return CANVAS_1X;
    if (ow >= VIEW_W * 3 && oh >= VIEW_H * 3)
        return CANVAS_3X;
    if (ow >= VIEW_W * 2 && oh >= VIEW_H * 2)
        return CANVAS_2X;
    if (ow >= VIEW_W * 3 / 2 && oh >= VIEW_H * 3 / 2)
        return CANVAS_1_5X;
    return CANVAS_1X;
}

/* Sizes the canvas, texture and picture buffer for a scale. Keeps the old
 * ones if the new ones cannot be had. */
static void use_scale(int want)
{
    int w = VIEW_W * want / 2, h = VIEW_H * want / 2;
    SDL_Texture *t;
    uint32_t *px;
    uint8_t *vs;
    if (want == scale)
        return;
    t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    px = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof *px);
    vs = (uint8_t *)malloc((size_t)w * (size_t)h * 4 + 64);
    if (!t || !px || !vs) {
        if (t)
            SDL_DestroyTexture(t);
        free(px);
        free(vs);
        return;
    }
    if (texture)
        SDL_DestroyTexture(texture);
    free(pixels);
    free(video_store);
    texture = t;
    pixels = px;
    video_store = vs;
    video = (uint32_t *)(void *)(vs + (64 - (uintptr_t)vs % 64) % 64);
    pw = w;
    ph = h;
    scale = want;
    SDL_RenderSetLogicalSize(renderer, pw, ph);
    canvas_init(&canvas, pixels, pw, ph, pw);
    canvas_set_scale(&canvas, scale);
    canvas_clear(&canvas, COL_BG);
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
        use_scale(CANVAS_1X);
    if (!texture) {
        fprintf(stderr, "SDL window setup failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    wake_event = SDL_RegisterEvents(1);
    if (fullscreen)
        set_fullscreen(1);
    use_scale(best_scale());
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
    fprintf(f, "P6\n%d %d\n255\n", pw, ph);
    for (i = 0; i < pw * ph; i++) {
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
    free(pixels);
    free(video_store);
    pixels = NULL;
    video_store = NULL;
    scale = 0;
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
            if (e.type == SDL_WINDOWEVENT) {
                int was = scale;
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    use_scale(best_scale());
                if (scale != was) {
                    live_on_screen = 0;
                    return FT_NONE; /* redraw at the new size */
                }
                present(); /* expose */
            }
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

/* Home screen motion: about 60 frames a second while something moves,
 * then 12 a second for the drifting background. */
#define MOTION_FRAME_MS 16
#define AMBIENT_FRAME_MS 80

static ui_motion motion;
static Uint32 motion_tick;

static int window_shell(const shell *s)
{
    char clock[8];
    Uint32 now = SDL_GetTicks();
    int moving = 0, dt = motion_tick ? (int)(now - motion_tick) : MOTION_FRAME_MS;
    live_on_screen = 0;
    use_scale(best_scale());
    SDL_EnableScreenSaver();
    sim_clock_text(clock, sizeof clock);
    motion_tick = now;
    if (s->screen == SCR_HOME) {
        ui_targets t;
        view_home_targets(s, &t);
        moving = motion_step(&motion, &t, s->tab, dt > 100 ? 100 : dt);
    } else {
        motion.valid = 0; /* back on Home, the rows start where they belong */
    }
    view_shell_motion(&canvas, s, clock, s->screen == SCR_HOME ? &motion : NULL);
    present();
    return s->screen != SCR_HOME ? 0 : moving ? MOTION_FRAME_MS : AMBIENT_FRAME_MS;
}

static void window_startup(uint32_t t_ms, ft_lang lang)
{
    use_scale(best_scale());
    view_startup(&canvas, t_ms, lang);
    present();
}

static void window_racer(const racer *r, const uint32_t best[RACER_LEVELS], ft_lang lang, const char *status)
{
    live_on_screen = 0;
    view_racer(&canvas, r, best, lang, status);
    present();
}

static void window_penalty(const penalty *p, ft_lang lang, const char *status)
{
    live_on_screen = 0;
    view_penalty(&canvas, p, lang, status);
    present();
}

static void window_garden(const garden *g, ft_lang lang, const char *status)
{
    live_on_screen = 0;
    view_garden(&canvas, g, lang, status);
    present();
}

/* --stats: painted frames per second and the milliseconds each one spent
 * in the player's picture, our overlays and the upload, averaged over a
 * second (the upload of a frame is counted with the next one). */
static Uint64 stat_t[3];
static void stats(ft_canvas *c, const player_status *ps)
{
    static Uint64 window_start;
    static int frames;
    static char line[128] = "measuring...";
    Uint64 now = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
    ft_rect box = {8, 8, 0, 30};
    double ms = 1000.0 / (double)hz;
    frames++;
    if (!window_start)
        window_start = now;
    if (now - window_start >= hz) {
        snprintf(line, sizeof line, "%d fps  picture %.1f  overlay %.1f  upload %.1f ms  %dp", frames,
                 (double)stat_t[0] * ms / frames, (double)stat_t[1] * ms / frames, (double)stat_t[2] * ms / frames,
                 ps->video_h);
        window_start = now;
        stat_t[0] = stat_t[1] = stat_t[2] = 0;
        frames = 0;
    }
    box.w = text_width(&font_caption, line) + 20;
    canvas_round(c, box, 8, 0xC0000000u);
    canvas_text(c, &font_caption, box.x + 10, box.y + 4, line, COL_TEXT);
}

/* Repaints only for a new picture or a visible overlay change: the loop
 * wakes far more often than either happens. */
static void window_live(const live *l, player *p, const shell *s, const channel_list *chs)
{
    static uint32_t last_sig;
    Uint64 t0 = SDL_GetPerformanceCounter(), t1, t2;
    char clock[8];
    int best = best_scale(), picture;
    uint32_t sig;
    if (scale != (best > CANVAS_1_5X ? CANVAS_1_5X : best)) {
        use_scale(best > CANVAS_1_5X ? CANVAS_1_5X : best);
        live_on_screen = 0;
    }
    picture = player_render(p, video, pw, ph, pw);
    SDL_DisableScreenSaver(); /* nobody touches the keys while watching */
    sim_clock_text(clock, sizeof clock);
    sig = view_live_signature(l, player_get(p), s, chs, clock, picture != 0);
    if (live_on_screen && picture != PLAYER_NEW_PICTURE && sig == last_sig)
        return;
    t1 = SDL_GetPerformanceCounter();
    if (picture)
        memcpy(pixels, video, (size_t)pw * (size_t)ph * sizeof pixels[0]);
    view_live(&canvas, l, player_get(p), s, chs, clock, picture != 0);
    t2 = SDL_GetPerformanceCounter();
    if (sim_show_stats) {
        stat_t[0] += t1 - t0;
        stat_t[1] += t2 - t1;
        stats(&canvas, player_get(p));
    }
    present();
    stat_t[2] += SDL_GetPerformanceCounter() - t2;
    last_sig = sig;
    live_on_screen = 1;
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

/* The browser opens on top; the TV window steps aside until the family
 * comes back to it from the taskbar or Dock. */
static int window_open_url(const char *url)
{
    if (SDL_OpenURL(url) != 0)
        return -1;
    SDL_MinimizeWindow(window);
    return 0;
}

const sim_frontend frontend_window = {
    "window", window_folders, window_open, window_close, window_poll, window_shell, window_racer, window_penalty,
    window_garden, window_live, window_wake, window_is_fullscreen, window_open_url, window_startup
};
