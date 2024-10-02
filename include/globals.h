#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include <unistd.h>

#include <keyboard.h>

#define PAGE_SIZE getpagesize()

typedef struct {
    Keyboard keyboard;
} GlobalType;

extern GlobalType GLOBALS;

#endif
