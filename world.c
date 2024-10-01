#include <world.h>

#include <stdlib.h>

unsigned int* WORLD_ARRAY = NULL;
int MAXX = 0;
int MAXY = 0;
int MAXID = 1;

void world_gen() {
    if (WORLD_ARRAY == NULL) return;

    for (int i=0; i<MAXX*MAXY; i++)
        WORLD_ARRAY[i] = rand() % MAXID;
}

int world_init(int maxx, int maxy, int maxid) {
    if (WORLD_ARRAY != NULL) return 0;

    MAXID = maxid;
    MAXX = maxx;
    MAXY = maxy;
    WORLD_ARRAY = calloc(maxx * maxy, sizeof(int));

    world_gen();

    return WORLD_ARRAY != NULL;
}

int world_getxy(int x, int y) {
    return WORLD_ARRAY[x * MAXX + y];
}

void world_free(int, int) {
    if (WORLD_ARRAY == NULL) return;
    free(WORLD_ARRAY);
}


