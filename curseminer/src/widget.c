#include "curseminer/widget.h"
#include "curseminer/core_game.h"
#include "curseminer/frontend.h"

int widget_draw_game(GameContext *game, void (*f_draw_tile)(int, int, Skin*)) {
    Skin* skin;
    DirtyFlags *df = game->cache_dirty_flags;

    int tiles_drawn = 0;

    // No tiles to draw
    if (df->command == 0) {

    // Draw only dirty tiles
    } else if (df->command == 1) {

        size_t s = df->stride;
        int maxx = game->viewport_w;

        for (int i = 0; i < s; i++) {
            if (df->groups[i]) {

                for (int j = 0; j < s; j++) {

                    int index = i * s + j;
                    byte_t flag = df->flags[index];

                    if (flag == 1) {

                        int x = index % maxx;
                        int y = index / maxx;

                        skin = game_world_getxy(game, x, y);
                        f_draw_tile(x, y, skin);
                        tiles_drawn++;
                    }
                }
            }
        }

    // Update all tiles
    } else if (df->command == -1) {
        for (int y=0; y < game->viewport_h; y++) {
            for (int x=0; x < game->viewport_w; x++) {

                skin = game_world_getxy(game, x, y);
                f_draw_tile(x, y, skin);
                tiles_drawn++;
            }
        }

        game_flush_dirty(game);
    }

    df->command = 0;

    return tiles_drawn;
}
