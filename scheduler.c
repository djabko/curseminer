#include <scheduler.h>
#include <globals.h>

#include "sys/time.h"
#include "stdio.h"


const unsigned char FLAG_RQ_BIT         = 0b00000001;
const unsigned char FLAG_RQ_KILLED      = 0b00000010;
const unsigned char FLAG_RQ_SLEEPING    = 0b00000100;
const unsigned char FLAG_RQ_RAN         = 0b00001000;
const unsigned char FLAG_RQ_CHANGED     = 0b00010000;
const unsigned char FLAG_RQ_PARAMS      = 0b00100000;
const unsigned char FLAG_RQ_NEW         = 0b01000000;
const unsigned char FLAG_RQ_CUSTOM2     = 0b10000000;

unsigned int GLOBAL_TASK_COUNT = 0;
SleepingRunQueue* g_sleeping_rq;

Task* create_task(Task* task, TimeStamp* starttime, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {
    task->flags = '\0';
    task->occupied = 1;
    task->func = func;
    task->stack = stack;
    task->next = (Task*) task;

    if (starttime == NULL) gettimeofday(&(task->next_run), NULL);
    else task->next_run = *starttime;

    if (runtime < 1) timer_never(&(task->kill_time));
    else {
        gettimeofday(&(task->kill_time), NULL); 
        task->kill_time.tv_sec += runtime;
    }
    
    GLOBAL_TASK_COUNT++;
    return task;
}

void rm_task(Task* task) {
    task->occupied = 0;
    GLOBAL_TASK_COUNT--;
}

int tk_kill(Task* task) {
    task->flags |= FLAG_RQ_KILLED;
    return 0;
}

RunQueue* rq_init() {
    RunQueue* rq = malloc(RQ_MEMPOOL_SIZE);
    rq->mempool = (Task*) (rq + 1);

    rq->max = (RQ_MEMPOOL_SIZE - sizeof(RunQueue)) / sizeof(Task);
    //rq->max = ((RQ_MEMPOOL_SIZE - sizeof(RunQueue)) / (sizeof(Task) * sizeof(Task*)));

    for (int i = 0; i < rq->max; i++) 
        (rq->mempool+i)->occupied = 0;

    rq->count = 0;
    rq->head = NULL;
    rq->tail = NULL;
    rq->next = NULL;

    return rq;
}

int rq_empty(RunQueue* rq) {
    return rq->count <= 0;
}

int rq_full(RunQueue* rq) {
    return rq->max <= rq->count;
}

Task* rq_add(RunQueue* rq, TimeStamp* starttime, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {

    // TODO: Allocate more memory
    if (rq == NULL || rq_full(rq)) return NULL;

    Task* t = rq->mempool;
    while (t->occupied) t++;

    create_task(t, starttime, runtime, func, stack);
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
    if (rq == NULL || rq_empty(rq)) return -1;
    Task* old = rq->head;

    if (!old->occupied) return -1;

    if (rq->head == rq->tail) {
        rq->head = NULL;
        rq->tail = NULL;

    } else {
        rq->head = rq->head->next;
        rq->tail->next = rq->head;
    }

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

    //printf("Running task %x; sleeping: %d\n", current, current->flags & FLAG_RQ_SLEEPING);
    if (0 == (current->flags & FLAG_RQ_SLEEPING))
        current->func(current, current->stack);
    rq->head = rq->head->next;
    rq->tail->next = current;
    rq->tail = current;

    return 0;
}

int rq_init_sleeping() {
    g_sleeping_rq = (SleepingRunQueue*) malloc(PAGE_SIZE);

    if (g_sleeping_rq == NULL) return -1;
    int max = (PAGE_SIZE - sizeof(SleepingRunQueue)) / sizeof(SleepingTask);

    g_sleeping_rq->mempool = (SleepingTask*) (g_sleeping_rq + 1);
    g_sleeping_rq->head = NULL;
    g_sleeping_rq->tail = NULL;


    for (int i=0; i<max; i++) {
        g_sleeping_rq->mempool[i].task = NULL;
        g_sleeping_rq->mempool[i].next = NULL;
    }

    return max;
}

int rq_sleeping_add(Task* tk) {
    if (tk == NULL || g_sleeping_rq == NULL) return -1;
    
    SleepingTask *stk = g_sleeping_rq->mempool;

    while (stk->task != NULL) stk++;

    stk->task = tk;

    if (g_sleeping_rq->head == NULL || g_sleeping_rq->tail == NULL) {
        g_sleeping_rq->head = stk;
        g_sleeping_rq->tail = stk;

    } else {
        g_sleeping_rq->tail->next = stk;
        g_sleeping_rq->tail = stk;
    }

    return 0;
}

int rq_sleeping_rm(SleepingTask* stk) {
    if (g_sleeping_rq == NULL || stk == NULL) return -1;

    SleepingTask* prev = NULL;

    if (prev == NULL) g_sleeping_rq->head = stk->next;
    else prev->next = stk->next;

    stk->task = NULL;

    return 0;
}

int rq_free_sleeping() {
    free(g_sleeping_rq);
}

void tk_sleep(Task* task, unsigned int seconds) {
    TimeStamp ts;
    gettimeofday(&ts, NULL);
    ts.tv_sec += seconds;

    task->next_run = ts;
    task->flags |= FLAG_RQ_SLEEPING;
    rq_sleeping_add(task);
}

RunQueueList* rqll_init() {
    RunQueueList* rqll = malloc(LLRQ_MEMPOOL_SIZE);
    rqll->mempool = (RunQueue**) (rqll + 1);

    rqll->max = (LLRQ_MEMPOOL_SIZE - sizeof(RunQueueList)) / sizeof(RunQueue*);
    rqll->head = NULL;
    rqll->count = 0;

    for (int i=0; i<rqll->max; i++) rqll->mempool[i] = NULL;

    return rqll;
}

RunQueue* rqll_add(RunQueueList* rqll) {
    if (rqll_full(rqll)) return NULL;

    RunQueue* rq_ptr = rqll->mempool[0];
    RunQueue* lastblock = rqll->mempool[ rqll->max - 1 ];
    while (rq_ptr != NULL && rq_ptr < lastblock) rq_ptr++;
    if (rq_ptr != NULL) return NULL;

    rq_ptr = rq_init();

    RunQueue* prev = rqll->head;
    if (prev == NULL) {
        rqll->head = rq_ptr;
    } else {
        while (prev->next != NULL) prev = prev->next;
        prev->next = rq_ptr;
    }
    rqll->count++;

    return rq_ptr;
}

int rqll_rm(RunQueueList* rqll, RunQueue* target) {
    if (target == NULL) return -1;

    RunQueue* rq_ptr = rqll->mempool[0];
    RunQueue* lastblock = rqll->mempool[0] + rqll->max - 1;
    while (rq_ptr != target && rq_ptr < lastblock) rq_ptr++;

    if (rq_ptr != target) return -1;
    free(rq_ptr);

    rq_ptr = NULL;
    return 0;
}

int rqll_empty(RunQueueList* rqll) {
    return rqll->count <= 0;
}

int rqll_full(RunQueueList* rqll) {
    return rqll->count >= rqll->max;
}


int wake_tasks() {
    SleepingTask* stk = g_sleeping_rq->head;
    int i = 0;
    while (stk != NULL) {
        Task* tk = stk->task;
        if (timer_nready(&(tk->next_run))) {
            tk->flags &= !FLAG_RQ_SLEEPING;
            SleepingTask* next = stk->next;
            rq_sleeping_rm(stk);
            stk = next;

        } else {
            stk = stk->next;
        }

        i++;
    }
    return i;
}

// Linear search, very slow
int schedule(RunQueue* rq, TimeStamp* starttime, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {
    Task* t = rq_add(rq, starttime, runtime, func, stack);

    return (t != NULL) - 1;
}

void schedule_run(RunQueueList* rqll) {
    int tasks_in_queue = 1;

    while (tasks_in_queue) {
        tasks_in_queue = 0;

        RunQueue* rq = rqll->head;
        while (rq != NULL) {
            int status = rq_run(rq);
            tasks_in_queue += status == 0;
            rq = rq->next;
        }

        timer_synchronize();
    }
}


