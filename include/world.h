#ifndef WORLD_HEADER
#define WORLD_HEADER

#include <game.h>
#include <timer.h>

typedef struct EntityType EntityType;
typedef struct EntityController EntityController;
typedef struct Entity {
    EntityType* type;
    EntityController* controller;
    TimeStamp next_move;
    int id, x, y, vx, vy, speed, health;
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
