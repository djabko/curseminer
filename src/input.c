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

event_t NCURSES_MAPPING[] = {};

void ncurses_map_event(InputEvent *ev, int key) {
    event_type_t type = E_TYPE_KB;
    event_state_t state = ES_DOWN;
    event_mods_t mods = 0;
    event_t id = 0;

    if ('A' <= key && key <= 'Z') {
        mods |= E_MOD_0;
        key += ' ';
    }

    switch (key) {
        case 'w':
            id = E_KB_W;
            break;
        case 'a':
            id = E_KB_A;
            break;
        case 'd':
            id = E_KB_D;
            break;
        case 's':
            id = E_KB_S;
            break;
        case 'c':
            id = E_KB_C;
            break;
        case 'g':
            id = E_KB_G;
            break;
        case 'q':
            id = E_KB_Q;
            break;
        default:
            id = E_NULL;
    }

    ev->type = type;
    ev->state = state;
    ev->mods = mods;
    ev->id = id;
}

void handle_event_ncurses(int signo) {
    char key = getch();

    InputEvent ev;
    ncurses_map_event(&ev, key);

    InputHandler *ih = &GLOBALS.keyboard;

    for (int i = 0; i < ih->handler_c; i++)
        ih->handlers[i](&ev);
}

int input_register_handler(void (*func)(InputEvent*)) {
    InputHandler *ih = &GLOBALS.keyboard;

    if (ih->handler_c >= MAX_INPUT_HANDLERS) return -1;

    ih->handlers[ ih->handler_c++ ] = func;
}

int input_init_ncurses() {
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

    GLOBALS.keyboard.handler_c = 0;

    switch (frontend) {

        case GAME_FRONTEND_NCURSES:
            input_init_ncurses();
            break;

        default:
            log_debug("ERROR: invalid input frontend provided: %d", frontend);
    }
}

