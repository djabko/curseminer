#include "globals.h"
#include "games/curseminer.h"

int game_curseminer_init(int) {
    game_create_skin(SKIN_NULL,                  ' ', 0, 0, 0, 255, 255, 255);
    game_create_skin(SKIN_DEFAULT,               '*', 0, 0, 0, 120, 120, 120);
    game_create_skin(SKIN_GOLD,                  'o', 0, 0, 0, 255, 215,   0);
    game_create_skin(SKIN_DIAMOND,               '&', 0, 0, 0,  80, 240, 220);
    game_create_skin(SKIN_IRON,                  'f', 0, 0, 0, 120, 120, 120);
    game_create_skin(SKIN_REDORE,                '.', 0, 0, 0, 120,   6,   2);
    game_create_skin(SKIN_PLAYER,                'D', 0, 0, 0, 255, 215,   0);
    game_create_skin(SKIN_CHASER,                'M', 0, 0, 0, 215, 215,  50);
    game_create_skin(SKIN_INVENTORY_SELECTED,    ' ', 0, 0, 0,  42, 133,  57);

    int c = GLOBALS.game->skins_c;

    for (int i=0; i<c; i++)
        game_create_entity_type( GLOBALS.game->skins + i );

    GLOBALS.game->entity_types_c = c;

    return 1;
}

int game_curseminer_update() {

}

int game_curseminer_free() {

}
