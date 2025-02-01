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

typedef enum ChunkType {
    CHUNK_TYPE_NORMAL,
    CHUNK_TYPE_FOREST,
    CHUNK_TYPE_OCEAN
} ChunkType;

typedef struct Chunk {
    char *data;
    int tl_x, tl_y;
    ChunkType type;

    struct Chunk *top, *bottom, *left, *right;
} Chunk;

typedef struct ChunkDescriptor {
    int tl_x, tl_y;
    Chunk *ptr;
} ChunkDescriptor;

typedef struct ChunkArena {
    int count, max;
    size_t chunk_s;
    Chunk *start, *free, *end;
    struct ChunkArena* next;
} ChunkArena;

typedef struct World {
    char *world_array;
    ChunkArena *chunk_arenas;
    Queue64 *entities; // TODO: should be a linked list
    int maxx, maxy, entity_c, entity_maxc;
} World;

World* world_init(int, int);
unsigned char world_getxy(int, int);
void world_setxy(int, int, int);
void world_free();

#endif
