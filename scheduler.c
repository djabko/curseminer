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
ll_head* g_sleeping_tasks;
ll_head* g_dying_tasks;


Task* create_task(Task* task, int delay, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {
    task->flags = '\0';
    task->occupied = 1;
    task->func = func;
    task->stack = stack;
    task->next = (Task*) task;

    if (runtime < 1) timer_never(&(task->kill_time));
    else {
        gettimeofday(&(task->kill_time), NULL); 
        task->kill_time.tv_sec += runtime;
        ll_insert(g_dying_tasks, task);
    }
    
    tk_sleep(task, delay);
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

Task* rq_add(RunQueue* rq, int delay, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {

    // TODO: Allocate more memory
    if (rq == NULL || rq_full(rq)) return NULL;

    Task* t = rq->mempool;
    while (t->occupied) t++;

    create_task(t, delay, runtime, func, stack);
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

    if (0 == (current->flags & FLAG_RQ_SLEEPING))
        current->func(current, current->stack);

    rq->head = rq->head->next;
    rq->tail->next = current;
    rq->tail = current;

    return 0;
}

void tk_sleep(Task* task, unsigned int seconds) {
    if (seconds < 1) return;

    TimeStamp* ts = &(task->next_run);
    gettimeofday(ts, NULL);
    ts->tv_sec += seconds;

    task->flags |= FLAG_RQ_SLEEPING;

    ll_insert(g_sleeping_tasks, task);
}

ll_head* scheduler_init() {
    ll_head* rqll = ll_init(1);
    
    // Initialize internal ll of sleeping tasks
    if (g_sleeping_tasks == NULL) {
        g_sleeping_tasks = ll_init(1);
    }

    // Initialize internal ll of dying tasks
    if (g_dying_tasks == NULL) {
        g_dying_tasks = ll_init(1);
    }

    return rqll;
}

void scheduler_free() {
    if (g_sleeping_tasks != NULL)
        free(g_sleeping_tasks);

    if (g_dying_tasks != NULL)
        free(g_dying_tasks);
}

// TODO: Keep track of all rqll's via global variable
void scheduler_free_rqll(ll_head* rqll) {
    ll_node* node = rqll->node;
    while (node->data != NULL) {

        RunQueue* rq = (RunQueue*) node->data;
        RunQueue* prev = rq;
        rq = rq->next;

        free(prev);
    }

    free(rqll);
}


RunQueue* scheduler_new_rq(ll_head* rqll) {
    RunQueue* rq = rq_init();
    ll_insert(rqll, rq);
    return rq;
}

int wake_tasks() {
    ll_node* stk = g_sleeping_tasks->node;
    int i = 0;
    while (stk != NULL) {
        Task* tk = stk->data;
        if (timer_nready(&(tk->next_run))) {
            tk->flags &= !FLAG_RQ_SLEEPING;
            ll_node* next = stk->next;
            ll_rm(g_sleeping_tasks, tk);
            stk = next;

        } else {
            stk = stk->next;
        }

        i++;
    }
    return i;
}

int kill_dying_tasks() {
    ll_node* dtk = g_dying_tasks->node;
    int i = 0;
    while (dtk != NULL) {
        Task* tk = dtk->data;
        if (timer_nready(&(tk->kill_time))) {
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


// Linear search, very slow
int schedule(RunQueue* rq, int delay, int runtime, int (*func)(Task*, Stack64*), Stack64* stack) {
    Task* t = rq_add(rq, delay, runtime, func, stack);

    return (t != NULL) - 1;
}

void schedule_run(ll_head* rqll) {
    int tasks_running = 1;

    while (tasks_running) {
        timer_print_now();
        printf("\t");
        tasks_running = 0;

        ll_node* node = rqll->node;
        while (node != NULL) {

            RunQueue* rq = (RunQueue*) node->data;
            for (int i=0; i<rq->count; i++) {
                tasks_running += rq_run(rq) == 0;
            }

            node = node->next;
        }

        timer_synchronize();
        printf("\t");
        timer_print_now();
        printf("\n");
    }
}


ll_head* ll_init(int n) {
    n = n * (n>0) + (n<=0);
    ll_head* head = malloc(n * PAGE_SIZE);
    head->mempool = (ll_node*) (head+1);
    head->node = NULL;
    head->count = 0;
    head->max = (n * PAGE_SIZE) / sizeof(ll_node) - sizeof(ll_head);
    printf("%d / %d = %d\n", n*PAGE_SIZE, sizeof(ll_node), head->max);

    for (int i=0; i<(head->max); i++) {
        head->mempool[i].next = NULL;
        head->mempool[i].data = NULL;
    }

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

    ll_node* node = head->node;
    
    // Case 1: list is empty
    if (node == NULL) {
        head->node = new_node;
        new_node->data = ptr;
        head->count++;
        return 0;
    }


    // Case 2: find childless node at the end of list
    while(node->next != NULL) node = node->next;

    new_node->data = ptr;
    node->next = new_node;
    head->count++;

    return 0;
}

int ll_rm(ll_head* head, void* ptr) {
    if (ll_empty(head) != 0) return -1;

    ll_node* prev = NULL;
    ll_node* node = head->node;
    
    while (node != ptr && node->next != NULL) {
        prev = node;
        node = node->next;
    }

    if (node == NULL || node->data != ptr) return 0;

    // Deallocate node
    node->data = NULL;

    // Remove from list
    if (prev == NULL) head->node = node->next;
    else prev->next = node->next;
    head->count--;

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


