#include <run_queue.h>
#include "stdio.h"


const unsigned char FLAG_RQ_BIT     = 0b00000001;
const unsigned char FLAG_RQ_KILLED  = 0b00000010;
const unsigned char FLAG_RQ_NEW     = 0b00000100;
const unsigned char FLAG_RQ_RAN     = 0b00001000;
const unsigned char FLAG_RQ_CHANGED = 0b00010000;
const unsigned char FLAG_RQ_PARAMS  = 0b00010000;
const unsigned char FLAG_RQ_CUSTOM1 = 0b01000000;
const unsigned char FLAG_RQ_CUSTOM2 = 0b10000000;

unsigned int GLOBAL_TASK_COUNT = 0;


Stack64* st_init(int capacity) {
    Stack64* st = malloc(sizeof(Stack64) + capacity * sizeof(uint64_t));
    uint64_t* mempool = (uint64_t*) (st + 1);
    st->top =  mempool - 1;
    st->capacity = capacity;
    st->count = 0;
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


Task* create_task(Task* task, int time, int (*func)(Task*, Stack64*), Stack64* stack) {
    task->flags = '\0';
    task->occupied = 1;
    task->time = time;
    task->func = func;
    task->stack = stack;
    task->next = (Task*) task;
    
    GLOBAL_TASK_COUNT++;
    return task;
}

void rm_task(Task* task) {
    task->occupied = 0;
    GLOBAL_TASK_COUNT--;
}

void kill_task(Task* task) {
    task->flags |= FLAG_RQ_KILLED;
}

RunQueue* rq_init() {
    RunQueue* rq = malloc(RQ_MEMPOOL_SIZE);
    rq->mempool = (Task*) (rq + 1);

    rq->max_task = (RQ_MEMPOOL_SIZE - sizeof(RunQueue)) / sizeof(Task);

    for (int i = 0; i < rq->max_task; i++) 
        (rq->mempool+i)->occupied = 0;

    rq->count = 0;
    rq->head = NULL;
    rq->tail = NULL;

    return rq;
}

Task* rq_add(RunQueue* rq, int time, int (*func)(Task*, Stack64*), Stack64* stack) {

    // TODO: Allocate more memory
    if (rq->count >= rq->max_task) return NULL;

    Task* t = rq->mempool;
    while (t->occupied) t++;

    create_task(t, time, func, stack);
    if (rq->head == NULL || rq->tail == NULL) {
        rq->head = t;
        rq->tail = t;

    } else {
        rq->tail->next = t;
        rq->tail = t;
    }

    t->flags |= FLAG_RQ_NEW;
    if (rq->count == 0)
        t->flags |= FLAG_RQ_BIT;

    rq->count++;
    return t;
}

int rq_pop(RunQueue* rq) {
    Task* old = rq->head;

    if (!old->occupied) return -1;
    rq->head = rq->head->next;
    rq->tail->next = rq->head;

    rq->count--;
    rm_task(old);

    return 0;
}

int rq_run(RunQueue* rq) {
    if (rq == NULL || rq->head == NULL) return -1;

    Task* current = rq->head;

    if (!current->occupied) return -1;
    
    if (current->flags & FLAG_RQ_KILLED) 
        return rq_pop(rq);

    int output = current->func(current, current->stack);
    rq->head = rq->head->next;
    rq->tail->next = current;
    rq->tail = current;

    return output;
}

