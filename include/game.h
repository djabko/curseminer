#ifndef GAME_HEADER
#define GAME_HEADER

#include <stack64.h>

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
} GEntityTypeID;


typedef struct Skin {
    Node node; // &node should be the same as Skin* 
    int id;
    char character;
    unsigned char bg_r, 
                  bg_g,
                  bg_b,
                  fg_r,
                  fg_g,
                  fg_b;
} Skin;

typedef struct GEntityType {
    int id;
    Skin* skin;
} GEntityType;

int game_init();
GEntityType* game_world_getxy(int, int);
Skin* game_getskins();

#endif
