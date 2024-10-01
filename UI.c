#include "ncurses.h"
#include "stdlib.h"
#include "math.h"

#include <stack64.h>
#include <UI.h>
#include <game.h>

#define RGB_TO_CURSES(x) ((int)((float)x*3.90625))  // 1000/256 conversion

typedef void (*voidfunc) ();

Stack64* MENU_STACK = NULL;
int LINES = 0;
int COLS = 0;

double TIME_COUNT = 0.0f;
int TIME_SEC = 0.0f;

WINDOW *gamewin, *uiwin;


typedef struct {
   double x, y; 
} Position;

typedef struct {
   Position a, b;
} Line;


int max(double a, double b) {
    return a * (b <= a) + b * (a < b);
}

int min(double a, double b) {
    return a * (a <= b) + b * (b < a);
}


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

void draw_footer() {
}

void draw_sides() {
}

void draw_skeleton() {
    draw_header();
    draw_footer();
    draw_sides();
}

void draw_clock_needle(WINDOW* win, double x1, double y1, char c, double d, double angle) {
    double x2, y2;

    x2 = x1 + d * cos(angle);
    y2 = y1 + d * sin(angle);

    draw_line(win, c, x1, y1, x2, y2); 
}

void draw_rt_clock(WINDOW* win, int x, int y, int r) {
    int sc = TIME_SEC % 60;
    int mn = TIME_SEC % (60*60);
    int hr = (TIME_SEC+60*60) % (60*60*24);
    double angleS = sc * (2*M_PI/60) - (M_PI/2);
    double angleM = mn * (2*M_PI/(60*60)) - (M_PI/2);
    double angleH = hr * (2*M_PI/(24*60*60)) - (M_PI/2);

    draw_clock_needle(win, x, y, '|', r*0.75,-1*(M_PI/2));
    draw_clock_needle(win, x, y, '-', r*1.00, 0*(M_PI/2));
    draw_clock_needle(win, x, y, '|', r*0.75, 1*(M_PI/2));
    draw_clock_needle(win, x, y, '-', r*1, 2*(M_PI/2));

    draw_clock_needle(win, x, y, '.', r*0.75, angleS);
    draw_clock_needle(win, x, y, 'm', r*0.50, angleM);
    draw_clock_needle(win, x, y, 'H', r*0.25, angleH);
    attron(A_BOLD);
    mvwaddch(win, y, x, '+');
    attroff(A_BOLD);
}

void UI_update_time(double n, int s) {
    TIME_COUNT += n;
    TIME_SEC = s;
}

void draw_gamewin() {
    werase(gamewin);

    int mx, my;
    getmaxyx(gamewin, my, mx);

    GEntityType* entity;

    for (int x=0; x<=mx; x++) {
        for (int y=0; y<=my; y++) {
            entity = game_world_getxy(x, y);
            wattron(gamewin, COLOR_PAIR(entity->skin->id));
            mvwaddch(gamewin, y, x, entity->skin->character);
        }
    }

    box(gamewin, 0, 0);
    wrefresh(gamewin);
}

void draw_uiwin() {
    werase(uiwin);
    draw_rt_clock(uiwin, 10, 5, COLS*.05);
    box(uiwin, 0, 0);
    wrefresh(uiwin);
}

void draw_main_menu() {
    draw_gamewin();
    draw_uiwin();
}

int init_colors() {
    Skin* skin = game_getskins();

    int color_id = 0;
    while (skin != NULL) {
        int bg = color_id++;
        int fg = color_id++;

        init_color(bg, RGB_TO_CURSES(skin->bg_r), RGB_TO_CURSES(skin->bg_g), RGB_TO_CURSES(skin->bg_b));
        init_color(fg, RGB_TO_CURSES(skin->fg_r), RGB_TO_CURSES(skin->fg_g), RGB_TO_CURSES(skin->fg_b));

        init_pair(skin->id, fg, bg);
        skin = (Skin*) skin->node.next;
    }

    return 1;
}

int UI_init() {
    initscr();
    start_color();
    raw();
    curs_set(0);
    noecho();
    getmaxyx(stdscr, LINES, COLS);

    gamewin = newwin(LINES*.6, COLS*.6, LINES * .05, COLS*.05);
    uiwin = newwin(LINES*.2, COLS*.8, LINES * .8, COLS*.05);

    int status = game_init();
    if (status != 0) {
        printf("Error initializing game: %d\n", status);
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
    return 0;
}

int UI_exit() {
    endwin();
    free(MENU_STACK);
    return 1;
}

