#ifndef GAME_HEADER
#define GAME_HEADER

#include "stack64.h"
#include "scheduler.h"
#include "world.h"

#define E_MOD_CROUCHING E_MOD_0
#define TILE_BREAK_DISTANCE 10

/* Screen Cache */
typedef struct DirtyFlags {
    byte_t *groups, *flags;
    size_t stride, groups_used;
    int64_t command, extra, extra2, extra3;
} DirtyFlags;

extern byte_t *WORLD_ENTITY_CACHE;
extern byte_t *GAME_ENTITY_CACHE;
extern DirtyFlags *GAME_DIRTY_FLAGS;

int game_on_screen(int, int);
void game_cache_set(byte_t*, int, int, byte_t);
void gamew_cache_set(byte_t*, int, int, byte_t);
byte_t game_cache_get(byte_t*, int, int);
byte_t gamew_cache_get(byte_t*, int, int);
byte_t game_world_dirty(int x, int y);
void game_set_dirty(int, int, int);
void game_flush_dirty();

typedef byte_t color_t;

typedef enum {
    DIRECTION_NULL,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_UP_LEFT,
    DIRECTION_UP_RIGHT,
    DIRECTION_DOWN_LEFT,
    DIRECTION_DOWN_RIGHT
} Direction;

typedef enum {
    SKIN_NULL,
    SKIN_DEFAULT,
    SKIN_GOLD,
    SKIN_DIAMOND,
    SKIN_IRON,
    SKIN_REDORE,
    SKIN_PLAYER,
    SKIN_CHASER,
    SKIN_INVENTORY_SELECTED,
    SKIN_END,
} SkinTypeID;

typedef enum {
    ENTITY_AIR,
    ENTITY_STONE,
    ENTITY_GOLD,
    ENTITY_DIAMOND,
    ENTITY_IRON,
    ENTITY_REDORE,
    ENTITY_PLAYER,
    ENTITY_CHASER,
    ENTITY_END,
} EntityTypeID;


typedef struct Skin {
    int id;
    char character;
    unsigned char bg_r, 
                  bg_g,
                  bg_b,
                  fg_r,
                  fg_g,
                  fg_b;
} Skin;

typedef struct EntityType {
    int id;
    char *name;
    Skin *skin;
} EntityType;

typedef struct World World;
typedef struct GameContext {
    World *world;
    int world_view_x, world_view_y, skins_c, skins_max, entity_types_c,
        entity_types_max, scroll_threshold;
    EntityType *entity_types;
    Skin *skins;
} GameContext;

int game_init();
void game_free();
int game_update(Task*, Stack64*);
EntityType *game_world_getxy(int x, int y);
EntityType *game_cache_getxy(int i);
int game_world_setxy(int x, int y, EntityTypeID);
int game_cache_setxy(int i, EntityTypeID);
GameContext* game_get_context();

#endif
