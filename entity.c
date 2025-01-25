#include <globals.h>
#include <stack64.h>
#include <timer.h>
#include <entity.h>

#include <stdio.h>

EntityController* DEFAULT_CONTROLLER = NULL;
EntityController* KEYBOARD_CONTROLLER = NULL;
Entity* ENTITY_ARRAY = NULL;
int MAX = 32;

void tick_entity_behaviour(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;
    switch (qu_dequeue(qu)) {

        case be_attack:

        case be_interact:

        case be_place:
            game_world_setxy(e->x, e->y, ge_stone);

        case be_move:
            int interval = e->speed * 10;

            if (interval < timer_diff_milisec(&TIMER_NOW, &e->last_moved)) {
                e->x += e->vx;
                e->y += e->vy;
                e->last_moved = TIMER_NOW;
        }
    }
}

void player_highlight_tile(int x, int y) {
    /* TODO: 1. Make skin a property of each Entity
     *       2. Rename EntityType.skin with EntityType.default_skin
     *       3. Use this function to set skin of entity at (x,y) to sk_highlighted */
    return;
};

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() 
 */
void default_tick(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;
    e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);
    qu_enqueue(qu, be_move);

    tick_entity_behaviour(e);
}

/* Defines player action for each tick
 * Check keyboard states and places the appropriate action on player's behaviour queue
 */
void player_tick(Entity* player) {
    Queue64* qu = player->controller->behaviour_queue;
    int up = kb_down(KB_W);
    int down = kb_down(KB_S);
    int left = kb_down(KB_A);
    int right = kb_down(KB_D);
    int place_tile = kb_down(KB_C);

    if (!(up || down || left || right)) {
        player->vx = 0;
        player->vy = 0;

    } else {

        if (up) {
            player->vy = -1;
            player->facing = ENTITY_FACING_UP;
            player_highlight_tile(player->x, player->y-1);
        } else if (down) {
            player->vy = 1;
            player->facing = ENTITY_FACING_DOWN;
            player_highlight_tile(player->x, player->y+1);
        }

        if (left) {
            player->vx = -1;
            player->facing = ENTITY_FACING_LEFT;
            player_highlight_tile(player->x-1, player->y);
        } else if (right) {
            player->vx = 1;
            player->facing = ENTITY_FACING_RIGHT;
            player_highlight_tile(player->x+1, player->y);
        }

    }

    if (place_tile) {
        qu_enqueue(qu, be_place);
    }

    if (qu_empty(qu) && (up || down || right || left))
        qu_enqueue(qu, be_move);

    tick_entity_behaviour(player);
}


void default_find_path(Entity* e, int x, int y) {
    if (e->x < x && game_world_getxy(e->x+1, e->y)->id == ge_air)  e->vx =   1;
    else if (e->x > x && game_world_getxy(e->x-1, e->y)->id == ge_air)  e->vx =  -1;
    else e->vx =   0;

    if (e->y < y && game_world_getxy(e->x, e->y+1)->id == ge_air)  e->vy =   1;
    else if (e->y > y && game_world_getxy(e->x, e->y-1)->id == ge_air)  e->vy =  -1;
    else e->vy =   0;
}

int create_default_entity_controller() {
    DEFAULT_CONTROLLER = calloc(2, sizeof(EntityController));
    DEFAULT_CONTROLLER->behaviour_queue = qu_init(8);
    DEFAULT_CONTROLLER->tick = default_tick;
    DEFAULT_CONTROLLER->find_path = default_find_path;

    KEYBOARD_CONTROLLER = DEFAULT_CONTROLLER + 1;
    *KEYBOARD_CONTROLLER = *DEFAULT_CONTROLLER;
    KEYBOARD_CONTROLLER->tick = player_tick;

    return 1;
}

void entity_set_keyboard_controller(Entity* e) {
    if (!e) return;
    e->controller = KEYBOARD_CONTROLLER;
}


Entity* entity_spawn(World* world, EntityType* type, int x, int y, EntityFacing face, int num, int t) {
    if (ENTITY_ARRAY == NULL)
        ENTITY_ARRAY = calloc(world->entity_maxc, sizeof(Entity));

    int i=0;
    while (ENTITY_ARRAY[i].type != NULL) i++;
    Entity* new_entity = ENTITY_ARRAY + i;

    new_entity->x = x;
    new_entity->y = y;
    new_entity->type = type;
    new_entity->id = world->entity_c++;
    new_entity->vx = 0;
    new_entity->vy = 0;
    new_entity->speed = 20;
    new_entity->health = 10;
    new_entity->facing = face;
    new_entity->last_moved = TIMER_NEVER;

    create_default_entity_controller();
    new_entity->controller = DEFAULT_CONTROLLER;

    qu_enqueue(world->entities, (uint64_t) new_entity);

    return new_entity;
}

void entity_rm(World* world, Entity* entity) {
    if (world->entity_c <= 0) return;
    entity->id = -1;
    entity->type = NULL;
    world->entity_c--;
}

void entity_kill_by_id(int id) {}

void entity_kill_by_pos(int x, int y) {}

