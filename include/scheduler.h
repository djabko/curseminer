#ifndef RUN_QUEUE_HEADER
#define RUN_QUEUE_HEADER

#include <timer.h>
#include <stack64.h>

#define RQ_MEMPOOL_SIZE 4096 * 16

const extern unsigned char FLAG_RQ_BIT;
const extern unsigned char FLAG_RQ_KILLED;
const extern unsigned char FLAG_RQ_SLEEPING;
const extern unsigned char FLAG_RQ_NEW;
const extern unsigned char FLAG_RQ_RAN;
const extern unsigned char FLAG_RQ_CHANGED;
const extern unsigned char FLAG_RQ_PARAMS;
const extern unsigned char FLAG_RQ_CUSTOM2;

extern unsigned int GLOBAL_TASK_COUNT;

// TODO: typedef ll_head as rqll_head for clarity

// TODO: move to utils.h
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
typedef struct Task {
    unsigned char flags, occupied, byte2, byte3;
    int (*func) (struct Task*, Stack64*);
    void (*callback) (struct Task*);
    Stack64 *stack;
    TimeStamp next_run, kill_time;
    struct Task* next;
} Task;

int tk_kill(Task*);

/* RunQueue*/
typedef struct RunQueue {
    Task *mempool, *head, *tail;
    struct RunQueue *next;

    unsigned int count, max;
    int lock;
} RunQueue;


RunQueue* rq_init();
Task* rq_add(RunQueue*, int, int, int (*func)(Task*, Stack64*), Stack64*, void (*callback)(Task*));
int rq_pop(RunQueue*);
int rq_run(RunQueue*);
int rq_kill_all_tasks(RunQueue*);


/* Linked List of run queues*/
typedef struct RunQueueList {
    RunQueue *head, **mempool;
    int count, max;
} RunQueueList;

ll_head* scheduler_init();
void scheduler_free();
void scheduler_free_rqll();
int rqll_kill_all_tasks(ll_head*);
int kill_all_tasks();
RunQueue* scheduler_new_rq_(ll_head*);
RunQueue* scheduler_new_rq();

int schedule (RunQueue*, int, int, int (*func)(Task*, Stack64*), Stack64*);
int schedule_cb(RunQueue*, int, int, int (*func)(Task*, Stack64*), Stack64*, void (*callback)(Task*));
void schedule_run (ll_head*);


void tk_sleep(Task*, unsigned int);
int wake_tasks();
int kill_dying_tasks();



#endif
