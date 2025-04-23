#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "scheduler.h"
#include "files.h"
#include "timer.h"
#include "games/curseminer.h"
#include "games/other.h"
#include "frontends/headless.h"
#include "frontends/ncurses.h"
#include "frontends/sdl2.h"

#define MIN_ARGS 0
#define UPDATE_RATE 120 // times per second
#define SCREEN_REFRESH_RATE 20 // times per second
#define KEYBOARD_EMPTY_RATE 1000000 / 2

typedef enum {
    FRONTEND_HEADLESS,
    FRONTEND_NCURSES,
    FRONTEND_SDL2,
    FRONTEND_END,
} frontend_t;

struct Globals GLOBALS = {
    .player = NULL,
    .input_context = E_CTX_0,
};

static RunQueue* g_runqueue = NULL;

static void init(frontend_t frontend, const char *title) {
    timer_init(UPDATE_RATE);

    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
    GLOBALS.runqueue = g_runqueue;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    frontend_init_t fuii;
    frontend_exit_t fuie;
    frontend_init_t fini;
    frontend_exit_t fine;

    if (frontend == FRONTEND_NCURSES) {
        fuii = frontend_ncurses_ui_init;
        fuie = frontend_ncurses_ui_exit;
        fini = frontend_ncurses_input_init;
        fine = frontend_ncurses_input_exit;

    } else if (frontend == FRONTEND_SDL2) {
        fuii = frontend_sdl2_ui_init;
        fuie = frontend_sdl2_ui_exit;
        fini = frontend_sdl2_input_init;
        fine = frontend_sdl2_input_exit;

    } else {
        fuii = frontend_headless_ui_init;
        fuie = frontend_headless_ui_exit;
        fini = frontend_headless_input_init;
        fine = frontend_headless_input_exit;
    }

    frontend_register_ui(fuii, fuie);
    frontend_register_input(fini, fine);
    frontend_init(title);

    log_debug("Initialized...\n");
}

static int exit_state() {
    log_debug("Exiting...");

    frontend_exit();

    game_free(GLOBALS.game);
    scheduler_free();

    return 0;
}

/*
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
*/

static void cb_exit(Task* task) {
    scheduler_kill_all_tasks();
}

static void main_event_handler(InputEvent *ev) {
    scheduler_kill_all_tasks();
}

int main(int argc, const char** argv) {
    if (argc < MIN_ARGS+1) return -1;

    const char *nogui_string = "-nogui";
    const char *tui_string = "-tui";
    const char *gui_string = "-gui";
    const char *title = "Curseminer!";
    int frontend = FRONTEND_SDL2;

    for (int i=1; i<argc; i++) {
        if (1 < argc) {
            if (0 == strncmp(argv[i], nogui_string, 7)) {
                frontend = FRONTEND_HEADLESS;

            } else if (0 == strncmp(argv[i], tui_string, 7)) {
                frontend = FRONTEND_NCURSES;

            } else if (0 == strncmp(argv[i], gui_string, 7)) {
                frontend = FRONTEND_SDL2;
            }
        }
    }

    GameContextCFG gcfg = {
        .skins_max = 32,
        .entity_types_max = 32,
        .scroll_threshold = 5,

        /*
        .f_init = game_curseminer_init,
        .f_update = game_curseminer_update,
        .f_free = game_curseminer_free,
        */
        .f_init = game_other_init,
        .f_update = game_other_update,
        .f_free = game_other_free,
    };

    init(frontend, title);

    GLOBALS.game = game_init(&gcfg);
    Stack64 *gst = st_init(1);
    st_push(gst, (uint64_t) GLOBALS.game);

    schedule_cb(g_runqueue, 0, 0, game_update, gst, cb_exit);

    frontend_register_event(E_KB_Q, E_CTX_GAME, main_event_handler);
    frontend_register_event(E_KB_Q, E_CTX_NOISE, main_event_handler);
    frontend_register_event(E_KB_Q, E_CTX_CLOCK, main_event_handler);

    schedule_run(GLOBALS.runqueue_list);

    return exit_state();
}
 
