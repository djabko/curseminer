#ifndef STACK64_HEADER
#define STACK64_HEADER

#include "stdlib.h"
#include "stdint.h"


/* 8 Byte Stack */
typedef struct {
    uint64_t *top;
    int capacity, count;
} Stack64;

Stack64* st_init(int capacity);
int st_push(Stack64* st, uint64_t data);
uint64_t st_pop(Stack64*);
uint64_t st_peak(Stack64*);
int st_empty(Stack64*);
int st_full(Stack64*);
void st_destroy(Stack64*);
void st_print(Stack64*);

#endif
