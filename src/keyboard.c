#include <stdlib.h>
#include <stdio.h>

#include "ncurses.h"

#include "globals.h"
#include "keyboard.h"

#define NCURSES_KEYS_MAX 1024
int* KEYMAP;

void keyboard_init() {
    KEYMAP = calloc(NCURSES_KEYS_MAX, sizeof(KeyState));

    KEYMAP[ KEY_LEFT ]  = KB_LEFT;
    KEYMAP[ KEY_RIGHT ] = KB_RIGHT;
    KEYMAP[ KEY_UP ]    = KB_UP;
    KEYMAP[ KEY_DOWN ]  = KB_DOWN;
    KEYMAP[ 'w' ]       = KB_W;
    KEYMAP[ 's' ]       = KB_S;
    KEYMAP[ 'a' ]       = KB_A;
    KEYMAP[ 'd' ]       = KB_D;
    KEYMAP[ 'c' ]       = KB_C;
    KEYMAP[ 'g' ]       = KB_G;
    KEYMAP[ 'q' ]       = KB_Q;
    KEYMAP[ ASCII_ESC ] = KB_ESC;

    for (KeyID i=0; i< KB_END; i++) {
        KeyState* ks = &GLOBALS.keyboard.keys[i];

        ks->id = i;
        timer_never( &(ks->last_pressed) );
        ks->down = 0;
        ks->held = 0;
        
        GLOBALS.keyboard.keys[i].id = i;
    }
}

// Each string must correspond by position to it's KeyID enum
const char* KEYID_STR_MAP[] = {
    "^",
    "up_arrow",
    "down_arrow",
    "left_arrow",
    "right_arrow",
    "W_key",
    "S_key",
    "A_key",
    "D_key",
    "C_key",
    "G_key",
    "Q_key",
    "esc_key",
    "$"
};

void keyboard_poll() {
    int c = getch();
    KeyState* ks = NULL;

    if (c != -1) {
        ks = &GLOBALS.keyboard.keys[ KEYMAP[c] ];

        ks->held = ks->down; // If it was already pressed, a subsequent signal means it's being held continuously
        ks->down = 1;

        ks->last_pressed = TIMER_NOW;
    }

    KeyState* ptr_s = GLOBALS.keyboard.keys + KB_START + 1;
    KeyState* ptr_e = GLOBALS.keyboard.keys + KB_END;

    for (ks = ptr_s; ks < ptr_e; ks++) {
        if (100 < timer_diff_millisec(&TIMER_NOW, &(ks->last_pressed))) {
            ks->down = 0;
            ks->held = 0;
        }
    }
}

const char* keyid_to_string(KeyID id) {
    return KEYID_STR_MAP[id];
}

int kb_down(KeyID id) {
    return GLOBALS.keyboard.keys[id].down;
}

