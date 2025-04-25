#ifndef WORLD_HEADER
#define WORLD_HEADER

#include <stdbool.h>

#include "core_game.h"
#include "timer.h"

typedef struct EntityType EntityType;
typedef struct EntityController EntityController;

typedef struct Skin {
    uint16_t glyph, rotation;
    uint8_t offset_x, offset_y;
    bool flip_x, flip_y;

    uint8_t bg_r, bg_g, bg_b, bg_a,
            fg_r, fg_g, fg_b, fg_a;
} Skin;

typedef struct Entity {
    EntityType* type;
    Skin skin;
    EntityController* controller;
    milliseconds_t next_tick;
    int id, x, y, vx, vy, speed, health, facing, inventory_index;
    int *inventory;
    bool moving, moved;
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

typedef enum {
    ENTITY_AIR,
    ENTITY_STONE,
    ENTITY_GOLD,
    ENTITY_DIAMOND,
    ENTITY_IRON,
    ENTITY_REDORE,
    ENTITY_PLAYER,
    ENTITY_CHASER,
    ENTITY_END,
} EntityTypeID;

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
