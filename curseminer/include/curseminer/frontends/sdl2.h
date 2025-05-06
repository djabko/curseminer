#ifndef FRONTEND_SDL2_HEADER
#define FRONTEND_SDL2_HEADER

#define SPRITES_PATH "../assets/spritesheets/"

int frontend_sdl2_ui_init(Frontend*, const char*);
int frontend_sdl2_ui_loop(Frontend*);
void frontend_sdl2_ui_exit(Frontend*);

int frontend_sdl2_input_init(Frontend*, const char*);
void frontend_sdl2_input_exit(Frontend*);

#endif
