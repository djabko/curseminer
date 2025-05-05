#include "curseminer/globals.h"
#include "curseminer/scheduler.h"
#include "curseminer/frontend.h"

Frontend g_frontend;

static void (*g_mapper_ctx_array[E_CTX_END][E_END])(InputEvent*);

static void handler_stub(InputEvent *ie) {}

bool frontend_pack_event(
        InputEvent *ie, uint8_t x, uint8_t y, uint8_t z, uint8_t r) {

    if (!ie) return -1;

    static int bits = 16;

    ie->data = (x << (bits * 0)) + (y << (bits * 1))
             + (z << (bits * 2)) + (r << (bits * 3));

    return 0;
}

int frontend_register_event(
        event_t id, event_ctx_t ctx, void (*func)(InputEvent*)) {

    if (id < 0 || E_END <= id) return -1; 

    g_mapper_ctx_array[ctx][id] = func;

    return 0;
}

int frontend_register_ui(frontend_init_t f_init, frontend_exit_t f_exit) {
    if (!f_init || !f_exit) return false;

    g_frontend.f_ui_init = f_init;
    g_frontend.f_ui_exit = f_exit;

    return true;
}

bool frontend_set_glyphset(const char *name) {
    return g_frontend.f_set_glyphset(name);
}

int frontend_register_input(frontend_init_t f_init, frontend_exit_t f_exit) {
    if (!f_init || !f_exit) return false;

    g_frontend.f_input_init = f_init;
    g_frontend.f_input_exit = f_exit;

    return true;
}

void frontend_dispatch_event(event_ctx_t ctx, InputEvent *ie) {
    g_mapper_ctx_array[ctx][ie->id](ie);
}

int frontend_init(const char *title) {
    for (int i = E_CTX_0; i < E_CTX_END; i++) {
        for (int j = E_NULL; j < E_END; j++) {
            g_mapper_ctx_array[i][j] = handler_stub;
        }
    }

    g_frontend.f_ui_init(&g_frontend, title);
    g_frontend.f_input_init(&g_frontend);

    return true;
}

void frontend_exit() {
    g_frontend.f_ui_exit(&g_frontend);
    g_frontend.f_input_exit(&g_frontend);
}
