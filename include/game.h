#ifndef GAME_HEADER
#define GAME_HEADER

#include <stack64.h>
#include <world.h>

typedef unsigned char color_t;

typedef enum {
    sk_null,
    sk_default,
    sk_gold,
    sk_diamond,
    sk_iron,
    sk_redore,
    sk_player,
} SkinTypeID;

typedef enum {
    ge_null,
    ge_stone,
    ge_gold,
    ge_diamond,
    ge_iron,
    ge_redore,
    ge_player,
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
    Skin* skin;
} EntityType;

typedef struct World World;
typedef struct GameContext {
    World* world;
    int world_view_x, world_view_y, skins_c, skins_maxc, entity_types_c, entity_types_maxc;
    EntityType* entity_types;
    Skin* skins;
} GameContext;

int game_init();
EntityType* game_world_getxy(int, int);
GameContext* game_get_context();

#endif
