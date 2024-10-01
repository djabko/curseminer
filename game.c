#include "stdlib.h"

#include <game.h>
#include <world.h>
#include <entity.h>
#include <stack64.h>
#include <globals.h>
#include <scheduler.h>


RunQueue* GAME_RUNQUEUE;

int SKIN_COUNT = 0;
Skin *PREV_SKIN, *SKIN_MEMPOOL;

int GENTITYT_COUNT = 0;
GEntityType* GENTITYT_MEMPOOL;
Queue64* GENTITY_QUEUE;

int WORLD_VIEWX = 0;
int WORLD_VIEWY = 0;


void create_skin(Skin* skin, char c, color_t bg_r, color_t bg_g, color_t bg_b, color_t fg_r, color_t fg_g, color_t fg_b) {
    skin->node.prev = NULL;
    skin->node.next = NULL;

    if (PREV_SKIN != NULL) {
        skin->node.prev = &(PREV_SKIN->node);
        skin->node.prev->next = &(skin->node);
    }

    PREV_SKIN = skin;


    skin->id = SKIN_COUNT++;
    skin->character = c;

    skin->bg_r = bg_r;
    skin->bg_g = bg_g;
    skin->bg_b = bg_b;
    skin->fg_r = fg_r;
    skin->fg_g = fg_g;
    skin->fg_b = fg_b;
}

void create_entity_type(GEntityType* gentity, Skin* skin) {
    gentity->skin = skin;
    gentity->id = GENTITYT_COUNT++;
}

int init_skins() {
    PREV_SKIN = NULL;
    SKIN_MEMPOOL = malloc(PAGE_SIZE);

    create_skin(SKIN_MEMPOOL + sk_null,     ' ', 0, 0, 0, 255, 255, 255);
    create_skin(SKIN_MEMPOOL + sk_default,  '*', 0, 0, 0, 120, 120, 120);
    create_skin(SKIN_MEMPOOL + sk_gold,     'o', 0, 0, 0, 255, 215,   0);
    create_skin(SKIN_MEMPOOL + sk_diamond,  '&', 0, 0, 0,  80, 240, 220);
    create_skin(SKIN_MEMPOOL + sk_iron,     'f', 0, 0, 0, 120, 120, 120);
    create_skin(SKIN_MEMPOOL + sk_redore,   '.', 0, 0, 0, 120,   6,   2);
    create_skin(SKIN_MEMPOOL + sk_player,   'D', 0, 0, 0, 255, 215,   0);

    return SKIN_MEMPOOL != NULL;
}

int init_entity_types() {
    GENTITYT_MEMPOOL = malloc(PAGE_SIZE);

    create_entity_type(GENTITYT_MEMPOOL + ge_null, SKIN_MEMPOOL + sk_null);
    create_entity_type(GENTITYT_MEMPOOL + ge_stone, SKIN_MEMPOOL + sk_default);
    create_entity_type(GENTITYT_MEMPOOL + ge_gold, SKIN_MEMPOOL + sk_gold);
    create_entity_type(GENTITYT_MEMPOOL + ge_diamond, SKIN_MEMPOOL + sk_diamond);
    create_entity_type(GENTITYT_MEMPOOL + ge_iron, SKIN_MEMPOOL + sk_iron);
    create_entity_type(GENTITYT_MEMPOOL + ge_redore, SKIN_MEMPOOL + sk_redore);
    create_entity_type(GENTITYT_MEMPOOL + ge_player, SKIN_MEMPOOL + sk_player);

    return GENTITYT_MEMPOOL != NULL;
}

int update_game_world(Task* task, Stack64* stack) {
    WORLD_VIEWX++;
    WORLD_VIEWY++;
    tk_sleep(task, 1);

    return 0;
}

int game_init() {
    int status = 0;

    if (init_skins() != 0)
        status--;

    if (init_entity_types() != 0)
        status--;

    GENTITY_QUEUE = qu_init(1);

    GEntity* new_entity = entity_spawn(GENTITYT_MEMPOOL + ge_player, 5, 5, 1, 0);
    if (new_entity != NULL)
        qu_enqueue(GENTITY_QUEUE, (uint64_t) new_entity);

    world_init(256, 256, GENTITYT_COUNT-1);

    GAME_RUNQUEUE = scheduler_new_rq();
    schedule(GAME_RUNQUEUE, 0, 0, update_game_world, NULL);

    return status;
}


GEntityType* game_world_getxy(int x, int y) {
    int id = world_getxy(x + WORLD_VIEWX, y + WORLD_VIEWY);

    return GENTITYT_MEMPOOL + id;
}

Skin* game_getskins() {
    return SKIN_MEMPOOL;
}

