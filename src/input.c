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


/* Ncurses Specific */
event_t g_ncurses_mapping_kb['z'+1];
InputEvent g_ncurses_mapping_ms[BUTTON4_TRIPLE_CLICKED];
timer_t g_input_timer = 0;
Queue64 *g_queued_kdown = NULL;

void init_keys_ncurses() {
    for (int i = 0; i < 'Z'+1; i++) 
        g_ncurses_mapping_kb[i] = E_NULL;

    for (int i = 0; i < BUTTON4_TRIPLE_CLICKED; i++) 
        g_ncurses_mapping_ms[i] = (InputEvent){0, 0, 0, 0};

    log_debug("MOUSE BSTATES:\nBUTTON1_PRESSED: %d\nBUTTON1_RELEASED: %d\nBUTTON2_PRESSED: %d\nBUTTON2_RELEASED: %d\nBUTTON3_PRESSED: %d\nBUTTON3_RELEASED: %d\n",
            BUTTON1_PRESSED, BUTTON1_RELEASED, BUTTON2_PRESSED, BUTTON2_RELEASED, BUTTON3_PRESSED, BUTTON3_RELEASED);

    g_ncurses_mapping_kb['w'] = E_KB_W;
    g_ncurses_mapping_kb['a'] = E_KB_A;
    g_ncurses_mapping_kb['s'] = E_KB_S;
    g_ncurses_mapping_kb['d'] = E_KB_D;
    g_ncurses_mapping_kb['c'] = E_KB_C;
    g_ncurses_mapping_kb['g'] = E_KB_G;
    g_ncurses_mapping_kb['q'] = E_KB_Q;

    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].id = E_MS_LMB;
    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].id = E_MS_MMB;
    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].id = E_MS_RMB;
    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].id = E_MS_LMB;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].id = E_MS_MMB;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].id = E_MS_RMB;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].mods = E_NOMOD;

}

void map_event_ncurses(InputEvent *ev, int key) {
    event_type_t type;
    event_t id;
    event_mod_t mods = 0;

    //log_debug("NCURSES KEY: %d", key);
    if (key == -103) {
        MEVENT mouse;
        if (getmouse(&mouse) != OK) log_debug("ERROR GETTING MOUSE EVENT!");
        log_debug("MOUSE BSTATE: %d", mouse.bstate);

        ev->type = E_TYPE_MS;
        ev->id = g_ncurses_mapping_kb[mouse.bstate];

    } else {
        if ('A' <= key && key <= 'Z') {
            ev->mods |= 1U << E_MOD_0;
            key += ' ';
        } else
            ev->mods = E_NOMOD;

        ev->id = g_ncurses_mapping_kb[key];
        ev->type = E_TYPE_KB;
    }

}

void handle_kup_ncurses(int signo) {
    InputEvent ev;

    ev.mods = E_NOMOD;
    ev.type = E_TYPE_KB;
    ev.state = ES_UP;

    event_t id = qu_dequeue(g_queued_kdown);
    while (id != -1) {
        ev.id = id;
        g_mapper_mod_array[ev.mods][ev.id](&ev);
        id = qu_dequeue(g_queued_kdown);
    }
}

void handle_kdown_ncurses(int signo) {
    InputEvent ev;

    char key = getch();

    map_event_ncurses(&ev, key);

    // Set timer to simulate key release after n nanoseconds
    if (ev.type == E_TYPE_KB) {
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
    }

    /*log_debug("InputEvent: {%d, %c, %d, %d}", 
            ev.id, ev.state==ES_UP ? 'u' : 'd', ev.type, ev.mods);
            */
    if (ev.type == E_TYPE_KB)
        g_mapper_mod_array[ev.mods][ev.id](&ev);
}

int input_init_ncurses() {
    init_keys_ncurses();

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


/* Generic Functions */
int input_register_event(event_t id, event_mod_t mod, void (*func)(InputEvent*)) {
    g_mapper_mod_array[mod][id] = func;
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

