#ifndef RUN_QUEUE
#define RUN_QUEUE

#include "stdlib.h"
#include "stdint.h"

#define RQ_MEMPOOL_SIZE 4096 * 16

const extern unsigned char FLAG_RQ_BIT;
const extern unsigned char FLAG_RQ_KILLED;
const extern unsigned char FLAG_RQ_NEW;
const extern unsigned char FLAG_RQ_RAN;
const extern unsigned char FLAG_RQ_CHANGED;
const extern unsigned char FLAG_RQ_PARAMS;
const extern unsigned char FLAG_RQ_CUSTOM1;
const extern unsigned char FLAG_RQ_CUSTOM2;

extern unsigned int GLOBAL_TASK_COUNT;


typedef struct {
    uint64_t *top;
    int capacity, count;
} Stack64;

Stack64* st_init(int capacity);
int st_push(Stack64* st, uint64_t data);
uint64_t st_pop(Stack64*);
int st_empty(Stack64*);
int st_full(Stack64*);
void st_destroy(Stack64*);
void st_print(Stack64*);

typedef struct Task {
    unsigned char flags, occupied, byte2, byte3;
    int time;
    int (*func) (struct Task*, Stack64*);
    Stack64* stack;
    struct Task* next;
} Task;

typedef struct {
    Task *mempool, *head, *tail;
    unsigned int count, max_task;
} RunQueue;

void kill_task(Task*);
RunQueue* rq_init();
Task* rq_add(RunQueue*, int, int (*func)(Task*, Stack64*), Stack64*);
int rq_pop(RunQueue*);
int rq_run(RunQueue*);

#endif
