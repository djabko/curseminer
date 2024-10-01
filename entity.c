#include <entity.h>
#include <globals.h>
#include <stdio.h>

Entity* entity_spawn(World* world, EntityType* type, int x, int y, int num, int t) {
    if (world->entity_c == world->entity_maxc) {
        world->entity_maxc *= 2;
        world->entities = realloc(world->entities, world->entity_maxc * sizeof(Entity));
    }

    Entity* new_entity = world->entities + world->entity_c;
    fprintf(stderr, "Accessing: %p + %d\n", world->entities, world->entities->x);

    new_entity->x = x;
    new_entity->y = y;
    new_entity->type = type;
    new_entity->id = world->entity_c++;
    new_entity->vx = 0;
    new_entity->vy = 0;
    new_entity->speed = 2;
    new_entity->health = 10;

    return new_entity;
}

void entity_rm(World* world, Entity* entity) {
    if (world->entity_c <= 0) return;
    entity->id = -1;
    world->entity_c--;
}

void entity_kill_by_id(int id) {}

void entity_kill_by_pos(int x, int y) {}

