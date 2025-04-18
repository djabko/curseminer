#include <stdbool.h>

#include "SDL.h"
#include <SDL2/SDL_image.h>

#include "globals.h"
#include "GUI.h"
#include "core_game.h"
#include "games/curseminer.h"
#include "games/other.h"

static RunQueue *g_runqueue;
static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Surface* g_surface;
static int g_tile_w = 20;
static int g_tile_h = 20;
static int g_tile_maxx;
static int g_tile_maxy;
static int g_sprite_size = 32;
static int g_sprite_offset = 0;
void (*draw_tile_f)(EntityType*, SDL_Rect*);

GameContext *g_game = NULL;

typedef struct Texture {
    SDL_Texture *obj;
    const Uint32 format;
    const int access, w, h;
} Texture;

static Texture g_canvas;
static Texture g_spritesheet;

static void assert_SDL(bool condition, const char *msg) {
    assert_log(condition, "%sSDL2 Error: '%s'", msg, SDL_GetError());
}

static int texture_init(Texture *t, SDL_Texture *obj) {
    int ret = SDL_QueryTexture(obj, &t->format, &t->access, &t->w, &t->h);

    assert_SDL(obj != NULL, NULL);

    t->obj = obj;

    return ret;
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

    SDL_RenderCopy(g_renderer, g_spritesheet.obj, &src, dst);
}

static int job_animate(Task* task, Stack64* st) {
    g_sprite_frame = (g_sprite_frame + 1) % (g_spritesheet.w / g_sprite_size);

    game_flush_dirty(g_game);
    tk_sleep(task, 200);
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

    tmp = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, display_mode.w, display_mode.h);

    texture_init(&g_canvas, tmp);

    SDL_SetRenderDrawColor(g_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    assert_SDL(g_renderer, "Failed to create SDL2 renderer\t");

    // 5. Init spritesheet
    if (spritesheet_path) {
        g_surface = IMG_Load(spritesheet_path);

        assert_SDL(g_surface != NULL, NULL);

        tmp = SDL_CreateTextureFromSurface(g_renderer, g_surface);

        texture_init(&g_spritesheet, tmp);

        SDL_FreeSurface(g_surface);

        draw_tile_f = draw_tile_sprite;

    } else draw_tile_f = draw_tile_rect;

    // 6. Register interrupts
    input_register_event(E_KB_J, E_CTX_GAME, intr_zoom_in);
    input_register_event(E_KB_K, E_CTX_GAME, intr_zoom_out);

    if (1 < g_spritesheet.w / g_sprite_size) {
        g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
        schedule(g_runqueue, 0, 0, job_animate, NULL);
    }
}

void flush_screen() {
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_RenderCopy(g_renderer, g_canvas.obj, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void draw_game() {
    SDL_SetRenderTarget(g_renderer, g_canvas.obj);

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
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}
