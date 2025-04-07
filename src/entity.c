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

static int g_behaviours_free_spots = 0;
static behaviour_t g_behaviour_count = 0;
static behaviour_t g_behaviour_max = 0;
static behaviour_func_t* g_behaviours;

int entity_command(Entity *e, behaviour_t be) {
    if (be <= 0 && g_behaviour_max <= be) return -1;

    return qu_enqueue(e->controller->behaviour_queue, be);
}

void set_entity_velocity(Entity *e, int vx, int vy) {
    e->vx = vx;
    e->vy = vy;
}

behaviour_t entity_create_behaviour(behaviour_func_t func) {
    behaviour_t be;

    if (0 < g_behaviours_free_spots) {

        for (be = 0; be < g_behaviour_count; be++) {
            if (g_behaviours[be] == NULL) break;
        }

        g_behaviours_free_spots--;

        if (g_behaviour_max <= be) return -1;

    } else {

        if (g_behaviour_count <= g_behaviour_max) {

            g_behaviour_max = g_behaviour_max < 1 ? 2 * g_behaviour_count : 8;
            size_t new_size = g_behaviour_max * sizeof(behaviour_func_t);

            g_behaviours = realloc(g_behaviours, new_size);
        }
    }

    g_behaviours[ g_behaviour_count++ ] = func;

    return g_behaviour_count;
}

void entity_process_behaviours(Entity *e) {
    Queue64* qu = e->controller->behaviour_queue;

    if (qu_empty(qu)) return;

    behaviour_t be = (behaviour_t) qu_dequeue(qu);

    g_behaviours[be](e);
}

int entity_remove_behaviour(behaviour_t be) {
    if (g_behaviour_max <= be)
        return -1;

    g_behaviours[be] = NULL;
    g_behaviours_free_spots++;

    return 0;
}

void entity_update_position(Entity *e) {
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

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() if player is
 * nearby.
 */
void default_tick(Entity* e) {
    return;
}

void default_find_path(Entity* e, int x, int y) {
    return;
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

    // Is this the right order?

    entity_process_behaviours(e);

    e->controller->tick(e);

    entity_update_position(e);
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

