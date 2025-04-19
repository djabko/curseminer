#include "stdio.h"
#include "string.h"

#include <globals.h>
#include <scheduler.h>
#include <files.h>
#include <timer.h>
#include <UI.h>
#include <GUI.h>

#define MIN_ARGS 0
#define UPDATE_RATE 120 // times per second
#define SCREEN_REFRESH_RATE 20 // times per second
#define KEYBOARD_EMPTY_RATE 1000000 / 2

struct Globals GLOBALS = {
    .player = NULL,
    .input_context = E_CTX_0,
};

static RunQueue* g_runqueue = NULL;
static game_frontend_t g_frontend;


static void init(game_frontend_t frontend, const char *title,
        const char *spritesheet_path) {

    input_init(GAME_FRONTEND_NCURSES);
    input_init(GAME_FRONTEND_SDL2);
    timer_init(UPDATE_RATE);

    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
    g_frontend = frontend;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    if (frontend == GAME_FRONTEND_HEADLESS)
        UI_init(1);

    else if (frontend == GAME_FRONTEND_NCURSES)
        UI_init(0);

    else if (frontend == GAME_FRONTEND_SDL2)
        GUI_init(title, spritesheet_path);

    log_debug("Initialized...\n");
}

static int exit_state() {
    log_debug("Exiting...");

    if (g_frontend == GAME_FRONTEND_SDL2)
        GUI_exit();
    else
        UI_exit();

    game_free(GLOBALS.game);
    scheduler_free();

    return 0;
}

static int job_ui (Task* task, Stack64* stack) {
    milliseconds_t sec = TIMER_NOW.tv_sec;
    milliseconds_t msec = TIMER_NOW.tv_usec / 1000;

    UI_update_time(sec * 1000 + msec);

    int quit = UI_loop();

    if (quit)
        tk_kill(task);

    tk_sleep(task, 1000 / SCREEN_REFRESH_RATE);

    return 0;
}

static int job_gui(Task *task, Stack64 *stack) {
    GUI_loop();

    tk_sleep(task, 1000 / SCREEN_REFRESH_RATE);
    return 0;
}

static unsigned long TRACK = 0;
static int job_gui_poll_input(Task *task, Stack64 *stack) {
    input_SDL2_poll();
}

static milliseconds_t LAST_SYNC_MS;
static int job_gui_poll_input_track(Task *task, Stack64 *stack) {
    log_debug("Input polls: %lu, time since last sync: %lu", TRACK, TIMER_NOW_MS - LAST_SYNC_MS);
    LAST_SYNC_MS = TIMER_NOW_MS;
    TRACK = 0;

    tk_sleep(task, 1000);
}

static void cb_exit(Task* task) {
    scheduler_kill_all_tasks();
}

static void main_event_handler(InputEvent *ev) {
    scheduler_kill_all_tasks();
}

int main(int argc, const char** argv) {
    if (argc < MIN_ARGS+1) return -1;

    const char *title, *spritesheet;
    title = "Curseminer!";
    spritesheet = "assets/spritesheets/tiles.gif";

    const char *nogui_string = "-nogui";
    const char *tui_string = "-tui";
    const char *gui_string = "-gui";
    int frontend = GAME_FRONTEND_SDL2;

    for (int i=1; i<argc; i++) {
        if (1 < argc) {
            if (0 == strncmp(argv[i], nogui_string, 7)) {
                frontend = GAME_FRONTEND_HEADLESS;

            } else if (0 == strncmp(argv[i], tui_string, 7)) {
                frontend = GAME_FRONTEND_NCURSES;

            } else if (0 == strncmp(argv[i], gui_string, 7)) {
                frontend = GAME_FRONTEND_SDL2;
            }
        }
    }

    if (!access(spritesheet, F_OK) == 0)
        spritesheet = NULL;

    init(frontend, title, spritesheet);

    input_register_event(E_KB_Q, E_CTX_GAME, main_event_handler);
    input_register_event(E_KB_Q, E_CTX_NOISE, main_event_handler);
    input_register_event(E_KB_Q, E_CTX_CLOCK, main_event_handler);

    Stack64 *gst = st_init(1);
    st_push(gst, (uint64_t) GLOBALS.game);

    if (g_frontend == GAME_FRONTEND_SDL2) {
        schedule_cb(g_runqueue, 0, 0, job_gui, NULL, cb_exit);
        schedule_cb(g_runqueue, 0, 0, job_gui_poll_input, NULL, cb_exit);
    } else {
        schedule_cb(g_runqueue, 0, 0, job_ui, NULL, cb_exit);
    }

    schedule_cb(g_runqueue, 0, 0, game_update, gst, cb_exit);

    schedule_run(GLOBALS.runqueue_list);

    return exit_state();
}
 
