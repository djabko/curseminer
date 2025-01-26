#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ncurses.h"

#include "globals.h"
#include "stack64.h"
#include "UI.h"
#include "game.h"
#include "keyboard.h"

#define RGB_TO_CURSES(x) ((int)((float)x*3.90625))  // 1000/256 conversion
#define MAX_TITLE 32

typedef void (*voidfunc) ();

Stack64* MENU_STACK = NULL;
int LINES = 0;
int COLS = 0;

int TIME_MSEC = 0;

typedef struct {
    WINDOW* win;
    int x, y, w, h;
    char title[MAX_TITLE];
} window_t;

window_t gamewin, uiwin = {};

typedef struct {
   double x, y; 
} Position;

typedef struct {
   Position a, b;
} Line;


/* Math Helper Functions */
double euc_dist(Line* line) {
    return floor( 
            sqrt(
                pow(line->b.x - line->a.x, 2) +
                pow(line->b.y - line->a.y, 2)
                ));
}

int man_dist(Line* line) {
    return round(
            fabs(line->b.x - line->a.x) +
            fabs(line->b.y - line->a.y)
            );
} 

int che_dist(Line* line) {
    return round(
            max(
                fabs(line->a.x - line->b.x),
                fabs(line->a.y - line->b.y)
                )
            );
}


/* Draw Functions */
void draw_fill(WINDOW* win, char c, int x1, int y1, int x2, int y2) {
    for (int x=x1; x<=x2; x++) {
        for (int y=y1; y<=y2; y++) {
            mvwaddch(win, y, x, c);
        }
    }
}

void draw_line(WINDOW* win, char c, int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) + (x2 <= x1) * -1;
    int sy = (y1 < y2) + (y2 <= y1) * -1;
    int err = dx - dy;

    while (1) {
        mvwaddch(win, y1, x1, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;

        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void draw_header() {
    for (int i=0; i<COLS; i++)
        mvaddch(1, i, '=');

    for (int i=0; i<COLS; i++)
        mvaddch(3, i, '=');
}

void draw_clock_needle(WINDOW* win, double x1, double y1, char c, double d, double angle) {
    double x2, y2;

    x2 = x1 + d * cos(angle);
    y2 = y1 + d * sin(angle);

    draw_line(win, c, x1, y1, x2, y2); 
}

void draw_rt_clock(WINDOW* win, int x, int y, int r) {
    int time_sec = TIME_MSEC / 1000;

    int sc = time_sec % 60;
    int mn = time_sec % (60*60);
    int hr = (time_sec+60*60) % (60*60*24);
    double angleS = sc * (2*M_PI/60) - (M_PI/2);
    double angleM = mn * (2*M_PI/(60*60)) - (M_PI/2);
    double angleH = hr * (2*M_PI/(24*60*60)) - (M_PI/2);

    draw_clock_needle(win, x, y  , '|', r*0.75,-1*(M_PI/2));
    draw_clock_needle(win, x, y  , '-', r*1.00, 0*(M_PI/2));
    draw_clock_needle(win, x, y+1, '|', r*0.75, 1*(M_PI/2));
    draw_clock_needle(win, x, y  , '-', r*1.00, 2*(M_PI/2));

    draw_clock_needle(win, x, y, '.', r*0.75, angleS);
    draw_clock_needle(win, x, y, 'm', r*0.50, angleM);
    draw_clock_needle(win, x, y, 'H', r*0.25, angleH);
    attron(A_BOLD);
    mvwaddch(win, y, x, '+');
    attroff(A_BOLD);
}

void draw_keyboard_state(WINDOW* scr, int x, int y) {
    for (KeyID i=KB_START+1; i<KB_END; i++) {
        mvwprintw(scr, y, x, "%c%s", keyid_to_string(i)[0], GLOBALS.keyboard.keys[i].down ? "*" : ".");
        x += 3;
    }
}

void draw_gamewin() {

    int mx = gamewin.w;
    int my = gamewin.h;

    EntityType* entity;

    for (int x=0; x <= gamewin.w; x++) {
        for (int y=0; y <= gamewin.h; y++) {

            entity = game_world_getxy(x, y);
            wattron(gamewin.win, COLOR_PAIR(entity->skin->id));
            mvwaddch(gamewin.win, y, x, entity->skin->character);
        }
    }

    box(gamewin.win, 0, 0);
    wnoutrefresh(gamewin.win);
}

void draw_uiwin() {
    werase(uiwin.win);

    draw_rt_clock(uiwin.win, uiwin.h/2+2, uiwin.h/2, uiwin.h/2);

    mvwprintw(uiwin.win, 5, 20, "Player: (%d, %d) [%c%d, %c%d]",
            GLOBALS.player->y, GLOBALS.player->x,
            GLOBALS.player->vy<0 ? '-':'+', GLOBALS.player->vy,
            GLOBALS.player->vx<0 ? '-':'+', GLOBALS.player->vx
            );

    EntityType* entity = game_world_getxy(1, 1);
    mvwprintw(uiwin.win, 7, 20, "Entity Type at (1,1): %lu", entity->id);

    mvwprintw(uiwin.win, (TIME_MSEC/100)%(int)(LINES*.2), COLS*.4, "!"); // draw splash icon

    box(uiwin.win, 0, 0);
    draw_keyboard_state(uiwin.win, (int)COLS*.5, 5);
    wnoutrefresh(uiwin.win);
}

void draw_main_menu() {
    draw_gamewin();
    draw_uiwin();
}


/* Init Functions */
int init_colors() {
    GameContext* game = game_get_context();

    int color_id = 0;
    for (int i=0; i<game->skins_c; i++) {
        int bg = color_id++;
        int fg = color_id++;

        Skin* skin = game->skins + i;
        init_color(bg, RGB_TO_CURSES(skin->bg_r), RGB_TO_CURSES(skin->bg_g), RGB_TO_CURSES(skin->bg_b));
        init_color(fg, RGB_TO_CURSES(skin->fg_r), RGB_TO_CURSES(skin->fg_g), RGB_TO_CURSES(skin->fg_b));

        init_pair(skin->id, fg, bg);
    }

    return 1;
}

int init_window(window_t* window, int x, int y, int w, int h, const char* title) {
    window->win = newwin(h, w, y, x);
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    strncpy(window->title, title, MAX_TITLE);

    return NULL < window->win;
}


/* Public functions used by other components */
void UI_update_time(int msec) {
    TIME_MSEC = msec;
}


int UI_init() {
    initscr();
    start_color();
    raw();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, 1);

    ESCDELAY = 25;
    getmaxyx(stdscr, LINES, COLS);

    //init_window(&gamewin, LINES*.6, COLS*.6, LINES * .05, COLS*.05, "Game");
    init_window(&gamewin, COLS*.05, LINES*.05, COLS*.6, LINES*.6, "Game");
    GLOBALS.view_port_maxx = COLS * .6 - 2;
    GLOBALS.view_port_maxy = LINES *.6 - 2;

    init_window(&uiwin, COLS*.05, LINES*.8, COLS*.8, LINES*.2, "UI");

    int status = game_init();
    if (status != 0) {
        fprintf(stderr, "Error initializing game: %d\n", status);
        UI_exit();
        return 0;
    }

    init_colors();

    MENU_STACK = st_init(16);
    st_push(MENU_STACK, (uint64_t) draw_main_menu);

    return 1;
}

int UI_loop() {
    if (st_peek(MENU_STACK) == -1) return -1;
    voidfunc draw_func = (voidfunc) st_peek(MENU_STACK);
    draw_func();

    doupdate();
    return 0;
}

int UI_exit() {
    delwin(gamewin.win);
    delwin(uiwin.win);
    endwin();
    free(MENU_STACK);
    return 1;
}

