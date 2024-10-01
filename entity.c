#include <entity.h>
#include <globals.h>

#define GENTITY_MAX PAGE_SIZE / sizeof(GEntity)

int GENTITY_COUNT = 0;
int GENTITY_ID = 0;
GEntity* GENTITY_MEMPOOL = NULL;

GEntity* entity_spawn(GEntityType* type, int x, int y, int num, int t) {
    if (GENTITY_COUNT >= GENTITY_MAX) return NULL;

    if (GENTITY_MEMPOOL == NULL) {
        GENTITY_MEMPOOL = malloc(PAGE_SIZE);
        for (int i=0; i<GENTITY_MAX; i++)
            GENTITY_MEMPOOL[i].id = -1;
    }

    GEntity* new_entity = NULL;
    for (int i=0; i<GENTITY_MAX; i++)
        if (GENTITY_MEMPOOL[i].id == -1)
            new_entity = GENTITY_MEMPOOL + i;

    if (new_entity == NULL) return NULL;

    new_entity->type = type;
    new_entity->id = GENTITY_ID++;
    new_entity->x = x;
    new_entity->y = y;

    GENTITY_COUNT++;

#include <stdio.h>
    fprintf(stderr, "ENTITY: %p\tx: %lf\ty: %lf\n", new_entity, new_entity->x, new_entity->y);

    return new_entity;
}

void entity_rm(GEntity* entity) {
    if (GENTITY_COUNT <= 0) return;
    entity->id = -1;
    GENTITY_COUNT--;

    if (GENTITY_COUNT == 0)
        free(GENTITY_MEMPOOL);
}

void entity_kill_by_id(int id) {}

void entity_kill_by_pos(int x, int y) {}

