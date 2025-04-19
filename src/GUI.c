#include <stdbool.h>

#include <SDL.h>
#include <SDL2/SDL_image.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "globals.h"
#include "GUI.h"
#include "core_game.h"
#include "games/curseminer.h"
#include "games/other.h"

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
void (*draw_tile_f)(EntityType*, SDL_Rect*);

typedef struct Spritesheet {
    const char *name;
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
    if (path) {
        FILE *f = fopen(path, "rb");

        static byte_t buf[] = {0, 0, 0, 0, 0};

        fread(buf, 1, 4, f);

        if      (0 == strcmp(buf, MAGIC_PNG )) return FILE_FORMAT_PNG;
        else if (0 == strcmp(buf, MAGIC_GIF )) return FILE_FORMAT_GIF;
        else if (0 == strcmp(buf, MAGIC_JPEG)) return FILE_FORMAT_JPEG;
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
            "Error: Failed to read file '%s', %d bytes returned", filename, r);

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

    Spritesheet *sp = calloc(alloc_size, layers);
    sp->name = name;
    sp->frames = (SDL_Texture**) (sp + 1);
    sp->width = width;
    sp->height = height;
    sp->layers = layers;
    sp->stride = stride;
    sp->delay = delay;

    assert_log(sp != NULL,
            "Error: failed to allocate %lu for spritesheet '%s'",
            alloc_size * layers, name);

    return sp;
}

static SDL_Texture *spritesheet_init_frame(Spritesheet *sp, void *img_data,
        SDL_Renderer* renderer, int stride, int pitch) {

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
            img_data, sp->width, sp->height,
            stride * 8, pitch, g_bmr, g_bmg, g_bmb, g_bma);

    SDL_Texture *frame = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    assert_log(frame != NULL,
            "Failed to initialize SDL_Texture '%s'", sp->name);

    return frame;
}

static Spritesheet *spritesheet_from_png(PNG *png) {
    const int pitch = png->width * png->stride;
    const int layers = 1;

    Spritesheet *sp = spritesheet_alloc(
            png->name, png->width, png->height, layers, png->stride, 0);

    sp->frames[0] = spritesheet_init_frame(
            sp, png->data, g_renderer, png->stride, pitch);

    return sp;
}

static Spritesheet *spritesheet_from_gif(GIF *gif) {
    const int pitch = gif->width * gif->stride;

    Spritesheet *sp = spritesheet_alloc(
            gif->name, gif->width, gif->height, gif->layers, gif->stride,
            gif->delays[0]);

    void *data = gif->data;

    for (int i = 0; i < gif->layers; i++) {
        sp->frames[i] = spritesheet_init_frame(
                sp, data, g_renderer, gif->stride, pitch);

        data += gif->width * gif->height * gif->stride;
    }

    return sp;
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

    // TODO: rename group_segments to groups
    if (g_game) {
        size_t tiles = g_tile_maxx * g_tile_maxy;
        DirtyFlags *df = g_game->cache_dirty_flags;

        if (df->groups_available < tiles)
            game_resize_caches(g_game);

        flush_game_entity_cache(g_game);
        flush_world_entity_cache(g_game);
        game_flush_dirty(g_game);
    }
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

void draw_tile_rect(EntityType *type, SDL_Rect *rect) {
    Skin *skin = type->default_skin;

    SDL_SetRenderDrawColor(g_renderer, skin->fg_r, skin->fg_g, skin->fg_b, 0xff);

    SDL_RenderFillRect(g_renderer, rect);
}

static int g_sprite_frame = 0;
void draw_tile_sprite(EntityType *type, SDL_Rect *dst) {
    Skin *skin = type->default_skin;

    if (skin->glyph < 1) return draw_tile_rect(type, dst);

    SDL_Rect src;
    select_sprite(&src, skin->glyph - 1, g_sprite_frame);

    SDL_Texture *frame = g_spritesheet->frames[ g_sprite_frame ];
    SDL_RenderCopy(g_renderer, frame, &src, dst);
}

static int job_animate(Task* task, Stack64* st) {
    g_sprite_frame = (g_sprite_frame + 1) % (g_spritesheet->layers);

    game_flush_dirty(g_game);
    tk_sleep(task, g_spritesheet->delay);
}

int GUI_init(const char *title, const char *spritesheet_path) {
    SDL_Texture *tmp;

    // 1. Init SDL2
    int err = SDL_Init(SDL_INIT_VIDEO);

    assert_SDL(0 == err, "Failed to initialize SDL2\t");

    // 2. Init game
    recalculate_tile_size(g_tile_w);

    GameContextCFG gcfg_curseminer = {
        .skins_max = 32,
        .entity_types_max = 32,
        .scroll_threshold = 5,

        .f_init = game_curseminer_init,
        .f_update = game_curseminer_update,
        .f_free = game_curseminer_free,
    };

    GameContextCFG gcfg_other = {
        .skins_max = 32,
        .entity_types_max = 32,
        .scroll_threshold = 5,

        .f_init = game_other_init,
        .f_update = game_other_update,
        .f_free = game_other_free,
    };

    g_game = game_init(&gcfg_curseminer);

    GLOBALS.game = g_game;

    assert_log(GLOBALS.game != NULL,
            "ERROR: GUI failed to initialize game...");

    recalculate_tile_size(g_tile_w);

    // 3. Init window
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    g_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            display_mode.w, display_mode.h, SDL_WINDOW_SHOWN);

    assert_SDL(g_window, "Failed to create SDL2 window\t");
    
    // 4. Init renderer
    int renderer_flags = SDL_RENDERER_ACCELERATED;

    g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags);

    assert_SDL(g_renderer, "Failed to create SDL2 renderer\t");

    g_canvas = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, display_mode.w, display_mode.h);

    assert_SDL(g_canvas != NULL, "Failed to create SDL main canvas texture");

    SDL_SetRenderDrawColor(g_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    // 5. Init spritesheet
    g_spritesheet = NULL;
    file_format_t ff = check_file_format(spritesheet_path);

    if (ff == FILE_FORMAT_PNG || ff == FILE_FORMAT_JPEG) {
        PNG png;
        png.name = spritesheet_path;
        png.data = stbi_load(spritesheet_path, &png.width, &png.height, &png.stride, 0);
        g_spritesheet = spritesheet_from_png(&png);

    } else if (ff == FILE_FORMAT_GIF) {
        GIF gif;
        load_gif(spritesheet_path, &gif);
        g_spritesheet = spritesheet_from_gif(&gif);
    }

    if (g_spritesheet) {
        draw_tile_f = draw_tile_sprite;

        log_debug("Found %dx%d spritesheet '%s' with %d layers!",
                g_spritesheet->width, g_spritesheet->height,
                g_spritesheet->name, g_spritesheet->layers);

        // Create runqueue for idle sprite animations
        if (1 < g_spritesheet->layers) {
            g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
            schedule(g_runqueue, 0, 0, job_animate, NULL);
        }

    } else draw_tile_f = draw_tile_rect;

    // 6. Register interrupts
    input_register_event(E_KB_J, E_CTX_GAME, intr_zoom_in);
    input_register_event(E_KB_K, E_CTX_GAME, intr_zoom_out);
}

void flush_screen() {
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_RenderCopy(g_renderer, g_canvas, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void draw_game() {
    SDL_SetRenderTarget(g_renderer, g_canvas);

    SDL_Rect rect = {.x = 0, .y = 0, .w = g_tile_w, .h = g_tile_h};
    EntityType* type;
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

                        type = game_world_getxy(GLOBALS.game, x, y);
                        game_set_dirty(GLOBALS.game, x, y, 0);

                        rect.x = x * g_tile_w;
                        rect.y = y * g_tile_h;

                        draw_tile_f(type, &rect);
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

                type = game_world_getxy(GLOBALS.game, x, y);

                game_set_dirty(GLOBALS.game, x, y, 0);
                draw_tile_f(type, &rect);
            }
        }
    }

    flush_screen();
    df->command = 0;
}

int GUI_loop() {
    draw_game();
}

int GUI_exit() {
    if (g_spritesheet) free(g_spritesheet);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}
