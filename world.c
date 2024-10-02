#include <world.h>
#include <stdlib.h>
#include <stdio.h>

World* WORLD = NULL;
int MAXID = 0;

void world_gen() {
    if (WORLD->world_array == NULL) return;

    for (int i=0; i < (WORLD->maxx) * (WORLD->maxy); i++)
        WORLD->world_array[i] = rand() % MAXID;
}

World* world_init(int maxx, int maxy, int maxid) {
    if (WORLD != NULL) return 0;
    
    WORLD = calloc(1, sizeof(World));

    WORLD->maxx = maxx;
    WORLD->maxy = maxy;
    WORLD->entity_c = 0;
    WORLD->entity_maxc = 32;
    WORLD->world_array = calloc(maxx * maxy, sizeof(int));
    WORLD->entities = qu_init( WORLD->entity_maxc );

    MAXID = maxid;
    world_gen();

    return WORLD;
}

int world_getxy(int x, int y) {
    return WORLD->world_array[x * WORLD->maxy + y];
}

void world_free(int, int) {
    if (WORLD->world_array == NULL) return;
    free(WORLD->entities);
    free(WORLD->world_array);
    free(WORLD);
}

