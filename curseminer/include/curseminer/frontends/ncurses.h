#ifndef FRONTEND_NCURSES_HEADER
#define FRONTEND_NCURSES_HEADER

int frontend_ncurses_ui_init(Frontend*, const char*);
int frontend_ncurses_ui_loop(Frontend*);
void frontend_ncurses_ui_exit(Frontend*);

int frontend_ncurses_input_init(Frontend*);
void frontend_ncurses_input_exit(Frontend*);

#endif
