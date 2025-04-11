#include <stdbool.h>

#include "SDL.h"

#include "globals.h"
#include "GUI.h"

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Rect g_rect = {.x = 0, .y = 0, .w = 50, .h = 50};

static void assert_SDL(bool condition, const char *msg) {
    assert_log(condition, "%sSDL2 Error: '%s'", msg, SDL_GetError());
}

int GUI_init(const char *title) {
    int err = SDL_Init(SDL_INIT_VIDEO);

    assert_SDL(0 == err, "Failed to initialize SDL2\t");

    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);

    int width = display_mode.w;
    int height = display_mode.h;

    g_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width, height, SDL_WINDOW_SHOWN);

    assert_SDL(g_window, "Failed to create SDL2 window\t");
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);

    SDL_SetRenderDrawColor(g_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    assert_SDL(g_renderer, "Failed to create SDL2 renderer\t");
}

int GUI_loop() {
    SDL_SetRenderDrawColor(g_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 0x33, 0x33, 0x33, 0x33);
    SDL_RenderFillRect(g_renderer, &g_rect);
    SDL_RenderPresent(g_renderer);
}

int GUI_exit() {
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}
