#include <stdlib.h>

#include <globals.h>
#include <keyboard.h>
#include <ncurses.h>
#include <stdio.h>

int* KEYMAP;

void keyboard_init() {
    keyboard_clear();
    KEYMAP = calloc(1024, sizeof(int));

    KEYMAP[ KEY_LEFT ]  = KB_LEFT;
    KEYMAP[ KEY_RIGHT ] = KB_RIGHT;
    KEYMAP[ KEY_UP ]    = KB_UP;
    KEYMAP[ KEY_DOWN ]  = KB_DOWN;
    KEYMAP[ 'W' ]       = KB_W;
    KEYMAP[ 'w' ]       = KB_W;
    KEYMAP[ 'S' ]       = KB_S;
    KEYMAP[ 's' ]       = KB_S;
    KEYMAP[ 'A' ]       = KB_A;
    KEYMAP[ 'a' ]       = KB_A;
    KEYMAP[ 'D' ]       = KB_D;
    KEYMAP[ 'd' ]       = KB_D;
    KEYMAP[ 'Q' ]       = KB_Q;
    KEYMAP[ 'q' ]       = KB_Q;
    KEYMAP[ ASCII_ESC ] = KB_ESC;
}

void keyboard_poll() {
    int c = getch();
    if (0 <= c)
        GLOBALS.keyboard.keys[ KEYMAP[c] ] = 1;
}

void keyboard_clear() {
    for (int i=0; i<KEYS_MAX; i++) {
        GLOBALS.keyboard.keys[i] = 0;
    }
}

