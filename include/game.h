#ifndef GAME_HEADER
#define GAME_HEADER

#include <stack64.h>
#include <world.h>
#include <scheduler.h>


typedef struct DirtyFlags {
    byte *groups, *flags;
    size_t stride, groups_used;
    int64_t command, extra, extra2, extra3;
} DirtyFlags;

extern byte *WORLD_ENTITY_CACHE;
extern byte *GAME_ENTITY_CACHE;
extern DirtyFlags *GAME_DIRTY_FLAGS;

int game_on_screen(int, int);
void game_cache_set(byte*, int, int, byte);
void gamew_cache_set(byte*, int, int, byte);
byte game_cache_get(byte*, int, int);
byte gamew_cache_get(byte*, int, int);
byte game_world_dirty(int x, int y);
void game_set_dirty(int, int, int);
void game_flush_dirty();

typedef byte color_t;

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
    sk_null,
    sk_default,
    sk_gold,
    sk_diamond,
    sk_iron,
    sk_redore,
    sk_player,
    sk_chaser_mob,
    sk_end,
} SkinTypeID;

typedef enum {
    ge_air,
    ge_stone,
    ge_gold,
    ge_diamond,
    ge_iron,
    ge_redore,
    ge_player,
    ge_chaser_mob,
    ge_end
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

// TODO: add name
typedef struct EntityType {
    int id;
    Skin* skin;
} EntityType;

typedef struct World World;
typedef struct GameContext {
    World* world;
    int world_view_x, world_view_y, skins_c, skins_maxc, entity_types_c,
        entity_types_maxc, scroll_threshold;
    EntityType* entity_types;
    Skin* skins;
} GameContext;

int game_init();
void game_free();
int game_update(Task* task, Stack64* stack);
EntityType* game_world_getxy(int, int);
EntityType* _game_world_getxy(int);
int game_world_setxy(int, int, EntityTypeID);
GameContext* game_get_context();

#endif
