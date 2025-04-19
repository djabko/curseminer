#ifndef FRONTEND_NCURSES_HEADER
#define FRONTEND_NCURSES_HEADER

int frontend_ncurses_ui_init(const char*);
int frontend_ncurses_ui_loop();
void frontend_ncurses_ui_exit();

int frontend_ncurses_input_init();
void frontend_ncurses_input_exit();

#endif
