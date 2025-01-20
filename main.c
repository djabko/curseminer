#include "stdio.h"
#include "string.h"

#include <globals.h>
#include <scheduler.h>
#include <files.h>
#include <timer.h>
#include <UI.h>

#define MIN_ARGS 0
#define UPDATE_RATE 120 // times per second
#define SCREEN_REFRESH_RATE 120 // times per second
#define KEYBOARD_EMPTY_RATE 1000000 / 2

struct Globals GLOBALS = {
    .keyboard = {{{-1, {0}, -1, -1}}},
    .window = {
        .width=2<<6, 
        .height=2<<5, 
        .title="My game",
        .w = NULL,
        .monitor = NULL,
        .share = NULL
    },
    .player = NULL
};

ll_head* RQLL = NULL;
RunQueue* RUN_QUEUE = NULL;

TimeStamp quit_time;

void checkifNULL(void* ptr, const char* str) {
    if (ptr == NULL) printf("%s is a null pointer.\n", str);
}

void init() {
    timer_init(UPDATE_RATE);

    RQLL = scheduler_init();
    checkifNULL((void*)(
                RUN_QUEUE = scheduler_new_rq(RQLL)), "RQ_UI");
    

    //window_init();
    UI_init();
    keyboard_init();

    printf("Initialized...\n");
}

int exit_state() {
    printf("Exiting...");

    UI_exit();
    //window_free();
    scheduler_free();

    return 0;
}

int tsinit = 0;
TimeStamp last_screen_refresh;
int jobUI (Task* task, Stack64* stack) {
    
    static int interval = 1000 / SCREEN_REFRESH_RATE;
    if (!tsinit || interval <= timer_diff_milisec(&TIMER_NOW, &last_screen_refresh)) {
        last_screen_refresh = TIMER_NOW;
        tsinit = 1;
        fprintf(stderr, "refreshed: %lu\n", TIMER_NOW.tv_sec);

        int sec = TIMER_NOW.tv_sec;
        int msec = TIMER_NOW.tv_usec / 1000;
        UI_update_time(sec * 1000 + msec);

        int quit = UI_loop();
        if (quit)
            tk_kill(task);
    }

    return 0;
}

int jobInput (Task* task, Stack64* stack) {

    keyboard_poll();
    fprintf(stderr, "keyboard_polled:\n");
    if (kb_down(KB_Q))
        tk_kill(task);

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


int killing = 0;
void cb_exit(Task* task) {
    if (killing) return;
    killing++;
    kill_all_tasks(); // TODO: Calling multiple times causes segfault
}


int main(int argc, const char** argv) {
    if (argc < MIN_ARGS+1) return -1;
    init();

    int ui_runtime;
    if (argc == 2)
        ui_runtime = atoi(argv[1]);
    else 
        ui_runtime = 0;

    Stack64* stackUI = st_init(16);
    Stack64* stackIn = st_init(16);
    Stack64* stackIO = st_init(16);

    gettimeofday(&quit_time, NULL);
    quit_time.tv_sec += 1;

    schedule(RUN_QUEUE, 0, 0, jobMain, NULL);
    schedule_cb(RUN_QUEUE, 0, ui_runtime, jobUI, stackUI, cb_exit);
    schedule_cb(RUN_QUEUE, 0, 0, jobInput, stackIn, cb_exit);
    schedule(RUN_QUEUE, 0, 1, jobIO, stackIO);

    schedule_run(RQLL);

    return exit_state();
}
 
