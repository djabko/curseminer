#include "stack64.h"
#include "globals.h"

#include <string.h>
#include <stdio.h>


/* STACK FUNCTIONS */

Stack64* st_init(int pages) {
    Stack64* st = calloc(pages, PAGE_SIZE);

    uint64_t* mempool = (uint64_t*) (st + 1);
    st->head =  mempool - 1;
    st->capacity = capacity_from_pages(
            pages, sizeof(Stack64), sizeof(uint64_t));
    st->count = 0;

    return st;
}

int st_push(Stack64* st, uint64_t data) {
    if (st_full(st)) return -1;
    st->head++;
    *(st->head) = data;

    st->count++;
    return 0;
}

uint64_t st_pop(Stack64* st) {
    if (st_empty(st)) return -1;

    uint64_t data = *(st->head);
    st->head--;

    st->count--;
    return data;
}

uint64_t st_peek(Stack64* st) {
    if (st == NULL || st->head == NULL) return -1;
    return *(st->head);
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
    _log_debug("[ ");

    int i = 0;
    while (i < st->count) {
        _log_debug("%lu", *(st->head - i * sizeof(uint64_t)));
        if (i < st->count-1) _log_debug(", ");
        
        i++;
    }

    log_debug(" ]");
}


/* QUEUE FUNCTIONS */

Queue64* qu_init(int pages) {
    Queue64* qu = calloc(pages, PAGE_SIZE);

    qu->mempool = (uint64_t*)(qu+1);
    qu->count = 0;
    qu->capacity = capacity_from_pages(
            pages, sizeof(Queue64), sizeof(uint64_t));

    qu->head = -1;
    qu->tail = -1;

    return qu;
}

int qu_enqueue(Queue64* qu, uint64_t data) {
    if (qu_full(qu)) return -1;
    else if (qu_empty(qu)) {
        qu->head = 0;
        qu->tail = 0;
        qu->mempool[qu->head] = data;
    } else {
        qu->tail = (qu->tail + 1) % qu->capacity;
        qu->mempool[qu->tail] = data;
    }

    return qu->count++;
}

uint64_t qu_dequeue(Queue64* qu) {
    if (qu_empty(qu)) return -1;

    uint64_t data = qu->mempool[qu->head];
    qu->head = (qu->head + 1) % qu->capacity;
    qu->count--;

    if (qu->count <= 0) {
        qu->head = -1;
        qu->tail = -1;
    }

    return data;
}

uint64_t qu_peek(Queue64* qu) {
    return qu->mempool[qu->head];
}

uint64_t qu_peek_tail(Queue64* qu) {
    return qu->mempool[qu->tail];
}


// Returns the i'th element as offset from head, or 0
// TODO: add error handling
uint64_t qu_get(Queue64 *qu, int i, int *err) {
    if (i < 0 || qu->count <= i) return 0;
    return qu->mempool[ qu->head + i ];
}

uint64_t qu_next(Queue64* qu) {
    uint64_t data = qu_dequeue(qu);
    qu_enqueue(qu, data);
    return data;
}

int qu_empty(Queue64* qu) {
    if (qu->count == 0 || qu->head == -1 || qu->tail == -1) {
        qu->count = 0;
        return 1;
    }
    return 0;
}

int qu_full(Queue64* qu) {
    return qu->count >= qu->capacity;
}

void qu_print(Queue64* qu) {
    if (qu->count < 1) log_debug("<qu_empty>");
    for (int i = 0; i<qu->count; i++) {
        log_debug("%d: %p", i, (void*) qu->mempool[(qu->head + i) % qu->capacity]);
    }
    log_debug_nl();
}


/*
 * === HASH TABLE ===
 * Key == -1 is used to denote an empty hashtable entry.
 * Checks must be done before calling ht_insert() or cache_lookup().
 * Alternatively a hash function can be used which never returns -1
 */

unsigned long ht_hash(char* str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash;

}

HashTable *ht_init(int pages) {
    HashTable* ht = calloc(pages, PAGE_SIZE);

    ht->capacity = capacity_from_pages(
            pages, sizeof(HashTable), sizeof(HashTableEntry));
    ht->count = 0;
    ht->entries = (HashTableEntry*) (ht+1);

    for (int i = 0; i < ht->capacity; i++) {
        ht->entries[i].key = -1;
        ht->entries[i].value = -1;
    }

    return ht;
}

int ht_insert(HashTable* ht, unsigned long key, int64_t value) {
    if (ht == NULL) return -1;

    int i = key % ht->capacity;
    struct HashTableEntry* e = ht->entries + i;

    int j = 0;
    while (e->key != -1) {
        e = ht->entries + (i++ % ht->capacity);
        j++;

        if (ht->capacity < j)
            return -1;
    }

    e->key = key;
    e->value = value;

    return 1;
}

struct HashTableEntry* ht_entry(HashTable* ht, unsigned long key) {
    if (ht == NULL) return NULL;

    int i = key % ht->capacity;

    struct HashTableEntry* e = ht->entries + i;

    int j = 0;
    while (e->key != key) {
        e = ht->entries + (i++ % ht->capacity);
        j++;

        if (ht->capacity < j)
            return NULL;
    }

    return e;
}

int64_t ht_lookup(HashTable *ht, unsigned long key) {
    HashTableEntry *e = ht_entry(ht, key);

    if (!e) return -1;
    
    return e->value;

} inline int64_t ht_lookup(HashTable*, unsigned long);

int ht_clear(HashTable *ht, unsigned long key) {
    HashTableEntry *e = ht_entry(ht, key);

    if (!e) return -1;
    
    e->key = -1;

    return 1;

} inline int ht_clear(HashTable*, unsigned long);


/* Min Heap Functions */
Heap *minh_init(int pages) {
    Heap *h = calloc(pages, PAGE_SIZE);

    h->mempool = (HeapNode*) (h + 1);
    h->count = 0;
    h->capacity = capacity_from_pages(pages, sizeof(Heap), sizeof(HeapNode));

    return h;
}

void minh_print(Heap *h, char dw) {
    _log_debug("{");
    for (int i=0; i<h->count; i++) {
        HeapNode *node = h->mempool + i;
        _log_debug("%lu, ", dw ? node->data : node->weight);
    }
    log_debug("}");
}

int minh_insert(Heap *h, uint64_t data, uint64_t weight) {
    if (h->count >= h->capacity) return -1;

    int i = h->count++;
    int pi = (i - 1) / 2;

    HeapNode *node = h->mempool + i; 
    node->weight = weight;
    node->data = data;

    HeapNode *parent = h->mempool + pi;

    while (0 < i && weight < parent->weight) {
        swap(node, parent);

        i = pi;
        pi = (i - 1) / 2;
        node = parent;
        parent = h->mempool + pi;
    }

    return 1;
}

uint64_t minh_get(Heap *h, int i) {
    if (0 <= i && i <= h->count)
        return h->mempool[i].data;

    return 0;
}

uint64_t minh_pop(Heap *h) {
    if (h->count <= 0) return 0;

    uint64_t data = h->mempool[0].data;

    swap(h->mempool, (h->mempool + h->count - 1));

    h->count--;

    uint64_t w = h->mempool[0].weight;
    HeapNode *node, *child_r, *child_l, *target;

    for (int i = 0; i < h->count;) {
        int i_target, i_child_l, i_child_r;
        i_child_l = (i * 2) + 1;
        i_child_r = (i * 2) + 2;

        node = h->mempool + i;
        child_l = h->mempool + i_child_l;
        child_r = h->mempool + i_child_r;

        if (i_child_l >= h->count) break;
        else if (i_child_r >= h->count) i_target = i_child_l;
        else i_target = child_l->weight < child_r->weight ? i_child_l : i_child_r;

        target = h->mempool + i_target;

        if (target->weight < w) swap(node, target);

        i = i_target;
    }

    return data;
}


/* PRIORITY LIST */

PQueue64* pq_init(int pages) {
    PQueue64 *pq = calloc(pages, PAGE_SIZE);

    pq->heap.mempool = (HeapNode*) (pq + 1);
    pq->heap.count = 0;
    pq->heap.capacity = capacity_from_pages(
            pages, sizeof(PQueue64), sizeof(HeapNode));

    // TODO: Max Heap
    pq->heap_insert = minh_insert;
    pq->heap_pop = minh_pop;

    return pq;
}

int pq_enqueue(PQueue64* pq, void* ptr, uint64_t weight) {
    return pq->heap_insert(&pq->heap, (uint64_t) ptr, weight);
}

void pq_remove(PQueue64* pq, void* ptr, uint64_t weight) {
    // TODO: search for ptr and remove from heap
}

uint64_t _pq_peek(PQueue64* pq, char dw) {
    if (dw)
        return pq->heap.mempool[0].data;

    return pq->heap.mempool[0].weight;
}

void *pq_peek(PQueue64* pq) {
    return (void*) _pq_peek(pq, 1);
}

int pq_empty(PQueue64* pq) {
    return pq->heap.count == 0;
}

int pq_full(PQueue64* pq) {
    return pq->heap.count < pq->heap.capacity;
}

void *pq_dequeue(PQueue64 *pq) {
    return (void*) pq->heap_pop(&pq->heap);
}

void pq_free(PQueue64 *pq) {
    free(pq);
}
