#ifndef KEYBOARD_HEADER
#define KEYBOARD_HEADER

#define KEYS_MAX 256
#define ASCII_ESC 27

typedef enum KeyID {
    KB_UP,
    KB_DOWN,
    KB_LEFT,
    KB_RIGHT,
    KB_W,
    KB_S,
    KB_A,
    KB_D,
    KB_Q,
    KB_ESC,
} KeyID;

typedef struct {
    int keys[KEYS_MAX];
} Keyboard;

void keyboard_init();
void keyboard_poll();
void keyboard_clear();

#endif
