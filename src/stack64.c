#include "stack64.h"
#include "globals.h"

#include <string.h>
#include <stdio.h>


/* STACK FUNCTIONS */

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

uint64_t st_peek(Stack64* st) {
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
    _log_debug("[ ");

    int i = 0;
    while (i < st->count) {
        _log_debug("%lu", *(st->top - i * sizeof(uint64_t)));
        if (i < st->count-1) _log_debug(", ");
        
        i++;
    }

    log_debug(" ]");
}


/* PRIORITY LIST */

PQueue64* pq_init(int pages) {
    PQueue64* pq = calloc(PAGE_SIZE, pages);

    pq->mempool = (PriNode*) (pq + 1);
    pq->head = NULL;
    pq->tail = NULL;
    pq->count = 0;
    pq->capacity = (PAGE_SIZE * pages - sizeof(PQueue64)) / sizeof(PriNode);
    pq->free = st_init(pq->capacity);

    for (int i = 0; i < pq->capacity; i++)
        st_push(pq->free, (uint64_t) (pq->mempool + i));

    return pq;
}

PriNode* pq_get_free(PQueue64 *pq) {
    return (PriNode*) st_pop(pq->free);
}

PriNode* _pq_search(PQueue64 *pq, uint64_t weight, int rprev) {
    PriNode *prev = NULL;
    PriNode *node = pq->head;

    while (node && node->weight < weight) {
        prev = node;
        node = node->next;
    }

    return rprev ? prev : node;
}

PriNode* pq_search(PQueue64 *pq, uint64_t weight) {
    return _pq_search(pq, weight, 0);
}

PriNode* pq_search_prev(PQueue64 *pq, uint64_t weight) {
    return _pq_search(pq, weight, 1);
}

int pq_enqueue(PQueue64* pq, void* ptr, uint64_t weight) {
    if (pq->count >= pq->capacity) return -1;

    PriNode *new = pq_get_free(pq);

    // New head
    if (pq->count == 0) {
        pq->head = new;
        pq->tail = new;
        new->next = NULL;

    } else {
        PriNode *prev, *next;
        prev = pq_search_prev(pq, weight);;

        // Insert after head
        if (prev) {
            next = prev->next;
            prev->next = new;

        // Insert at head
        } else {
            next = pq->head;
            pq->head = new;
        }
        
        if (!next) pq->tail = new;
        
        new->next = next;
    }

    new->ptr = ptr;
    new->weight = weight;

    pq->count++;
    return 0;
}

void pq_remove(PQueue64* pq, void* ptr, uint64_t weight) {
    PriNode *prev = NULL;
    PriNode *node = pq->head;

    while (node && node->ptr != ptr) {
        prev = node;
        node = node->next;
    }

    if (!node) return;

    if (prev) prev->next = node->next;

    memset(node, 0, sizeof(PriNode));
    pq->count--;
    st_push(pq->free, (uint64_t) node);
}

void *pq_peek(PQueue64 *pq) {
    return pq->head ? pq->head->ptr : NULL;
}

void *pq_dequeue(PQueue64 *pq) {
    PriNode *old = pq->head;
    pq->head = old->next;

    void *ptr = old->ptr;
    memset(old, 0, sizeof(PriNode));
    pq->count--;
    st_push(pq->free, (uint64_t) old);

    return ptr;
}

void pq_free(PQueue64* pq) {
    free(pq);
}


/* QUEUE FUNCTIONS */

Queue64* qu_init(int n) {
    Queue64* qu = malloc(PAGE_SIZE * n);
    qu->mempool = (uint64_t*)(qu+1);
    qu->count = 0;
    qu->capacity = (PAGE_SIZE * n - sizeof(Queue64)) / sizeof(uint64_t);
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

HashTable *ht_init(int size) {
    HashTable* ht = calloc(sizeof(HashTable) + size * sizeof(HashTableEntry), 1);
    ht->max = size;
    ht->count = 0;
    ht->entries = (HashTableEntry*) (ht+1);

    for (int i=0; i<size; i++) {
        ht->entries[i].key = -1;
        ht->entries[i].value = -1;
    }

    return ht;
}

int ht_insert(HashTable* ht, unsigned long key, int64_t value) {
    if (ht == NULL) return -1;

    int i = key % ht->max;
    struct HashTableEntry* e = ht->entries + i;

    int j = 0;
    while (e->key != -1) {
        e = ht->entries + (i++ % ht->max);
        j++;

        if (ht->max < j)
            return -1;
    }

    e->key = key;
    e->value = value;

    return 1;
}

struct HashTableEntry* ht_entry(HashTable* ht, unsigned long key) {
    if (ht == NULL) return NULL;

    int i = key % ht->max;

    struct HashTableEntry* e = ht->entries + i;

    int j = 0;
    while (e->key != key) {
        e = ht->entries + (i++ % ht->max);
        j++;

        if (ht->max < j)
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

