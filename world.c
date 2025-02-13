#include <globals.h>
#include <world.h>
#include <util.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define DEFAULT_CHUNK_ARENA_SIZE PAGE_SIZE * 1

int ITERATOR = 0;
/* Global Variables
 * Initialized in world_init() 
 */
World *WORLD = NULL;
ChunkDescriptor *CHUNK_DESCRIPTORS;
NoiseLattice *LATTICE2D;
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
    int chunk_max = (mem_size - sizeof(ChunkArena)) / chunk_stride;

    // Make sure allocated memory has enough space for at least 1 chunk
    mem_size = mem_size < sizeof(ChunkArena) + sizeof(Chunk) + chunk_size
        ? DEFAULT_CHUNK_ARENA_SIZE : mem_size;

    ChunkArena *arena = calloc(mem_size, 1);

    Chunk *start = (Chunk*) (arena + 1);

    arena->count = 0;
    arena->max = chunk_max;
    arena->chunk_s = chunk_size;
    arena->start = start;
    arena->free = start;
    arena->end = (Chunk*) ((uintptr_t)start + chunk_max * chunk_stride);
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
    while (arena && is_arena_full(arena)) {
        prev = arena;
        arena = arena->next;
    }

    if (arena == NULL) {
        arena = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE, WORLD->maxx);
        prev->next = arena;
    } 
    
    Chunk *re = arena->free;

    arena->free = chunk_arena_next(arena, arena->free);
    arena->count++;

    return re;
}

int chunk_populate_p(double noise_value, double sparsity) {
    double v = noise_value;
    double p = sparsity;
    double e = 1 - p;

    // Normalize to range {0.0, 1.0}
    v = (v + 1) / 2;

    int id = 0;

    if (v <= p) id = ge_air;
    else if (v <= p+e*.5) id = ge_stone;
    else if (v <= p+e*.875) id = ge_iron;
    else if (v <= p+e*.9375) id = ge_redore;
    else if (v <= p+e*.96875) id = ge_gold;
    else if (v <= p+e*.984375) id = ge_diamond;

    return id;
}

int chunk_populate(World *world, Chunk *chunk) {
    static const double resolution = 20.0;
    int chunk_s = world->chunk_arenas->chunk_s;

    int startx = chunk->tl_x;
    int starty = chunk->tl_y;
    int endx = startx + chunk_s;
    int endy = starty + chunk_s;
    for (int x=startx; x<endx; x++) {
        for (int y=starty; y<endy; y++) {
            double _x = fabs(((double) x) / resolution);
            double _y = fabs(((double) y) / resolution);
            double v = perlin_noise_2D(LATTICE2D, _x, _y);

            double p = .60;
            int id = chunk_populate_p(v, p);

            world_setxy(x, y, id);
        }
    }

    return 1;
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

    ChunkDescriptor *cd = CHUNK_DESCRIPTORS + GLOBAL_CHUNK_COUNT++;
    *cd = (ChunkDescriptor) {.tl_x = x, .tl_y = y, .ptr = chunk};

    chunk_populate(world, chunk);

    return chunk;
}

Chunk *chunk_create(World *world, int x, int y) {
    return _chunk_create(world, x, y, NULL, NULL, NULL, NULL);
}

Chunk *chunk_lookup(World *world, int x, int y) {
    int chunk_s = world->chunk_arenas->chunk_s;

    if (x == 20 && y == -20) {

    }

    // Make sure (x,y) points to top-left corner of their chunk
    x = topleft_coordinate(x, chunk_s);
    y = topleft_coordinate(y, chunk_s);

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
    if (GLOBAL_CHUNK_COUNT < 1) return NULL;

    int chunk_s = world->chunk_arenas->chunk_s;
    x = topleft_coordinate(x, chunk_s);
    y = topleft_coordinate(y, chunk_s);

    ChunkDescriptor *nearest = CHUNK_DESCRIPTORS;
    int dist = man_dist(nearest->tl_x, nearest->tl_y, x, y);

    for (ChunkDescriptor *cd = CHUNK_DESCRIPTORS+1; cd->ptr; cd++) {

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
            // Should never happen
            log_debug("FATAL ERROR WHEN INSERTING CHUNK");
            exit(0);
    }
    
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
World *world_init(int chunk_s, int maxid) {
    if (WORLD != NULL) return 0;
    
    WORLD = calloc(1, sizeof(World));
    CHUNK_DESCRIPTORS = calloc(PAGE_SIZE, sizeof(ChunkDescriptor));

    int latlen = 100;
    LATTICE2D = noise_init(latlen * latlen, 2, latlen, fade);

    MAXID = maxid;
    WORLD->maxx = chunk_s;
    WORLD->maxy = chunk_s;
    WORLD->chunk_arenas = chunk_init_arena(DEFAULT_CHUNK_ARENA_SIZE, chunk_s);
    WORLD->entity_c = 0;
    WORLD->entity_maxc = 32;
    WORLD->entities = qu_init( WORLD->entity_maxc );

    chunk_create(WORLD, 0, 0);

    return WORLD;
}

unsigned char world_getxy(int x, int y) {
    int chunk_s = WORLD->chunk_arenas->chunk_s;
    Chunk *chunk = chunk_lookup(WORLD, x, y);

    // Algorithm: keep inserting chunk to the nearest neighbour until a new chunk is present at (x,y)
    if (!chunk) {
        int tl_x = topleft_coordinate(x, chunk_s);
        int tl_y = topleft_coordinate(y, chunk_s);

        Chunk *nearest = chunk_nearest(WORLD, tl_x, tl_y);
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
                    nearest = chunk_insert(WORLD, nearest, DIRECTION_RIGHT);

                // Nearest chunk is to the right, insert on it's left
                } else if (0 > diff_x) {
                    nearest = chunk_insert(WORLD, nearest, DIRECTION_LEFT);
                }

            // Change along y
            } else {
                
                // Nearest chunk is below, insert above
                if (diff_y < 0) {
                    nearest = chunk_insert(WORLD, nearest, DIRECTION_UP);

                // Nearest chunk is above, insert below
                } else if (diff_y > 0) {
                    nearest = chunk_insert(WORLD, nearest, DIRECTION_DOWN);
                }
            }

            if (!nearest) log_debug("ERROR: attempting to overwrite existing chunk (%d,%d)!\n", old_tl_x, old_tl_y);

            diff_x = tl_x - nearest->tl_x;
            diff_y = tl_y - nearest->tl_y;
        }

        chunk = nearest;
    }

    // Convert coordinates to chunk-local
    x = fabs(x % chunk_s);
    y = fabs(y % chunk_s);
    int ret = chunk->data[x * chunk_s + y];

    return ret;
}

void world_setxy(int x, int y, int tid) {
    int chunk_s = WORLD->chunk_arenas->chunk_s;
    int tl_x = topleft_coordinate(x, chunk_s);
    int tl_y = topleft_coordinate(y, chunk_s);
    Chunk *chunk = chunk_lookup(WORLD, tl_x, tl_y);

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

void world_free(int, int) {
    if (WORLD->world_array == NULL) return;
    chunk_free_all();
    free(WORLD->entities);
    free(WORLD->world_array);
    free(WORLD);
}

