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
    printf("UI\n");
}

int jobInput (Task* task, Stack64* stack) {
}

int jobIO (Task* task, Stack64* stack) {
}

int main(int argc, const char** argv) {

    if (argc < MIN_ARGS+1) return -1;
    init();

    Stack64* stackUI = st_init(16);
    Stack64* stackIn = st_init(16);
    Stack64* stackIO = st_init(16);

    gettimeofday(&quit_time, NULL);
    quit_time.tv_sec += 1;

    // change name to timeout
    schedule(RUN_QUEUE, NULL, 5, jobUI, stackUI);
    schedule(RUN_QUEUE, NULL, 1, jobInput, stackUI);
    schedule(RUN_QUEUE, NULL, 1, jobIO, stackUI);

    schedule_run(RQLL);

    return exit_state();
}
 
