#include <globals.h>
#include <stack64.h>
#include <timer.h>
#include <entity.h>

#include <stdio.h>

EntityController* DEFAULT_CONTROLLER = NULL;
EntityController* KEYBOARD_CONTROLLER = NULL;
Entity* ENTITY_ARRAY = NULL;
int MAX = 32;

void tick_move_entity(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;
    if (qu_dequeue(qu) == be_move && timer_ready(&e->next_move)) {
        e->x += e->vx;
        e->y += e->vy;

        timer_now(&e->next_move);
        e->next_move.tv_sec += e->speed / 100;
        e->next_move.tv_usec += (e->speed % 100) * 10000;
    }
}

void player_tick(Entity* player) {
    Queue64* qu = player->controller->behaviour_queue;
    int up = GLOBALS.keyboard.keys[KB_W] || GLOBALS.keyboard.keys[KB_UP];
    int down = GLOBALS.keyboard.keys[KB_S] || GLOBALS.keyboard.keys[KB_DOWN];
    int right = GLOBALS.keyboard.keys[KB_D] || GLOBALS.keyboard.keys[KB_RIGHT];
    int left = GLOBALS.keyboard.keys[KB_A] || GLOBALS.keyboard.keys[KB_LEFT];

    if (!up && !down) {
        player->vy = 0;
    } else if (up) {
        player->vy = -1;
    } else if (down) {
        player->vy = 1;
    }

    if (!left && !right) {
        player->vx = 0;
    } else if (left) {
        player->vx = -1;
    } else if (right) {
        player->vx = 1;
    }

    if (qu_empty(qu) && (up || down || right || left))
        qu_enqueue(qu, be_move);

    tick_move_entity(player);
}

void default_tick(Entity* e) {
    if (e->x != 20 || e->y != 10) {
        Queue64* qu = e->controller->behaviour_queue;
        e->controller->find_path(e, 20, 10);
        qu_enqueue(qu, be_move);
    }

    tick_move_entity(e);
}

void default_find_path(Entity* e, int x, int y) {
    if      (e->x < x)  e->vx =   1;
    else if (e->x > x)  e->vx =  -1;
    else                e->vx =   0;

    if      (e->y < y)  e->vy =   1;
    else if (e->y > y)  e->vy =  -1;
    else                e->vy =   0;
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


Entity* entity_spawn(World* world, EntityType* type, int x, int y, int num, int t) {
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
    timer_now(&new_entity->next_move);

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

