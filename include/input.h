#ifndef KEYBOARD_HEADER
#define KEYBOARD_HEADER

#define ASCII_ESC 27

#include <stdint.h>

#include "timer.h"

typedef enum {
    GAME_FRONTEND_NCURSES
} game_frontent_t;

typedef enum {
    E_TYPE_NULL,
    E_TYPE_KB,
    E_TYPE_MS,
    E_TYPE_END,
} event_type_t;

typedef enum {
    ES_UP,
    ES_DOWN,
} event_state_t;

typedef enum {
    E_NOMOD = 0,
    E_MOD_0,
    E_MOD_1,
    E_MOD_2,
    E_MOD_3,
    E_MOD_4,
    E_MOD_5,
    E_MOD_6,
    E_MOD_7,
} event_mod_t;

typedef enum {
    E_NULL,
    E_KB_UP,
    E_KB_DOWN,
    E_KB_LEFT,
    E_KB_RIGHT,
    E_KB_A,
    E_KB_B,
    E_KB_C,
    E_KB_D,
    E_KB_E,
    E_KB_F,
    E_KB_G,
    E_KB_H,
    E_KB_I,
    E_KB_J,
    E_KB_K,
    E_KB_L,
    E_KB_M,
    E_KB_N,
    E_KB_O,
    E_KB_P,
    E_KB_Q,
    E_KB_R,
    E_KB_S,
    E_KB_T,
    E_KB_U,
    E_KB_V,
    E_KB_W,
    E_KB_X,
    E_KB_Y,
    E_KB_Z,
    E_KB_ESC,
    E_MS_LMB,
    E_MS_RMB,
    E_MS_MMB,
    E_END,
} event_t;

typedef enum {
    E_CTX_0,
    E_CTX_1,
    E_CTX_2,
    E_CTX_3,
    E_CTX_4,
    E_CTX_5,
    E_CTX_6,
    E_CTX_7,
    E_CTX_END,
} event_ctx_t;

typedef struct {
    event_t id;
    event_type_t type;
    event_state_t state;
    event_mod_t mods; // Bitmap
    uint64_t data;
} InputEvent;

void input_init(game_frontent_t);
int input_register_event(event_t, event_ctx_t, void (*)(InputEvent*));
void print_ncurses_mapping(const char* tag);

#endif
