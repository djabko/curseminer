#include "curseminer/frontend.h"
#include "curseminer/frontends/headless.h"

static bool set_glyphset(const char*) {
    return false;
}

int frontend_headless_ui_init(Frontend *fr, const char* title) {
    fr->f_set_glyphset = set_glyphset;

    return 0;
}

void frontend_headless_ui_exit(Frontend*) {}

int frontend_headless_input_init(Frontend*, const char*) {
    return 0;
}

void frontend_headless_input_exit(Frontend*) {}
