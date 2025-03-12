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
            if (game_on_screen(e->x, e->y))
                gamew_cache_set(GAME_ENTITY_CACHE, e->x, e->y, 0);

            e->x += e->vx;
            e->y += e->vy;

            if (game_on_screen(e->x, e->y))
                gamew_cache_set(GAME_ENTITY_CACHE, e->x, e->y, e->type->id);
    }
}

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() 
 */
void default_tick(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;
    e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);
    qu_enqueue(qu, be_move);

    tick_entity_behaviour(e);
}

static inline void set_entity_facing(Entity* e, int x, int y, int vx, int vy, EntityFacing direction) {
    e->facing = direction;

    int id = world_getxy(GLOBALS.game->world, x, y);
    id = id ? id : gamew_cache_get(GAME_ENTITY_CACHE, x, y);

    if (id == 0) {
        e->vx = vx;
        e->vy = vy;
    } else {
        e->vx = 0;
        e->vy = 0;
    }
}

void default_find_path(Entity* e, int x, int y) {
    if (e->x == x && e->y == y) return;

    #define _up 1 << 0
    #define _down 1 << 1
    #define _left 1 << 2
    #define _right 1 << 3
    #define _ul 1 << 4
    #define _ur 1 << 5
    #define _dl 1 << 6
    #define _dr 1 << 7

    const int up = (e->y > y) ? _up : 0;
    const int down = (e->y < y) ? _down : 0;
    const int left = (e->x > x) ? _left : 0;
    const int right = (e->x < x) ? _right : 0;
    const int ul = (up && left) ? _ul : 0;
    const int ur = (up && right) ? _ur : 0;
    const int dl = (down && left) ? _dl : 0;
    const int dr = (down && right) ? _dr : 0;

    const int bitmap = (ul + ur + dl + dr > 0) ? 
                        ul + ur + dl + dr  :
                        up + down + left + right;

    int v = 1;
    x = e->x;
    y = e->y;
    switch (bitmap) {
        case _ul:
            set_entity_facing(e, x-1, y-1, -v, -v, ENTITY_FACING_UL);
            break;
        case _ur:
            set_entity_facing(e, x+1, y-1, +v, -v, ENTITY_FACING_UR);
            break;
        case _dl:
            set_entity_facing(e, x-1, y+1, -v, +v, ENTITY_FACING_DL);
            break;
        case _dr:
            set_entity_facing(e, x+1, y+1, +v, +v, ENTITY_FACING_DR);
            break;
        case _up:
            set_entity_facing(e, x+0, y-1, !v, -v, ENTITY_FACING_UP);
            break;
        case _down:
            set_entity_facing(e, x+0, y+1, !v, +v, ENTITY_FACING_DOWN);
            break;
        case _left:
            set_entity_facing(e, x-1, y+0, -v, !v, ENTITY_FACING_LEFT);
            break;
        case _right:
            set_entity_facing(e, x+1, y+0, +v, !v, ENTITY_FACING_RIGHT);
            break;
        default: 
            log_debug("ERROR: unable to execute pathfinding for entity %d, direction bitmap: %d}", e->id, bitmap);
            break;
    }

    #undef _up
    #undef _down
    #undef _left
    #undef _right
    #undef _ul
    #undef _ur
    #undef _dl
    #undef _dr
}

/* Defines player action for each tick */
void player_tick(Entity* player) {
    Queue64* qu = player->controller->behaviour_queue;
    int up = kb_down(KB_W);
    int down = kb_down(KB_S);
    int left = kb_down(KB_A);
    int right = kb_down(KB_D);
    int place_tile = kb_down(KB_C);

    int x = 0;
    int y = 0;
    if (up || down || left || right) {
        x = player->x;
        y = player->y;

        if (up && left)
            player->controller->find_path(player, x-1, y-1);
        else if (up && right)
            player->controller->find_path(player, x+1, y-1);
        else if (down && left)
            player->controller->find_path(player, x-1, y+1);
        else if (down && right)
            player->controller->find_path(player, x+1, y+1);
        else if (up)
            player->controller->find_path(player, x+0, y-1);
        else if (down)
            player->controller->find_path(player, x+0, y+1);
        else if (left)
            player->controller->find_path(player, x-1, y+0);
        else if (right)
            player->controller->find_path(player, x+1, y+0);
        else
            log_debug("ERROR: unable to execute pathfinding for player");

    } else {
        player->vx = 0;
        player->vy = 0;
    }

    if (place_tile) {
        qu_enqueue(qu, be_place);
    }

    if (qu_empty(qu) && (up || down || right || left))
        qu_enqueue(qu, be_move);

    tick_entity_behaviour(player);
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

