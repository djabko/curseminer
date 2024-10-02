#include <stack64.h>
#include <entity.h>
#include <globals.h>
#include <stdio.h>

EntityController* DEFAULT_CONTROLLER = NULL;
Entity* ENTITY_ARRAY = NULL;
int MAX = 32;

void default_tick(Entity* e) {
    Queue64* qu = e->controller->behaviour_queue;
    if (qu_dequeue(qu) == be_move) {
        e->x += e->vx * e->speed;
        e->y += e->vy * e->speed;
    }

    if (e->x < 50) {
        e->controller->find_path(e, 50, 50);
        qu_enqueue(qu, be_move);
    }
}

void default_find_path(Entity* e, int x, int y) {
    if      (e->x < x)  e->vx =   e->speed;
    else if (e->x > x)  e->vx = -(e->speed);
    else                e->vx =   0;

    if      (e->y < y)  e->vy =   e->speed;
    else if (e->y > y)  e->vy = -(e->speed);
    else                e->vy =   0;
}

int create_default_entity_controller() {
    DEFAULT_CONTROLLER = calloc(1, sizeof(EntityController));
    DEFAULT_CONTROLLER->behaviour_queue = qu_init(8);
    DEFAULT_CONTROLLER->tick = default_tick;
    DEFAULT_CONTROLLER->find_path = default_find_path;

    return 1;
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
    new_entity->speed = 1;
    new_entity->health = 10;

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









