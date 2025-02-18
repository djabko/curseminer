#include <globals.h>
#include <world.h>
#include <util.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define DEFAULT_CHUNK_ARENA_SIZE PAGE_SIZE * 16

int ITERATOR = 0;
/* Global Variables
 * Initialized in world_init() 
 */
ChunkDescriptor *CHUNK_DESCRIPTORS;
NoiseLattice *LATTICE_2D;
int GLOBAL_CHUNK_COUNT = 0;
int MAXID = 0;


/* Internal Helper Functions */
int is_arena_full(ChunkArena *arena) {
    return arena->free == arena->end;
}

// TODO: Standardize, this expensive function is often called three times successively 
int topleft_coordinate(int coordinate, int chunk_s) {
    if (0 <= coordinate) return chunk_s * (coordinate / chunk_s);
    else return chunk_s * ((coordinate + 1) / chunk_s) - chunk_s;
}

ChunkArena *chunk_init_arena(size_t mem_size, int chunk_size) {

    size_t chunk_stride = sizeof(Chunk) + chunk_size * chunk_size;
    size_t min_mem = sizeof(ChunkArena) + chunk_stride;
    int chunk_max = (mem_size - sizeof(ChunkArena)) / chunk_stride;

    // Make sure allocated memory has enough space for at least 1 chunk
    if (mem_size < min_mem)
        log_debug("ERROR: attempting to allocate arena of %luB, which is too small for chunk allocation (%luB)", mem_size, min_mem);

    ChunkArena *arena = calloc(mem_size, 1);

    Chunk *start = (Chunk*) (arena + 1);

    arena->count = 0;
    arena->max = chunk_max;
    arena->start = start;
    arena->free = start;
    arena->end = (Chunk*) ((uintptr_t)start + chunk_max * chunk_stride);
    arena->next = NULL;
    arena->descriptors = CHUNK_DESCRIPTORS + GLOBAL_CHUNK_COUNT; 

    return arena;
}

// Retrieves the next chunk in memory arena or NULL
Chunk *chunk_arena_next(World* world, ChunkArena *arena, Chunk *chunk) {
    if (arena == NULL || chunk == NULL || (chunk < arena->start && arena->end <= chunk)) return NULL;

    uintptr_t ptr = (uintptr_t) (chunk);
    ptr += world->chunk_mem_stride;

    return (Chunk*) ptr;
}

// Returns a free area of memory for chunk allocation
Chunk *chunk_get_free(World *world, ChunkDescriptor **cd) {
    ChunkArena *prev = NULL;
    ChunkArena *arena = world->chunk_arenas;

    // Look for arena with free space
    while (arena && is_arena_full(arena)) {
        prev = arena;
        arena = arena->next;
    }

    // All arenas are full
    if (arena == NULL) {

        size_t minmem = sizeof(ChunkArena) + world->chunk_mem_used + world->chunk_mem_stride;

        // Allocate new arena
        if (minmem <= world->chunk_mem_max) {
            // TODO: this check should be done in chunk_init_arena()
            size_t mem = world->chunk_mem_max - world->chunk_mem_used;
            mem = mem <= DEFAULT_CHUNK_ARENA_SIZE ? mem : DEFAULT_CHUNK_ARENA_SIZE;
            arena = chunk_init_arena(mem, world->chunk_s);

            if (!world->chunk_arenas) world->chunk_arenas = arena;
            if (prev) prev->next = arena;

            world->chunk_mem_used += mem;

        // Cannot allocate new arena without violating memory constraints, in this case reuse oldest arena
        } else {
            arena = world->chunk_arenas;
            world->chunk_arenas = world->chunk_arenas->next;
            prev->next = arena;
            arena->next = NULL;
            
            GLOBAL_CHUNK_COUNT -= arena->count;
            arena->count = 0;
            arena->free = arena->start;
            memset((void*) arena->descriptors, 0, arena->max * sizeof(ChunkDescriptor));
        }
    } 
    
    Chunk *re = arena->free;
    *cd = arena->descriptors + arena->count;

    arena->free = chunk_arena_next(world, arena, arena->free);
    arena->count++;

    return re;
}

int chunk_populate_void(double noise_sample) {
    return ge_air;
}

int chunk_populate_plains(double noise_sample) {
    double v = noise_sample;
    double p = 0.7;
    double e = 1 - p;

    // Normalize to range {0.0, 1.0}
    v = (v + 1) / 2;

    int id = 0;

    if (v <= p) id = ge_air;
    else if (v <= p+e*.5) id = ge_stone;
    else if (v <= p+e*.6) id = ge_iron;
    else id = ge_redore;

    return id;
}

int chunk_populate_forest(double noise_sample) {
    // Stub code
    return chunk_populate_void(noise_sample);
}

int chunk_populate_ocean(double noise_sample) {
    // Stub code
    return chunk_populate_void(noise_sample);
}

int chunk_populate_mountains(double noise_sample) {
    double v = noise_sample;
    double p = 0.5;
    double e = 1 - p;

    v = (v + 1) / 2;

    int id = 0;

    if (v <= p) id = ge_air;
    else if (v <= p+e*.4) id = ge_stone;
    else if (v <= p+e*.6) id = ge_iron;
    else if (v <= p+e*.7) id = ge_redore;
    else if (v <= p+e*.8) id = ge_gold;
    else if (v <= p+e*.9) id = ge_diamond;

    return id;
}

int chunk_populate_mine(double noise_sample) {
    double v = noise_sample;
    double p = 0.65;
    double e = 1 - p;

    v = (v + 1) / 2;

    int id = 0;

    if (v <= p) id = ge_air;
    else if (v <= p+e*.5) id = ge_stone;
    else if (v <= p+e*.75) id = ge_iron;
    else if (v <= p+e*.87) id = ge_redore;
    else if (v <= p+e*.98) id = ge_gold;
    else if (v <= p+e*.99) id = ge_diamond;

    return id;
}

int chunk_populate_redore(double noise_sample) {
    // Stub code
    return chunk_populate_void(noise_sample);
}

ChunkType chunk_determine_type(World *world, Chunk *chunk) {
    int latlen = LATTICE_2D->length;
    double resolution = 200.0;
    double f_x = ((double) chunk->tl_x) / resolution;
    double f_y = ((double) chunk->tl_y) / resolution;

    if (f_x < 0) f_x = latlen + fmod(f_x, latlen - 1) - 1;
    if (f_y < 0) f_y = latlen + fmod(f_y, latlen - 1) - 1;

    double v = perlin_noise_2D(LATTICE_2D, f_x, f_y);

    unsigned char is_pls = -0.25 < v && v < +0.25;
    unsigned char is_mts = +0.25 <= v && v <= +0.50;
    unsigned char is_mns = -0.50 <= v && v <= -0.25;

    if (is_pls) return CHUNK_TYPE_PLAINS;
    else if (is_mts) return CHUNK_TYPE_MOUNTAINS;
    else if (is_mns) return CHUNK_TYPE_MINE;
    else return CHUNK_TYPE_VOID;
}

int chunk_populate(World *world, Chunk *chunk) {
    int (*populate_f) (double);
    double (*noise_f) (NoiseLattice*, double, double);

    noise_f = perlin_noise_2D;

    switch (chunk->type) {
        case CHUNK_TYPE_PLAINS:
            populate_f = chunk_populate_plains;
            break;

        case CHUNK_TYPE_FOREST:
            populate_f = chunk_populate_forest;
            break;

        case CHUNK_TYPE_OCEAN:
            populate_f = chunk_populate_ocean;
            break;

        case CHUNK_TYPE_MOUNTAINS:
            populate_f = chunk_populate_mountains;
            break;

        case CHUNK_TYPE_REDORE:
            populate_f = chunk_populate_redore;
            break;

        case CHUNK_TYPE_MINE:
            populate_f = chunk_populate_mine;
            noise_f = perlin_noise_2D_var;
            break;

        default:
            populate_f = chunk_populate_void;
    }

    int chunk_s = world->chunk_s;
    double resolution = 20.0;

    int startx = chunk->tl_x;
    int starty = chunk->tl_y;
    int endx = startx + chunk_s;
    int endy = starty + chunk_s;

    for (int x=startx; x<endx; x++) {
        for (int y=starty; y<endy; y++) {
            double lattice_x = fabs(((double) x) / resolution);
            double lattice_y = fabs(((double) y) / resolution);
            double v = noise_f(LATTICE_2D, lattice_x, lattice_y);

            int tid = populate_f(v);

            int _x = abs(x) % chunk_s;
            int _y = abs(y) % chunk_s;

            chunk->data[_x * chunk_s + (_y % chunk_s)] = tid;
        }
    }

    return 1;
}

Chunk *_chunk_create(World *world, int x, int y, Chunk *top, Chunk *bottom, Chunk *left, Chunk *right) {

    ChunkDescriptor *cd;
    Chunk *chunk = chunk_get_free(world, &cd);
    chunk->data = (char*) (chunk + 1);

    chunk->tl_x = x;
    chunk->tl_y = y;

    chunk->top = top;
    chunk->bottom = bottom;
    chunk->left = left;
    chunk->right = right;

    GLOBAL_CHUNK_COUNT++;
    *cd = (ChunkDescriptor) {.tl_x = x, .tl_y = y, .ptr = chunk};

    chunk->type = chunk_determine_type(world, chunk);
    chunk_populate(world, chunk);

    return chunk;
}

Chunk *chunk_create(World *world, int x, int y) {
    return _chunk_create(world, x, y, NULL, NULL, NULL, NULL);
}

Chunk *chunk_lookup(World *world, int x, int y) {
    int chunk_s = world->chunk_s;

    // Make sure (x,y) points to top-left corner of their chunk
    x = topleft_coordinate(x, chunk_s);
    y = topleft_coordinate(y, chunk_s);

    ChunkArena *arena = world->chunk_arenas;
    ChunkDescriptor *start, *end, *cd;
    start = CHUNK_DESCRIPTORS;
    end = start + world->chunk_max;

    for (cd = start; cd < end; cd++) {
        if (cd->tl_x == x && cd->tl_y == y) return cd->ptr;
    }

    return NULL;
}

// Find chunk closest to position (x,y)
Chunk *chunk_nearest(World *world, int x, int y) {
    if (GLOBAL_CHUNK_COUNT < 1) return NULL;

    int chunk_s = world->chunk_s;
    x = topleft_coordinate(x, chunk_s);
    y = topleft_coordinate(y, chunk_s);

    ChunkDescriptor *nearest = CHUNK_DESCRIPTORS;
    int dist = man_dist(nearest->tl_x, nearest->tl_y, x, y);

    ChunkDescriptor *start, *end, *cd;
    start = CHUNK_DESCRIPTORS + 1;
    end = CHUNK_DESCRIPTORS + world->chunk_max;

    for (cd = start; cd < end; cd++) {
        if (!cd->ptr) continue;

        int tmp = man_dist(cd->tl_x, cd->tl_y, x, y);

        if (tmp < dist && 0 < tmp) {
            dist = tmp;
            nearest = cd;
        }
    }

    return nearest->ptr;
}

Chunk *chunk_insert(World* world, Chunk* source, Direction dir) {
    /* 1. Check based on direction
     * 2. Check if position not occupied
     * 3. Create new chunk
     * 4. Check for neighbours
     */

    int chunk_s = world->chunk_s;

    Chunk *new_chunk = NULL;

    int x, y;
    Chunk *top, *bottom, *left, *right;
    switch (dir) {
        case DIRECTION_UP:
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
            // Should never happen
            log_debug("FATAL ERROR WHEN INSERTING CHUNK");
            exit(0);
    }
    
    return new_chunk;
}

void chunk_free_all(World *world) {
    ChunkArena *arena = world->chunk_arenas;
    while (arena) {
        ChunkArena* rm = arena;
        arena = arena->next;
        free(rm);
    }
}


/* Interface World Functions */
World *world_init(int chunk_s, int maxid, size_t chunk_mem_max) {
    World *new_world = calloc(1, sizeof(World));

    size_t chunk_mem_stride = sizeof(Chunk) + chunk_s * chunk_s;
    int chunk_max = chunk_mem_max / chunk_mem_stride;
    CHUNK_DESCRIPTORS = calloc(chunk_max, sizeof(ChunkDescriptor));

    MAXID = maxid;
    new_world->chunk_s = chunk_s;
    new_world->chunk_arenas = NULL;
    new_world->entity_c = 0;
    new_world->entity_maxc = 32;
    new_world->entities = qu_init( new_world->entity_maxc );
    new_world->chunk_max = chunk_max;
    new_world->chunk_mem_used = 0;
    new_world->chunk_mem_max = chunk_mem_max;
    new_world->chunk_mem_stride = chunk_mem_stride;

    int latlen = 100;
    LATTICE_2D = noise_init(latlen * latlen, 2, latlen, fade);

    chunk_create(new_world, 0, 0);

    return new_world;
}

unsigned char world_getxy(World* world, int x, int y) {
    int chunk_s = world->chunk_s;
    Chunk *chunk = chunk_lookup(world, x, y);

    // Algorithm: keep inserting chunk to the nearest neighbour until a new chunk is present at (x,y)
    if (!chunk) {
        int tl_x = topleft_coordinate(x, chunk_s);
        int tl_y = topleft_coordinate(y, chunk_s);

        Chunk *nearest = chunk_nearest(world, tl_x, tl_y);
        if (!nearest) log_debug("ERROR: unable to find chunk closes to position (%d,%d); GLOBAL_CHUNK_COUNT: %d", tl_x, tl_y, GLOBAL_CHUNK_COUNT);

        int diff_x = tl_x - nearest->tl_x;
        int diff_y = tl_y - nearest->tl_y;

        while (diff_x != 0 || diff_y != 0) {

            // Used only for debugging
            int old_tl_x = nearest->tl_x;
            int old_tl_y = nearest->tl_y;

            // Change along x
            if (fabs(diff_y) < fabs(diff_x)) {

                // Nearest chunk is to the left, insert on it's right
                if (0 < diff_x) {
                    nearest = chunk_insert(world, nearest, DIRECTION_RIGHT);

                // Nearest chunk is to the right, insert on it's left
                } else if (0 > diff_x) {
                    nearest = chunk_insert(world, nearest, DIRECTION_LEFT);
                }

            // Change along y
            } else {
                
                // Nearest chunk is below, insert above
                if (diff_y < 0) {
                    nearest = chunk_insert(world, nearest, DIRECTION_UP);

                // Nearest chunk is above, insert below
                } else if (diff_y > 0) {
                    nearest = chunk_insert(world, nearest, DIRECTION_DOWN);
                }
            }

            if (!nearest) log_debug("ERROR: failed to insert chunk at (%d,%d)!\n", old_tl_x, old_tl_y);

            diff_x = tl_x - nearest->tl_x;
            diff_y = tl_y - nearest->tl_y;
        }

        chunk = nearest;
    }

    // Convert coordinates to chunk-local
    x = fabs(x % chunk_s);
    y = fabs(y % chunk_s);
    int ret = chunk->data[x * chunk_s + y];

    if (ret < 0 || ge_end <= ret) {

        log_debug("FATAL ERROR");
    }

    return ret;
}

void world_setxy(World *world, int x, int y, int tid) {
    int chunk_s = world->chunk_s;
    int tl_x = topleft_coordinate(x, chunk_s);
    int tl_y = topleft_coordinate(y, chunk_s);
    Chunk *chunk = chunk_lookup(world, tl_x, tl_y);

    if (!chunk) {
        log_debug("ERROR: attempting to set nonexistent chunk at (%d,%d) to entity ID: %d", x, y, tid);
        return;
    }
    else if (ge_end <= tid) {
        log_debug("ERROR: attempting to set (%d,%d) to invalid entity ID: %d", x, y, tid);
        return;
    }

    x = fabs(x % chunk_s);
    y = fabs(y % chunk_s);

    chunk->data[x * chunk_s + y] = tid;
}

void world_free(World *world) {
    chunk_free_all(world);
    noise_free(LATTICE_2D);
    free(world->entities);
    free(world);
}

