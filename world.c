#include <globals.h>
#include <world.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define DEFAULT_CHUNK_ARENA_SIZE PAGE_SIZE * 16

int ITERATOR = 0;
/* Global Variables
 * Initialized in world_init() 
 */
World *WORLD = NULL;
ChunkDescriptor *CHUNK_DESCRIPTORS;
int MAXID = 0;


/* Internal Chunk Functions */
int is_arena_full(ChunkArena *arena) {
    return arena->free == arena->end;
}

// TODO: Reuse math code from UI.c
int _che_dist(int x1, int y1, int x2, int y2) {
    return round( max(x1 - x2, y1 - y2) );
}

ChunkArena *chunk_init_arena(size_t mem_size, int chunk_length) {
    
    int chunk_max = (mem_size - sizeof(ChunkArena)) / ( sizeof(Chunk) + chunk_length );

    // Make sure allocated memory has enough space for at least 1 chunk
    mem_size = mem_size < sizeof(ChunkArena) + sizeof(Chunk) + chunk_length
        ? DEFAULT_CHUNK_ARENA_SIZE : mem_size;

    ChunkArena *arena = calloc(mem_size, 1);
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

// Retrieves the next chunk in memory arena or NULL
Chunk *chunk_arena_next(ChunkArena *arena, Chunk *chunk) {
    if (arena == NULL || chunk == NULL || (chunk < arena->start && arena->end <= chunk)) return NULL;

    size_t stride = sizeof(Chunk) + arena->chunk_s * arena->chunk_s;
    uintptr_t ptr = (uintptr_t) (chunk);
    ptr += stride;

    return (Chunk*) ptr;
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

    arena->free = chunk_arena_next(arena, arena->free);
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

Chunk *chunk_lookup(World *world, int x, int y) {
    int chunk_s = world->chunk_arenas->chunk_s;

    if (x < 0 || chunk_s-1 < x || y < 0 || chunk_s-1 < y) {

    }

    // Make sure (x,y) points to top-left corner of their chunk
    if (x < 0) x -= chunk_s;
    if (y < 0) y -= chunk_s;

    x = (x / chunk_s) * chunk_s;
    y = (y / chunk_s) * chunk_s;

    ChunkArena *arena = world->chunk_arenas;
    for (; arena != NULL; arena = arena->next) {
        for (ChunkDescriptor *cd = CHUNK_DESCRIPTORS; cd->ptr; cd++) {
            if (cd->tl_x == x && cd->tl_y == y) return cd->ptr;
        }
    }

    return NULL;
}

// Find chunk closest to position (x,y)
Chunk *chunk_nearest(World *world, int x, int y) {
    ChunkArena *arena = world->chunk_arenas;

    if (arena == NULL || arena->start == NULL) return NULL;

    x = (x / arena->chunk_s) * arena->chunk_s;
    y = (y / arena->chunk_s) * arena->chunk_s;

    int min_dist = 0;
    Chunk *nearest = arena->start;
    while (arena != NULL) {
        for (Chunk *chunk = arena->start;
             chunk < arena->free;
             chunk = chunk_arena_next(arena, chunk)) {

            int dist = _che_dist(nearest->tl_x, nearest->tl_y, chunk->tl_x,   chunk->tl_y);

            if (dist < min_dist && 0 < dist) {
                min_dist = dist;
                nearest = chunk;
            }
        }
        arena = arena->next;
    }

    fprintf(stderr, "Found nearest neighbour to (%d,%d) at (%d,%d)\n", x, y, nearest->tl_x, nearest->tl_y);
    return nearest;
}

Chunk *chunk_insert(World* world, Chunk* source, Direction dir) {
    /* 1. Check based on direction
     * 2. Check if position not occupied
     * 3. Create new chunk
     * 4. Check for neighbours
     */

    fprintf(stderr, "Inserting chunk to the %x of (%d,%d)\n", dir, source->tl_x, source->tl_y);

    int chunk_s = world->chunk_arenas->chunk_s;

    Chunk *new_chunk = NULL;

    int x, y;
    Chunk *top, *bottom, *left, *right;
    switch (dir) {
        case DIRECTION_UP:
            if (source->top) return NULL;

            x = source->tl_x;
            y = source->tl_y - chunk_s;

            top = chunk_lookup(world, x, y - chunk_s);
            bottom = source;
            left = chunk_lookup(world, x - chunk_s, y);
            right = chunk_lookup(world, x + chunk_s, y);

            new_chunk = _chunk_create(world, x, y, top, bottom, left, right);

            source->top = new_chunk;
            break;

        case DIRECTION_DOWN:
            if (source->bottom) return NULL;

            x = source->tl_x;
            y = source->tl_y + chunk_s;

            top = source;
            bottom = chunk_lookup(world, x, y + chunk_s);
            left = chunk_lookup(world, x - chunk_s, y);
            right = chunk_lookup(world, x + chunk_s, y);

            new_chunk = _chunk_create(world, x, y, top, bottom, left, right);

            source->bottom = new_chunk;
            break;

        case DIRECTION_LEFT:
            if (source->left) return NULL;

            x = source->tl_x - chunk_s;
            y = source->tl_y;

            top = chunk_lookup(world, x, y - chunk_s);
            bottom = chunk_lookup(world, x, y + chunk_s);
            left = chunk_lookup(world, x - chunk_s, y);
            right = source;

            new_chunk = _chunk_create(world, x, y, top, bottom, left, right);

            source->left = new_chunk;
            break;

        case DIRECTION_RIGHT:
            if (source->right) return NULL;

            x = source->tl_x + chunk_s;
            y = source->tl_y;

            top = chunk_lookup(world, x, y - chunk_s);
            bottom = chunk_lookup(world, x, y + chunk_s);
            left = source;
            right = chunk_lookup(world, x + chunk_s, y);

            new_chunk = _chunk_create(world, x, y, top, bottom, left, right);

            source->right = new_chunk;
            break;

        default:
            fprintf(stderr, "FATAL ERROR WHEN INSERTING CHUNK\n");
            exit(0);
    }
    
    fprintf(stderr, "Inserted new chunk at (%d,%d)\n", new_chunk->tl_x, new_chunk->tl_y);

    return new_chunk;
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
    CHUNK_DESCRIPTORS = calloc(PAGE_SIZE, sizeof(ChunkDescriptor));

    MAXID = maxid;
    WORLD->maxx = chunk_s;
    WORLD->maxy = chunk_s;
    WORLD->chunk_arenas = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE, chunk_s);
    WORLD->entity_c = 0;
    WORLD->entity_maxc = 32;
    WORLD->entities = qu_init( WORLD->entity_maxc );

    Chunk *chunk1 = chunk_create(WORLD, 0, 0);
    WORLD->world_array = chunk1->data;

    //world_gen();

    return WORLD;
}

unsigned char world_getxy(int x, int y) {
    if (x < 0 || WORLD->maxx <= x
     || y < 0 || WORLD->maxy <= y) return 0;

    return WORLD->world_array[x * WORLD->maxy + y];
}

void world_setxy(int x, int y, int tid) {
    if (x < 0 || WORLD->maxx < x || y < 0 || WORLD->maxy < y) return;

    WORLD->world_array[x * WORLD->maxy + y] = tid;
}

void world_free(int, int) {
    if (WORLD->world_array == NULL) return;
    chunk_free_all();
    free(WORLD->entities);
    free(WORLD->world_array);
    free(WORLD);
}

