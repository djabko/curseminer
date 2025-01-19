#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include <stack64.h>
#include <game.h>
#include <world.h>

typedef enum {
    be_move,
    be_place,
    be_attack,
    be_interact
} BehaviourID;

typedef enum {
    ENTITY_FACING_UP,
    ENTITY_FACING_RIGHT,
    ENTITY_FACING_LEFT,
    ENTITY_FACING_DOWN,
    ENTITY_FACING_END,
} EntityFacing;

typedef struct EntityController {
    Queue64* behaviour_queue;
    void (*tick)(Entity*);
    void (*find_path)(Entity*, int, int);
} EntityController;

Entity* entity_spawn(World*, EntityType*, int, int, EntityFacing, int, int);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(World*, Entity*);
void entity_set_keyboard_controller(Entity*);

#endif
