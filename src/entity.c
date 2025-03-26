#include <stdbool.h>
#include <stdio.h>

#include "globals.h"
#include "util.h"
#include "stack64.h"
#include "timer.h"
#include "entity.h"

EntityController* DEFAULT_CONTROLLER = NULL;
EntityController* KEYBOARD_CONTROLLER = NULL;
Entity* ENTITY_ARRAY = NULL;
int MAX = 32;

int entity_command(Entity *e, BehaviourID be) {
    return qu_enqueue(e->controller->behaviour_queue, be);
}

static inline void set_entity_facing(Entity* e, int vx, int vy, EntityFacing direction) {
    e->facing = direction;

    e->vx = vx;
    e->vy = vy;
}

void tick_entity_behaviour(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;

    if (qu_empty(qu)) return;

    int v = 1;

    switch (qu_dequeue(qu)) {

        case be_attack:
            break;

        case be_interact:
            break;

        case be_place:
            game_world_setxy(e->x, e->y, ENTITY_STONE);
            break;

        case be_move:
            e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);
            break;

        case be_stop: 
            e->vx = 0;
            e->vy = 0;
            break;

        case be_face_up:
            set_entity_facing(e, !v, -v, ENTITY_FACING_UP);
            break;

        case be_face_down:
            set_entity_facing(e, !v, +v, ENTITY_FACING_DOWN);
            break;

        case be_face_left:
            set_entity_facing(e, -v, !v, ENTITY_FACING_LEFT);
            break;

        case be_face_right:
            set_entity_facing(e, +v, !v, ENTITY_FACING_RIGHT);
            break;

        case be_face_ul:
            set_entity_facing(e, -v, -v, ENTITY_FACING_UL);
            break;

        case be_face_ur:
            set_entity_facing(e, +v, -v, ENTITY_FACING_UR);
            break;

        case be_face_dl:
            set_entity_facing(e, -v, +v, ENTITY_FACING_DL);
            break;

        case be_face_dr:
            set_entity_facing(e, +v, +v, ENTITY_FACING_DR);
            break;
    }
}

void update_entity_position(Entity *e) {
    if (game_on_screen(e->x, e->y))
        gamew_cache_set(GAME_ENTITY_CACHE, e->x, e->y, 0);

    int nx = e->x + e->vx;
    int ny = e->y + e->vy;

    int id = world_getxy(GLOBALS.game->world, nx, ny);
    id = id ? id : gamew_cache_get(GAME_ENTITY_CACHE, nx, ny);

    if (!id) {
        e->x += e->vx;
        e->y += e->vy;
    }

    if (game_on_screen(e->x, e->y))
        gamew_cache_set(GAME_ENTITY_CACHE, e->x, e->y, e->type->id);
}

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() if player is
 * nearby.
 */
void default_tick(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;

    Entity *p = GLOBALS.player;

    bool moving = e->vx != 0 || e->vy != 0;

    if (moving) {
        if (man_dist(e->x, e->y, p->x, p->y) > 40) {
            entity_command(e, be_stop);
            return;
        }

        e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);
        tick_entity_behaviour(e);
        update_entity_position(e);

    } else {
        entity_command(e, be_move);
        tick_entity_behaviour(e);
    }
}

void default_find_path(Entity* e, int x, int y) {
    if (e->x == x && e->y == y) return;

    const int up = (e->y > y);
    const int down = (e->y < y);
    const int left = (e->x > x);
    const int right = (e->x < x);

    BehaviourID be = be_stop;
    
    if      (up && left)    be = be_face_ul;
    else if (up && right)   be = be_face_ur;
    else if (down && left)  be = be_face_dl;
    else if (down && right) be = be_face_dr;
    else if (up)            be = be_face_up;
    else if (down)          be = be_face_down;
    else if (left)          be = be_face_left;
    else if (right)         be = be_face_right;
    else log_debug(
"ERROR: unable to execute pathfinding for entity %p(%d, %d) with respect to\
point (%d, %d)}", e->id, e->x, e->y, x, y, be);

    entity_command(e, be);
}

/* Defines player action for each tick */
void player_tick(Entity* player) {
    tick_entity_behaviour(player);
    update_entity_position(player);
}

int create_default_entity_controller() {
    DEFAULT_CONTROLLER = calloc(2, sizeof(EntityController));
    DEFAULT_CONTROLLER->behaviour_queue = qu_init(1);
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
    new_entity->next_tick = TIMER_NOW_MS;

    create_default_entity_controller();
    new_entity->controller = DEFAULT_CONTROLLER;

    pq_enqueue(world->entities, new_entity, TIMER_NOW_MS);

    gamew_cache_set(GAME_ENTITY_CACHE, new_entity->x, new_entity->y, new_entity->type->id);

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

