#ifndef FRONTEND_SDL2_HEADER
#define FRONTEND_SDL2_HEADER

int frontend_sdl2_ui_init(const char*);
int frontend_sdl2_ui_loop();
void frontend_sdl2_ui_exit();

int frontend_sdl2_input_init();
void frontend_sdl2_input_exit();

#endif
