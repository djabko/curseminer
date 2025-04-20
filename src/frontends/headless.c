#include "frontend.h"
#include "frontends/headless.h"

int frontend_headless_ui_init(Frontend*, const char* title) {}
void frontend_headless_ui_exit(Frontend*) {}

int frontend_headless_input_init(Frontend*, const char*) {}
void frontend_headless_input_exit(Frontend*) {}
