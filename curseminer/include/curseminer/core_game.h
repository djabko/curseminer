#ifndef GAME_HEADER
#define GAME_HEADER

#include <stdbool.h>

#include "curseminer/stack64.h"
#include "curseminer/scheduler.h"
#include "curseminer/frontend.h"
#include "curseminer/world.h"

#define GAME_REFRESH_RATE 20
#define E_MOD_CROUCHING E_MOD_0
#define TILE_BREAK_DISTANCE 10

#define game_cache_get(game, cache, x, y)                           \
    cache[(y) * (game)->viewport_w + (x)]

#define game_cache_set(game, cache, x, y, value)                    \
    do {                                                            \
        game_set_dirty((game), (x), (y), (1));                      \
        (cache) [(y) * (game)->viewport_w + (x)] = (value);         \
    } while(0);

#define gamew_cache_get(game, cache, x, y)                          \
    game_cache_get((game),                                          \
                   (cache),                                         \
                   (x) - (game)->world_view_x,                      \
                   (y) - (game)->world_view_y)

#define gamew_cache_set(game, cache, x, y, value)                   \
    game_cache_set((game),                                          \
                   (cache),                                         \
                   (x) - (game)->world_view_x,                      \
                   (y) - (game)->world_view_y,                      \
                   (value))


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

typedef struct DirtyFlags {
    byte_t *groups, *flags;
    size_t stride;
    int64_t command, groups_available, extra2, extra3;
} DirtyFlags;

typedef struct Skin Skin;

typedef struct EntityType {
    int id;
    char *name;
    Skin *default_skin;
} EntityType;

typedef struct World World;

typedef struct GameContext {
    World *world;

    int world_view_x, world_view_y, skins_c, skins_max, entity_types_c,
        entity_types_max, scroll_threshold, viewport_w, viewport_h;

    EntityType *entity_types;
    Skin *skins;

    // Can't be embedded, need to scale with screen size
    Entity **cache_entity;
    byte_t *cache_world;
    DirtyFlags *cache_dirty_flags;

    int (*f_init)(struct GameContext*, int);
    int (*f_update)();
    int (*f_free)();
} GameContext;

typedef struct GameContextCFG {
    int skins_max, entity_types_max, scroll_threshold;

    int (*f_init)(struct GameContext*, int);
    int (*f_update)();
    int (*f_free)();
} GameContextCFG;


/* Screen Tile Cache */
int world_from_mouse_xy(InputEvent*, int *world_x, int *world_y);
int game_on_screen(GameContext*,int, int);
byte_t game_world_dirty(GameContext*, int x, int y);
void game_set_dirty(GameContext*, int, int, int);
void game_flush_dirty(GameContext*);
void flush_game_entity_cache(GameContext*);
void flush_world_entity_cache(GameContext*);
bool game_resize_viewport(GameContext*, int w, int h);


/* Game Interface */
GameContext *game_init(GameContextCFG*, World*);
void game_free(GameContext*);
int game_update(Task*, Stack64*);

EntityType *game_world_getxy_type(GameContext *game, int x, int y);
Skin *game_world_getxy(GameContext*, int x, int y);
Skin *game_cache_getxy(GameContext*, int i);
int game_world_setxy(GameContext*, int x, int y, entity_id_t);

bool game_set_glyphset(GameContext*, const char*);
void game_create_skin(GameContext*, Skin*, int id, color_t, color_t, color_t,
        color_t, color_t, color_t);
EntityType *game_create_entity_type(GameContext*, Skin*);

#endif
