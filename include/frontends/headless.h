#ifndef HEADER_FRONTEND_HEADLESS
#define HEADER_FRONTEND_HEADLESS

int frontend_headless_ui_init(Frontend*, const char*);
int frontend_headless_ui_loop(Frontend*);
void frontend_headless_ui_exit(Frontend*);

int frontend_headless_input_init(Frontend*, const char*);
void frontend_headless_input_exit(Frontend*);

#endif
