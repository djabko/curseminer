#include <globals.h>
#include <world.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_CHUNK_ARENA_SIZE PAGE_SIZE * 16

/* Global Variables
 * Initialized in world_init() 
 */
World *WORLD = NULL;
int MAXID = 0;


/* Internal Chunk Functions */
int is_arena_full(ChunkArena *arena) {
    return arena->free == arena->end;
}

// TODO: include chunk->data in memory arena
ChunkArena *chunk_init_arena(size_t mem_size, int chunk_length) {
    
    int chunk_max = (mem_size - sizeof(ChunkArena)) / ( sizeof(Chunk) + chunk_length );

    // Make sure allocated memory has enough space for at least 1 chunk
    mem_size = mem_size < sizeof(ChunkArena) + sizeof(Chunk) + chunk_length
        ? DEFAULT_CHUNK_ARENA_SIZE : mem_size;

    ChunkArena *arena = malloc(mem_size);
    Chunk *start = (Chunk*) (arena + 1);

    arena->count = 0;
    arena->max = chunk_max;
    arena->chunk_s = chunk_length;
    arena->start = start;
    arena->free = start;
    arena->end = start + chunk_max;
    arena->next = NULL;

    return arena;
}

// Returns a free area of memory for chunk allocation
Chunk *chunk_get_free(World *world) {
    ChunkArena *prev;
    ChunkArena *arena = world->chunk_arenas;
    while (is_arena_full(arena)) {
        prev = arena;
        arena = arena->next;
    }

    if (arena == NULL) {
        arena = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE, WORLD->maxx * WORLD->maxy);
        prev->next = arena;
    } 
    
    Chunk *re = arena->free;
    size_t stride = sizeof(Chunk) + arena->chunk_s * arena->chunk_s;
    uintptr_t ptr = (uintptr_t) (arena->free);
    ptr += stride;
    arena->free = (Chunk*) ptr;

    arena->count++;

    return re;
}

Chunk *_chunk_create(World *world, int x, int y, Chunk *top, Chunk *bottom, Chunk *left, Chunk *right) {

    Chunk *chunk = chunk_get_free(world);
    chunk->data = (char*) (chunk + 1);
    chunk->tl_x = x;
    chunk->tl_y = y;
    chunk->type = CHUNK_TYPE_NORMAL;

    chunk->top = top;
    chunk->bottom = bottom;
    chunk->left = left;
    chunk->right = right;

    return chunk;
}

Chunk *chunk_create(World *world, int x, int y) {
    fprintf(stderr, "Created chunk [%d/%d]\n", world->chunk_arenas->count, world->chunk_arenas->max);
    return _chunk_create(world, x, y, NULL, NULL, NULL, NULL);
}

void chunk_free_all() {
    ChunkArena *arena = WORLD->chunk_arenas;
    while (arena) {
        ChunkArena* rm = arena;
        arena = arena->next;
        free(rm);
    }
}


/* Interface World Functions */
void world_gen() {
    if (WORLD->world_array == NULL) return;

    for (int i=ge_air; i < (WORLD->maxx) * (WORLD->maxy); i++)
        WORLD->world_array[i] = rand() % MAXID;
}

World *world_init(int chunk_s, int maxid) {
    if (WORLD != NULL) return 0;
    
    WORLD = calloc(1, sizeof(World));

    MAXID = maxid;
    WORLD->maxx = chunk_s;
    WORLD->maxy = chunk_s;
    WORLD->chunk_arenas = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE, chunk_s);
    WORLD->entity_c = 0;
    WORLD->entity_maxc = 32;
    WORLD->entities = qu_init( WORLD->entity_maxc );

    WORLD->world_array = chunk_create(WORLD, 0, 0)->data;

    //world_gen();

    return WORLD;
}

unsigned char world_getxy(int x, int y) {
    if (x < 0 || WORLD->maxx < x
     || y < 0 || WORLD->maxy < y) return 0;

    return WORLD->world_array[x * WORLD->maxy + y];
}

void world_setxy(int x, int y, int tid) {
    if (WORLD->maxx < x || WORLD->maxy < y) return;

    WORLD->world_array[x * WORLD->maxy + y] = tid;
}

void world_free(int, int) {
    if (WORLD->world_array == NULL) return;
    chunk_free_all();
    free(WORLD->entities);
    free(WORLD->world_array);
    free(WORLD);
}

