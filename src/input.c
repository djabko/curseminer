#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include "ncurses.h"
#include "globals.h"
#include "entity.h"
#include "scheduler.h"
#include "UI.h"
#include "input.h"

#undef tv_nsec

void (*g_mapper_mod_array[E_END][E_MOD_7])(InputEvent*);

void handler_stub(InputEvent *ie) {}

event_t g_ncurses_mapping['z'+1];
timer_t g_input_timer = 0;
Queue64 *g_queued_kdown = NULL;


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
    event_mod_t mods = 0;
    event_t id = 0;

    if ('A' <= key && key <= 'Z') {
        mods |= 1U << E_MOD_0;
        key += ' ';
    }

    id = g_ncurses_mapping[key];

    ev->type = type;
    ev->mods = mods;
    ev->id = id;
}

void handle_kup_ncurses(int signo) {
    InputEvent ev;

    ev.mods = E_NOMOD;
    ev.type = E_TYPE_KB;
    ev.state = ES_UP;

    event_t id = qu_dequeue(g_queued_kdown);
    while (id != -1) {
        log_debug("KEY UP: %d", id);
        ev.id = id;
        g_mapper_mod_array[ev.mods][ev.id](&ev);
        id = qu_dequeue(g_queued_kdown);
    }
}

void handle_kdown_ncurses(int signo) {
    InputEvent ev;

    // key down

    char key = getch();

    ncurses_map_event(&ev, key);

    // if not in queue then enqueue event
    if(!qu_contains(g_queued_kdown, key)) {
        qu_enqueue(g_queued_kdown, ev.id);
        ev.state = ES_DOWN;
    }


    struct itimerspec its;

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 900000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    timer_settime(g_input_timer, 0, &its, NULL);

    log_debug("KEY DOWN: %d", ev.id);

    g_mapper_mod_array[ev.mods][ev.id](&ev);
}

int input_register_event(event_t id, event_mod_t mod, void (*func)(InputEvent*)) {
    g_mapper_mod_array[mod][id] = func;
}

int input_init_ncurses() {
    ncurses_init_keys();

    g_queued_kdown = qu_init(1);


    struct sigevent se = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = SIGALRM,
    };

    timer_create(CLOCK_MONOTONIC, &se, &g_input_timer);



    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = handle_kdown_ncurses;
    sigaction(SIGIO, &sa, NULL);

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC);
    fcntl(STDIN_FILENO, F_SETOWN, getpid());

    sa.sa_handler = handle_kup_ncurses;
    sigaction(SIGALRM, &sa, NULL);
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

