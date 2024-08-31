#ifndef RUN_QUEUE_HEADER
#define RUN_QUEUE_HEADER

#include <timer.h>
#include <stack64.h>

#define RQ_MEMPOOL_SIZE 4096 * 16
#define LLRQ_MEMPOOL_SIZE 4096 * 1

const extern unsigned char FLAG_RQ_BIT;
const extern unsigned char FLAG_RQ_KILLED;
const extern unsigned char FLAG_RQ_NEW;
const extern unsigned char FLAG_RQ_RAN;
const extern unsigned char FLAG_RQ_CHANGED;
const extern unsigned char FLAG_RQ_PARAMS;
const extern unsigned char FLAG_RQ_CUSTOM1;
const extern unsigned char FLAG_RQ_CUSTOM2;

extern unsigned int GLOBAL_TASK_COUNT;


/* Task */
typedef struct Task {
    unsigned char flags, occupied, byte2, byte3;
    int (*func) (struct Task*, Stack64*);
    Stack64* stack;
    TimeStamp next_run, kill_time;
    struct Task* next;
} Task;

int tk_kill(Task*);


/* RunQueue*/
typedef struct RunQueue {
    Task *mempool, *head, *tail;
    unsigned int count, max;
    struct RunQueue *next;
} RunQueue;

RunQueue* rq_init();
Task* rq_add(RunQueue*, TimeStamp*, int, int (*func)(Task*, Stack64*), Stack64*);
int rq_pop(RunQueue*);
int rq_run(RunQueue*);


/* Linked List of run queues*/
typedef struct RunQueueList {
    RunQueue *head, **mempool;
    int count, max;
} RunQueueList;

RunQueueList* rqll_init();
RunQueue* rqll_add(RunQueueList*);
int rqll_rm(RunQueueList*, RunQueue*);
int rqll_empty(RunQueueList*);
int rqll_full(RunQueueList*);

int schedule (RunQueue*, TimeStamp*, int, int (*func)(Task*, Stack64*), Stack64*);
void schedule_run (RunQueueList*);

#endif
