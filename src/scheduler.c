#include <sys/time.h>
#include <stdio.h>

#include "globals.h"
#include <scheduler.h>


const unsigned char RQ_FLAG_BIT         = 0b00000001;
const unsigned char RQ_FLAG_KILLED      = 0b00000010;
const unsigned char RQ_FLAG_SLEEPING    = 0b00000100;
const unsigned char RQ_FLAG_RAN         = 0b00001000;
const unsigned char RQ_FLAG_CHANGED     = 0b00010000;
const unsigned char RQ_FLAG_PARAMS      = 0b00100000;
const unsigned char RQ_FLAG_NEW         = 0b01000000;
const unsigned char RQ_FLAG_CUSTOM2     = 0b10000000;

unsigned int GLOBAL_TASK_COUNT = 0;

static ll_head* g_default_rqll = NULL;
static ll_head* g_dying_tasks = NULL;
static PQueue64* g_sleep_queue = NULL;


static Task* create_task(Task* task, RunQueue* rq, int delay, int runtime,
        int (*func)(Task*, Stack64*), Stack64* stack, void (*callback)(Task*)) {

    task->flags = '\0';
    task->occupied = 1;
    task->func = func;
    task->stack = stack;
    task->callback = callback;
    task->next = (Task*) task;
    task->next_run = 0;
    task->runqueue = rq;

    if (runtime < 1) task->kill_time = 0;
    else {
        task->kill_time = TIMER_NOW_MS + runtime;
        ll_insert(g_dying_tasks, task);
    }
    
    tk_sleep(task, delay);
    GLOBAL_TASK_COUNT++;

    return task;
}

static void rm_task(Task* task) {
    task->occupied = 0;
    GLOBAL_TASK_COUNT--;
}

int tk_kill(Task* task) {
    task->flags |= RQ_FLAG_KILLED;
    return 0;
}

RunQueue* rq_init() {
    RunQueue* rq = malloc(RQ_MEMPOOL_SIZE);
    rq->mempool = (Task*) (rq + 1);

    rq->max = (RQ_MEMPOOL_SIZE - sizeof(RunQueue)) / sizeof(Task);

    for (int i = 0; i < rq->max; i++) 
        (rq->mempool+i)->occupied = 0;

    rq->count = 0;
    rq->running = 0;
    rq->lock = 0;
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

void rq_add(RunQueue* rq, Task *tk) {
    if (rq->head == NULL || rq->tail == NULL) {
        rq->head = tk;
        rq->tail = tk;

    } else {
        tk->next = rq->head;
        rq->tail->next = tk;
        rq->tail = tk;
    }

    rq->running++;
}

Task* rq_create(RunQueue* rq, int delay, int runtime, int
        (*func)(Task*, Stack64*), Stack64* stack, void (*callback)(Task*)) {

    // TODO: Allocate more memory
    if (rq == NULL || rq_full(rq)) return NULL;

    Task* tk = rq->mempool;
    while (tk->occupied) tk++;

    create_task(tk, rq, delay, runtime, func, stack, callback);
    rq_add(rq, tk);

    rq->count++;

    return tk;
}

Task *rq_pop(RunQueue* rq) {
    if (rq == NULL || rq_empty(rq)) return NULL;

    Task* old = rq->head;

    if (!old->occupied) return NULL;

    if (rq->head == rq->tail) {
        rq->head = NULL;
        rq->tail = NULL;

    } else {
        rq->head = rq->head->next;
        rq->tail->next = rq->head;
    }

    rq->running--;

    return old;
}

int rq_poprm(RunQueue* rq) {
    Task *tk = rq_pop(rq);

    rq->count--;
    rm_task(tk);

    return 0;
}

int rq_run(RunQueue* rq) {
    if (rq == NULL || rq->head == NULL) return -1;

    Task* current = rq->head;

    if (!current->occupied) return -1;

    if (current->flags & RQ_FLAG_KILLED) {

        if (current->callback)
            current->callback(current);

        rq_poprm(rq);
        return 1;
    }

    current->func(current, current->stack);

    // Unlink from other tasks if sleeping
    if (current->flags & RQ_FLAG_SLEEPING) rq_pop(rq);
    else {
        rq->head = rq->head->next;
        rq->tail->next = current;
        rq->tail = current;
    }

    return 0;
}

void tk_sleep(Task* task, unsigned int miliseconds) {
    if (miliseconds < 1) return;

    task->next_run = TIMER_NOW_MS + miliseconds;

    task->flags |= RQ_FLAG_SLEEPING;
    pq_enqueue(g_sleep_queue, task, task->next_run);

    // Unlink task from runqueue
}

ll_head* scheduler_init() {
    ll_head* rqll = ll_init(1);
    
    // Initialize internal ll of sleeping tasks
    if (g_sleep_queue == NULL) {
        g_sleep_queue = pq_init(1);
    }

    // Initialize internal ll of dying tasks
    if (g_dying_tasks == NULL) {
        g_dying_tasks = ll_init(1);
    }

    g_default_rqll = rqll;
    return rqll;
}

void scheduler_free() {
    if (g_sleep_queue != NULL)
        pq_free(g_sleep_queue);

    if (g_dying_tasks != NULL)
        free(g_dying_tasks);
}

// TODO: Keep track of all rqll's via global variable
void scheduler_free_rqll(ll_head* rqll) {
    if (rqll == NULL) return;

    ll_node* node = rqll->node;

    if (node == NULL || node->data == NULL) return;
    RunQueue* rq = (RunQueue*) node->data;

    while (rq != NULL) {
        RunQueue* prev = rq;
        rq = rq->next;
        free(prev);
        prev = rq;
    }

    free(rqll);
    rqll = NULL;
}


/* TODO: Rewrite tasks & runqueues to use ll_head
 *  This is a mess because rqll is ll_head and RunQueue is a separate linked
 *  list implementation; first the RunQueue is added to rqll list, then it must
 *  be added to the RunQueue list
 */
RunQueue* scheduler_new_rq_(ll_head* rqll) {

    // 1. Insert into rqll (type ll_head)
    RunQueue* new_rq = rq_init();
    ll_insert(rqll, new_rq);

    // 2. Insert into the RunQueue structure
    RunQueue* rq = (RunQueue*) rqll->tail->data;
    rq->next = new_rq;

    
    rq->next = new_rq;
    return new_rq;
}

RunQueue* scheduler_new_rq() {
    return scheduler_new_rq_(g_default_rqll);
}

int wake_tasks() {
    Task *stk = (Task*) pq_peek(g_sleep_queue);

    int i = 0;
    while (!pq_empty(g_sleep_queue) && stk->next_run <= TIMER_NOW_MS) {
        stk = pq_dequeue(g_sleep_queue);
        stk->flags &= ~RQ_FLAG_SLEEPING;
        rq_add(stk->runqueue, stk);
        i++;
    }

    return i;
}

int rq_kill_all_tasks(RunQueue* rq) {
    if (!rq) return -1;
    Task* task = rq->head;

    int count = 0;
    int running = rq->running;

    for (int i=0; i < running; i++) {
        if ( !(task->flags & RQ_FLAG_KILLED) ) {
            tk_kill(task);
            count++;
        }

        task = task->next;
    }


    return count;
}

/* TODO: Currently doesn't iterate RunQueueLists (rqll) because they are not
 * tracked in any way other than g_default_rqll and g_sleeping_tasks
 */
int kill_rqll(ll_head* rqll) {
    if (ll_empty(rqll)) return -1;

    int count = 0;
    ll_node* node = rqll->node;

    // Iterate all RunQueues on list
    for (int i = 0; i < rqll->count; i++) {
        RunQueue* rq = (RunQueue*) node->data;

        if (rq->lock) continue;

        rq->lock = 1;
        count += rq_kill_all_tasks(rq);
        rq->lock = 0;

        node = node->next;
    }

    return count;
}

int scheduler_kill_all_tasks() {

    // Force wake all tasks and kill them
    Task *stk = (Task*) pq_peek(g_sleep_queue);
    while (!pq_empty(g_sleep_queue)) {
        stk = pq_dequeue(g_sleep_queue);

        stk->flags &= RQ_FLAG_KILLED;

        rq_add(stk->runqueue, stk);
    }

    int count = kill_rqll(g_default_rqll);
    
    return count;
}

int kill_dying_tasks() {
    ll_node* dtk = g_dying_tasks->node;
    int i = 0;
    while (dtk != NULL) {
        Task* tk = dtk->data;
        if (TIMER_NOW_MS <= tk->kill_time) {
            tk_kill(tk);
            ll_node* next = dtk->next;
            ll_rm(g_dying_tasks, tk);
            dtk = next;

        } else {
            dtk = dtk->next;
        }

        i++;
    }
    return i;
}


int schedule(RunQueue* rq, int delay, int runtime,
        int (*func)(Task*, Stack64*), Stack64* stack) {

    return schedule_cb(rq, delay, runtime, func, stack, NULL);
}

int schedule_cb(RunQueue* rq, int delay, int runtime,
        int (*func)(Task*, Stack64*), Stack64* stack, void (*callback)(Task*)) {

    Task* t = rq_create(rq, delay, runtime, func, stack, callback);

    return (t != NULL) - 1;
}


void schedule_run(ll_head* rqll) {
    int tasks_running = 1;

    while (tasks_running) {
        tasks_running = 0;

        wake_tasks();

        ll_node* node = rqll->node;
        while (node != NULL) {

            RunQueue* rq = (RunQueue*) node->data;

            for (int i = rq->running; 0 < i; i--)
                rq_run(rq);

            tasks_running += rq->count;
            node = node->next;
        }

        timer_synchronize();
    }
}


// TODO: initialize tail
ll_head* ll_init(int n) {
    n = n * (n>0) + (n<=0);
    ll_head* head = calloc(n, PAGE_SIZE);
    head->mempool = (ll_node*) (head+1);
    head->node = NULL;
    head->count = 0;
    head->max = (n * PAGE_SIZE) / sizeof(ll_node) - sizeof(ll_head);
    log_debug("%d / %lu = %d", n*PAGE_SIZE, sizeof(ll_node), head->max);

    return head;
}

int ll_insert(ll_head* head, void* ptr) {
    if (ll_full(head) != 0 || ptr == NULL) return -1;

    // Find place for new_node in mempool
    ll_node* new_node = head->mempool;
    for (int i=0; i<(head->max); i++) {
        if (new_node->data == NULL) break;
        new_node++;
    }
    
    new_node->data = ptr;
    head->count++;
   
    // Case 1: list is empty
    if (head->node == NULL) {
        head->node = new_node;
        head->tail = new_node;
       
    // Case 2: append to tail
    } else {
        head->tail->next = new_node;
        head->tail = new_node;
    }

    return 0;
}

int ll_rm(ll_head* head, void* ptr) {
    if (ll_empty(head) != 0) return -1;

    ll_node* prev = NULL;
    ll_node* node = head->node;
    
    // Search for node with ptr
    while (node->data != ptr && node->next != NULL) {
        prev = node;
        node = node->next;
    }

    // Case: Not found
    if (node == NULL || node->data != ptr) return 0;
    else {

        // Remove from list
        if (node == head->node) head->node = node->next;
        else if (node == head->tail) {
            prev->next = NULL;
            head->tail = prev;
        }
        else  prev->next = node->next;

        // Deallocate node
        node->data = NULL;
        node->next = NULL;
        head->count--;
    }

    return 1;
}

int ll_count(ll_head* head) {
    if (head == NULL) return -1;
    return head->count;
}

int ll_empty (ll_head* head) {
    if (head == NULL) return -1;
    return head->count == 0;
}

int ll_full (ll_head* head) {
    if (head == NULL) return -1;
    return head->count >= head->max;
}


