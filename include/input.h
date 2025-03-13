#ifndef KEYBOARD_HEADER
#define KEYBOARD_HEADER

#define ASCII_ESC 27

#include "timer.h"

typedef enum {
    ES_UP,
    ES_DOWN,
} event_state_t;

typedef enum {
    E_MOD_0 =  0,
    E_MOD_1 =  1,
    E_MOD_2 =  2,
    E_MOD_3 =  4,
    E_MOD_4 =  8,
    E_MOD_5 = 16,
    E_MOD_6 = 32,
    E_MOD_7 = 64,
} event_mod_t;

typedef enum {
    E_START,
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
    milliseconds_t start, end;
    event_t id;
    event_t state;
    event_mod_t mod;
    short repeat, extra;
} InputEvent;

#define MAX_INPUT_HANDLERS 16
typedef struct {
    void (*handlers[MAX_INPUT_HANDLERS])(event_t);
    int handler_c;
} Keyboard;

void keyboard_init();
int input_register_handler(void (*)(event_t));

#endif
