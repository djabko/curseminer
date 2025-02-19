#ifndef STACK64_HEADER
#define STACK64_HEADER

#include "stdlib.h"
#include "stdint.h"


/* Double Linked List */
typedef struct Node {
    struct Node *prev, *next;
} Node;

// TODO: implement utility functions for linked lists
typedef struct {
    Node *head, *tail;
} LLHead;


/* 8 Byte Stack */
typedef struct {
    uint64_t *top;
    int capacity, count;
} Stack64;

Stack64* st_init(int capacity);
int st_push(Stack64*, uint64_t);
uint64_t st_pop(Stack64*);
uint64_t st_peek(Stack64*);
int st_empty(Stack64*);
int st_full(Stack64*);
void st_free(Stack64*);
void st_print(Stack64*);


/* 8 Byte Queue */
typedef struct {
    uint64_t *mempool;
    int head, tail, capacity, count;
} Queue64;

Queue64* qu_init(int n);
int qu_enqueue(Queue64*, uint64_t);
uint64_t qu_dequeue(Queue64*);
uint64_t qu_peek(Queue64*);
uint64_t qu_peek_tail(Queue64*);
uint64_t qu_get(Queue64*, int, int*);
uint64_t qu_next(Queue64*);
int qu_empty(Queue64*);
int qu_full(Queue64*);
void qu_print(Queue64*);

#define qu_foreach(qu, type, e)         \
    int __ITERATOR_1804289383 = 1;      \
    int __ERROR_CODE_846930886 = -1;    \
    for(type e = (type) qu->mempool[qu->head]; e; e = (type) qu_get(qu, __ITERATOR_1804289383++, &__ERROR_CODE_846930886))


/* 8 Byte Hashtable */
typedef struct HashTableEntry {
    unsigned long key;
    int64_t value;
} HashTableEntry;

typedef struct HashTable {
    int count, max;
    struct HashTableEntry* entries;
} HashTable;

HashTable *ht_init(int size);
int ht_insert(HashTable*, unsigned long, int64_t);
int64_t ht_lookup(HashTable*, unsigned long);

#endif
