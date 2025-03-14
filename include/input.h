#ifndef KEYBOARD_HEADER
#define KEYBOARD_HEADER

#define ASCII_ESC 27

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
    E_MOD_0 =   1,
    E_MOD_1 =   2,
    E_MOD_2 =   4,
    E_MOD_3 =   8,
    E_MOD_4 =  16,
    E_MOD_5 =  32,
    E_MOD_6 =  64,
    E_MOD_7 = 128,
} event_mods_t;

typedef enum {
    E_NULL,
    E_KB_UP,
    E_KB_DOWN,
    E_KB_LEFT,
    E_KB_RIGHT,
    E_KB_W,
    E_KB_S,
    E_KB_A,
    E_KB_D,
    E_KB_C,
    E_KB_G,
    E_KB_Q,
    E_KB_ESC,
    E_MS_LMB,
    E_MS_RMB,
    E_MS_MMB,
    E_END,
} event_t;

typedef struct {
    event_t id;
    event_type_t type;
    event_state_t state;
    event_mods_t mods;
} InputEvent;

#define MAX_INPUT_HANDLERS 16
typedef struct {
    void (*handlers[MAX_INPUT_HANDLERS])(InputEvent*);
    int handler_c;
} InputHandler;

void input_init(game_frontent_t);
int input_register_handler(void (*)(InputEvent*));
void map_input(InputEvent*, void (*)()); // Replace multiple handlers with one handler and a jump table

#endif
