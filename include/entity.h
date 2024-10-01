#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include <game.h>
#include <world.h>

Entity* entity_spawn(World*, EntityType*, int, int, int, int);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(World*, Entity*);

#endif
