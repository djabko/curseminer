#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "curseminer/globals.h"
#include "curseminer/util.h"
#include "curseminer/core_game.h"
#include "curseminer/world.h"
#include "curseminer/entity.h"
#include "curseminer/stack64.h"

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

void flush_world_entity_cache(GameContext *game) {
    for (int y = 0; y < game->viewport_h; y++) {
        for (int x = 0; x < game->viewport_w; x++) {

            int _x = x + game->world_view_x;
            int _y = y + game->world_view_y;
            int tid = world_getxy(game->world, _x, _y);

            game_cache_set(game, game->cache_world, x, y, tid);
        }
    }
}

void flush_game_entity_cache(GameContext *game) {
    DirtyFlags *df = game->cache_dirty_flags;
    size_t cache_size = df->groups_available * df->stride;

    memset(game->cache_entity, 0, cache_size * sizeof(Entity*));

    Heap *h = &game->world->entities->heap;

    int i = 0;
    while (i < h->count) {
        Entity *e = (Entity*) h->mempool[i].data;

        if (game_on_screen(game, e->x, e->y)) {

            gamew_cache_set(game, game->cache_entity, e->x, e->y, e);
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

        log_debug("CoreGame: Allocating caches for %zd tiles (%dx%d)",
                tiles_on_screen, width, height);

        game->cache_world = realloc(game->cache_world, tiles_on_screen);
        game->cache_entity = realloc(game->cache_entity, tiles_on_screen * sizeof(Entity*));
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
    skin->rotation = 0;
    skin->offset_x = 0;
    skin->offset_y = 0;
    skin->flip_x = false;
    skin->flip_y = false;

    skin->bg_r = bg_r;
    skin->bg_g = bg_g;
    skin->bg_b = bg_b;
    skin->bg_a = 0xff;

    skin->fg_r = fg_r;
    skin->fg_g = fg_g;
    skin->fg_b = fg_b;
    skin->fg_a = 0xff;

    game->skins_c++;
}

EntityType* game_create_entity_type(GameContext *game, Skin* skin) {
    EntityType *type = game->entity_types + game->entity_types_c;

    type->default_skin = skin;
    type->id = game->entity_types_c++;

    return type;
}

int job_game_init_next(Task *tk, Stack64 *st) {
    if (GLOBALS.game) game_exit(GLOBALS.game);

    GameContextCFG *cfg = (GameContextCFG*) qu_next(GLOBALS.games_qu);

    int i = 0;
    GameContext *tmp;
    qu_foreach(GLOBALS.games_qu, GameContextCFG*, tmp) {
        log_debug("%d. %p", i, tmp->f_init);
        i++;
    }

    GLOBALS.game = game_init(cfg, GLOBALS.world);

    tk_kill(tk);

    return 0;
}

int game_update(Task* task, Stack64* stack) {
    if (!GLOBALS.game)
        GLOBALS.game = (GameContext*) qu_next(GLOBALS.games_qu);

    GameContext *game = GLOBALS.game;
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

GameContext *game_init(GameContextCFG *cfg, World *world) {
    GameContext *game = calloc(sizeof(GameContext), 1);

    assert_log (game != NULL,
            "ERROR: UI failed to initialize game...");

    game->skins_max = cfg->skins_max;
    game->entity_types_max = cfg->entity_types_max;
    game->scroll_threshold = cfg->scroll_threshold;
    game->f_init = cfg->f_init;
    game->f_update = cfg->f_update;
    game->f_exit = cfg->f_exit;

    game->skins = calloc(game->skins_max, sizeof(Skin));
    game->entity_types = calloc(game->entity_types_max, sizeof(EntityType));
    game->world = world;
    game->behaviours = NULL;

    entity_init_default_controller();

    game_resize_viewport(game, GLOBALS.view_port_maxx, GLOBALS.view_port_maxy);
    game->f_init(game, 0);
    
    return game;
}

// TODO: free behaviours
void game_exit(GameContext *game) {
    game->f_exit();
    pq_clear(game->world->entities);
    free(game->behaviours);
    free(game->cache_entity);
    free(game->cache_world);
    free(game->cache_dirty_flags);
    free(game->entity_types);
    free(game->skins);
    free(game);
}

EntityType *game_world_getxy_type(GameContext *game, int x, int y) {
    Entity *e = game_cache_get(game, game->cache_entity, x, y);

    if (e) return e->type;

    byte_t id = game_cache_get(game, game->cache_world, x, y);

    assert_log(0 <= id && id < game->entity_types_c,
            "attempting to access invalid id %d at position (%d,%d)", id, x, y);

    return game->entity_types + id;
}

Skin *game_world_getxy(GameContext *game, int x, int y) {
    if (!game) return NULL;

    Entity *e = game_cache_get(game, game->cache_entity, x, y);

    if (e && e->skin.glyph) return &e->skin;
    else if (e) return e->type->default_skin;

    byte_t id = game_cache_get(game, game->cache_world, x, y);

    assert_log(0 <= id && id < game->entity_types_c,
            "attempting to access invalid id %d at position (%d,%d)", id, x, y);

    return game->entity_types[id].default_skin;
}

Skin *game_cache_getxy(GameContext *game, int index) {
    Entity *e = game->cache_entity[index];

    if (e && e->skin.glyph) return &e->skin;
    else if (e) return e->type->default_skin;

    byte_t id = game->cache_world[index];

    assert_log(index < game->viewport_w * game->viewport_h,
            "attempting to access invalid game screen cache ID at index=%d", index);

    assert_log(id < 0 || game->entity_types_c <= id,
            "attempting to access invalid id %d at index %d", id, index);

    return game->entity_types[id].default_skin;
}

int game_world_setxy(GameContext *game, int x, int y, entity_id_t tid) {
    world_setxy(game->world, x, y, tid);
    
    x -= game->world_view_x;
    y -= game->world_view_y;

    if (0 < x && 0 < y && x < game->viewport_w && y < game->viewport_h)
        game_cache_set(game, game->cache_world, x, y, tid);
    
    return 0;
}
