#define COMPILE_FRONTEND_NCURSES
#define COMPILE_FRONTEND_SDL2

#include <stdio.h>
#include <string.h>

#include "curseminer/globals.h"
#include "curseminer/scheduler.h"
#include "curseminer/time.h"
#include "curseminer/games/curseminer.h"
#include "curseminer/games/other.h"
#include "curseminer/frontends/headless.h"

#ifdef COMPILE_FRONTEND_NCURSES
#include "curseminer/frontends/ncurses.h"
#endif

#ifdef COMPILE_FRONTEND_SDL2
#include "curseminer/frontends/sdl2.h"
#endif

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
    .game = NULL,
    .games_qu = NULL,
    .input_context = E_CTX_0,
    .view_port_maxx = 5,
    .view_port_maxy = 5,
};

static RunQueue* g_runqueue = NULL;

static void init(frontend_t frontend, const char *title) {
    time_init(UPDATE_RATE);

    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq_(GLOBALS.runqueue_list);
    GLOBALS.runqueue = g_runqueue;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    frontend_init_ui_t fuii = frontend_headless_ui_init;
    frontend_exit_ui_t fuie = frontend_headless_ui_exit;
    frontend_init_input_t fini = frontend_headless_input_init;
    frontend_exit_input_t fine = frontend_headless_input_exit;

#ifdef COMPILE_FRONTEND_NCURSES
    if (frontend == FRONTEND_NCURSES) {
        fuii = frontend_ncurses_ui_init;
        fuie = frontend_ncurses_ui_exit;
        fini = frontend_ncurses_input_init;
        fine = frontend_ncurses_input_exit;
    }
#else
        assert_log(frontend != FRONTEND_NCURSES,
                "Ncurses frontend is not available in this build.");
#endif

#ifdef COMPILE_FRONTEND_SDL2
    if (frontend == FRONTEND_SDL2) {
        fuii = frontend_sdl2_ui_init;
        fuie = frontend_sdl2_ui_exit;
        fini = frontend_sdl2_input_init;
        fine = frontend_sdl2_input_exit;
    }
#else
        assert_log(frontend != FRONTEND_SDL2,
                "SDL2 frontend is not available in this build.")
#endif

    frontend_register_ui(fuii, fuie);
    frontend_register_input(fini, fine);
    frontend_init(title);

    log_debug("Initialized...\n");
}

static int exit_state() {
    log_debug("Exiting...");

    frontend_exit();

    game_exit(GLOBALS.game);
    scheduler_free();

    return 0;
}

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
    int frontend;

#ifdef COMPILE_FRONTEND_SDL2
    frontend = FRONTEND_SDL2;
#elifdef COMPILE_FRONTEND_NCURSES
    frontend = FRONTEND_NCURSES;
#else
    frontend = FRONTEND_HEADLESS;
#endif

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

    int games = 2;
    GLOBALS.games_qu = qu_init(1);
    GameContextCFG *gcfgs = calloc(games, sizeof(GameContextCFG));

    GameContextCFG gcfg = {
        .skins_max = 12,
        .entity_types_max = 12,
        .scroll_threshold = 2,
    };

    gcfg.f_init = game_curseminer_init,
    gcfg.f_update = game_curseminer_update,
    gcfg.f_exit = game_curseminer_free,
    gcfgs[0] = gcfg;
    qu_enqueue(GLOBALS.games_qu, (uint64_t) (gcfgs + 0));

    gcfg.f_init = game_other_init,
    gcfg.f_update = game_other_update,
    gcfg.f_exit = game_other_free,
    gcfgs[1] = gcfg;
    qu_enqueue(GLOBALS.games_qu, (uint64_t) (gcfgs + 1));
    qu_next(GLOBALS.games_qu);

    init(frontend, title);

    GLOBALS.world = world_init(20, 1000, 64 * PAGE_SIZE);
    GLOBALS.game = game_init(gcfgs, GLOBALS.world);

    schedule_cb(g_runqueue, 0, 0, game_update, NULL, cb_exit);

    frontend_register_event(E_KB_Q, E_CTX_GAME, main_event_handler);
    frontend_register_event(E_KB_Q, E_CTX_NOISE, main_event_handler);
    frontend_register_event(E_KB_Q, E_CTX_CLOCK, main_event_handler);

    schedule_run(GLOBALS.runqueue_list);

    return exit_state();
}
