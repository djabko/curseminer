#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <ncurses.h>

#include "globals.h"
#include "util.h"
#include "stack64.h"
#include "core_game.h"
#include "frontend.h"
#include "frontends/ncurses.h"
#include "games/curseminer.h"
#include "games/other.h"

#define RGB_TO_CURSES(x) ((int)((float)x*3.90625))  // 1000/256 conversion
#define NAME_MAX 64
#define GLYPH_MAX 256
#define REFRESH_RATE 120

// TODO: Read from file or define via function
#define GLYPHSET_00_NAME    "tiles_00"
#define GLYPHSET_01_NAME    "tiles_01"
#define GLYPHSET_02_NAME    "tiles_02"
#define GLYPHSET_00         " *o&f.DM()-=r'"
#define GLYPHSET_01         " 123456789abcd"
#define GLYPHSET_02         " @~#^*+-0=xryz"

/* Drawing */

typedef void (*voidfunc) ();

typedef struct window_t {
    WINDOW *win;
    Queue64 *draw_qu;
    int glyph;
    event_ctx_t input_context;

    struct window_t *parent;
    void (*draw_func)(struct window_t*);

    int x, y, w, h;
    char title[NAME_MAX];
    int active, hidden;
} window_t;

typedef struct {
    int count, max;
    Queue64 *window_qu;
    window_t *mempool;
} windowmgr_t;

int LINES;
int COLS;
static window_t *g_widgetwin;
static windowmgr_t WINDOW_MGR;
static NoiseLattice *LATTICE1D;
static Stack64* MENU_STACK;
static milliseconds_t TIME_MSEC;
static bool g_glyph_init[GLYPH_MAX];
static char *g_glyph_charset;
HashTable *g_glyph_charsets;

Skin g_win_skin_0 =     {.glyph = GLYPH_MAX-1, .bg_r=0, .bg_g=0, .bg_b=0, .fg_r=215, .fg_g=215, .fg_b=50};
Skin g_win_skin_1 =     {.glyph = GLYPH_MAX-2, .bg_r=0, .bg_g=0, .bg_b=0, .fg_r=80, .fg_g=240, .fg_b=220};
Skin g_win_skin_2 =     {.glyph = GLYPH_MAX-3, .bg_r=0, .bg_g=0, .bg_b=0, .fg_r=120, .fg_g=6, .fg_b=2};

static inline char sign_of_int(int x) {
    if      (x == 0) return ' ';
    else if (x < 0)  return '-';
    else if (x > 0)  return '+';
    return '\0';
}


/* Utility Functions */
#define assert_ncurses(condition, ...)      \
    if (!(condition)) {                     \
        _log_debug("ERROR: ");              \
        log_debug(__VA_ARGS__);             \
        frontend_ncurses_exit(-1);          \
    }


int COLORS_COUNT = 0;
int new_glyph(Skin *skin) {
    int bg = COLORS_COUNT++;
    int fg = COLORS_COUNT++;
    int err = 0;

    err += init_color(bg, RGB_TO_CURSES(skin->bg_r), RGB_TO_CURSES(skin->bg_g), RGB_TO_CURSES(skin->bg_b));
    err += init_color(fg, RGB_TO_CURSES(skin->fg_r), RGB_TO_CURSES(skin->fg_g), RGB_TO_CURSES(skin->fg_b));

    err += init_pair(skin->glyph, fg, bg);

    g_glyph_init[skin->glyph] = true;

    log_debug("Initialized pair: %d bg(%d %d %d) fg(%d %d %d)",
            skin->glyph, skin->bg_r, skin->bg_g, skin->bg_b,
            skin->fg_r, skin->fg_g, skin->fg_b);

    return err;
}


/* Input Handler Functions*/
static double WIDGET_WIN_C = 1;
static double WIDGET_WIN_O = 1;
static int WIDGET_WIN_R = 10;

static void ui_input_widget_toggle(InputEvent *ie) {
    if (ie->state == ES_UP) {
        
        GLOBALS.input_context =
            GLOBALS.input_context == E_CTX_GAME ? E_CTX_NOISE : E_CTX_GAME;
    }
}

static void ui_input_noise_zoomin(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        WIDGET_WIN_C += 0.1;
}

static void ui_input_noise_zoomout(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        WIDGET_WIN_C -= 0.1;
}

static void ui_input_noise_mover(InputEvent *ie) {
    if (ie->state == ES_DOWN) {

        if (ie->mods & E_MOD_0) {
            g_widgetwin->draw_func =
                (void(*)(window_t*)) g_widgetwin->draw_qu->mempool
                                    [g_widgetwin->draw_qu->head];
            
            GLOBALS.input_context = E_CTX_CLOCK;

        } else WIDGET_WIN_O += 0.1;
    }
}

static void ui_input_noise_movel(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        WIDGET_WIN_O -= 0.1;
}

static void ui_input_clock_zoomin(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        WIDGET_WIN_R += 1;
}

static void ui_input_clock_zoomout(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        WIDGET_WIN_R -= 1;
}

static void ui_input_clock_move(InputEvent *ie) {
    if (ie->state == ES_DOWN && ie->mods & E_MOD_0) {

        g_widgetwin->draw_func =
            (void(*)(window_t*)) g_widgetwin->draw_qu->mempool
                                [g_widgetwin->draw_qu->tail];

        GLOBALS.input_context = E_CTX_NOISE;
    }
}


/* Draw Functions */
static void draw_fill(WINDOW* win, char c, int x1, int y1, int x2, int y2) {
    for (int x=x1; x<=x2; x++) {
        for (int y=y1; y<=y2; y++) {
            mvwaddch(win, y, x, c);
        }
    }
}

static void draw_line(WINDOW* win, const chtype c, int x1, int y1, int x2, int y2) {
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

static void _draw_square(WINDOW *win, int x1, int y1, int x2, int y2,
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

static void draw_square(WINDOW *win, int x1, int y1, int x2, int y2) {
   _draw_square(win, x1, y1, x2, y2,
            ACS_HLINE, ACS_HLINE, ACS_VLINE, ACS_VLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
}

static void box_win(window_t *window) {
    wattron(stdscr, COLOR_PAIR(window->glyph));

    mvwprintw(stdscr, window->y-2, window->x, window->title);

    draw_square(stdscr,
            window->x - 1, window->y - 1,
            window->x + window->w,
            window->y + window->h);

    wattroff(stdscr, COLOR_PAIR(window->glyph));
}

static void box_win_clear(window_t * window) {
    chtype spc = ' ';

    mvwprintw(stdscr, window->y-2, window->x, "                      ");

    _draw_square(stdscr,
            window->x - 1, window->y - 1,
            window->x + window->w,
            window->y + window->h,
            spc, spc, spc, spc, spc, spc, spc, spc);
}

static void draw_clock_needle(WINDOW* win, double x1, double y1, char c, double d, double angle) {
    double x2, y2;

    x2 = x1 + d * cos(angle);
    y2 = y1 + d * sin(angle);

    draw_line(win, c, x1, y1, x2, y2); 
}

static void draw_rt_clock(WINDOW* win, int x, int y, int r) {
    seconds_t time_sec = TIME_MSEC / 1000;

    time_t sc = time_sec % 60;
    time_t mn = time_sec % (60*60);
    time_t hr = (time_sec) % (60*60*12);

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

static void draw_keyboard_state(WINDOW* scr, int x, int y) {

}

//int SKIPPED_UPDATES = 0;
static void draw_gamewin(window_t *gamewin) {
    EntityType* type;

    DirtyFlags *df = GLOBALS.game->cache_dirty_flags;

    // No tiles to draw
    if (df->command == 0) {
        return;

    // Draw only dirty tiles
    } else if (df->command == 1) {

        size_t s = df->stride;
        int maxx = GLOBALS.view_port_maxx;

        for (int i = 0; i < s; i++) {
            if (df->groups[i]) {

                for (int j = 0; j < s; j++) {

                    int index = i * s + j;
                    byte_t flag = df->flags[index];

                    if (flag == 1) {

                        int x = index % maxx;
                        int y = index / maxx;

                        type = game_world_getxy(GLOBALS.game, x, y);

                        int glyph = type->default_skin->glyph;

                        if (!g_glyph_init[glyph]) new_glyph(type->default_skin);

                        wattron(gamewin->win, COLOR_PAIR(glyph));
                        mvwaddch(gamewin->win, y, x, g_glyph_charset[glyph]);
                        game_set_dirty(GLOBALS.game, x, y, 0);
                        wattroff(gamewin->win, COLOR_PAIR(glyph));
                    }
                }
            }
        }

    // Update all tiles
    } else if (df->command == -1) {
        for (int y=0; y < gamewin->h; y++) {
            for (int x=0; x < gamewin->w; x++) {

                type = game_world_getxy(GLOBALS.game, x, y);

                int glyph = type->default_skin->glyph;

                if (!g_glyph_init[glyph]) new_glyph(type->default_skin);

                wattron(gamewin->win, COLOR_PAIR(glyph));
                mvwaddch(gamewin->win, y, x, g_glyph_charset[glyph]);
                wattroff(gamewin->win, COLOR_PAIR(glyph));
            }
        }

        game_flush_dirty(GLOBALS.game);
    }

    df->command = 0;

    wnoutrefresh(gamewin->win);
}

static void draw_uiwin(window_t *uiwin) {
    werase(uiwin->win);

    draw_rt_clock(uiwin->win, uiwin->h/2+2, uiwin->h/2, uiwin->h/2);

    mvwprintw(uiwin->win, 5, 20, "Player: (%d, %d) [%c%d, %c%d]",
            GLOBALS.player->y, GLOBALS.player->x,
            sign_of_int(GLOBALS.player->vy),
            abs(GLOBALS.player->vy),
            sign_of_int(GLOBALS.player->vx),
            abs(GLOBALS.player->vx)
            );

    EntityType* entity = game_world_getxy(GLOBALS.game, 1, 1);
    mvwprintw(uiwin->win, 7, 20, "Entity Type at (1,1): %d", entity->id);

    mvwprintw(uiwin->win, (TIME_MSEC/100)%uiwin->h, uiwin->w/2, "!"); // draw splash icon

    mvwprintw(uiwin->win, 3, (int)COLS*.5, "Memory used: %lu/%lu", GLOBALS.game->world->chunk_mem_used, GLOBALS.game->world->chunk_mem_max);

    draw_keyboard_state(uiwin->win, (int)COLS*.5, 5);
    wnoutrefresh(uiwin->win);
}

static void draw_invwin(window_t *invwin) {
    // TODO: check if inventory changed

    if (GLOBALS.player->inventory == NULL) return;

    werase(invwin->win);

    for (int i = 0; i <= 10; i++) {

        if (i == GLOBALS.player->inventory_index)
            wattron(invwin->win, COLOR_PAIR(0));
        else
            wattron(invwin->win, COLOR_PAIR(i));

        mvwprintw(invwin->win, i, 1, "%d. %d (%d)",
                i, i, GLOBALS.player->inventory[i]);

        mvwprintw(stdscr, invwin->y, invwin->x+invwin->w+2, "index=%d",
                GLOBALS.player->inventory_index);
    }

    wnoutrefresh(invwin->win);
}

static void draw_widgetwin_rt_clock(window_t *widgetwin) {
    werase(widgetwin->win);

    draw_rt_clock(widgetwin->win, widgetwin->w/2, widgetwin->h/2, WIDGET_WIN_R);

    wnoutrefresh(widgetwin->win);
}

static void draw_widgetwin_noise(window_t *widgetwin, double (noise_func)(NoiseLattice*, double)) {
    if (!widgetwin->active || widgetwin->hidden) return;
    werase(widgetwin->win);

    double c = WIDGET_WIN_C;
    double o = WIDGET_WIN_O;
    //double rescale_factor = 1;
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

    wnoutrefresh(widgetwin->win);
}

static void draw_widgetwin_value_noise(window_t *widgetwin) {
    draw_widgetwin_noise(widgetwin, value_noise_1D);
}

static void draw_widgetwin_perlin_noise(window_t *widgetwin) {
    draw_widgetwin_noise(widgetwin, perlin_noise_1D);
}

static void draw_main_menu() {
    Queue64* qu = WINDOW_MGR.window_qu;

    qu_foreach(qu, window_t*, win) {
        if (!(win->active) || win->hidden) continue;

        win->draw_func(win);
    }
}

/* Init Functions */
static int init_colors() {
    for (int i = 0; i < GLYPH_MAX; i++) {
        g_glyph_init[i] = false;
    }

    return 0;
}

static int init_glyphsets() {
    g_glyph_charsets = ht_init(1);

    log_debug("Inserting %s with key=%lu", GLYPHSET_00_NAME, ht_hash(GLYPHSET_00_NAME));
    ht_insert(g_glyph_charsets, ht_hash(GLYPHSET_00_NAME), (uint64_t) GLYPHSET_00);
    ht_insert(g_glyph_charsets, ht_hash(GLYPHSET_01_NAME), (uint64_t) GLYPHSET_01);
    ht_insert(g_glyph_charsets, ht_hash(GLYPHSET_02_NAME), (uint64_t) GLYPHSET_02);
}

int init_window(window_t *window, window_t *parent, event_ctx_t ectx, int x, int y, int w, int h, const char *title, int glyph) {
    window->win = newwin(h, w, y, x);
    window->parent = parent;
    window->draw_qu = qu_init(1);
    window->draw_func = (void (*)(window_t*)) (window->draw_qu);
    window->input_context = ectx;
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->glyph = glyph;
    window->hidden = 0;
    window->active = 1;
    strncpy(window->title, title, NAME_MAX);

    return NULL < (void*) window->win;
}

static int window_insert_draw_func(window_t *win, void (draw_func)(window_t*)) {
    if (!win) return -1;

    qu_enqueue(win->draw_qu, (uint64_t) draw_func);
    win->draw_func = draw_func;

    return 1;
}

static void free_window(window_t *window) {
    free(window->draw_qu);
}

static int window_mgr_init(int size) {
    WINDOW_MGR.count = 0;
    WINDOW_MGR.max = size;
    WINDOW_MGR.window_qu = qu_init(size);
    WINDOW_MGR.mempool = calloc(sizeof(window_t), size);

    return WINDOW_MGR.mempool != NULL;
}

window_t *window_mgr_add(window_t *parent, event_ctx_t ectx, int x, int y, int w, int h, const char* title, Skin *skin) {
    if (WINDOW_MGR.count >= WINDOW_MGR.max) return NULL;

    if (!g_glyph_init[skin->glyph]) new_glyph(skin);

    window_t *win = &WINDOW_MGR.mempool[ WINDOW_MGR.count++ ];
    init_window(win, parent, ectx, x, y, w, h, title, skin->glyph);
    qu_enqueue(WINDOW_MGR.window_qu, (uint64_t) win);

    return win;
}

void window_mgr_redraw_visible() {
    qu_foreach(WINDOW_MGR.window_qu, window_t*, w) {
        if (w->active && !w->hidden)
            box_win(w);
    }
}

void window_mgr_free() {
    qu_foreach(WINDOW_MGR.window_qu, window_t*, win) {
        free_window(win);
    }

    free(WINDOW_MGR.window_qu);
    free(WINDOW_MGR.mempool);
    memset(&WINDOW_MGR, 0, sizeof(WINDOW_MGR));
}

void frontend_ncurses_exit() {
    window_mgr_free();
    endwin();
    free(MENU_STACK);
}


/* Public functions used by other components */
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

int job_loop(Task *task, Stack64 *st) {
    if (st_peek(MENU_STACK) == -1) return -1;
    voidfunc draw_func = (voidfunc) st_peek(MENU_STACK);
    draw_func();

    doupdate();

    tk_sleep(task, REFRESH_RATE / 1000);

    return 0;
}



/* Input Handling*/

#define msec_to_nsec(ms) ms * 1000000
#define g_keyup_delay 250

typedef struct {
    event_t id;
    milliseconds_t last_pressed;
} KeyDownState;

#define KDS_MAX 3
KeyDownState KEY_DOWN_STATES_ARRAY[KDS_MAX];
KeyDownState *NEXT_AVAILABLE_KDS = KEY_DOWN_STATES_ARRAY;

#define NCURSES_KBMAP_MAX 512
#define NCURSES_MSMAP_MAX 2048

static event_t g_ncurses_mapping_kb[ NCURSES_KBMAP_MAX ];
static InputEvent g_ncurses_mapping_ms[ NCURSES_MSMAP_MAX ];
static event_t g_ncurses_mapping_fn[ NCURSES_KBMAP_MAX ];

static timer_t g_input_timer = 0;
static Queue64 *g_queued_kdown = NULL;

static void init_keys_ncurses() {
    for (int i = 0; i < NCURSES_KBMAP_MAX; i++) 
        g_ncurses_mapping_kb[i] = E_NULL;

    for (int i = 0; i < NCURSES_MSMAP_MAX; i++) 
        g_ncurses_mapping_ms[i] = (InputEvent){0, 0, 0, 0, 0};

    for (int c = 'a'; c <= 'z'; c++)
        g_ncurses_mapping_kb[c] = E_KB_A + c - 'a';

    for (int i = 1; i <= 12; i++) {
        g_ncurses_mapping_kb[KEY_F(i)] = E_KB_F1 + i - 1;
    }

    g_ncurses_mapping_kb[ KEY_UP ] = E_KB_UP;
    g_ncurses_mapping_kb[ KEY_DOWN ] = E_KB_DOWN;
    g_ncurses_mapping_kb[ KEY_LEFT ] = E_KB_LEFT;
    g_ncurses_mapping_kb[ KEY_RIGHT ] = E_KB_RIGHT;

    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].id = E_MS_LMB;
    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON1_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].id = E_MS_MMB;
    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON2_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].id = E_MS_RMB;
    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].state = ES_DOWN;
    g_ncurses_mapping_ms[ BUTTON3_PRESSED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].id = E_MS_LMB;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON1_RELEASED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].id = E_MS_MMB;
    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON2_RELEASED ].mods = E_NOMOD;

    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].id = E_MS_RMB;
    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].state = ES_UP;
    g_ncurses_mapping_ms[ BUTTON3_RELEASED ].mods = E_NOMOD;

    int states[] = {BUTTON1_PRESSED, BUTTON2_PRESSED, BUTTON3_PRESSED,
        BUTTON1_RELEASED, BUTTON2_RELEASED, BUTTON3_RELEASED, -1};

    for (int *p=states; *p != -1; p++) {
        g_ncurses_mapping_ms[*p].type = E_TYPE_MS;
    }
}

static void map_event_ncurses(InputEvent *ev, int key) {

    if (key == KEY_MOUSE) {
        MEVENT mouse;
        if (getmouse(&mouse) != OK) log_debug("ERROR GETTING MOUSE EVENT!");

        *ev = g_ncurses_mapping_ms[mouse.bstate];

        frontend_pack_event(ev, mouse.x, mouse.y, mouse.z, 0);

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

static void handle_kdown_ncurses(int signo) {
    InputEvent ev;

    int key = getch();

    if (key == -1) return;

    event_ctx_t ctx = GLOBALS.input_context;
    map_event_ncurses(&ev, key);

    if (ev.type == E_TYPE_KB) {

        ev.state = ES_DOWN;

        // Reset keyup timer
        struct itimerspec its = {0};

        its.it_value.tv_nsec = msec_to_nsec(g_keyup_delay);

        timer_settime(g_input_timer, 0, &its, NULL);

        /* Process all pending ES_UP events and ensure there is space for
           this event. */
        if (!qu_empty(g_queued_kdown)) {

            for (int i = 0; i < g_queued_kdown->count; i++) {

                KeyDownState *kds = (KeyDownState*) qu_next(g_queued_kdown);

                bool is_expired = TIMER_NOW_MS >= kds->last_pressed + g_keyup_delay;
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

                    frontend_dispatch_event(ctx, &ev_up);
                }
            }
        }

        KeyDownState new_kds = {.id = ev.id, .last_pressed = TIMER_NOW_MS};
        KeyDownState *p = KEY_DOWN_STATES_ARRAY;
        int c = g_queued_kdown->count;

        /* If this event is already in the KDS array just update it with the
           new KeyDownState.last_pressed value. */
        if      (0 < c && new_kds.id == p[0].id) p[0] = new_kds;
        else if (1 < c && new_kds.id == p[1].id) p[1] = new_kds;
        else if (2 < c && new_kds.id == p[2].id) p[2] = new_kds;
        else {
            p = NEXT_AVAILABLE_KDS++;
            *p = new_kds;
            qu_enqueue(g_queued_kdown, (uint64_t) p);
        }
    }

    frontend_dispatch_event(ctx, &ev);
}

static void handle_kup_ncurses(int signo) {
    event_ctx_t ctx = GLOBALS.input_context;

    while (!qu_empty(g_queued_kdown)) {
        KeyDownState *kds = (KeyDownState*) qu_dequeue(g_queued_kdown);

        InputEvent ev_up = {
            .id = kds->id,
            .type = E_TYPE_KB,
            .state = ES_UP,
            .mods = E_NOMOD,
        };

        frontend_dispatch_event(ctx, &ev_up);

        NEXT_AVAILABLE_KDS--;
    }
}

static int charset_normalize_name(char *dst, const char *src, char target) {
    int len = strlen(src);
    len = NAME_MAX < len ? NAME_MAX : len;
    
    int i = len - 1;
    for (; 0 <= i; i--) {
        if (src[i] == target) break;
    }

    strncpy(dst, src, i - 1);
    dst[i] = '\0';

    return i;
}

static bool set_glyphset(const char *name) {
    char buf[NAME_MAX];
    unsigned long key = ht_hash(name);
    uint64_t result = ht_lookup(g_glyph_charsets, key);

    if (result == -1)  {
        log_debug("Failed to find charset: %s", name);
        strncpy(buf, name, NAME_MAX);
        charset_normalize_name(buf, name, '.');

        name = buf;
        key = ht_hash(name);
        result = ht_lookup(g_glyph_charsets, key);
    }

    if (result != -1) {
        g_glyph_charset = (const char*) result;

        // Don't reset g_win_skin_2 or the others
        for (int i = 0; i < GLYPH_MAX - 3; i++)
            g_glyph_init[i] = false;

        clear();
        window_mgr_redraw_visible();

        return true;
    }

    log_debug("Failed to find charset: %s", name);
    return false;
}



/* External APIs */
int frontend_ncurses_ui_init(Frontend* fr, const char *title) {
    fr->f_set_glyphset = set_glyphset;

    MENU_STACK = st_init(16);

    initscr();
    start_color();
    raw();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, 1);
    keypad(stdscr, TRUE);
    mouseinterval(0);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    ESCDELAY = 0;

    getmaxyx(stdscr, LINES, COLS);

    st_push(MENU_STACK, (uint64_t) draw_main_menu);

    int gwx, gwy, gww, gwh, uwx, uwy, uww, uwh, iwx, iwy, iww, iwh;
    gwx = COLS  * .05 - 1;
    gwy = LINES * .08;
    gww = COLS  * .6;
    gwh = LINES * .6;

    uwx = COLS  * .05 - 1;
    uwy = LINES * .8 - 1;
    uww = COLS  * .8 - 1;
    uwh = LINES * .2 - 1;

    iwx = gwx + gww + 2;
    iwy = gwy;
    iww = 10;
    iwh = 10;

    window_t *gamewin, *uiwin, *invwin;

    window_mgr_init(4);
    gamewin     = window_mgr_add(NULL, E_CTX_GAME, gwx, gwy, gww, gwh, "Game", &g_win_skin_0);
    uiwin       = window_mgr_add(NULL, E_CTX_GAME, uwx, uwy, uww, uwh, "UI", &g_win_skin_1);
    invwin      = window_mgr_add(NULL, E_CTX_GAME, iwx, iwy, iww, iwh, "Inventory", &g_win_skin_1);
    g_widgetwin = window_mgr_add(gamewin, E_CTX_NOISE, gwx+2, gwy+2, gww, gwh, "Widget", &g_win_skin_2);

    assert_ncurses(gamewin && uiwin && invwin && g_widgetwin,
            "Failed to initialize windows: game<%p> ui<%p> inv<%p> widget<%p>",
            gamewin, uiwin, invwin, g_widgetwin);

    window_insert_draw_func(gamewin,        draw_gamewin);
    window_insert_draw_func(uiwin,          draw_uiwin);
    window_insert_draw_func(invwin,         draw_invwin);
    window_insert_draw_func(g_widgetwin,    draw_widgetwin_rt_clock);
    window_insert_draw_func(g_widgetwin,    draw_widgetwin_perlin_noise);

    frontend_register_event(E_KB_G, E_CTX_GAME, ui_input_widget_toggle);
    frontend_register_event(E_KB_G, E_CTX_NOISE, ui_input_widget_toggle);
    frontend_register_event(E_KB_G, E_CTX_CLOCK, ui_input_widget_toggle);

    frontend_register_event(E_KB_W, E_CTX_NOISE, ui_input_noise_zoomin);
    frontend_register_event(E_KB_S, E_CTX_NOISE, ui_input_noise_zoomout);
    frontend_register_event(E_KB_A, E_CTX_NOISE, ui_input_noise_movel);
    frontend_register_event(E_KB_D, E_CTX_NOISE, ui_input_noise_mover);

    frontend_register_event(E_KB_W, E_CTX_CLOCK, ui_input_clock_zoomin);
    frontend_register_event(E_KB_S, E_CTX_CLOCK, ui_input_clock_zoomout);
    frontend_register_event(E_KB_A, E_CTX_CLOCK, ui_input_clock_move);
    frontend_register_event(E_KB_D, E_CTX_CLOCK, ui_input_clock_move);

    GLOBALS.view_port_x = gwx;
    GLOBALS.view_port_y = gwy;
    GLOBALS.view_port_maxx = gww;
    GLOBALS.view_port_maxy = gwh;
    GLOBALS.tile_w = 1;
    GLOBALS.tile_h = 1;

    g_widgetwin->active = 0;

    init_colors();
    init_glyphsets();

    box_win(gamewin);
    box_win(uiwin);
    box_win(invwin);

    wnoutrefresh(stdscr);

    schedule(GLOBALS.runqueue, 0, 0, job_loop, NULL);

    LATTICE1D = noise_init(100, 1, 100, smoothstep);

    return 1;
}

void frontend_ncurses_ui_exit(Frontend* fr) {
    frontend_ncurses_exit();
}

int frontend_ncurses_input_init(Frontend* fr, const char*) {
    
    /* 1. Initialized data structures */
    init_keys_ncurses();
    g_queued_kdown = qu_init(1);

    struct sigevent se = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = SIGALRM,
    };

    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    /* 2. Create timer to simulate ES_KEYUP events using SIGALRM interrupt */
    timer_create(CLOCK_MONOTONIC, &se, &g_input_timer);

    sa.sa_handler = handle_kup_ncurses;
    sigaction(SIGALRM, &sa, NULL);

    /* 3. Register SIGIO interrupt to trigger when input is available on stdin
     *    and process it in handle_kdown_ncurses */
    sa.sa_handler = handle_kdown_ncurses;
    sigaction(SIGIO, &sa, NULL);

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC);
    fcntl(STDIN_FILENO, F_SETOWN, getpid());


    set_glyphset(GLYPHSET_00_NAME);

    return 1;
}

void frontend_ncurses_input_exit(Frontend* fr) {}
