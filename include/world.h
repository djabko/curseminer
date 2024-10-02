#ifndef WORLD_HEADER
#define WORLD_HEADER

#include <game.h>

typedef struct EntityType EntityType;
typedef struct Entity {
    EntityType* type;
    int id, x, y, vx, vy, speed, health;
    // Controller
} Entity;

typedef struct World {
    int* world_array;
    Queue64* entities;
    int maxx, maxy, entity_c, entity_maxc;
} World;

World* world_init(int, int, int);
int world_getxy(int, int);
void world_free();

#endif
