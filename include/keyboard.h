#ifndef KEYBOARD_HEADER
#define KEYBOARD_HEADER

#define ASCII_ESC 27

#include "timer.h"

typedef enum KeyID {
    KB_START,
    KB_UP,
    KB_DOWN,
    KB_LEFT,
    KB_RIGHT,
    KB_W,
    KB_S,
    KB_A,
    KB_D,
    KB_C,
    KB_Q,
    KB_ESC,
    KB_END,
} KeyID;

typedef struct {
    KeyID id;
    TimeStamp last_pressed;
    int down, held;
} KeyState;

typedef struct {
    KeyState keys[KB_END];
} Keyboard;

const char* keyid_to_string(KeyID);
void keyboard_init();
void keyboard_poll();
void keyboard_clear();
int kb_down(KeyID);

#endif
