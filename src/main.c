#include "stdio.h"
#include "string.h"

#include <globals.h>
#include <scheduler.h>
#include <files.h>
#include <timer.h>
#include <UI.h>

#define MIN_ARGS 0
#define UPDATE_RATE 120 // times per second
#define SCREEN_REFRESH_RATE 20 // times per second
#define KEYBOARD_EMPTY_RATE 1000000 / 2

struct Globals GLOBALS = {
    .player = NULL,
    .input_context = E_CTX_0,
};

ll_head* g_runqueue_list = NULL;
RunQueue* g_runqueue = NULL;


void init(int nogui_mode) {
    input_init(GAME_FRONTEND_NCURSES);
    timer_init(UPDATE_RATE);

    g_runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(g_runqueue_list);

    assert_log(g_runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    UI_init(nogui_mode);

    log_debug("Initialized...\n");
}

int exit_state() {
    log_debug("Exiting...");

    UI_exit();
    game_free();
    scheduler_free();

    return 0;
}

int job_ui (Task* task, Stack64* stack) {
    milliseconds_t sec = TIMER_NOW.tv_sec;
    milliseconds_t msec = TIMER_NOW.tv_usec / 1000;

    UI_update_time(sec * 1000 + msec);

    int quit = UI_loop();

    if (quit)
        tk_kill(task);

    tk_sleep(task, 1000 / SCREEN_REFRESH_RATE);

    return 0;
}

void cb_exit(Task* task) {
    kill_all_tasks();
}

void main_event_handler(InputEvent *ev) {
    kill_all_tasks();
}

int main(int argc, const char** argv) {
    if (argc < MIN_ARGS+1) return -1;

    const char *nogui_string = "-nogui";
    int nogui_mode = 0;

    for (int i=1; i<argc; i++)
        if (1 < argc)
            if (0 == strncmp(argv[i], nogui_string, 7)) nogui_mode = 1;

    init(nogui_mode);

    input_register_event(E_KB_Q, E_CTX_GAME, main_event_handler);
    input_register_event(E_KB_Q, E_CTX_NOISE, main_event_handler);
    input_register_event(E_KB_Q, E_CTX_CLOCK, main_event_handler);

    schedule_cb(g_runqueue, 0, 0, job_ui, NULL, cb_exit);
    schedule_cb(g_runqueue, 0, 0, game_update, NULL, cb_exit);

    schedule_run(g_runqueue_list);

    return exit_state();
}
 
