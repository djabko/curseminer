#include "stdlib.h"
#include <stdio.h>

#include <globals.h>
#include <game.h>
#include <world.h>
#include <entity.h>
#include <stack64.h>
#include <scheduler.h>


GameContext* GAME;
RunQueue* GAME_RUNQUEUE;

void create_skin(int id, char c, color_t bg_r, color_t bg_g, color_t bg_b, color_t fg_r, color_t fg_g, color_t fg_b) {
    if (GAME->skins_c == GAME->skins_maxc) {
        GAME->skins_maxc *= 2;
        GAME->skins = realloc(GAME->skins, GAME->skins_maxc * sizeof(Skin));
    }
    
    Skin* skin = GAME->skins + GAME->skins_c++;

    skin->id = id;
    skin->character = c;

    skin->bg_r = bg_r;
    skin->bg_g = bg_g;
    skin->bg_b = bg_b;
    skin->fg_r = fg_r;
    skin->fg_g = fg_g;
    skin->fg_b = fg_b;
}

void create_entity_type(Skin* skin) {
    if (GAME->entity_types_c == GAME->entity_types_maxc) {
        GAME->entity_types_maxc *= 2;
        GAME->entity_types = realloc(GAME->entity_types, GAME->entity_types_maxc * sizeof(EntityType));
    }

    EntityType* entitiyt = GAME->entity_types + GAME->entity_types_c;
    entitiyt->skin = skin;
    entitiyt->id = GAME->entity_types_c++;
}

int init_skins() {
    create_skin(sk_null,        ' ', 0, 0, 0, 255, 255, 255);
    create_skin(sk_default,     '*', 0, 0, 0, 120, 120, 120);
    create_skin(sk_gold,        'o', 0, 0, 0, 255, 215,   0);
    create_skin(sk_diamond,     '&', 0, 0, 0,  80, 240, 220);
    create_skin(sk_iron,        'f', 0, 0, 0, 120, 120, 120);
    create_skin(sk_redore,      '.', 0, 0, 0, 120,   6,   2);
    create_skin(sk_player,      'D', 0, 0, 0, 255, 215,   0);
    create_skin(sk_chaser_mob,  'M', 0, 0, 0, 215, 215,   50);
    return 1;
}

void init_entity_types() {
    int c = GAME->skins_c;
    for (int i=0; i<c; i++)
        create_entity_type( GAME->skins + i );
    GAME->entity_types_c = c;
}

int update_game_world(Task* task, Stack64* stack) {
    Queue64* entity_qu = GAME->world->entities;

    Entity* end = (Entity*) qu_peek(entity_qu);;
    Entity* e = (Entity*) qu_next(entity_qu);

    int i = entity_qu->count;

    do {
        e->controller->tick(e);
        e = (Entity*) qu_next(entity_qu);
        i--;

    } while (e != end && 0 < i);

    tk_sleep(task, 100);
    return 0;
}

int game_init() {
    int status = 0;

    GAME = calloc(sizeof(GameContext), 1);
    GAME->skins_c = 0;
    GAME->entity_types_c = 0;
    GAME->skins_maxc = 32;
    GAME->entity_types_maxc = 32;
    GAME->skins = calloc(GAME->skins_maxc, sizeof(Skin));
    GAME->entity_types = calloc(GAME->entity_types_maxc, sizeof(EntityType));
    GAME->world_view_x = 0;
    GAME->world_view_y = 0;

    init_skins();
    init_entity_types();
    GAME->world = world_init(256, 256, GAME->skins_c - 1);
    
    int e_x = rand() % (GAME->world_view_x + 20);
    int e_y = rand() % (GAME->world_view_y + 20);
    Entity* player = entity_spawn(GAME->world, GAME->entity_types + ge_player, 20, 20, ENTITY_FACING_RIGHT, 1, 0);
    entity_set_keyboard_controller(player);
    GLOBALS.player = player;

    e_x = rand() % (GAME->world_view_x + 20);
    e_y = rand() % (GAME->world_view_y + 20);
    Entity* entity = entity_spawn(GAME->world, GAME->entity_types + ge_chaser_mob, e_x, e_y, ENTITY_FACING_RIGHT, 1, 0);
    entity->speed = 100;

    GAME_RUNQUEUE = scheduler_new_rq();
    schedule(GAME_RUNQUEUE, 0, 0, update_game_world, NULL);

    GLOBALS.game = GAME;

    return status;
}

void game_free() {
    world_free();
    free(GAME->world);
    free(GAME->entity_types);
    free(GAME->skins);
    free(GAME);
}

GameContext* game_get_context() {
    return GAME;
}

// TODO: Use a hashtable instead
EntityType* game_world_getxy(int x, int y) {
    x += GAME->world_view_x;
    y += GAME->world_view_y;

    /* world_view updating should take place in a separate task */
    if (GLOBALS.player->x < GAME->world_view_x+1) GAME->world_view_x--;
    if (GLOBALS.player->x > GAME->world_view_x + GLOBALS.view_port_maxx) GAME->world_view_x++;
    if (GLOBALS.player->y < GAME->world_view_y+1) GAME->world_view_y--;
    if (GLOBALS.player->y > GAME->world_view_y + GLOBALS.view_port_maxy) GAME->world_view_y++;

    Queue64* entity_qu = GAME->world->entities;

    qu_foreach(entity_qu, Entity*, e) {
        if (e->x == x && e->y == y) return e->type;
    }

    int id = world_getxy(x + GAME->world_view_x, y + GAME->world_view_y);
    return GAME->entity_types + id;
}

int game_world_setxy(int x, int y, EntityTypeID tid) {
    x += GAME->world_view_x;
    y += GAME->world_view_y;
    world_setxy(x, y, tid);
    return 0;
}

