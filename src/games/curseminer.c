#include <stdio.h>

#include "games/curseminer.h"
#include "globals.h"
#include "util.h"
#include "entity.h"

typedef enum {
     g_skin_null,
     g_skin_default,
     g_skin_gold,
     g_skin_diamond,
     g_skin_iron,
     g_skin_redore,
     g_skin_player,
     g_skin_chaser,
     g_skin_end,
} skin_t;

behaviour_t
    be_stop,
    be_move,
    be_move_one,
    be_face_up,
    be_face_down,
    be_face_left,
    be_face_right,
    be_face_ul,
    be_face_ur,
    be_face_dl,
    be_face_dr,
    be_place,
    be_break,
    be_attack,
    be_interact;

static GameContext *g_game;
static Skin g_skins[g_skin_end];
static EntityType* g_etypes[g_skin_end];
static EntityController g_player_controller;

static bool g_player_moving_changed = false;
static bool g_player_moving_up = false;
static bool g_player_moving_down = false;
static bool g_player_moving_left = false;
static bool g_player_moving_right = false;
static bool g_player_crouching = false;

static void game_input_player_state(InputEvent *ie, bool *on_kd_true, bool *on_kd_reset) {
    if (ie->state == ES_DOWN) {
        *on_kd_true = true;
        *on_kd_reset = false;

    } else *on_kd_true = false;

    g_player_crouching = ie->mods && E_MOD_CROUCHING;

    g_player_moving_changed = true;
}

static void game_input_move_up(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_up, &g_player_moving_down);
}

static void game_input_move_left(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_left, &g_player_moving_right);
}

static void game_input_move_down(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_down, &g_player_moving_up);
}

static void game_input_move_right(InputEvent *ie) {
    game_input_player_state(ie, &g_player_moving_right, &g_player_moving_left);
}

static void game_input_place_tile(InputEvent *ie) {
    if (ie->state == ES_DOWN)
        qu_enqueue(GLOBALS.player->controller->behaviour_queue, be_place);
}

void game_input_set_glyphset_01(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_00.png");
}

void game_input_set_glyphset_02(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_01.png");
}

void game_input_set_glyphset_03(InputEvent *ie) {
    if (ie->state == ES_UP) game_set_glyphset(g_game, "tiles_02.gif");
}

static void chaser_tick(Entity*);
static void chaser_find_path(Entity *e, int x, int y);
static void game_input_spawn_chaser(InputEvent *ie) {
    if (ie->state == ES_DOWN) {
        int x, y;
        int s = (20 + rand()) % 150;

        if (world_from_mouse_xy(ie, &x, &y) != 0) return;

        Entity *e = entity_spawn(g_game, g_game->world,
                g_game->entity_types + g_skin_chaser,
                x, y, ENTITY_FACING_RIGHT, 1, 0);

        e->controller->tick = chaser_tick;
        e->controller->find_path = chaser_find_path;

        e->speed = s;
    }
}

static void game_input_break_tile(InputEvent *ie) {
    entity_command(GLOBALS.player, be_break);
}

static void game_input_break_tile_mouse(InputEvent *ie) {
    // TODO must be moved outside interrupt handler logic, this is too slow
    if (ie->state == ES_DOWN) {
        int x, y;

        if (world_from_mouse_xy(ie, &x, &y) != 0) return;

        EntityType *id = game_world_getxy(g_game, x, y);
        int d = man_dist(GLOBALS.player->x, GLOBALS.player->y, x, y);

        bool is_tile_exists = id != E_TYPE_NULL;
        bool is_player_close = d < TILE_BREAK_DISTANCE;

        if (is_tile_exists && is_player_close)
            game_world_setxy(g_game, x, y, E_TYPE_NULL);
    }
}

static void game_input_inventory_up(InputEvent *ie) {
    Entity* p = GLOBALS.player;

    if (ie->state == ES_DOWN)
        p->inventory_index = (p->inventory_index + 1) % ENTITY_END;
}

static void game_input_inventory_down(InputEvent *ie) {
    Entity* p = GLOBALS.player;

    if (ie->state == ES_DOWN) {
        p->inventory_index = p->inventory_index - 1;

        if (p->inventory_index < 0)
            p->inventory_index += ENTITY_END - 1;
    }
}

static void be_place_f(Entity *e) {
    if (entity_inventory_selected(e)) {
        int id = e->inventory_index;
        game_world_setxy(g_game, e->x, e->y, id);
        e->inventory[id]--;
    }
}

static void be_break_f(Entity *e) {
    int f = e->facing;
    int c_x = 
           1 * (f == ENTITY_FACING_RIGHT
            || f == ENTITY_FACING_UR || f == ENTITY_FACING_DR)
        + -1 * (f == ENTITY_FACING_LEFT
            || f == ENTITY_FACING_UL || f == ENTITY_FACING_DL);

    int c_y = 
           1 * (f == ENTITY_FACING_DOWN
            || f == ENTITY_FACING_DL || f == ENTITY_FACING_DR)
        + -1 * (f == ENTITY_FACING_UP
            || f == ENTITY_FACING_UL || f == ENTITY_FACING_UR);

    int x = e->x + c_x;
    int y = e->y + c_y;
    int tid = world_getxy(g_game->world, x, y);

    entity_inventory_add(g_game, e, tid);
    world_setxy(g_game->world, x, y, 0);

    if (game_on_screen(g_game, x, y))
        gamew_cache_set(g_game, g_game->cache_world, x, y, 0);
}

static void be_move_one_f(Entity *e) {
    e->moving = true;
    entity_update_position(g_game, e);
    e->moving = false;
}

static void be_move_f(Entity *e) {
    e->moving = true;
}

static void be_stop_f(Entity *e) {
    e->moving = false;
}

static void be_face_up_f (Entity *e) {
    e->facing = ENTITY_FACING_UP;
}

static void be_face_down_f (Entity *e) {
    e->facing = ENTITY_FACING_DOWN;
}

static void be_face_left_f (Entity *e) {
    e->facing = ENTITY_FACING_LEFT;
}

static void be_face_right_f (Entity *e) {
    e->facing = ENTITY_FACING_RIGHT;
}

static void be_face_ul_f (Entity *e) {
    e->facing = ENTITY_FACING_UL;
}

static void be_face_ur_f (Entity *e) {
    e->facing = ENTITY_FACING_UR;
}

static void be_face_dl_f (Entity *e) {
    e->facing = ENTITY_FACING_DL;
}

static void be_face_dr_f (Entity *e) {
    e->facing = ENTITY_FACING_DR;
}

/* Defines default entity action for each tick
 * Current behaviour is to follow the player via find_path() if player is
 * nearby.
 */
static void chaser_tick(Entity* e) {
    Entity *p = GLOBALS.player;
    int radius = 40;
    bool is_in_radius = man_dist(e->x, e->y, p->x, p->y) <= radius;

    //entity_clear_behaviours(e);
    e->controller->find_path(e, GLOBALS.player->x, GLOBALS.player->y);

    if (is_in_radius && !e->moving) {
        entity_command(e, be_move);

    } else if (!is_in_radius && e->moving) {
        entity_command(e, be_stop);
    }
}

static void chaser_find_path(Entity *e, int x, int y) {
    if (e->x == x && e->y == y) return;

    const int up = (e->y > y);
    const int down = (e->y < y);
    const int left = (e->x > x);
    const int right = (e->x < x);

    behaviour_t be = be_stop;
    
    if      (up && left)    be = be_face_ul;
    else if (up && right)   be = be_face_ur;
    else if (down && left)  be = be_face_dl;
    else if (down && right) be = be_face_dr;
    else if (up)            be = be_face_up;
    else if (down)          be = be_face_down;
    else if (left)          be = be_face_left;
    else if (right)         be = be_face_right;
    else log_debug("ERROR: unable to execute pathfinding for entity %p(%d, %d)"
            "with respect to point (%d, %d)}", e, e->x, e->y, x, y);

    entity_command(e, be);
}

static void player_tick(Entity *player) {
    entity_update_position(g_game, player);
}

static void player_path_find(Entity *player, int x, int y) {}

int game_curseminer_init(GameContext *game, int) {
    g_game = game;

    game_set_glyphset(game, "tiles_02.gif");
    game_set_glyphset(game, "tiles_00.png");

    int glyph = 0;
    int i = 0;

    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 5, 5, 5);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 120, 120, 120);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 255, 215,   0);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0,  80, 240, 220);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 120, 120, 120);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 120,   6,   2);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 255, 215,   0);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0, 215, 215,  50);
    game_create_skin(game, g_skins + i++, glyph++, 0, 0, 0,  42, 133,  57);

    for (i = 0; i < g_skin_end; i++)
        g_etypes[i] = game_create_entity_type(g_game, g_skins + i);

    g_game->entity_types_c = g_skin_end;

    be_place = entity_create_behaviour(be_place_f);
    be_break = entity_create_behaviour(be_break_f);
    be_move = entity_create_behaviour(be_move_f);
    be_move_one = entity_create_behaviour(be_move_one_f);
    be_stop = entity_create_behaviour(be_stop_f);
    be_face_up = entity_create_behaviour(be_face_up_f);
    be_face_down = entity_create_behaviour(be_face_down_f);
    be_face_left = entity_create_behaviour(be_face_left_f);
    be_face_right = entity_create_behaviour(be_face_right_f);
    be_face_ul = entity_create_behaviour(be_face_ul_f);
    be_face_ur = entity_create_behaviour(be_face_ur_f);
    be_face_dl = entity_create_behaviour(be_face_dl_f);
    be_face_dr = entity_create_behaviour(be_face_dr_f);

    entity_create_controller(&g_player_controller, player_tick, player_path_find);
    Entity *player = entity_spawn(game, game->world, g_etypes[g_skin_player],
            20, 20, ENTITY_FACING_RIGHT, 1, 0);

    player->speed = 1;
    player->controller = &g_player_controller;


    frontend_register_event(E_KB_UP,   E_CTX_GAME, game_input_move_up);
    frontend_register_event(E_KB_DOWN, E_CTX_GAME, game_input_move_down);
    frontend_register_event(E_KB_LEFT, E_CTX_GAME, game_input_move_left);
    frontend_register_event(E_KB_RIGHT,E_CTX_GAME, game_input_move_right);
    frontend_register_event(E_KB_W,    E_CTX_GAME, game_input_move_up);
    frontend_register_event(E_KB_A,    E_CTX_GAME, game_input_move_left);
    frontend_register_event(E_KB_S,    E_CTX_GAME, game_input_move_down);
    frontend_register_event(E_KB_D,    E_CTX_GAME, game_input_move_right);
    frontend_register_event(E_KB_C,    E_CTX_GAME, game_input_place_tile);
    frontend_register_event(E_KB_E,    E_CTX_GAME, game_input_inventory_up);
    frontend_register_event(E_KB_R,    E_CTX_GAME, game_input_inventory_down);
    frontend_register_event(E_KB_Z,    E_CTX_GAME, game_input_break_tile);
    frontend_register_event(E_MS_LMB,  E_CTX_GAME, game_input_spawn_chaser);
    frontend_register_event(E_MS_RMB,  E_CTX_GAME, game_input_break_tile_mouse);
    frontend_register_event(E_KB_F1, E_CTX_GAME, game_input_set_glyphset_01);
    frontend_register_event(E_KB_F2, E_CTX_GAME, game_input_set_glyphset_02);
    frontend_register_event(E_KB_F3, E_CTX_GAME, game_input_set_glyphset_03);

    GLOBALS.player = player;

    return 1;
}

int game_curseminer_update() {
    GameContext *game = g_game;

    Entity *plr = GLOBALS.player;
    int wvx = game->world_view_x;
    int wvy = game->world_view_y;
    int sth = game->scroll_threshold;
    int mxx = GLOBALS.view_port_maxx;
    int mxy = GLOBALS.view_port_maxy;

    byte_t leftb = plr->x < wvx + sth;
    byte_t topb = plr->y < wvy + sth;
    byte_t rightb = plr->x >= wvx + mxx - sth;
    byte_t bottomb = plr->y >= wvy + mxy - sth;

    if (leftb || topb || rightb || bottomb || g_player_moving_changed) {
        
        // Check for screen scrolling
        if      (leftb)      game->world_view_x--;
        else if (rightb)     game->world_view_x++;
        if      (topb)       game->world_view_y--;
        else if (bottomb)    game->world_view_y++;

        flush_world_entity_cache(g_game);
        flush_game_entity_cache(g_game);
        game_flush_dirty(g_game);

        // Check for player movement
        if (g_player_moving_changed) {
            Entity *player = GLOBALS.player;
            bool up = g_player_moving_up;
            bool down = g_player_moving_down;
            bool left = g_player_moving_left;
            bool right = g_player_moving_right;

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

    return 0;
}

int game_curseminer_free() {
    return 0;
}
