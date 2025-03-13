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

event_t map_key_to_event(int key) {
    event_t ev;

    switch (key) {
        case 'w':
            ev = E_KB_W;
            break;
        case 'a':
            ev = E_KB_A;
            break;
        case 'd':
            ev = E_KB_D;
            break;
        case 's':
            ev = E_KB_S;
            break;
        case 'g':
            ev = E_KB_G;
            break;
        case 'q':
            ev = E_KB_Q;
            break;
        default:
            return 0;
    }

    return ev;
}

void handle_event(int signo) {
    char key = getch();

    event_t ev = map_key_to_event(key);
    Keyboard *kb = &GLOBALS.keyboard;

    for (int i = 0; i < kb->handler_c; i++)
        kb->handlers[i](ev);

}

int input_register_handler(void (*func)(event_t)) {
    Keyboard *kb = &GLOBALS.keyboard;

    if (kb->handler_c >= MAX_INPUT_HANDLERS) return -1;

    kb->handlers[ kb->handler_c++ ] = func;
}

void keyboard_init() {

    GLOBALS.keyboard.handler_c = 0;

    void (*event_handler) = handle_event;

    struct sigaction sa;
    sa.sa_handler = event_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGIO, &sa, NULL);

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC);
    fcntl(STDIN_FILENO, F_SETOWN, getpid());
}

