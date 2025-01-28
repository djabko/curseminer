#include <globals.h>
#include <world.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_CHUNK_ARENA_SIZE PAGE_SIZE * 16

/* Global Variables
 * Initialized in world_init() 
 */
const int DEFAULT_CHUNK_LENGTH = 256;
World *WORLD = NULL;
int MAXID = 0;


/* Internal Chunk Functions */
ChunkArena *chunk_init_arena(size_t size) {

    // Make sure allocated memory has enough space for at least 1 chunk
    size = sizeof(ChunkArena) + sizeof(Chunk) < size
        ? size : DEFAULT_CHUNK_ARENA_SIZE;

    ChunkArena *arena = malloc(size);
    Chunk *start = (Chunk*) (arena+1);

    int chunk_max = (size - sizeof(ChunkArena)) / sizeof(Chunk);

    arena->count = 0;
    arena->max = chunk_max;
    arena->start = start;
    arena->free = start;
    arena->end = start + chunk_max;
    arena->next = NULL;

    return arena;
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

World *world_init(int maxx, int maxy, int maxid) {
    if (WORLD != NULL) return 0;
    
    WORLD = calloc(1, sizeof(World));

    MAXID = maxid;
    WORLD->maxx = maxx;
    WORLD->maxy = maxy;
    WORLD->chunk_arenas = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE);
    WORLD->entity_c = 0;
    WORLD->entity_maxc = 32;
    WORLD->world_array = calloc(maxx * maxy, sizeof(int));
    WORLD->entities = qu_init( WORLD->entity_maxc );

    //world_gen();

    return WORLD;
}

int world_getxy(int x, int y) {
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

