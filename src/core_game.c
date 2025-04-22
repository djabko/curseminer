#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "globals.h"
#include "util.h"
#include "core_game.h"
#include "world.h"
#include "entity.h"
#include "stack64.h"

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


/* Helper Functions */
int game_on_screen(GameContext *game, int x, int y) {
    int minx = game->world_view_x;
    int miny = game->world_view_y;
    int maxx = minx + game->viewport_w;
    int maxy = miny + game->viewport_h;

    return minx <= x && miny <= y && x < maxx && y < maxy;
}

void game_cache_set(GameContext *game, byte_t *cache, int x, int y, byte_t tid) {
    game_set_dirty(game, x, y, 1);
    cache[y * game->viewport_w + x] = tid;
}

byte_t game_cache_get(byte_t *cache, int x, int y) {
    return cache[y * GLOBALS.game->viewport_w + x];
}

void gamew_cache_set(GameContext *game, byte_t *cache, int x, int y, byte_t tid) {
    x -= game->world_view_x;
    y -= game->world_view_y;

    game_cache_set(game, cache, x, y, tid);
}

byte_t gamew_cache_get(GameContext *game, byte_t *cache, int x, int y) {
    x -= game->world_view_x;
    y -= game->world_view_y;

    return game_cache_get(cache, x, y);
}

void flush_world_entity_cache(GameContext *game) {
    DirtyFlags *df = game->cache_dirty_flags;
    size_t cache_size = df->groups_available * df->stride;

    for (int y = 0; y < game->viewport_h; y++) {
        for (int x = 0; x < game->viewport_w; x++) {

            int _x = x + game->world_view_x;
            int _y = y + game->world_view_y;
            int tid = world_getxy(game->world, _x, _y);

            game->cache_world[y * game->viewport_w + x] = tid;
        }
    }
}

void flush_game_entity_cache(GameContext *game) {
    DirtyFlags *df = game->cache_dirty_flags;
    size_t cache_size = df->groups_available * df->stride;

    memset(game->cache_entity, 0, cache_size);

    Heap *h = &game->world->entities->heap;

    int i = 0;
    while (i < h->count) {
        Entity *e = (Entity*) h->mempool[i].data;

        if (game_on_screen(game, e->x, e->y)) {

            int x = e->x - game->world_view_x;
            int y = e->y - game->world_view_y;

            game->cache_entity[y * game->viewport_w + x] = e->type->id;
        }

        i++;
    }
}

static void game_resize_dirty_flags(GameContext *game, size_t tiles_on_screen) {
    static size_t s = 64;

    DirtyFlags *df = game->cache_dirty_flags;

    uint64_t groups = (tiles_on_screen) / s;
    size_t alloc_size = sizeof(DirtyFlags) + groups + s * groups;
    tiles_on_screen = groups * s;

    df = realloc(df, alloc_size);
    memset(df, 0, alloc_size);

    byte_t *ptr = (byte_t*) df + s;
    df->groups = (byte_t*) (ptr);
    df->flags = (byte_t*) (ptr + groups);
    df->stride = s;
    df->groups_available = groups;
    df->command = 0;

    game->cache_dirty_flags = df;
}

bool game_resize_viewport(GameContext *game, int width, int height) {
    static const size_t s = 64;

    if (!game) return false;

    game->viewport_w = width;
    game->viewport_h = height;

    size_t tiles_on_screen = width * height;
    uint64_t groups = tiles_on_screen / s;
    groups = s * ((groups + s) / s);

    tiles_on_screen = s * groups;
    DirtyFlags *df = game->cache_dirty_flags;

    if (!df || s * df->groups_available < tiles_on_screen) {

        log_debug("CoreGame: Allocating caches for %d tiles (%dx%d)",
                tiles_on_screen, width, height);

        game->cache_world = realloc(game->cache_world, tiles_on_screen);
        game->cache_entity = realloc(game->cache_entity, tiles_on_screen);
        game_resize_dirty_flags(game, tiles_on_screen);
    }

    flush_world_entity_cache(game);
    flush_game_entity_cache(game);

    return true;
}

void game_set_dirty(GameContext *game, int x, int y, int v) {
    DirtyFlags *df = game->cache_dirty_flags;

    if (df->command == -1) return;

    int maxx = game->viewport_w;
    size_t offset = y * maxx + x;
    size_t s = df->stride;

    df->groups[offset / s] = v;
    df->flags[offset] = v;
    df->command = 1;
}

void game_flush_dirty(GameContext *game) {
    DirtyFlags *df = game->cache_dirty_flags;

    if (df->command == -1) return;

    byte_t* groups = df->groups;
    size_t s = df->stride;
    size_t gu = (game->viewport_w * game->viewport_h) / df->stride;

    for (int i = 0; i < gu; i++) {
        groups[i] = 0;

        for (int j = 0; j < s; j++) {
            (groups + i * s) [j] = 0;
        }
    }

    df->command = -1;
}

bool game_set_glyphset(GameContext *game, const char* name) {
    bool res = frontend_set_glyphset(name);

    flush_game_entity_cache(game);
    flush_world_entity_cache(game);
    game_flush_dirty(game);

    return res;
}

void game_create_skin(GameContext *game, Skin *skin, int id,
        color_t bg_r, color_t bg_g, color_t bg_b,
        color_t fg_r, color_t fg_g, color_t fg_b) {

    skin->glyph = id;

    skin->bg_r = bg_r;
    skin->bg_g = bg_g;
    skin->bg_b = bg_b;
    skin->fg_r = fg_r;
    skin->fg_g = fg_g;
    skin->fg_b = fg_b;

    game->skins_c++;
}

EntityType* game_create_entity_type(GameContext *game, Skin* skin) {
    EntityType *type = game->entity_types + game->entity_types_c;

    type->default_skin = skin;
    type->id = game->entity_types_c++;

    return type;
}

int game_update(Task* task, Stack64* stack) {
    GameContext *game = (GameContext*) st_peek(stack);
    PQueue64* entity_pq = game->world->entities;
    milliseconds_t next_tick = _pq_peek(entity_pq, 0);

    if (!pq_empty(entity_pq) && next_tick <= TIMER_NOW_MS) {

        while (_pq_peek(entity_pq, 0) <= TIMER_NOW_MS) {
            Entity *e = (Entity*) pq_dequeue(entity_pq);

            entity_tick_abstract(game, e);
            e->next_tick = TIMER_NOW_MS + e->speed * 10;

            pq_enqueue(entity_pq, e, e->next_tick);

            next_tick = _pq_peek(entity_pq, 0);
        }
    }

    game->f_update();

    tk_sleep(task, 1000 / GAME_REFRESH_RATE);

    return 0;
}

GameContext *game_init(GameContextCFG *cfg) {
    GameContext *game = calloc(sizeof(GameContext), 1);

    assert_log (game != NULL,
            "ERROR: UI failed to initialize game...");

    game->cache_entity = NULL;
    game->cache_world = NULL;
    game->cache_dirty_flags = NULL;

    game->skins_max = cfg->skins_max;
    game->entity_types_max = cfg->entity_types_max;
    game->scroll_threshold = cfg->scroll_threshold;
    game->f_init = cfg->f_init;
    game->f_update = cfg->f_update;
    game->f_free = cfg->f_free;

    game->skins_c = 0;
    game->entity_types_c = 0;
    game->skins = calloc(game->skins_max, sizeof(Skin));
    game->entity_types = calloc(game->entity_types_max, sizeof(EntityType));
    game->world_view_x = 0;
    game->world_view_y = 0;
    game->viewport_w = 0;
    game->viewport_h = 0;

    game->world = world_init(20, game->skins_c - 1, PAGE_SIZE * 64);

    entity_init_default_controller();

    game_resize_viewport(game, GLOBALS.view_port_maxx, GLOBALS.view_port_maxy);
    game->f_init(game, 0);

    return game;
}

void game_free(GameContext *game) {
    game->f_free();
    world_free(game->world);
    free(game->entity_types);
    free(game->skins);
    free(game);
}

EntityType *game_world_getxy(GameContext *game, int x, int y) {
    if (!game) return NULL;

    int id = game_cache_get(game->cache_entity, x, y);
    id = id > 0 ? id : game_cache_get(game->cache_world, x, y);

    assert_log(0 <= id && id < game->entity_types_c,
            "attempting to access invalid id %d at position (%d,%d)", id, x, y);

    return game->entity_types + id;
}

EntityType *game_cache_getxy(GameContext *game, int index) {
    int id = game->cache_entity[index];
    id = id > 0 ? id : game->cache_world[index];

    assert_log(index < game->viewport_w * game->viewport_h,
            "attempting to access invalid game screen cache ID at index=%d", index);

    assert_log(id < 0 || game->entity_types_c <= id,
            "attempting to access invalid id %d at index %d", id, index);

    return game->entity_types + id;
}

int game_world_setxy(GameContext *game, int x, int y, entity_id_t tid) {
    world_setxy(game->world, x, y, tid);
    
    x -= game->world_view_x;
    y -= game->world_view_y;

    if (0 < x && 0 < y && x < game->viewport_w && y < game->viewport_h)
        game_cache_set(game, game->cache_world, x, y, tid);
    
    return 0;
}

int game_cache_setxy(GameContext *game, int index, entity_id_t tid) {
    if (index < game->viewport_w * game->viewport_h) return -1;

    game->cache_entity[index] = tid;

    return 0;
}
