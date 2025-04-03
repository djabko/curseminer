#include <stdbool.h>
#include <stdio.h>

#include "globals.h"
#include "util.h"
#include "stack64.h"
#include "timer.h"
#include "entity.h"

EntityController DEFAULT_CONTROLLER;
Entity* ENTITY_ARRAY = NULL;
int MAX = 32;

int entity_command(Entity *e, BehaviourID be) {
    return qu_enqueue(e->controller->behaviour_queue, be);
}

void set_entity_velocity(Entity *e, int vx, int vy) {
    e->vx = vx;
    e->vy = vy;
}

void update_entity_position(Entity *e) {
    if (!e->moving) return;

    if (game_on_screen(e->x, e->y))
        gamew_cache_set(GAME_ENTITY_CACHE, e->x, e->y, 0);

    int v = 1;

    switch (e->facing) {
        case ENTITY_FACING_UL:
            set_entity_velocity(e, -v, -v);
            break;

        case ENTITY_FACING_UR:
            set_entity_velocity(e, +v, -v);
            break;

        case ENTITY_FACING_DL:
            set_entity_velocity(e, -v, +v);
            break;

        case ENTITY_FACING_DR:
            set_entity_velocity(e, +v, +v);
            break;

        case ENTITY_FACING_UP:
            set_entity_velocity(e, !v, -v);
            break;

        case ENTITY_FACING_DOWN:
            set_entity_velocity(e, !v, +v);
            break;

        case ENTITY_FACING_LEFT:
            set_entity_velocity(e, -v, !v);
            break;

        case ENTITY_FACING_RIGHT:
            set_entity_velocity(e, +v, !v);
            break;

        default:
            log_debug("ERROR: Invalid direction for entity %p", e);
    }

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
            if (entity_inventory_selected(e)) {
                int id = e->inventory_index;
                game_world_setxy(e->x, e->y, id);
                e->inventory[id]--;
            }

            break;

        case be_break:
            int f = e->facing;
            int c_x = 
                   1 * (f == ENTITY_FACING_RIGHT
                    || f == ENTITY_FACING_UR || f == ENTITY_FACING_DR)
                + -1 * (f == ENTITY_FACING_LEFT
                    || f == ENTITY_FACING_UL || f == ENTITY_FACING_DL);

            int c_y = 
                   1 * (f == ENTITY_FACING_DOWN
                    || f == ENTITY_FACING_DL || f == ENTITY_FACING_DR)
                + -1 * (f == ENTITY_FACING_UP
                    || f == ENTITY_FACING_UL || f == ENTITY_FACING_UR);

            int x = e->x + c_x;
            int y = e->y + c_y;
            int tid = world_getxy(GLOBALS.game->world, x, y);

            entity_inventory_add(e, tid);
            world_setxy(GLOBALS.game->world, x, y, 0);

            if (game_on_screen(x, y))
                gamew_cache_set(WORLD_ENTITY_CACHE, x, y, 0);

            break;

        case be_move_one:
            e->moving = true;
            update_entity_position(e);
            e->moving = false;
            break;

        case be_move:
            e->moving = true;
            break;

        case be_stop: 
            e->moving = false;
            break;

        case be_face_up:
            e->facing = ENTITY_FACING_UP;
            break;

        case be_face_down:
            e->facing = ENTITY_FACING_DOWN;
            break;

        case be_face_left:
            e->facing = ENTITY_FACING_LEFT;
            break;

        case be_face_right:
            e->facing = ENTITY_FACING_RIGHT;
            break;

        case be_face_ul:
            e->facing = ENTITY_FACING_UL;
            break;

        case be_face_ur:
            e->facing = ENTITY_FACING_UR;
            break;

        case be_face_dl:
            e->facing = ENTITY_FACING_DL;
            break;

        case be_face_dr:
            e->facing = ENTITY_FACING_DR;
            break;
    }
}

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() if player is
 * nearby.
 */
void default_tick(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;

    Entity *p = GLOBALS.player;

    int radius = 40;

    bool is_in_radius = man_dist(e->x, e->y, p->x, p->y) <= radius;
    if (!is_in_radius && e->moving) {
        entity_command(e, be_stop);

    } else if (is_in_radius && !e->moving) {
        entity_command(e, be_move);
    }

    if (is_in_radius) {
        e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);
        update_entity_position(e);
    }

    tick_entity_behaviour(e);
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
    else log_debug("ERROR: unable to execute pathfinding for entity %p(%d, %d)"
            "with respect to point (%d, %d)}", e, e->x, e->y, x, y);

    entity_command(e, be);
}

void entity_inventory_add(Entity *e, int tid) {
    if (!e->inventory) {
        int count = GLOBALS.game->entity_types_c;

        e->inventory = calloc(count, sizeof(typeof(count)));
    }
    
    e->inventory[tid]++;
}

int entity_inventory_get(Entity *e, int tid) {
    if (e->inventory) return *(e->inventory + tid);
    return 0;
}

int entity_inventory_selected(Entity* e) {
    return entity_inventory_get(e, e->inventory_index);
}

void entity_tick_abstract(Entity* e) {
    tick_entity_behaviour(e);

    e->controller->tick(e);

    update_entity_position(e);
}

int entity_create_controller(
        EntityController *controller,
        void(*f_tick)(Entity*),
        void(*f_find_path)(Entity*,int,int)) {

    controller->behaviour_queue = qu_init(1);
    controller->tick = f_tick;
    controller->find_path = f_find_path;

    return 1;
}

int entity_init_default_controller() {
    entity_create_controller(&DEFAULT_CONTROLLER,
            default_tick, default_find_path);
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
    new_entity->moving = false;
    new_entity->inventory = NULL;
    new_entity->inventory_index = 0;

    new_entity->controller = &DEFAULT_CONTROLLER;

    pq_enqueue(world->entities, new_entity, TIMER_NOW_MS);

    gamew_cache_set(GAME_ENTITY_CACHE, new_entity->x, new_entity->y, new_entity->type->id);

    return new_entity;
}

void entity_rm(World* world, Entity* entity) {
    if (world->entity_c <= 0) return;
    entity->id = -1;
    entity->type = NULL;
    if (entity->inventory) free(entity->inventory);
    world->entity_c--;
}

void entity_kill_by_id(int id) {}

void entity_kill_by_pos(int x, int y) {}

