#include <stdbool.h>

#include <SDL.h>
#include <SDL2/SDL_image.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "globals.h"
#include "core_game.h"
#include "frontend.h"
#include "frontends/sdl2.h"
#include "scheduler.h"

#define SPRITE_MAX 256
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

#define NAME_MAX 64

#define g_spritesheet_path "assets/spritesheets/tiles.gif"

typedef enum file_format_t {
    FILE_FORMAT_INVALID = -1,
    FILE_FORMAT_PNG,
    FILE_FORMAT_GIF,
    FILE_FORMAT_JPEG,
} file_format_t;

static RunQueue *g_runqueue;
static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static int g_tile_w = 20;
static int g_tile_h = 20;
static int g_tile_maxx;
static int g_tile_maxy;
static int g_sprite_size = 32;
static int g_sprite_offset = 0;
static void (*draw_tile_f)(Skin*, SDL_Rect*);

typedef struct Spritesheet {
    const char name[NAME_MAX];
    SDL_Texture **frames;
    int width, height, layers, stride;
    milliseconds_t delay;
} Spritesheet;

// Can be used for jpeg too
typedef struct PNG {
    const char *name;
    void *data;
    int width, height, stride;
} PNG;

typedef struct GIF {
    const char *name;
    void *data;
    int width, height, layers, stride;
    int *delays;
} GIF;

GameContext *g_game = NULL;
GIF *g_gif = NULL;
static SDL_Texture *g_canvas;
static Spritesheet *g_spritesheet;

static void assert_SDL(bool condition, const char *msg) {
    assert_log(condition, "%s\nSDL2 Error: '%s'", msg, SDL_GetError());
}

static file_format_t check_file_format(const char *path) {
    bool exists = 0 == access(path, R_OK);

    if (exists) {
        FILE *f = fopen(path, "rb");

        static byte_t buf[] = {0, 0, 0, 0, 0};

        fread(buf, 1, 4, f);

        if      (0 == memcmp(buf, MAGIC_PNG,  4)) return FILE_FORMAT_PNG;
        else if (0 == memcmp(buf, MAGIC_GIF,  4)) return FILE_FORMAT_GIF;
        else if (0 == memcmp(buf, MAGIC_JPEG, 4)) return FILE_FORMAT_JPEG;
    }
    
    return FILE_FORMAT_INVALID;
}

static void load_gif(const char *filename, GIF *gif) {
    size_t size;
    FILE *f = fopen(filename, "rb");

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    assert_log(0 < size, "Error: File '%s' is empty", filename);

    void *buf = malloc(size);

    size_t r = fread(buf, 1, size, f);

    assert_log(r == size,
            "Error: Failed to read file '%s', %lu bytes returned", filename, r);

    gif->name = filename;
    gif->data = stbi_load_gif_from_memory(buf, size, &gif->delays,
            &gif->width, &gif->height, &gif->layers, &gif->stride, 0);

    assert_log(gif->data, "Error: Failed to load '%s' as GIF", filename);

    free(buf);
}

static Spritesheet *spritesheet_alloc(const char *name, int width, int height, int layers,
        int stride, milliseconds_t delay) {

    const size_t alloc_size =
        sizeof(Spritesheet) + layers * sizeof(SDL_Texture*);

    Spritesheet *sp = calloc(alloc_size, 1);

    assert_log(sp != NULL,
            "Error: failed to allocate %lu for spritesheet '%s'",
            alloc_size, name);

    strncpy(sp->name, name, NAME_MAX);
    sp->frames = (SDL_Texture**) (sp + 1);
    sp->width = width;
    sp->height = height;
    sp->layers = layers;
    sp->stride = stride;
    sp->delay = delay;

    return sp;
}

static SDL_Texture *spritesheet_init_frame(Spritesheet *sp, void *img_data,
        SDL_Renderer* renderer, int stride, int pitch) {

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
            img_data, sp->width, sp->height,
            stride * 8, pitch, g_bmr, g_bmg, g_bmb, g_bma);

    SDL_Texture *frame = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    assert_SDL(frame != NULL, "Failed to initialize SDL_Texture");

    return frame;
}

static Spritesheet *spritesheet_from_png(PNG *png, SDL_Renderer *renderer) {
    const int pitch = png->width * png->stride;
    const int layers = 1;

    Spritesheet *sp = spritesheet_alloc(
            png->name, png->width, png->height, layers, png->stride, 0);

    sp->frames[0] = spritesheet_init_frame(
            sp, png->data, renderer, png->stride, pitch);

    return sp;
}

static Spritesheet *spritesheet_from_gif(GIF *gif, SDL_Renderer *renderer) {
    const int pitch = gif->width * gif->stride;

    Spritesheet *sp = spritesheet_alloc(
            gif->name, gif->width, gif->height, gif->layers, gif->stride,
            gif->delays[0]);

    void *data = gif->data;

    for (int i = 0; i < gif->layers; i++) {
        sp->frames[i] = spritesheet_init_frame(
                sp, data, renderer, gif->stride, pitch);

        data += gif->width * gif->height * gif->stride;
    }

    return sp;
}

// Make sure to free Spritesheet after use
static Spritesheet *load_spritesheet_from_file(SDL_Renderer *renderer, const char *path) {
    Spritesheet *ss = NULL;
    file_format_t ff = check_file_format(path);

    if (ff == FILE_FORMAT_PNG || ff == FILE_FORMAT_JPEG) {
        PNG png;
        png.name = path;
        png.data = stbi_load(path, &png.width, &png.height, &png.stride, 0);
        ss = spritesheet_from_png(&png, renderer);

    } else if (ff == FILE_FORMAT_GIF) {
        GIF gif;
        load_gif(path, &gif);
        ss = spritesheet_from_gif(&gif, renderer);
    }

    return ss;
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

    if (g_game) {
        game_resize_viewport(g_game, g_tile_maxx, g_tile_maxy);
        game_flush_dirty(g_game);
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
    if (skin->glyph < 1) return draw_tile_rect(skin, dst);

    // Fill background
    draw_tile_rect(g_game->entity_types->default_skin, dst);

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

void draw_game() {
    SDL_SetRenderTarget(g_renderer, g_canvas);

    SDL_Rect rect = {.x = 0, .y = 0, .w = g_tile_w, .h = g_tile_h};
    Skin* skin;
    DirtyFlags *df = GLOBALS.game->cache_dirty_flags;

    // No tiles to draw
    if (df->command == 0) {
        return;

    // Draw only dirty tiles
    } else if (df->command == 1) {

        size_t s = df->stride;
        int maxx = GLOBALS.view_port_maxx;

        for (int i = 0; i < s; i++) {
            if (df->groups[i]) {

                for (int j = 0; j < s; j++) {

                    int index = i * s + j;
                    byte_t flag = df->flags[index];

                    if (flag == 1) {

                        int x = index % maxx;
                        int y = index / maxx;

                        skin = game_world_getxy(GLOBALS.game, x, y);
                        game_set_dirty(GLOBALS.game, x, y, 0);

                        rect.x = x * g_tile_w;
                        rect.y = y * g_tile_h;

                        draw_tile_f(skin, &rect);
                    }
                }
            }
        }


    // Update all tiles
    } else if (df->command == -1) {
        SDL_RenderClear(g_renderer);

        for (int y = 0; y < g_tile_maxy; y++) {
            rect.y = y * g_tile_h;

            for (int x = 0; x < g_tile_maxx; x++) {
                rect.x = x * g_tile_w;

                skin = game_world_getxy(GLOBALS.game, x, y);

                game_set_dirty(GLOBALS.game, x, y, 0);
                draw_tile_f(skin, &rect);
            }
        }
    }

    flush_screen();
    df->command = 0;
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
    bool keyup = event->type == SDL_KEYUP;
    bool keydown = event->type == SDL_KEYDOWN;
    bool mouseup = event->type == SDL_MOUSEBUTTONDOWN;
    bool mousedown = event->type == SDL_MOUSEBUTTONUP;

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

    } else if (mouseup || mousedown) {
        uint8_t mbtn = event->button.button;

        ie.type = E_TYPE_MS;
        ie.state = mouseup ? ES_UP : ES_DOWN;

        if (SDL_BUTTON_LEFT <= mbtn && mbtn <= SDL_BUTTON_RIGHT) {
            int x, y;

            SDL_GetMouseState(&x, &y);

            x /= GLOBALS.tile_w;
            y /= GLOBALS.tile_h;

            frontend_pack_event(&ie, x, y, 0, 0);

            ie.id = g_sdl2_mapping_ms[mbtn];

            frontend_dispatch_event(ctx, &ie);
        }
    }

    return 0;
}

static bool set_glyphset(const char *name) {
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
    draw_tile_f = draw_tile_sprite;

    return true;
}



/* Scheduler Jobs */
static int job_animate(Task *task, Stack64 *st) {
    if (!g_spritesheet || g_spritesheet->layers <= 1) return 0;

    g_sprite_frame = (g_sprite_frame + 1) % (g_spritesheet->layers);

    game_flush_dirty(g_game);
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
        g_game = GLOBALS.game;
        schedule(GLOBALS.runqueue, 0, 0, job_loop, NULL);
        schedule(GLOBALS.runqueue, 0, 0, job_poll, NULL);
        schedule(GLOBALS.runqueue, 0, 0, job_animate, NULL);
        tk_kill(task);
    }

    return 0;
}



/* External APIs */
int frontend_sdl2_ui_init(Frontend *fr, const char *title) {
    fr->f_set_glyphset = set_glyphset;

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
    draw_tile_f = draw_tile_rect;

    recalculate_tile_size(g_tile_w);

    schedule(GLOBALS.runqueue, 0, 0, job_wait_for_game, NULL);

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

int frontend_sdl2_input_init(Frontend *fr, const char*) {
    SDL_AddEventWatch(handle_event_SDL2, NULL);
    init_SDL2_keys();

    return 0;
}

void frontend_sdl2_input_exit(Frontend *fr) {}
