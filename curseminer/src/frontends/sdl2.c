#include <stdbool.h>

#include <SDL.h>
#include <SDL2/SDL_image.h>

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#include "curseminer/globals.h"
#include "curseminer/core_game.h"
#include "curseminer/frontend.h"
#include "curseminer/frontends/sdl2.h"
#include "curseminer/scheduler.h"
#include "curseminer/widget.h"
#include "curseminer/util.h"

#define REFRESH_RATE 20

typedef const Uint32 bitmask_sdl;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define g_bmr 0xff000000
#define g_bmg 0x00ff0000
#define g_bmb 0x0000ff00
#define g_bma 0x000000ff
#else
#define g_bmr 0x000000ff
#define g_bmg 0x0000ff00
#define g_bmb 0x00ff0000
#define g_bma 0xff000000
#endif

#define MAGIC_PNG  "\x89\x50\x4e\x47\x00"
#define MAGIC_GIF  "\x47\x49\x46\x38\x00"
#define MAGIC_JPEG "\xff\xd8\xff\xe0\x00"

typedef enum file_format_t {
    FILE_FORMAT_INVALID = -1,
    FILE_FORMAT_PNG,
    FILE_FORMAT_GIF,
    FILE_FORMAT_JPEG,
} file_format_t;

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static int g_tile_w = 20;
static int g_tile_h = 20;
static int g_tile_maxx;
static int g_tile_maxy;
static int g_sprite_size = 32;
static int g_sprite_offset = 0;
static void (*f_draw_tile)(Skin*, SDL_Rect*);

static SDL_Texture *g_canvas;
static Spritesheet *g_spritesheet;

static void assert_SDL(bool condition, const char *msg) {
    assert_log(condition, "%s\nSDL2 Error: '%s'", msg, SDL_GetError());
}

static void recalculate_tile_size(int size) {
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);

    int width = display_mode.w;
    int height = display_mode.h;

    g_tile_w = size;
    g_tile_h = size;

    g_tile_maxx = width / size;
    g_tile_maxy = height / size;
    GLOBALS.view_port_maxx = g_tile_maxx;
    GLOBALS.view_port_maxy = g_tile_maxy;

    GLOBALS.tile_w = g_tile_w;
    GLOBALS.tile_h = g_tile_h;

    if (GLOBALS.game) {
        game_resize_viewport(GLOBALS.game, g_tile_maxx, g_tile_maxy);
        game_flush_dirty(GLOBALS.game);
    }
}

static void intr_redraw_everything(InputEvent *ie) {
    if (ie->state == ES_UP) 
        recalculate_tile_size(g_tile_w);
}

static void intr_zoom_out(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int size = g_tile_w - 1;
        recalculate_tile_size(size);
    }
}

static void intr_zoom_in(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int size = g_tile_w + 1;
        recalculate_tile_size(size);
    }
}

int select_sprite(SDL_Rect *rect, int row, int col) {
    if (!rect) return -1;

    rect->x = col * g_sprite_size + g_sprite_offset;
    rect->y = row * g_sprite_size + g_sprite_offset;
    rect->w = g_sprite_size;
    rect->h = g_sprite_size;

    return 0;
}

void draw_tile_rect(Skin *skin, SDL_Rect *rect) {
    SDL_SetRenderDrawColor(g_renderer, skin->fg_r, skin->fg_g, skin->fg_b, 0xff);

    SDL_RenderFillRect(g_renderer, rect);
}

static int g_sprite_frame = 0;
void draw_tile_sprite(Skin *skin, SDL_Rect *dst) {

    // Fill background
    draw_tile_rect(GLOBALS.game->entity_types->default_skin, dst);

    if (skin->glyph < 1) return;

    // TODO: Would be faster to just pregenerate 1D textures
    int idx = skin->glyph - 1;
    int row = idx % (g_spritesheet->height / g_sprite_size);
    int col = idx / (g_spritesheet->height / g_sprite_size);

    SDL_Rect src;
    select_sprite(&src, row, col);

    SDL_RendererFlip flip = SDL_FLIP_NONE;
    flip = skin->flip_x ? flip | SDL_FLIP_HORIZONTAL : flip;
    flip = skin->flip_y ? flip | SDL_FLIP_VERTICAL   : flip;

    SDL_Texture *frame = g_spritesheet->frames[ g_sprite_frame ];
    SDL_RenderCopyEx(g_renderer, frame, &src, dst, (double) skin->rotation, NULL, flip);
}

void flush_screen() {
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_RenderCopy(g_renderer, g_canvas, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

static void draw_tile(int x, int y, Skin *skin) {
    SDL_Rect r = {
        x * g_tile_w,
        y * g_tile_h,
        g_tile_w,
        g_tile_h
    };

    f_draw_tile(skin, &r);
}

void draw_game() {
    SDL_SetRenderTarget(g_renderer, g_canvas);

    int tiles_drawn = widget_draw_game(GLOBALS.game, draw_tile);

    if (tiles_drawn) flush_screen();
}



/* Input Handling */
#define MAX_MAPPING 256
static event_t g_sdl2_mapping_kb[ MAX_MAPPING ];
static event_t g_sdl2_mapping_ms[ MAX_MAPPING ];

static void init_SDL2_keys() {
    g_sdl2_mapping_kb[SDL_QUIT] = E_KB_ESC;

    for (int i = SDLK_a; i <= SDLK_z; i++)
        g_sdl2_mapping_kb[i] = i - SDLK_a + E_KB_A;

    g_sdl2_mapping_ms[SDL_BUTTON_LEFT] = E_MS_LMB;
    g_sdl2_mapping_ms[SDL_BUTTON_MIDDLE] = E_MS_MMB;
    g_sdl2_mapping_ms[SDL_BUTTON_RIGHT] = E_MS_RMB;
}

static int handle_event_SDL2(void *userdata, SDL_Event *event) {
    bool keyup      = event->type == SDL_KEYUP;
    bool keydown    = event->type == SDL_KEYDOWN;
    bool mouseup    = event->type == SDL_MOUSEBUTTONDOWN;
    bool mousedown  = event->type == SDL_MOUSEBUTTONUP;
    bool mousehover = event->type == SDL_MOUSEMOTION;

    if ((keyup || keydown) && event->key.repeat) return 0;

    static InputEvent ie = {
            .id = E_NULL,
            .type = E_TYPE_KB,
            .state = ES_UP,
            .mods = E_NOMOD,
    };

    SDL_Keycode sym = event->key.keysym.sym;
    event_ctx_t ctx = GLOBALS.input_context;

    if (event->type == SDL_QUIT) exit(0);

    else if (keyup || keydown) {
        ie.type = E_TYPE_KB;
        ie.state = keyup ? ES_UP : ES_DOWN;

        if (SDLK_a <= sym && sym <= SDLK_z) {
            ie.id = g_sdl2_mapping_kb[sym];

            frontend_dispatch_event(ctx, &ie);

        } else if (SDLK_F1 <= sym && sym <= SDLK_F12) {
            ie.id = E_KB_F1 + sym - SDLK_F1;

            frontend_dispatch_event(ctx, &ie);

        } else if (sym == SDLK_RSHIFT || sym == SDLK_LSHIFT) {
            ie.mods = keydown ? E_MOD_0 : E_NOMOD;
        }

    } else if (mouseup || mousedown || mousehover) {
        uint8_t mbtn = event->button.button;

        ie.type = E_TYPE_MS;
        ie.state = mouseup ? ES_UP : ES_DOWN;

        if (mousehover)
            ie.id = E_MS_HOVER;

        else if (SDL_BUTTON_LEFT <= mbtn && mbtn <= SDL_BUTTON_RIGHT)
            ie.id = g_sdl2_mapping_ms[mbtn];

        else return 0;

        int x, y;
        SDL_GetMouseState(&x, &y);

        x /= GLOBALS.tile_w;
        y /= GLOBALS.tile_h;

        frontend_pack_event(&ie, x, y, 0, 0);
        frontend_dispatch_event(ctx, &ie);
    }

    return 0;
}

static bool set_glyphset(const char *name) {
    if (!name) {
        f_draw_tile = draw_tile_rect;
        return true;
    }

    int len = strlen(SPRITES_PATH) + strlen(name) + 1;
    char path[len];

    strcpy(path, SPRITES_PATH);
    strcat(path, name);

    Spritesheet *ss = load_spritesheet_from_file(g_renderer, path);
    
    if (ss == NULL) return false;

    if (g_spritesheet) {
        for (int i = 0; i < g_spritesheet->layers; i++)
            SDL_DestroyTexture(g_spritesheet->frames[i]);

        free(g_spritesheet);
    }

    g_spritesheet = ss;
    g_sprite_frame = 0;
    f_draw_tile = draw_tile_sprite;

    return true;
}

static void draw_point(Skin *skin, int x, int y, int thickness) {
    SDL_SetRenderDrawColor(g_renderer, skin->fg_r, skin->fg_g, skin->fg_b, 0xff);

    SDL_RenderDrawPoint(g_renderer, x, y);
}

static void draw_line(Skin *skin, int x1, int y1, int x2, int y2, int thickness) {
    SDL_SetRenderDrawColor(g_renderer, skin->fg_r, skin->fg_g, skin->fg_b, 0xff);

    SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
}

static void fill_rect(Skin *skin, int x1, int y1, int x2, int y2) {
    SDL_Rect rect = {.x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1};

    draw_tile_rect(skin, &rect);
}



/* Scheduler Jobs */
static int job_animate(Task *task, Stack64 *st) {
    if (!g_spritesheet || g_spritesheet->layers <= 1) return 0;

    g_sprite_frame = (g_sprite_frame + 1) % (g_spritesheet->layers);

    game_flush_dirty(GLOBALS.game);
    tk_sleep(task, g_spritesheet->delay);

    return 0;
}

int job_poll(Task *task, Stack64 *st) {
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {}

    if (ev.type == SDL_QUIT)
        scheduler_kill_all_tasks();

    return 0;
}

static int job_loop(Task *task, Stack64 *st) {
    draw_game();
    tk_sleep(task, 1000 / REFRESH_RATE);
    return 0;
}

static int job_wait_for_game(Task *task, Stack64 *st) {
    if (!GLOBALS.game) tk_sleep(task, 1000);

    else {
        schedule(GLOBALS.runqueue, 0, 0, job_loop, NULL);
        schedule(GLOBALS.runqueue, 0, 0, job_poll, NULL);
        schedule(GLOBALS.runqueue, 0, 0, job_animate, NULL);
        tk_kill(task);
    }

    return 0;
}



/* External APIs */
int frontend_sdl2_ui_init(Frontend *fr, const char *title) {
    // 1. Init SDL2
    int err = SDL_Init(SDL_INIT_VIDEO);
    assert_SDL(0 == err, "Failed to initialize SDL2\t");

    // 2. Init window
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    g_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            display_mode.w, display_mode.h, SDL_WINDOW_SHOWN);

    assert_SDL(g_window, "Failed to create SDL2 window\t");
    
    // 3. Init renderer
    int renderer_flags = SDL_RENDERER_ACCELERATED;

    g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags);

    assert_SDL(g_renderer, "Failed to create SDL2 renderer\t");

    g_canvas = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, display_mode.w, display_mode.h);

    assert_SDL(g_canvas != NULL, "Failed to create SDL main canvas texture");

    // 4. Init draw func, tasks and interrupts
    f_draw_tile = draw_tile_rect;

    recalculate_tile_size(g_tile_w);

    schedule(GLOBALS.runqueue, 0, 0, job_wait_for_game, NULL);

    // 5. Init frontend
    fr->f_set_glyphset = set_glyphset;
    fr->f_draw_point = draw_point;
    fr->f_draw_line = draw_line;
    fr->f_fill_rect = fill_rect;
    fr->width = display_mode.w;
    fr->height = display_mode.h;

    frontend_register_event(E_KB_F5, E_CTX_GAME, intr_redraw_everything);
    frontend_register_event(E_KB_J, E_CTX_GAME, intr_zoom_in);
    frontend_register_event(E_KB_K, E_CTX_GAME, intr_zoom_out);

    return 0;
}

void frontend_sdl2_ui_exit(Frontend *fr) {
    if (g_spritesheet) free(g_spritesheet);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}

int frontend_sdl2_input_init(Frontend *fr) {
    SDL_AddEventWatch(handle_event_SDL2, NULL);
    init_SDL2_keys();

    return 0;
}

void frontend_sdl2_input_exit(Frontend *fr) {}
