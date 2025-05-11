#ifndef H_CM_WIDGET
#define H_CM_WIDGET

#include "curseminer/util.h"
#include "curseminer/core_game.h"
#include "curseminer/world.h"

typedef struct Widget {
    const char *title;
    int x, y, w, h, t;

    Queue64 *draw_qu;
    Skin *skin;
    event_ctx_t input_context;
    bool active, visible;
} Widget;

/* Widgets */
int widget_draw_game(GameContext*, void (*f_draw_tile)(int x, int y, Skin*));
void widget_draw_inventory(Entity*);
void widget_draw_12hclock(GameContext*);
void widget_draw_noise(double (noise_func)(NoiseLattice*, double));

#endif
