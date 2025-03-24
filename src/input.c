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

void (*g_mapper_ctx_array[E_CTX_END][E_END])(InputEvent*);

void handler_stub(InputEvent *ie) {}


/* Ncurses Specific */
#define msec_to_nsec(ms) ms * 1000000
#define g_keyup_delay msec_to_nsec(100)

typedef struct {
    event_t id;
    milliseconds_t last_pressed;
} KeyDownState;

#define KDS_MAX 3
KeyDownState KEY_DOWN_STATES_ARRAY[KDS_MAX];
KeyDownState *NEXT_AVAILABLE_KDS = KEY_DOWN_STATES_ARRAY;


event_t g_ncurses_mapping_kb['z'+1];
InputEvent g_ncurses_mapping_ms[BUTTON4_TRIPLE_CLICKED];
timer_t g_input_timer = 0;
Queue64 *g_queued_kdown = NULL;

void init_keys_ncurses() {
    for (int i = 0; i < 'Z'+1; i++) 
        g_ncurses_mapping_kb[i] = E_NULL;

    for (int i = 0; i < BUTTON4_TRIPLE_CLICKED; i++) 
        g_ncurses_mapping_ms[i] = (InputEvent){0, 0, 0, 0, 0};

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

    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].id = E_MS_MMB;
    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].id = E_MS_RMB;
    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].type = E_TYPE_MS;
    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].mods = E_NOMOD;
}

void map_event_ncurses(InputEvent *ev, int key) {

    if (key == -103 || key == KEY_MOUSE) {
        MEVENT mouse;
        if (getmouse(&mouse) != OK) log_debug("ERROR GETTING MOUSE EVENT!");

        *ev = g_ncurses_mapping_ms[mouse.bstate];
        uint16_t x = mouse.x;
        uint16_t y = mouse.y;
        uint16_t z = mouse.z;

        int bits = 16;
        ev->data = (x << (bits * 0)) + (y << (bits * 1)) + (z << (bits * 2));

    } else {
        if ('A' <= key && key <= 'Z') {
            ev->mods |= 1U << (E_MOD_0 - 1);
            key += ' ';
        } else
            ev->mods = E_NOMOD;

        ev->id = g_ncurses_mapping_kb[key];
        ev->type = E_TYPE_KB;
    }

}

void handle_kdown_ncurses(int signo) {
    // 1. reset timer
    // 2. check queue & remove expired items
    // 3. enqueue current event

    InputEvent ev;

    char key = getch();

    if (key == -1) return;

    event_ctx_t ctx = GLOBALS.input_context;
    map_event_ncurses(&ev, key);
    ev.state = ES_DOWN;
    g_mapper_ctx_array[ctx][ev.id](&ev);

    if (ev.type == E_TYPE_KB) {

        // Reset keyup timer
        struct itimerspec its = {0};

        its.it_value.tv_nsec = msec_to_nsec(g_keyup_delay);

        timer_settime(g_input_timer, 0, &its, NULL);

        /* Process all pending ES_UP events and ensure there is space for
           this event. */
        if (!qu_empty(g_queued_kdown)) {

            for (int i = 0; i < g_queued_kdown->count; i++) {

                KeyDownState *kds = (KeyDownState*) qu_next(g_queued_kdown);

                bool is_expired = TIMER_NOW_MS < kds->last_pressed + g_keyup_delay;
                bool is_kds_full = KDS_MAX <= g_queued_kdown->count + 1;

                if (is_expired || is_kds_full) {
                    kds = (KeyDownState*) qu_dequeue(g_queued_kdown);
                    NEXT_AVAILABLE_KDS--;

                    i--;

                    InputEvent ev_up = {
                        .id = kds->id,
                        .type = E_TYPE_KB,
                        .state = ES_UP,
                        .mods = ev.mods,
                    };

                    //log_debug("[%lu] InputEvent: {%d %d %d %d} in E_CTX_%d => %p, lp=%lu [%d,%d]", TIMER_NOW_MS/1000, ev_up.id, ev_up.type, ev_up.state, ev_up.mods, ctx, g_mapper_ctx_array[ctx][ev_up.id], kds->last_pressed, is_expired, is_kds_full);
                    g_mapper_ctx_array[ctx][ev_up.id](&ev_up);
                }
            }
        }

        KeyDownState new_kds = {.id = ev.id, .last_pressed = TIMER_NOW_MS};
        KeyDownState *p = KEY_DOWN_STATES_ARRAY;
        int c = g_queued_kdown->count;

        if      (0 < c && new_kds.id == p[0].id) p[0] = new_kds;
        else if (1 < c && new_kds.id == p[1].id) p[1] = new_kds;
        else if (2 < c && new_kds.id == p[2].id) p[2] = new_kds;
        else {
            p = NEXT_AVAILABLE_KDS++;
            *p = new_kds;
            qu_enqueue(g_queued_kdown, (uint64_t) p);
        }
        //log_debug("[%lu] InputEvent: {%d %d %d %d} in E_CTX_%d => %p, lp=%lu", TIMER_NOW_MS/1000, ev.id, ev.type, ev.state, ev.mods, ctx, g_mapper_ctx_array[ctx][ev.id], new_kds.last_pressed/1000);
    }

    g_mapper_ctx_array[ctx][ev.id](&ev);
}

void handle_kup_ncurses(int signo) {
    // 1. dequeue all events

    event_ctx_t ctx = GLOBALS.input_context;

    while (!qu_empty(g_queued_kdown)) {
        KeyDownState *kds = (KeyDownState*) qu_dequeue(g_queued_kdown);

        InputEvent ev_up = {
            .id = kds->id,
            .type = E_TYPE_KB,
            .state = ES_UP,
            .mods = E_NOMOD,
        };

        //log_debug("[%lu] InputEvent: {%d %d %d %d} in E_CTX_%d => %p", TIMER_NOW_MS/1000, ev_up.id, ev_up.type, ev_up.state, ev_up.mods, ctx, g_mapper_ctx_array[ctx][ev_up.id]);
        g_mapper_ctx_array[ctx][ev_up.id](&ev_up);

        NEXT_AVAILABLE_KDS--;
    }
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

#undef KDS_MAX


/* Generic Functions */
int input_register_event(event_t id, event_ctx_t ctx, void (*func)(InputEvent*)) {
    g_mapper_ctx_array[ctx][id] = func;
    log_debug("Mapped %d [E_CTX_%d] to %p", id, ctx, func);
}

void input_init(game_frontent_t frontend) {

    for (int i = E_CTX_0; i < E_CTX_END; i++) {
        for (int j = E_NULL; j < E_END; j++) {
            g_mapper_ctx_array[i][j] = handler_stub;
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

