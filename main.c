#include "stdio.h"
#include "string.h"

#include <scheduler.h>
#include <files.h>
#include <timer.h>
#include <UI.h>

#define REFRESH_RATE 120
#define MIN_ARGS 0

ll_head* RQLL = NULL;
RunQueue* RUN_QUEUE = NULL;

TimeStamp quit_time;

void checkifNULL(void* ptr, const char* str) {
    if (ptr == NULL) printf("%s is a null pointer.\n", str);
}

void init() {
    timer_init(REFRESH_RATE);

    RQLL = scheduler_init();
    checkifNULL((void*)(
                RUN_QUEUE = scheduler_new_rq(RQLL)), "RQ_UI");

    UI_init();

    printf("Initialized...\n");
}

int exit_state() {
    printf("Exiting...");

    //scheduler_free_rqll(RQLL);
    scheduler_free();

    return 0;
}

int jobUI (Task* task, Stack64* stack) {
    TimeStamp now;
    timer_now(&now);
    UI_update_time(1.0f / REFRESH_RATE, now.tv_sec);

    UI_loop();

    return 0;
}

int jobInput (Task* task, Stack64* stack) {
    return 0;
}

int jobIO (Task* task, Stack64* stack) {
    return 0;
}

int jobMain (Task* task, Stack64* stack) {
    if (GLOBAL_TASK_COUNT < 2) tk_kill(task);
    wake_tasks();
    kill_dying_tasks();
    return 0;
}


void ui_callback(Task* task) {
    UI_exit();
    kill_all_tasks();
}


int main(int argc, const char** argv) {
    if (argc < MIN_ARGS+1) return -1;
    init();

    int ui_runtime = atoi(argv[1]);

    Stack64* stackUI = st_init(16);
    Stack64* stackIn = st_init(16);
    Stack64* stackIO = st_init(16);

    gettimeofday(&quit_time, NULL);
    quit_time.tv_sec += 1;

    schedule(RUN_QUEUE, 0, 0, jobMain, NULL);
    schedule_cb(RUN_QUEUE, 0, ui_runtime, jobUI, stackUI, ui_callback);
    schedule(RUN_QUEUE, 0, 1, jobInput, stackIn);
    schedule(RUN_QUEUE, 0, 1, jobIO, stackIO);

    schedule_run(RQLL);

    return exit_state();
}
 
