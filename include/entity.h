#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include <stack64.h>
#include <game.h>
#include <world.h>

typedef enum {
    be_move,
} BehaviourID;

typedef struct EntityController {
    Queue64* behaviour_queue;
    void (*tick)(Entity*);
    void (*find_path)(Entity*, int, int);
} EntityController;

Entity* entity_spawn(World*, EntityType*, int, int, int, int);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(World*, Entity*);
void entity_set_keyboard_controller(Entity*);

#endif
