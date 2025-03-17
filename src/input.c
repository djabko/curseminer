#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "ncurses.h"
#include "globals.h"
#include "entity.h"
#include "scheduler.h"
#include "UI.h"
#include "input.h"

void (*g_mapper_mod_array[E_END][E_MOD_7])(InputEvent*);

void handler_stub(InputEvent *ie) {}

event_t g_ncurses_mapping['z'+1];

void ncurses_init_keys() {
    for (int i = 0; i < 'Z'+1; i++) 
        g_ncurses_mapping[i] = E_NULL;

    g_ncurses_mapping['w'] = E_KB_W;
    g_ncurses_mapping['a'] = E_KB_A;
    g_ncurses_mapping['s'] = E_KB_S;
    g_ncurses_mapping['d'] = E_KB_D;
    g_ncurses_mapping['c'] = E_KB_C;
    g_ncurses_mapping['g'] = E_KB_G;
    g_ncurses_mapping['q'] = E_KB_Q;
}

void ncurses_map_event(InputEvent *ev, int key) {
    event_type_t type = E_TYPE_KB;
    event_state_t state = ES_DOWN;
    event_mod_t mods = 0;
    event_t id = 0;

    if ('A' <= key && key <= 'Z') {
        mods |= 1U << E_MOD_0;
        key += ' ';
    }

    id = g_ncurses_mapping[key];

    ev->type = type;
    ev->state = state;
    ev->mods = mods;
    ev->id = id;
}

void handle_event_ncurses(int signo) {
    char key = getch();

    InputEvent ev;
    ncurses_map_event(&ev, key);

    g_mapper_mod_array[ev.mods][ev.id](&ev);
}

int input_register_event(event_t id, event_mod_t mod, void (*func)(InputEvent*)) {
    g_mapper_mod_array[mod][id] = func;
}

int input_init_ncurses() {
    ncurses_init_keys();

    void (*event_handler) = handle_event_ncurses;

    struct sigaction sa;
    sa.sa_handler = event_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGIO, &sa, NULL);

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC);
    fcntl(STDIN_FILENO, F_SETOWN, getpid());
}

void input_init(game_frontent_t frontend) {

    for (int i = E_NOMOD; i < E_MOD_7; i++) {
        for (int j = E_NULL; j < E_END; j++) {
            g_mapper_mod_array[i][j] = handler_stub;
        }
    }

    switch (frontend) {

        case GAME_FRONTEND_NCURSES:
            input_init_ncurses();
            break;

        default:
            log_debug("ERROR: invalid input frontend provided: %d", frontend);
    }
}

