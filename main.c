#include "stdio.h"
#include "string.h"

#include <scheduler.h>
#include <files.h>
#include <timer.h>

#define REFRESH_RATE 5
#define MIN_ARGS 0

RunQueueList* RQLL = NULL;
RunQueue* RUN_QUEUE = NULL;

TimeStamp quit_time;

void checkifNULL(void* ptr, const char* str) {
    if (ptr == NULL) printf("%s is a null pointer.\n", str);
}

void init() {
    timer_init(REFRESH_RATE);

    RQLL = rqll_init();
    checkifNULL((void*)(RUN_QUEUE = rqll_add(RQLL)), "RQ_UI");

    printf("Initialized...\n");
}

int exit_state() {
    printf("Exiting...");

    free(RQLL);
    free(RUN_QUEUE);

    return 0;
}

int jobUI (Task* task, Stack64* stack) {
    printf("\r<=====>");
    fflush(stdout);
    tk_sleep(task, 1);
}

int jobInput (Task* task, Stack64* stack) {
    printf("\r<----->");
    fflush(stdout);
    tk_sleep(task, 2);
}

int jobIO (Task* task, Stack64* stack) {
    printf("\r<DONE!>\n");
    fflush(stdout);
    tk_sleep(task, 3);
}

int jobWake(Task* task, Stack64* stack) {
    TimeStamp* ts = (TimeStamp*) st_pop(stack);
    if (timer_nready(ts)) {
        wake_tasks();
        gettimeofday(ts, NULL);
        ts->tv_sec += 1;
    }

    st_push(stack, (uint64_t) ts);
}

int main(int argc, const char** argv) {

    if (argc < MIN_ARGS+1) return -1;
    init();

    Stack64* stackUI = st_init(16);
    Stack64* stackIn = st_init(16);
    Stack64* stackIO = st_init(16);
    Stack64* stackSL = st_init(2);

    gettimeofday(&quit_time, NULL);
    quit_time.tv_sec += 1;

    // change name to timeout
    schedule(RUN_QUEUE, 10, 0, jobUI, stackUI);
    schedule(RUN_QUEUE, 0, 0, jobInput, stackUI);
    schedule(RUN_QUEUE, 4, 0, jobIO, stackUI);
    schedule(RUN_QUEUE, 0, 0, jobWake, stackSL);

    TimeStamp *sleepts = malloc(sizeof(TimeStamp));
    gettimeofday(sleepts, NULL);
    st_push(stackSL, (uint64_t) sleepts);
    schedule_run(RQLL);

    return exit_state();
}
 
