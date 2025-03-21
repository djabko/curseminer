#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include <unistd.h>

typedef unsigned char byte;

#include "input.h"
#include "game.h"

/* Useful Macros */
#define PAGE_SIZE getpagesize()

#define min(a, b) a <= b ? a : b
#define max(a, b) a >= b ? a : b
#define swap(a, b) do {typeof(*a) tmp = *a; *a = *b; *b = tmp;} while (0);
#define foreach(ptr, e) for (typeof(ptr->mempool) e=ptr->mempool; e < ptr->mempool + ptr->count; e++)
#define foreachn(ptr, n, e) for (typeof(ptr) e=ptr; e < ptr + n; e++)

#ifdef DEBUG

#define DEBUG_FD stderr
#define log_debug_nl() fprintf(DEBUG_FD, "\n")
#define _log_debug(...)   fprintf(DEBUG_FD, __VA_ARGS__)
#define log_debug(...)                      \
    do {                                    \
    fprintf(DEBUG_FD, __VA_ARGS__);         \
    log_debug_nl();                         \
    } while (0)                                       

#define assert(condition) \
    if (!(condition)) {exit(-1);}

#define assert_log(condition, ...)          \
    if (!(condition)) {                     \
        _log_debug("ERROR: ");              \
        log_debug(__VA_ARGS__);             \
        exit(-1);                           \
    }

#else

#define log_debug_nl()
#define _log_debug(...)
#define log_debug(...)
#define assert(condition)
#define assert_log(condition, ...)
#endif

#define E_CTX_GAME E_CTX_0
#define E_CTX_NOISE E_CTX_1
#define E_CTX_CLOCK E_CTX_2

struct Globals{
    int view_port_x, view_port_y, view_port_maxx, view_port_maxy;
    Entity* player;
    GameContext* game;
    event_ctx_t input_context;
};

extern struct Globals GLOBALS;

#endif
