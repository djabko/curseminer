#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include "stack64.h"
#include "game.h"
#include "world.h"

typedef enum {
    be_stop,
    be_move,
    be_move_one,
    be_face_up,
    be_face_down,
    be_face_left,
    be_face_right,
    be_face_ul,
    be_face_ur,
    be_face_dl,
    be_face_dr,
    be_place,
    be_attack,
    be_interact
} BehaviourID;

typedef enum {
    ENTITY_FACING_UP,
    ENTITY_FACING_LEFT,
    ENTITY_FACING_RIGHT,
    ENTITY_FACING_DOWN,
    ENTITY_FACING_UL,
    ENTITY_FACING_UR,
    ENTITY_FACING_DL,
    ENTITY_FACING_DR,
    ENTITY_FACING_END,
} EntityFacing;

typedef struct EntityController {
    Queue64* behaviour_queue;
    void (*tick)(Entity*);
    void (*find_path)(Entity*, int, int);
} EntityController;

Entity* entity_spawn(World*, EntityType*, int, int, EntityFacing, int, int);
int entity_command(Entity*, BehaviourID);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(World*, Entity*);
void entity_set_keyboard_controller(Entity*);

#endif
