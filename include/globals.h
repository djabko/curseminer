#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include <unistd.h>

#include "keyboard.h"
#include "game.h"
#include "window.h"

#define PAGE_SIZE getpagesize()

struct Globals{
    Keyboard keyboard;
    WindowContext window;
    Entity* player;
    GameContext* game;
};

extern struct Globals GLOBALS;

#endif
