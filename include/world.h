#ifndef WORLD_HEADER
#define WORLD_HEADER

#include "game.h"
#include "timer.h"

typedef struct EntityType EntityType;
typedef struct EntityController EntityController;

// TODO: move to entity.h
typedef struct Entity {
    EntityType* type;
    EntityController* controller;
    milliseconds_t next_tick;
    int id, x, y, vx, vy, speed, health, facing;
} Entity;

typedef enum ChunkType {
    CHUNK_TYPE_VOID,
    CHUNK_TYPE_PLAINS,
    CHUNK_TYPE_FOREST,
    CHUNK_TYPE_OCEAN,
    CHUNK_TYPE_MOUNTAINS,
    CHUNK_TYPE_REDORE,
    CHUNK_TYPE_MINE,
} ChunkType;

typedef struct Chunk {
    char *data;
    int tl_x, tl_y;
    ChunkType type;

    struct Chunk *top, *bottom, *left, *right;
} Chunk;

typedef struct ChunkArena {
    int count, max;
    Chunk *start, *free, *end;
    struct ChunkArena* next;
} ChunkArena;

typedef struct World {
    ChunkArena *chunk_arenas;
    PQueue64 *entities;
                       
    int chunk_s, entity_c, entity_maxc, chunk_max;
    size_t chunk_mem_used, chunk_mem_stride, chunk_mem_max;
} World;

World* world_init(int, int, size_t);
unsigned char world_getxy(World*, int, int);
void world_setxy(World*, int, int, int);
void world_free(World*);

#endif
