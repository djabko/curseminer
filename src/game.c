#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "globals.h"
#include "util.h"
#include "game.h"
#include "world.h"
#include "entity.h"
#include "stack64.h"

#define GAME_REFRESH_RATE 20

GameContext *g_game;

byte_t *WORLD_ENTITY_CACHE;
byte_t *GAME_ENTITY_CACHE;
DirtyFlags *GAME_DIRTY_FLAGS;

bool g_player_moving_changed = false;
bool g_player_moving_up = false;
bool g_player_moving_down = false;
bool g_player_moving_left = false;
bool g_player_moving_right = false;
bool g_player_crouching = false;

int world_from_mouse_xy(InputEvent *ie, int *world_x, int *world_y) {
    uint16_t x = ie->data >> 16 * 0;
    uint16_t y = ie->data >> 16 * 1;
    int gwx = GLOBALS.view_port_x;
    int gwy = GLOBALS.view_port_y;
    int gwmx = gwx + GLOBALS.view_port_maxx;
    int gwmy = gwy + GLOBALS.view_port_maxy;
    int wvx = GLOBALS.game->world_view_x;
    int wvy = GLOBALS.game->world_view_y;

    // Mouse outside game window boundaries
    if (!(gwx <= x && gwy <= y && x <= gwmx && y <= gwmy)) return -1;

    *world_x = x - gwx + wvx;
    *world_y = y - gwy + wvy;

    return 0;
}

void game_input_player_state(InputEvent *ie, bool *on_kd_true, bool *on_kd_reset) {
    if (ie->state == ES_DOWN) {
        *on_kd_true = true;
        *on_kd_reset = false;

    } else *on_kd_true = false;

    g_player_crouching = ie->mods && E_MOD_CROUCHING;

    g_player_moving_changed = true;
}

void game_input_move_up(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_up, &g_player_moving_down);
}

void game_input_move_left(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_left, &g_player_moving_right);
}

void game_input_move_down(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_down, &g_player_moving_up);
}

void game_input_move_right(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_right, &g_player_moving_left);
}

void game_input_place_tile(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_place);
}

void game_input_spawn_chaser(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int x, y;
        int s = (20 + rand()) % 150;
        EntityType* skin = g_game->entity_types + ENTITY_CHASER;

        if (world_from_mouse_xy(ie, &x, &y) != 0) return;

        Entity *e = entity_spawn(g_game->world, skin,
                x, y, ENTITY_FACING_RIGHT, 1, 0);

        e->speed = s;
    }
}

void game_input_break_tile(InputEvent *ie) {
    entity_command(GLOBALS.player, be_break);
}

void game_input_break_tile_mouse(InputEvent *ie) {
    // TODO must be moved outside interrupt handler logic, this is too slow
    if (ie->state == ES_DOWN) {
        int x, y;

        if (world_from_mouse_xy(ie, &x, &y) != 0) return;

        EntityType *id = game_world_getxy(x, y);
        int d = man_dist(GLOBALS.player->x, GLOBALS.player->y, x, y);

        bool is_tile_exists = id != E_TYPE_NULL;
        bool is_player_close = d < TILE_BREAK_DISTANCE;

        if (is_tile_exists && is_player_close)
            game_world_setxy(x, y, E_TYPE_NULL);
    }
}


/* Helper Functions */
int game_on_screen(int x, int y) {
    int minx = g_game->world_view_x;
    int miny = g_game->world_view_y;
    int maxx = minx + GLOBALS.view_port_maxx;
    int maxy = miny + GLOBALS.view_port_maxy;

    return minx <= x && miny <= y && x < maxx && y < maxy;
}

void game_cache_set(byte_t *cache, int x, int y, byte_t tid) {
    game_set_dirty(x, y, 1);
    cache[y * GLOBALS.view_port_maxx + x] = tid;
}

byte_t game_cache_get(byte_t *cache, int x, int y) {
    return cache[y * GLOBALS.view_port_maxx + x];
}

void gamew_cache_set(byte_t *cache, int x, int y, byte_t tid) {
    x -= g_game->world_view_x;
    y -= g_game->world_view_y;

    game_cache_set(cache, x, y, tid);
}

byte_t gamew_cache_get(byte_t *cache, int x, int y) {
    x -= g_game->world_view_x;
    y -= g_game->world_view_y;

    return game_cache_get(cache, x, y);
}

void flush_world_entity_cache() {
    for (int y = 0; y < GLOBALS.view_port_maxy; y++) {
        for (int x = 0; x < GLOBALS.view_port_maxx; x++) {

            int _x = x + g_game->world_view_x;
            int _y = y + g_game->world_view_y;
            int tid = world_getxy(g_game->world, _x, _y);

            WORLD_ENTITY_CACHE[y * GLOBALS.view_port_maxx + x] = tid;
        }
    }
}

void flush_game_entity_cache() {
    for (int y = 0; y < GLOBALS.view_port_maxy; y++)
        for (int x = 0; x < GLOBALS.view_port_maxx; x++)
            GAME_ENTITY_CACHE[y * GLOBALS.view_port_maxx + x] = 0;

    Heap *h = &g_game->world->entities->heap;

    int i = 0;
    while (i < h->count) {
        Entity *e = (Entity*) h->mempool[i].data;

        if (game_on_screen(e->x, e->y)) {

            int x = e->x - g_game->world_view_x;
            int y = e->y - g_game->world_view_y;

            GAME_ENTITY_CACHE[y * GLOBALS.view_port_maxx + x] = e->type->id;
        }

        i++;
    }
}

void game_init_dirty_flags() {
    DirtyFlags *df = GAME_DIRTY_FLAGS;
    byte_t *ptr = (byte_t*) df;
    size_t s = 64;
    size_t tiles_on_screen = GLOBALS.view_port_maxx * GLOBALS.view_port_maxy;

    assert_log(tiles_on_screen <= s * s,
            "screen too big for cache of %lu tiles", s * s);

    df->groups = (byte_t*) (ptr + s * 1);
    df->flags = (byte_t*) (ptr + s * 2);
    df->stride = s;
    df->groups_used = (tiles_on_screen + s - 1) / s;
    df->command = 0;
}

void game_set_dirty(int x, int y, int v) {
    DirtyFlags *df = GAME_DIRTY_FLAGS;

    if (df->command == -1) return;

    int maxx = GLOBALS.view_port_maxx;
    size_t offset = y * maxx + x;
    size_t s = df->stride;

    df->groups[offset / s] = v;
    df->flags[offset] = v;
    df->command = 1;
}

void game_flush_dirty() {
    DirtyFlags *df = GAME_DIRTY_FLAGS;
    byte_t* groups = df->groups;
    size_t s = df->stride;
    size_t gu = df->groups_used;

    for (int i = 0; i < gu; i++) {
        groups[i] = 0;

        for (int j = 0; j < s; j++) {
            (groups + i * s) [j] = 0;
        }
    }

    df->command = -1;
}

void create_skin(int id, char c,
        color_t bg_r, color_t bg_g, color_t bg_b,
        color_t fg_r, color_t fg_g, color_t fg_b) {

    if (g_game->skins_c == g_game->skins_max) {

        g_game->skins_max *= 2;

        size_t ns = g_game->skins_max * sizeof(Skin);
        g_game->skins = realloc(g_game->skins, ns);
    }
    
    Skin* skin = g_game->skins + g_game->skins_c++;

    skin->id = id;
    skin->character = c;

    skin->bg_r = bg_r;
    skin->bg_g = bg_g;
    skin->bg_b = bg_b;
    skin->fg_r = fg_r;
    skin->fg_g = fg_g;
    skin->fg_b = fg_b;
}

void create_entity_type(Skin* skin) {
    if (g_game->entity_types_c == g_game->entity_types_max) {
        g_game->entity_types_max *= 2;

        size_t ns = g_game->entity_types_max * sizeof(EntityType);
        g_game->entity_types = realloc(g_game->entity_types, ns);
    }

    EntityType* entitiyt = g_game->entity_types + g_game->entity_types_c;
    entitiyt->skin = skin;
    entitiyt->id = g_game->entity_types_c++;
}

int init_skins() {
    create_skin(SKIN_NULL,        ' ', 0, 0, 0, 255, 255, 255);
    create_skin(SKIN_DEFAULT,     '*', 0, 0, 0, 120, 120, 120);
    create_skin(SKIN_GOLD,        'o', 0, 0, 0, 255, 215,   0);
    create_skin(SKIN_DIAMOND,     '&', 0, 0, 0,  80, 240, 220);
    create_skin(SKIN_IRON,        'f', 0, 0, 0, 120, 120, 120);
    create_skin(SKIN_REDORE,      '.', 0, 0, 0, 120,   6,   2);
    create_skin(SKIN_PLAYER,      'D', 0, 0, 0, 255, 215,   0);
    create_skin(SKIN_CHASER,  'M', 0, 0, 0, 215, 215,   50);
    return 1;
}

void init_entity_types() {
    int c = g_game->skins_c;
    for (int i=0; i<c; i++)
        create_entity_type( g_game->skins + i );
    g_game->entity_types_c = c;
}

int game_update(Task* task, Stack64* stack) {
    PQueue64* entity_pq = g_game->world->entities;
    milliseconds_t next_tick = _pq_peek(entity_pq, 0);

    if (!pq_empty(entity_pq) && next_tick <= TIMER_NOW_MS) {

        while (_pq_peek(entity_pq, 0) <= TIMER_NOW_MS) {
            Entity *e = (Entity*) pq_dequeue(entity_pq);

            e->controller->tick(e);
            e->next_tick = TIMER_NOW_MS + e->speed * 10;

            pq_enqueue(entity_pq, e, e->next_tick);

            next_tick = _pq_peek(entity_pq, 0);
        }
    }

    Entity *plr = GLOBALS.player;
    int wvx = g_game->world_view_x;
    int wvy = g_game->world_view_y;
    int sth = g_game->scroll_threshold;
    int mxx = GLOBALS.view_port_maxx;
    int mxy = GLOBALS.view_port_maxy;

    byte_t leftb = plr->x < wvx + sth;
    byte_t topb = plr->y < wvy + sth;
    byte_t rightb = plr->x >= wvx + mxx - sth;
    byte_t bottomb = plr->y >= wvy + mxy - sth;

    if (leftb || topb || rightb || bottomb || g_player_moving_changed) {
        
        // Check for screen scrolling
        if      (leftb)      g_game->world_view_x--;
        else if (rightb)     g_game->world_view_x++;
        if      (topb)       g_game->world_view_y--;
        else if (bottomb)    g_game->world_view_y++;

        flush_world_entity_cache();
        flush_game_entity_cache();
        game_flush_dirty();

        // Check for player movement
        if (g_player_moving_changed) {
            Entity *player = GLOBALS.player;
            bool up = g_player_moving_up;
            bool down = g_player_moving_down;
            bool left = g_player_moving_left;
            bool right = g_player_moving_right;

            log_debug("%d %d %d %d", up, down, left, right);

            if (up || down || left || right) {

                if      (up && left)    entity_command(player, be_face_ul);
                else if (up && right)   entity_command(player, be_face_ur);
                else if (down && left)  entity_command(player, be_face_dl);
                else if (down && right) entity_command(player, be_face_dr);
                else if (up)            entity_command(player, be_face_up);
                else if (down)          entity_command(player, be_face_down);
                else if (left)          entity_command(player, be_face_left);
                else if (right)         entity_command(player, be_face_right);

                if (!player->moving) {

                    if (g_player_crouching)
                        entity_command(player, be_move_one);
                    else 
                        entity_command(player, be_move);
                }


            } else if (player->moving)
                entity_command(player, be_stop);

            g_player_moving_changed = false;
        }
    }

    tk_sleep(task, 1000 / GAME_REFRESH_RATE);

    return 0;
}

int game_init() {
    int status = 0;

    size_t stride = GLOBALS.view_port_maxx * GLOBALS.view_port_maxy;
    byte_t *tmp  = calloc(stride * 2 + sizeof(DirtyFlags) * 64, 1);
    GAME_ENTITY_CACHE   = tmp + stride * 0;
    WORLD_ENTITY_CACHE  = tmp + stride * 1;
    GAME_DIRTY_FLAGS    = (DirtyFlags*) (tmp + stride * 2);

    g_game = calloc(sizeof(GameContext), 1);
    g_game->skins_c = 0;
    g_game->entity_types_c = 0;
    g_game->skins_max = 32;
    g_game->entity_types_max = 32;
    g_game->skins = calloc(g_game->skins_max, sizeof(Skin));
    g_game->entity_types = calloc(g_game->entity_types_max, sizeof(EntityType));
    g_game->world_view_x = 0;
    g_game->world_view_y = 0;
    g_game->scroll_threshold = 5;

    init_skins();
    init_entity_types();
    g_game->world = world_init(20, g_game->skins_c - 1, PAGE_SIZE * 64);
    game_init_dirty_flags();

    flush_world_entity_cache();
    game_flush_dirty();

    int e_x, e_y;
    Entity *player, *entity;

    player = entity_spawn(g_game->world, g_game->entity_types + ENTITY_PLAYER,
            20, 20, ENTITY_FACING_RIGHT, 1, 0);

    player->speed = 1;
    entity_set_keyboard_controller(player);
    input_register_event(E_KB_UP,   E_CTX_GAME, game_input_move_up);
    input_register_event(E_KB_DOWN, E_CTX_GAME, game_input_move_down);
    input_register_event(E_KB_LEFT, E_CTX_GAME, game_input_move_left);
    input_register_event(E_KB_RIGHT,E_CTX_GAME, game_input_move_right);
    input_register_event(E_KB_W,    E_CTX_GAME, game_input_move_up);
    input_register_event(E_KB_A,    E_CTX_GAME, game_input_move_left);
    input_register_event(E_KB_S,    E_CTX_GAME, game_input_move_down);
    input_register_event(E_KB_D,    E_CTX_GAME, game_input_move_right);
    input_register_event(E_KB_C,    E_CTX_GAME, game_input_place_tile);
    input_register_event(E_KB_Z,    E_CTX_GAME, game_input_break_tile);
    input_register_event(E_MS_LMB,  E_CTX_GAME, game_input_spawn_chaser);
    input_register_event(E_MS_RMB,  E_CTX_GAME, game_input_break_tile_mouse);

    GLOBALS.player = player;
    GLOBALS.game = g_game;

    return status;
}

void game_free() {
    world_free(g_game->world);
    free(g_game->entity_types);
    free(g_game->skins);
    free(g_game);
}

GameContext *game_get_context() {
    return g_game;
}

EntityType *game_world_getxy(int x, int y) {
    int id = game_cache_get(GAME_ENTITY_CACHE, x, y);
    id = id > 0 ? id : game_cache_get(WORLD_ENTITY_CACHE, x, y);

    assert_log(ENTITY_AIR <= id && id < ENTITY_END,
            "attempting to access invalid id %d at position (%d,%d)", id, x, y);

    return g_game->entity_types + id;
}

EntityType *game_cache_getxy(int index) {
    int id = GAME_ENTITY_CACHE[index];
    id = id > 0 ? id : WORLD_ENTITY_CACHE[index];

    assert_log(index < GLOBALS.view_port_maxx * GLOBALS.view_port_maxy,
            "attempting to access invalid game screen cache ID at index=%d", index);

    assert_log(id < ENTITY_AIR || ENTITY_END <= id,
            "attempting to access invalid id %d at index %d", id, index);

    return g_game->entity_types + id;
}

int game_world_setxy(int x, int y, EntityTypeID tid) {
    world_setxy(g_game->world, x, y, tid);
    
    x -= g_game->world_view_x;
    y -= g_game->world_view_y;

    if (0 < x && 0 < y && x < GLOBALS.view_port_maxx && y < GLOBALS.view_port_maxy)
        game_cache_set(WORLD_ENTITY_CACHE, x, y, tid);
    
    return 0;
}

int game_cache_setxy(int index, EntityTypeID tid) {
    if (index < GLOBALS.view_port_maxx * GLOBALS.view_port_maxy) return -1;

    GAME_ENTITY_CACHE[index] = tid;

    return 0;
}
