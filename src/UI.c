#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ncurses.h"

#include "globals.h"
#include "util.h"
#include "stack64.h"
#include "UI.h"
#include "game.h"
#include "keyboard.h"

#define RGB_TO_CURSES(x) ((int)((float)x*3.90625))  // 1000/256 conversion
#define MAX_TITLE 32

typedef void (*voidfunc) ();

typedef struct window_t {
    WINDOW *win;
    Queue64 *draw_qu;
    SkinTypeID color;
    struct window_t *parent;

    void (*draw_func)(struct window_t*);

    int x, y, w, h;
    char title[MAX_TITLE];
    int active, hidden;
} window_t;

typedef struct {
    int count, max;
    Queue64 *window_qu;
    window_t *mempool;
} windowmgr_t;

window_t *g_widgetwin;
windowmgr_t WINDOW_MGR = {};
NoiseLattice *LATTICE1D;

Stack64* MENU_STACK = NULL;
int LINES = 0;
int COLS = 0;

miliseconds_t TIME_MSEC = 0;

static inline char sign_of_int(int x) {
    if      (x == 0) return ' ';
    else if (x < 0)  return '-';
    else if (x > 0)  return '+';
    return '\0';
}


/* Draw Functions */
void draw_fill(WINDOW* win, char c, int x1, int y1, int x2, int y2) {
    for (int x=x1; x<=x2; x++) {
        for (int y=y1; y<=y2; y++) {
            mvwaddch(win, y, x, c);
        }
    }
}

void draw_line(WINDOW* win, const chtype c, int x1, int y1, int x2, int y2) {
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

void _draw_square(WINDOW *win, int x1, int y1, int x2, int y2,
        chtype l1, chtype l2, chtype l3, chtype l4,
        chtype c1, chtype c2, chtype c3, chtype c4) {

    mvwaddch(stdscr, y1, x1, c1);
    mvwaddch(stdscr, y1, x2, c2);
    mvwaddch(stdscr, y2, x1, c3);
    mvwaddch(stdscr, y2, x2, c4);

    draw_line(win, l1, x1+1, y1, x2-1, y1);
    draw_line(win, l2, x1+1, y2, x2-1, y2);
    draw_line(win, l3, x1, y1+1, x1, y2-1);
    draw_line(win, l4, x2, y1+1, x2, y2-1);
 
}

void draw_square(WINDOW *win, int x1, int y1, int x2, int y2) {
   _draw_square(win, x1, y1, x2, y2,
            ACS_HLINE, ACS_HLINE, ACS_VLINE, ACS_VLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
}

void box_win(window_t *window) {
    wattron(stdscr, COLOR_PAIR(window->color));
    mvwprintw(stdscr, window->y-2, window->x, window->title);
    draw_square(stdscr,
            window->x - 1, window->y - 1,
            window->x + window->w,
            window->y + window->h);
}

void box_win_clear(window_t * window) {
    chtype spc = ' ';

    mvwprintw(stdscr, window->y-2, window->x, "                      ");

    _draw_square(stdscr,
            window->x - 1, window->y - 1,
            window->x + window->w,
            window->y + window->h,
            spc, spc, spc, spc, spc, spc, spc, spc);
}

void draw_clock_needle(WINDOW* win, double x1, double y1, char c, double d, double angle) {
    double x2, y2;

    x2 = x1 + d * cos(angle);
    y2 = y1 + d * sin(angle);

    draw_line(win, c, x1, y1, x2, y2); 
}

void draw_rt_clock(WINDOW* win, int x, int y, int r) {
    seconds_t time_sec = TIME_MSEC / 1000;

    utime_t sc = time_sec % 60;
    utime_t mn = time_sec % (60*60);
    utime_t hr = (time_sec) % (60*60*12);

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

void draw_gamewin_nogui(window_t *gamewin) {
    for (int x=0; x <= gamewin->w; x++) {
        for (int y=0; y <= gamewin->h; y++) {
            EntityType *e = game_world_getxy(x, y); 
        }
    }
}

void draw_gamewin(window_t *gamewin) {
    EntityType* entity;

    for (int x=0; x <= gamewin->w; x++) {
        for (int y=0; y <= gamewin->h; y++) {

            entity = game_world_getxy(x, y);
            wattron(gamewin->win, COLOR_PAIR(entity->skin->id));
            mvwaddch(gamewin->win, y, x, entity->skin->character);
        }
    }

    wnoutrefresh(gamewin->win);
}

void draw_uiwin_nogui(window_t *uiwin) {
    EntityType *e = game_world_getxy(1, 1);
}

void draw_uiwin(window_t *uiwin) {
    werase(uiwin->win);

    draw_rt_clock(uiwin->win, uiwin->h/2+2, uiwin->h/2, uiwin->h/2);

    mvwprintw(uiwin->win, 5, 20, "Player: (%d, %d) [%c%d, %c%d]",
            GLOBALS.player->y, GLOBALS.player->x,
            sign_of_int(GLOBALS.player->vy),
            abs(GLOBALS.player->vy),
            sign_of_int(GLOBALS.player->vx),
            abs(GLOBALS.player->vx)
            );

    EntityType* entity = game_world_getxy(1, 1);
    mvwprintw(uiwin->win, 7, 20, "Entity Type at (1,1): %d", entity->id);

    mvwprintw(uiwin->win, (TIME_MSEC/100)%uiwin->h, uiwin->w/2, "!"); // draw splash icon

    mvwprintw(uiwin->win, 3, (int)COLS*.5, "Memory used: %lu/%lu", GLOBALS.game->world->chunk_mem_used, GLOBALS.game->world->chunk_mem_max);

    draw_keyboard_state(uiwin->win, (int)COLS*.5, 5);
    wnoutrefresh(uiwin->win);
}

void draw_widgetwin_nogui(window_t *widgetwin) {}

int WIDGET_WIN_R = 10;
void draw_widgetwin_rt_clock(window_t *widgetwin) {
    werase(widgetwin->win);

    draw_rt_clock(widgetwin->win, widgetwin->w/2, widgetwin->h/2, WIDGET_WIN_R);

    if (kb_down(KB_D)) WIDGET_WIN_R += .1;
    if (kb_down(KB_A)) WIDGET_WIN_R -= .1;

    wnoutrefresh(widgetwin->win);
}

double WIDGET_WIN_C = 1;
double WIDGET_WIN_O = 1;
void draw_widgetwin_noise(window_t *widgetwin, double (noise_func)(NoiseLattice*, double)) {
    if (!widgetwin->active || widgetwin->hidden) return;
    werase(widgetwin->win);

    double c = WIDGET_WIN_C;
    double o = WIDGET_WIN_O;
    double rescale_factor = 1;
    double frequency = 1;
    double amplitude = 1;
    double octaves = 1;
    int y = 0;

    for (int x=0; x<widgetwin->w; x++) {
        double _x = ((double) x) / c + o;
        double _y = 0;
        for (int i=1; i<=octaves; i++) _y += noise_func(LATTICE1D, _x * frequency * i) * amplitude;

        int new_y = (int) (_y * 10) + widgetwin->h/2;

        if (x > 0 && y != new_y)
            while (y != new_y) {
                int diff = new_y - y;
                if (diff < 0) y--;
                else y++;
                mvwaddch(widgetwin->win, y, x, '+');
            }

        else {
            y = new_y;
            mvwaddch(widgetwin->win, y, x, '+');
        }
    }

    if (kb_down(KB_D)) WIDGET_WIN_O += .1;
    if (kb_down(KB_A)) WIDGET_WIN_O -= .1;
    if (kb_down(KB_W)) WIDGET_WIN_C += .1;
    if (kb_down(KB_S) && WIDGET_WIN_C > 1) WIDGET_WIN_C -= .1;

    wnoutrefresh(widgetwin->win);
}

void draw_widgetwin_value_noise(window_t *widgetwin) {
    draw_widgetwin_noise(widgetwin, value_noise_1D);
}

void draw_widgetwin_perlin_noise(window_t *widgetwin) {
    draw_widgetwin_noise(widgetwin, perlin_noise_1D);
}

void draw_main_menu() {
    Queue64* qu = WINDOW_MGR.window_qu;

    qu_foreach(qu, window_t*, win) {
        if (!(win->active) || win->hidden) continue;

        win->draw_func(win);
    }
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

int init_window(window_t *window, window_t *parent, int x, int y, int w, int h, const char *title, SkinTypeID color) {
    window->win = newwin(h, w, y, x);
    window->parent = parent;
    window->draw_qu = qu_init(1);
    window->draw_func = (void (*)(window_t*)) (window->draw_qu);
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->color = color;
    window->hidden = 0;
    window->active = 1;
    strncpy(window->title, title, MAX_TITLE);

    return NULL < (void*) window->win;
}

int window_insert_draw_func(window_t *win, void (draw_func)(window_t*)) {
    if (!win) return -1;

    qu_enqueue(win->draw_qu, (uint64_t) draw_func);
    win->draw_func = draw_func;

    return 1;
}

void free_window(window_t *window) {
    free(window->draw_qu);
}

int window_mgr_init(int size) {
    WINDOW_MGR.count = 0;
    WINDOW_MGR.max = size;
    WINDOW_MGR.window_qu = qu_init(size);
    WINDOW_MGR.mempool = calloc(sizeof(window_t), size);

    return WINDOW_MGR.mempool != NULL;
}

window_t *window_mgr_add(window_t *parent, int x, int y, int w, int h, const char* title, SkinTypeID color) {
    if (WINDOW_MGR.count >= WINDOW_MGR.max) return NULL;

    window_t *win = &WINDOW_MGR.mempool[ WINDOW_MGR.count++ ];
    init_window(win, parent, x, y, w, h, title, color);
    qu_enqueue(WINDOW_MGR.window_qu, (uint64_t) win);

    return win;
}

void window_mgr_free() {
    qu_foreach(WINDOW_MGR.window_qu, window_t*, win) {
        free_window(win);
    }

    free(WINDOW_MGR.window_qu);
    free(WINDOW_MGR.mempool);
    memset(&WINDOW_MGR, 0, sizeof(WINDOW_MGR));
}


/* Public functions used by other components */
void UI_update_time(miliseconds_t msec) {
    TIME_MSEC = msec;
}

int UI_init(int nogui_mode) {
    MENU_STACK = st_init(16);

    if (!nogui_mode) {
        initscr();
        start_color();
        raw();
        cbreak();
        noecho();
        curs_set(0);
        nodelay(stdscr, 1);

        ESCDELAY = 25;

    }

    st_push(MENU_STACK, (uint64_t) draw_main_menu);

    getmaxyx(stdscr, LINES, COLS);

    int gwx, gwy, gww, gwh, uwx, uwy, uww, uwh;
    gwx = COLS  * .05 - 1;
    gwy = LINES * .08;
    gww = COLS  * .6;
    gwh = LINES * .6;

    uwx = COLS  * .05 - 1;
    uwy = LINES * .8 - 1;
    uww = COLS  * .8 - 1;
    uwh = LINES * .2 - 1;

    window_t *gamewin, *uiwin;

    window_mgr_init(4);
    gamewin    = window_mgr_add(NULL, gwx, gwy, gww, gwh, "Game", ge_player);
    uiwin      = window_mgr_add(NULL, uwx, uwy, uww, uwh, "UI", ge_diamond);
    g_widgetwin  = window_mgr_add(gamewin, gwx+2, gwy+2, gww, gwh, "Widget", ge_redore);

    window_insert_draw_func(gamewin,        nogui_mode ? draw_gamewin_nogui     : draw_gamewin);
    window_insert_draw_func(uiwin,          nogui_mode ? draw_uiwin_nogui       : draw_uiwin);
    window_insert_draw_func(g_widgetwin,    nogui_mode ? draw_widgetwin_nogui   : draw_widgetwin_rt_clock);
    window_insert_draw_func(g_widgetwin,    draw_widgetwin_perlin_noise);

    GLOBALS.view_port_maxx = gww - 1;
    GLOBALS.view_port_maxy = gwh - 1;

    int status = game_init();
    if (status != 0) {
        UI_exit();
        return 0;
    }

    init_colors();

    // These functions require colors initialized
    box_win(gamewin);
    box_win(uiwin);
    g_widgetwin->active = 0;

    wnoutrefresh(stdscr);

    LATTICE1D = noise_init(100, 1, 100, smoothstep);
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
    window_mgr_free();
    endwin();
    free(MENU_STACK);
    return 1;
}

void UI_show_window(window_t *win) {
    win->active = 1;
    if (win->parent) win->parent->active = 0;

    box_win(win);
}

void UI_hide_window(window_t *win) {
    box_win_clear(win);

    if (win->parent) win->parent->active = 1;
    win->active = 0;

    box_win(win);
}

void UI_toggle_widgetwin() {
    if (g_widgetwin->active) UI_hide_window(g_widgetwin);
    else UI_show_window(g_widgetwin);
}

