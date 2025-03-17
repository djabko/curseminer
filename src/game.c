#include "stdlib.h"
#include <stdio.h>

#include <globals.h>
#include <game.h>
#include <world.h>
#include <entity.h>
#include <stack64.h>

#define GAME_REFRESH_RATE 20

GameContext *g_game;

byte *WORLD_ENTITY_CACHE;
byte *GAME_ENTITY_CACHE;
DirtyFlags *GAME_DIRTY_FLAGS;


void game_input_move_up(InputEvent *ie) {
    GLOBALS.player->vy = -1;
    qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_move);
}

void game_input_move_left(InputEvent *ie) {
    GLOBALS.player->vx = -1;
    qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_move);
}

void game_input_move_down(InputEvent *ie) {
    GLOBALS.player->vy = +1;
    qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_move);
}

void game_input_move_right(InputEvent *ie) {
    GLOBALS.player->vx = +1;
    qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_move);
}

void game_input_tile_place(InputEvent *ie) {
    qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_place);
}


/* Helper Functions */
int game_on_screen(int x, int y) {
    int minx = g_game->world_view_x;
    int miny = g_game->world_view_y;
    int maxx = minx + GLOBALS.view_port_maxx;
    int maxy = miny + GLOBALS.view_port_maxy;

    return minx <= x && miny <= y && x < maxx && y < maxy;
}

void game_cache_set(byte *cache, int x, int y, byte tid) {
    game_set_dirty(x, y, 1);
    cache[y * GLOBALS.view_port_maxx + x] = tid;
}

byte game_cache_get(byte *cache, int x, int y) {
    return cache[y * GLOBALS.view_port_maxx + x];
}

void gamew_cache_set(byte *cache, int x, int y, byte tid) {
    x -= g_game->world_view_x;
    y -= g_game->world_view_y;

    game_cache_set(cache, x, y, tid);
}

byte gamew_cache_get(byte *cache, int x, int y) {
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
    byte *ptr = (byte*) df;
    size_t s = 64;
    size_t tiles_on_screen = GLOBALS.view_port_maxx * GLOBALS.view_port_maxy;

    assert_log(tiles_on_screen <= s * s,
            "screen too big for cache of %lu tiles", s * s);

    df->groups = (byte*) (ptr + s * 1);
    df->flags = (byte*) (ptr + s * 2);
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
    byte* groups = df->groups;
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

    if (g_game->skins_c == g_game->skins_maxc) {

        g_game->skins_maxc *= 2;

        size_t ns = g_game->skins_maxc * sizeof(Skin);
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
    if (g_game->entity_types_c == g_game->entity_types_maxc) {
        g_game->entity_types_maxc *= 2;

        size_t ns = g_game->entity_types_maxc * sizeof(EntityType);
        g_game->entity_types = realloc(g_game->entity_types, ns);
    }

    EntityType* entitiyt = g_game->entity_types + g_game->entity_types_c;
    entitiyt->skin = skin;
    entitiyt->id = g_game->entity_types_c++;
}

int init_skins() {
    create_skin(sk_null,        ' ', 0, 0, 0, 255, 255, 255);
    create_skin(sk_default,     '*', 0, 0, 0, 120, 120, 120);
    create_skin(sk_gold,        'o', 0, 0, 0, 255, 215,   0);
    create_skin(sk_diamond,     '&', 0, 0, 0,  80, 240, 220);
    create_skin(sk_iron,        'f', 0, 0, 0, 120, 120, 120);
    create_skin(sk_redore,      '.', 0, 0, 0, 120,   6,   2);
    create_skin(sk_player,      'D', 0, 0, 0, 255, 215,   0);
    create_skin(sk_chaser_mob,  'M', 0, 0, 0, 215, 215,   50);
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

    byte leftb = plr->x < wvx + sth;
    byte topb = plr->y < wvy + sth;
    byte rightb = plr->x >= wvx + mxx - sth;
    byte bottomb = plr->y >= wvy + mxy - sth;

    if (leftb || topb || rightb || bottomb) {
        if      (leftb)      g_game->world_view_x--;
        else if (rightb)     g_game->world_view_x++;
        if      (topb)       g_game->world_view_y--;
        else if (bottomb)    g_game->world_view_y++;

        flush_world_entity_cache();
        flush_game_entity_cache();
        game_flush_dirty();
    }

    tk_sleep(task, 1000 / GAME_REFRESH_RATE);

    return 0;
}

void game_event_handler(InputEvent *ev) {
    Queue64 *bq = GLOBALS.player->controller->behaviour_queue;

    if (ev->id == E_KB_W) {
        GLOBALS.player->vy = -1;
        qu_enqueue(bq, be_move);

    } else if (ev->id == E_KB_S) {
        GLOBALS.player->vy = +1;
        qu_enqueue(bq, be_move);

    } else if (ev->id == E_KB_A) {
        GLOBALS.player->vx = -1;
        qu_enqueue(bq, be_move);

    } else if (ev->id == E_KB_D) {
        GLOBALS.player->vx = +1;
        qu_enqueue(bq, be_move);

    } else if (ev->id == E_KB_C) {
        qu_enqueue(bq, be_place);
    }
}

int game_init() {
    int status = 0;

    size_t stride = GLOBALS.view_port_maxx * GLOBALS.view_port_maxy;
    byte *tmp  = calloc(stride * 2 + sizeof(DirtyFlags) * 64, 1);
    GAME_ENTITY_CACHE   = tmp + stride * 0;
    WORLD_ENTITY_CACHE  = tmp + stride * 1;
    GAME_DIRTY_FLAGS    = (DirtyFlags*) (tmp + stride * 2);

    g_game = calloc(sizeof(GameContext), 1);
    g_game->skins_c = 0;
    g_game->entity_types_c = 0;
    g_game->skins_maxc = 32;
    g_game->entity_types_maxc = 32;
    g_game->skins = calloc(g_game->skins_maxc, sizeof(Skin));
    g_game->entity_types = calloc(g_game->entity_types_maxc, sizeof(EntityType));
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

    // Spawn some moving entities
    player = entity_spawn(g_game->world, g_game->entity_types + ge_player,
            20, 20, ENTITY_FACING_RIGHT, 1, 0);
    player->speed = 1;
    entity_set_keyboard_controller(player);
    input_register_event(E_KB_W, E_NOMOD, game_input_move_up);
    input_register_event(E_KB_A, E_NOMOD, game_input_move_left);
    input_register_event(E_KB_S, E_NOMOD, game_input_move_down);
    input_register_event(E_KB_D, E_NOMOD, game_input_move_right);
    input_register_event(E_KB_C, E_NOMOD, game_input_tile_place);

    GLOBALS.player = player;

    e_x = (1 + rand()) % GLOBALS.view_port_maxx;
    e_y = (1 + rand()) % GLOBALS.view_port_maxy;
    entity = entity_spawn(g_game->world, g_game->entity_types + ge_chaser_mob,
            e_x, e_y, ENTITY_FACING_RIGHT, 1, 0);
    entity->speed = 50;

    e_x = (1 + rand()) % GLOBALS.view_port_maxx;
    e_y = (1 + rand()) % GLOBALS.view_port_maxy;
    entity = entity_spawn(g_game->world, g_game->entity_types + ge_diamond,
            e_x, e_y, ENTITY_FACING_RIGHT, 1, 0);
    entity->speed = 80;

    e_x = (1 + rand()) % GLOBALS.view_port_maxx;
    e_y = (1 + rand()) % GLOBALS.view_port_maxy;
    entity = entity_spawn(g_game->world, g_game->entity_types + ge_redore,
            e_x, e_y, ENTITY_FACING_RIGHT, 1, 0);
    entity->speed = 150;

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

    assert_log(ge_air <= id && id < ge_end,
            "attempting to access invalid id %d at position (%d,%d)", id, x, y);

    return g_game->entity_types + id;
}

EntityType *_game_world_getxy(int index) {
    int id = GAME_ENTITY_CACHE[index];
    id = id > 0 ? id : WORLD_ENTITY_CACHE[index];

    assert_log(id < ge_air || ge_end <= id,
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

