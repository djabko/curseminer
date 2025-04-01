#ifndef GAME_HEADER
#define GAME_HEADER

#include "stack64.h"
#include "scheduler.h"
#include "input.h"
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

int world_from_mouse_xy(InputEvent*, int *world_x, int *world_y);
int game_on_screen(int, int);
void game_cache_set(byte_t*, int, int, byte_t);
void gamew_cache_set(byte_t*, int, int, byte_t);
byte_t game_cache_get(byte_t*, int, int);
byte_t gamew_cache_get(byte_t*, int, int);
byte_t game_world_dirty(int x, int y);
void game_set_dirty(int, int, int);
void game_flush_dirty();
void flush_game_entity_cache();
void flush_world_entity_cache();

typedef byte_t color_t;
typedef int entity_id_t;

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

typedef struct Skin {
    int glyph;
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
    Skin *default_skin;
} EntityType;

typedef struct World World;
typedef struct GameContext {
    World *world;
    int world_view_x, world_view_y, skins_c, skins_max, entity_types_c,
        entity_types_max, scroll_threshold;
    EntityType *entity_types;
    Skin *skins;

    int (*f_init)(struct GameContext*, int);
    int (*f_update)();
    int (*f_free)();
} GameContext;


void game_create_skin(Skin*, int id, color_t, color_t, color_t,
        color_t, color_t, color_t);
void game_create_entity_type(EntityType*, Skin*);


int game_init();
void game_free();
int game_update(Task*, Stack64*);
EntityType *game_world_getxy(int x, int y);
EntityType *game_cache_getxy(int i);
int game_world_setxy(int x, int y, entity_id_t);
int game_cache_setxy(int i, entity_id_t);
GameContext* game_get_context();

#endif
