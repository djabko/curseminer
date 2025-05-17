#ifndef RUN_QUEUE_HEADER
#define RUN_QUEUE_HEADER

#include "curseminer/time.h"
#include "curseminer/stack64.h"

#define RQ_MEMPOOL_SIZE 4096 * 1

typedef unsigned char byte_t;

const extern unsigned char RQ_FLAG_BIT;
const extern unsigned char RQ_FLAG_KILLED;
const extern unsigned char RQ_FLAG_SLEEPING;
const extern unsigned char RQ_FLAG_NEW;
const extern unsigned char RQ_FLAG_RAN;
const extern unsigned char RQ_FLAG_CHANGED;
const extern unsigned char RQ_FLAG_PARAMS;
const extern unsigned char RQ_FLAG_CUSTOM2;

extern unsigned int GLOBAL_TASK_COUNT;

typedef struct ll_node {
    struct ll_node *next;
    void* data; 
} ll_node;

typedef struct ll_head {
    ll_node *mempool, *node, *tail;
    unsigned int count, max;
} ll_head;


ll_head* ll_init(int);
int ll_insert(ll_head*, void*);
int ll_rm(ll_head*, void*);
int ll_count(ll_head*);
int ll_empty(ll_head*);
int ll_full(ll_head*);
void ll_free(ll_head*);


/* Task */
struct RunQueue;
typedef struct Task {
    byte_t flags, occupied, extra, extra2;
    int (*func) (struct Task*, Stack64*);
    void (*callback) (struct Task*);

    struct RunQueue *runqueue;
    Stack64 *stack;
    milliseconds_t next_run, kill_time;

    struct Task* next;
} Task;

void tk_sleep(Task*, milliseconds_t);
int tk_kill(Task*);
int tk_kill_all();


/* RunQueue*/
typedef struct RunQueue {
    Task *mempool, *head, *tail;
    struct RunQueue *next;

    unsigned int count, max, running;
    int lock;
} RunQueue;

int rq_kill(RunQueue*);


/* Linked List of run queues*/
typedef struct RunQueueList {
    RunQueue *head, **mempool;
    int count, max;
} RunQueueList;

int rqll_kill(ll_head*);


/* Scheduler Functions */
ll_head *scheduler_init();
void scheduler_free();
RunQueue* scheduler_new_rq();
RunQueue* scheduler_new_rq_(ll_head* rqll);
RunQueueList *scheduler_new_rqll();
int scheduler_wake_tasks();
int scheduler_kill_all_tasks();

int schedule(RunQueue*, int, int, int (*func)(Task*, Stack64*), Stack64*);
int schedule_cb(RunQueue*, int, int, int (*func)(Task*, Stack64*), Stack64*, void (*callback)(Task*));
void schedule_run(ll_head*);

#endif
