#include <stdio.h>

#include "games/other.h"
#include "core_game.h"
#include "globals.h"
#include "util.h"
#include "entity.h"

#define g_skins_max 10
#define g_entity_type_max g_skins_max
static GameContext *g_game;
static Skin g_skins[g_skins_max];
static EntityController g_controller_0,
                        g_controller_1,
                        g_controller_2;

static void handler(InputEvent *ie) {
    if (ie->state == ES_UP) return;

    int x, y;
    world_from_mouse_xy(ie, &x, &y);

    int n = rand() % g_game->entity_types_c;
    EntityType *type = g_game->entity_types + n;
    Entity *e = entity_spawn(g_game, g_game->world, type, x, y, ENTITY_FACING_DOWN, 1, 0);
    e->moving = true;

    e->controller = &g_controller_1;
}

static void game_input_set_glyphset_01(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_00.png");
}

static void game_input_set_glyphset_02(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_01.png");
}

static void game_input_set_glyphset_03(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_02.gif");
}

static void tick_0(Entity* e) {}

static void tick_1(Entity* e) {
    /*
    if (world_getxy(g_game->world, e->x, e->y+1) == e->type->id) {
        entity_rm(g_game->world, e);
    }
    */

    e->skin.rotation += 20;

    entity_update_position(g_game, e);
}

static void tick_2(Entity* e) {}

int game_other_init(GameContext *game, int) {
    g_game = game;

    int glyph = 0;
    int i = 0;

    game_set_glyphset(game, "tiles_02.gif");

    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 33, 33, 33);
    game_create_skin(game, g_skins + i++, glyph++, 0, 32, 64, 0, 192, 255);
    game_create_skin(game, g_skins + i++, glyph++, 16, 32, 0, 128, 255, 0);
    game_create_skin(game, g_skins + i++, glyph++, 24, 0, 0, 255, 32, 0);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 24, 160, 160, 255);
    game_create_skin(game, g_skins + i++, glyph++, 8, 8, 8, 255, 208, 128);
    game_create_skin(game, g_skins + i++, glyph++, 240, 240, 255, 64, 0, 128);

    for (int j = 0; j < i; j++)
        game_create_entity_type(game, g_skins + j);

    game->entity_types_c = i;

    entity_create_controller(&g_controller_0, tick_0, NULL);
    entity_create_controller(&g_controller_1, tick_1, NULL);
    entity_create_controller(&g_controller_2, tick_2, NULL);

    Entity *player = entity_spawn(game, game->world, game->entity_types,
            20, 20, ENTITY_FACING_RIGHT, 1, 0);

    player->controller = &g_controller_0;

    frontend_register_event(E_MS_LMB, E_CTX_GAME, handler);
    frontend_register_event(E_KB_F1, E_CTX_GAME, game_input_set_glyphset_01);
    frontend_register_event(E_KB_F2, E_CTX_GAME, game_input_set_glyphset_02);
    frontend_register_event(E_KB_F3, E_CTX_GAME, game_input_set_glyphset_03);

    GLOBALS.player = player;

    return 0;
}

int game_other_update() {
    return 0;
}

int game_other_free() {
    return 0;
}
