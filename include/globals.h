#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include <unistd.h>

#include "keyboard.h"
#include "game.h"
#include "window.h"

#define DEBUG

/* Useful Macros */
#define PAGE_SIZE getpagesize()

#define min(a, b) a <= b ? a : b
#define max(a, b) a >= b ? a : b
#define foreach(ptr, e) for (typeof(ptr->mempool) e=ptr->mempool; e < ptr->mempool + ptr->count; e++)
#define foreachn(ptr, n, e) for (typeof(ptr) e=ptr; e < ptr + n; e++)

#ifdef DEBUG
#define DEBUG_FD stderr
#define log_debug(...)                  \
    ifdef (DEBUG) {                     \
        fprintf(DEBUG_FD, __VA_ARGS__); \
        print_newln(stderr);            \
    }
#else
#define log_debug(...) {}
#endif

#define assert(condition, ...)          \
    if (!(condition)) {                 \
        log_debug(__VA_ARGS__);              \
        exit(-1);                       \
    }


struct Globals{
    int view_port_maxx, view_port_maxy;
    Keyboard keyboard;
    WindowContext window;
    Entity* player;
    GameContext* game;
};

extern struct Globals GLOBALS;

#endif
