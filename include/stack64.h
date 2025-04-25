#ifndef STACK64_HEADER
#define STACK64_HEADER

#include "stdlib.h"
#include "stdint.h"


/* 8 Byte Stack */
typedef struct {
    uint64_t *head;
    int count, capacity;
} Stack64;

Stack64* st_init(int);
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
int qu_clear(Queue64*);

#define qu_foreach(qu, type, e)                                             \
    int __ITERATOR_1804289383 = 1;                                          \
    int __ERROR_CODE_846930886 = -1;                                        \
    for(type e = (type)                                                     \
            qu->mempool[qu->head];                                          \
            e;                                                              \
            e = (type) qu_get(                                              \
                qu, __ITERATOR_1804289383++, &__ERROR_CODE_846930886))


/* 8 Byte Hashtable */
typedef struct HashTableEntry {
    unsigned long key;
    int64_t value;
} HashTableEntry;

typedef struct HashTable {
    int count, capacity;
    struct HashTableEntry* entries;
} HashTable;

HashTable *ht_init(int);
int ht_insert(HashTable*, unsigned long, int64_t);
int64_t ht_lookup(HashTable*, unsigned long);
int ht_clear(HashTable*, unsigned long);
unsigned long ht_hash(const char*);


/* Min Heap */
typedef struct HeapNode {
    uint64_t weight, data;
} HeapNode;

typedef struct Heap {
    HeapNode *mempool;
    int count, capacity;
} Heap;

Heap *minh_init(int pages);
void minh_print(Heap*, char);
int minh_insert(Heap*, uint64_t, uint64_t);
uint64_t minh_get(Heap*, int);
uint64_t *minh_delete(Heap*, int);
uint64_t minh_pop(Heap*);


/* Priority Queue */
typedef struct PQueue64 {
    Heap heap;
    int (*heap_insert)(Heap*, uint64_t, uint64_t);
    uint64_t (*heap_pop)(Heap*);
} PQueue64;

PQueue64* pq_init(int);
int pq_enqueue(PQueue64*, void*, uint64_t);
void pq_remove(PQueue64*, void*, uint64_t);
void *pq_dequeue(PQueue64*);
uint64_t _pq_peek(PQueue64*, char);
void *pq_peek(PQueue64*);
int pq_empty(PQueue64*);
int pq_full(PQueue64*);
void pq_free(PQueue64*);

#endif
