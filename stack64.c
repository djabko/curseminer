#include "stack64.h"

#include <stdio.h>


Stack64* st_init(int capacity) {
    Stack64* st = malloc(sizeof(Stack64) + capacity * sizeof(uint64_t));
    uint64_t* mempool = (uint64_t*) (st + 1);
    st->top =  mempool - 1;
    st->capacity = capacity;
    st->count = 0;

    return st;
}

int st_push(Stack64* st, uint64_t data) {
    if (st_full(st)) return -1;
    st->top++;
    *(st->top) = data;

    st->count++;
    return 0;
}

uint64_t st_pop(Stack64* st) {
    if (st_empty(st)) return -1;

    uint64_t data = *(st->top);
    st->top--;

    st->count--;
    return data;
}

uint64_t st_peak(Stack64* st) {
    if (st == NULL || st->top == NULL) return -1;
    return *(st->top);
}

int st_empty(Stack64* st) {
    if (st == NULL) return -1;

    return st->count < 1;
}

int st_full(Stack64* st) {
    if (st == NULL) return -1;

    return st->count >= st->capacity;
}

void st_destroy(Stack64* st) {
    free(st);
}

void st_print(Stack64* st) {
    if (st == NULL) return;
    printf("[ ");

    int i = 0;
    while (i < st->count) {
        printf("%x", *(st->top - i * sizeof(uint64_t)));
        if (i < st->count-1) printf(", ");
        
        i++;
    }

    printf(" ]\n");
}

