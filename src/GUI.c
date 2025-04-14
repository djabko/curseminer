#include <stdbool.h>

#include "SDL.h"

#include "globals.h"
#include "GUI.h"
#include "core_game.h"
#include "games/curseminer.h"
#include "games/other.h"

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture* g_canvas;
static int g_tile_w = 20;
static int g_tile_h = 20;
static int g_tile_maxx;
static int g_tile_maxy;

GameContext *g_game = NULL;

static void assert_SDL(bool condition, const char *msg) {
    assert_log(condition, "%sSDL2 Error: '%s'", msg, SDL_GetError());
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

    if (g_game) {
        flush_game_entity_cache(g_game);
        flush_world_entity_cache(g_game);
        game_flush_dirty(g_game);
    }
}

static void intr_zoom_out(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int size = g_tile_w - 1;
        if (size >= 20)
            recalculate_tile_size(size);
    }
}

static void intr_zoom_in(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int size = g_tile_w + 1;
        recalculate_tile_size(size);
    }
}

int GUI_init(const char *title) {
    int err = SDL_Init(SDL_INIT_VIDEO);

    assert_SDL(0 == err, "Failed to initialize SDL2\t");

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

    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    g_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            display_mode.w, display_mode.h, SDL_WINDOW_SHOWN);

    assert_SDL(g_window, "Failed to create SDL2 window\t");
    
    int renderer_flags = SDL_RENDERER_ACCELERATED;

    g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags);

    g_canvas = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, display_mode.w, display_mode.h);

    SDL_SetRenderDrawColor(g_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    assert_SDL(g_renderer, "Failed to create SDL2 renderer\t");

    input_register_event(E_KB_J, E_CTX_GAME, intr_zoom_in);
    input_register_event(E_KB_K, E_CTX_GAME, intr_zoom_out);
}

void draw_tile(EntityType *type, SDL_Rect *rect) {
    Skin *skin = type->default_skin;

    SDL_SetRenderDrawColor(g_renderer, skin->fg_r, skin->fg_g, skin->fg_b, 0xff);

    SDL_RenderFillRect(g_renderer, rect);
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

                        draw_tile(type, &rect);
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
                draw_tile(type, &rect);
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
