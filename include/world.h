#ifndef WORLD_HEADER
#define WORLD_HEADER

#include <game.h>
#include <timer.h>

typedef struct EntityType EntityType;
typedef struct EntityController EntityController;

// TODO: move to entity.h
typedef struct Entity {
    EntityType* type;
    EntityController* controller;
    TimeStamp last_moved;
    int id, x, y, vx, vy, speed, health, facing;
} Entity;

typedef struct World {
    int* world_array;
    Queue64* entities; // should be a linked list
    int maxx, maxy, entity_c, entity_maxc;
} World;

World* world_init(int, int, int);
int world_getxy(int, int);
void world_setxy(int, int, int);
void world_free();

#endif
