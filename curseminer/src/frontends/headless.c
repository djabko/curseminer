#include "curseminer/frontend.h"
#include "curseminer/frontends/headless.h"

static bool set_glyphset(const char*) {
    return false;
}

static void draw_point (Skin*, int x, int y, int w) {

}

static void draw_line (Skin*, int x1, int y1, int x2, int y2, int w) {

}

static void draw_fill (Skin*, int x1, int y1, int x2, int y2) {

}

int frontend_headless_ui_init(Frontend *fr, const char* title) {
    fr->f_set_glyphset = set_glyphset;
    fr->f_draw_point = draw_point;
    fr->f_draw_line = draw_line;
    fr->f_fill_rect = draw_fill;
    fr->width = 0;
    fr->height = 0;

    return 0;
}

void frontend_headless_ui_exit(Frontend*) {}

int frontend_headless_input_init(Frontend*) {
    return 0;
}

void frontend_headless_input_exit(Frontend*) {}
