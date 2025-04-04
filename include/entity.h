#ifndef ENTITY_HEADER
#define ENTITY_HEADER

#include "stack64.h"
#include "core_game.h"
#include "world.h"

typedef int behaviour_t;

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

int entity_init_default_controller();
int entity_create_controller(EntityController*, void(*)(Entity*), void(*)(Entity*, int, int));
void entity_tick_abstract(Entity*);
void entity_update_position(Entity*);

Entity* entity_spawn(World*, EntityType*, int, int, EntityFacing, int, int);
int entity_command(Entity*, behaviour_t);
void entity_kill_by_id(int);
void entity_kill_by_pos(int, int);
void entity_rm(World*, Entity*);
void entity_set_keyboard_controller(Entity*);
void entity_inventory_add(Entity*, int);
int entity_inventory_get(Entity*, int);
int entity_inventory_selected(Entity*);

#endif
