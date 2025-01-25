#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include <unistd.h>

#include "keyboard.h"
#include "game.h"
#include "window.h"

#define PAGE_SIZE getpagesize()

struct Globals{
    int view_port_maxx, view_port_maxy;
    Keyboard keyboard;
    WindowContext window;
    Entity* player;
    GameContext* game;
};

extern struct Globals GLOBALS;

#endif
