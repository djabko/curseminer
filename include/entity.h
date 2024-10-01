#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include <game.h>

typedef struct GEntity {
    GEntityType* type;
    int id;
    double x, y, vx, vy; // velocity in cols per second
    void (*update) (struct GEntity*);
} GEntity;

GEntity* entity_spawn(GEntityType*, int, int, int, int);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(GEntity*);

#endif
