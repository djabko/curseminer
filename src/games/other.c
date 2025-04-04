#include <stdio.h>

#include "games/other.h"
#include "globals.h"
#include "util.h"
#include "entity.h"

#define g_skins_max 10
#define g_entity_type_max g_skins_max
static Skin g_skins[g_skins_max];
static EntityController g_player_controller;

static void handler(InputEvent *ie) {}

static void player_tick(Entity* e) {}

int game_other_init(GameContext *game, int) {
    int glyph = 0;
    int i = 0;

    game_create_skin(g_skins + i++, glyph++, 0, 0, 0, 255, 255, 255);
    game_create_skin(g_skins + i++, glyph++, 0, 32, 64, 0, 192, 255);
    game_create_skin(g_skins + i++, glyph++, 16, 32, 0, 128, 255, 0);
    game_create_skin(g_skins + i++, glyph++, 24, 0, 0, 255, 32, 0);
    game_create_skin(g_skins + i++, glyph++, 0, 0, 24, 160, 160, 255);
    game_create_skin(g_skins + i++, glyph++, 8, 8, 8, 255, 208, 128);
    game_create_skin(g_skins + i++, glyph++, 240, 240, 255, 64, 0, 128);

    for (int j = 0; j < i; j++)
        game_create_entity_type(g_skins + j);

    GLOBALS.game->entity_types_c = i;

    entity_create_controller(&g_player_controller, player_tick, NULL);

    Entity *player = entity_spawn(game->world, GLOBALS.game->entity_types,
            20, 20, ENTITY_FACING_RIGHT, 1, 0);

    player->controller = &g_player_controller;

    input_register_event(E_MS_LMB, E_CTX_GAME, handler);

    GLOBALS.player = player;

    return 1;
}

int game_other_update() {
}

int game_other_free() {
}

#undef g_skins_max
#undef g_entity_type_max
