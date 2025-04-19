#ifndef HEADER_FRONTEND_HEADLESS
#define HEADER_FRONTEND_HEADLESS

int frontend_headless_ui_init(const char*);
int frontend_headless_ui_loop();
void frontend_headless_ui_exit();

int frontend_headless_input_init();
void frontend_headless_input_exit();

#endif
