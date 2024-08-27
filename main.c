#include "stdio.h"
#include "string.h"

#include <run_queue.h>
#include <files.h>

#define MAX_TASK 10

RunQueue *RQ_NOND, *RQ_HIGH, *RQ_MID, *RQ_LOW;

int tk1(Task* task, Stack64* stack);
int tk2(Task* task, Stack64* stack);
int tk3(Task* task, Stack64* stack);

/*
typedef struct {
    char name[50];
    unsigned char id;
    int status;
    void (*func)(int* status);
} Task;
*/

// Linear search, very slow
int schedule(RunQueue* rq, int time, int (*func)(Task*, Stack64*), Stack64* stack) {
    Task* t = rq_add(rq, time, func, stack);

    return (t != NULL) - 1;
}

void runner(int loops) {
    int n = 0;

    while (GLOBAL_TASK_COUNT) {
        int count = 0;

        int i = 0;
        for (; i<RQ_NOND->count; i++) rq_run(RQ_NOND);
        count += i;

        for (i = 0; i<RQ_HIGH->count && count+i <= MAX_TASK; i++) rq_run(RQ_HIGH);
        count += i;

        for (i = 0; i<RQ_MID->count && count+i <= MAX_TASK; i++) rq_run(RQ_MID);
        count += i;

        for (i = 0; i<RQ_LOW->count && count+i <= MAX_TASK; i++) rq_run(RQ_LOW);
    }
}

int tk1(Task* task, Stack64* stack) {
    int k = st_pop(stack);

    if (k == -1 || k == 0) {
        Stack64* iostack = (Stack64*) st_pop(stack);
        schedule(RQ_LOW, 0, tk2, iostack);

        st_push(stack, 1);
    }
    else if (k > 10000000000) kill_task(task);
    else st_push(stack, k+1);

    if (k % 10000000 == 0)
        //printf("%d\n", k);

    return 0;
}

int tk2(Task* task, Stack64* stack) {
    int fd = st_pop(stack);
    char* buf = (char*) st_pop(stack);
    int nbytes = st_pop(stack);

    struct aiocb* cb = malloc(sizeof (struct aiocb));
    cb->aio_fildes = fd;
    cb->aio_offset = 0;
    cb->aio_buf = buf;
    cb->aio_nbytes = nbytes - 1;
    cb->aio_reqprio = 0;
    cb->aio_sigevent.sigev_notify = SIGEV_NONE;
    cb->aio_lio_opcode = LIO_NOP;

    st_push(stack, (uint64_t) cb);

    kill_task(task);

    schedule(RQ_LOW, 0, file_read, stack);
    return 0;
}

int tk3(Task* task, Stack64* stack) {
    //printf("\n");
    return 0;
}


Stack64* init() {
    RQ_NOND = rq_init();
    RQ_HIGH = rq_init();
    RQ_MID = rq_init();
    RQ_LOW = rq_init();

    Stack64* main_stack = st_init(16);
    st_push(main_stack, 100);

    printf("Initialized...\n");
    return main_stack;
}

int exit_state(Stack64* main_stack) {
    printf("Exiting with status %d...\n", st_pop(main_stack));
    st_destroy(main_stack);
    return 0;
}

int main(int argc, const char** argv) {

    if (argc < 2) return -1;

    Stack64* main_stack = init();
    Stack64* fstack = st_init(16);
    Stack64* iostack = st_init(16);

    int bufs = 128;
    char* buf = malloc(bufs);
    buf[bufs-1] = '\0';

    memcpy(buf, argv[1], bufs);

    st_push(iostack, bufs);
    st_push(iostack, (uint64_t) buf);
    st_push(iostack, file_open(buf, O_RDONLY));

    st_push(fstack, (uint64_t) iostack);
    st_push(fstack, -1);
    
    //schedule(RQ_NOND, 0, tk1, fstack);
    schedule(RQ_LOW, 0, tk2, iostack);
    runner(1);

    return exit_state(main_stack);
}
 
