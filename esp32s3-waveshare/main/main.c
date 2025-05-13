#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "curseminer/globals.h"
#include "curseminer/entity.h"
#include "curseminer/games/curseminer.h"
#include "curseminer/games/other.h"
#include "curseminer/frontends/esp32s3-waveshare.h"

#define g_update_rate 10

struct Globals GLOBALS = {
    .input_context = E_CTX_0,
    .view_port_maxx = 17,
    .view_port_maxy = 20,
};

static RunQueue *g_runqueue;

static void init(const char *title) {
    time_init(g_update_rate);
    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
    GLOBALS.runqueue = g_runqueue;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    frontend_init_ui_t fuii = frontend_esp32s3_ui_init;
    frontend_exit_ui_t fuie = frontend_esp32s3_ui_exit;
    frontend_init_input_t fini = frontend_esp32s3_input_init;
    frontend_exit_input_t fine = frontend_esp32s3_input_exit;

    frontend_register_ui(fuii, fuie);
    frontend_register_input(fini, fine);
    frontend_init(title);

    log_debug("Initialized...\n");
}

static void cb_exit(Task* task) {
    scheduler_kill_all_tasks();
}

void app_main(void)
{
    init("Curseminer!");

    GLOBALS.games_qu = qu_init(1);

    const int games = 2;
    GameContextCFG *gcfgs = calloc(games, sizeof(GameContextCFG));

    for (int i = 0; i < games; i++)
        qu_enqueue(GLOBALS.games_qu, (uint64_t) (gcfgs + i));

    GameContextCFG gcfg = {
        .skins_max = 12,
        .entity_types_max = 12,
        .scroll_threshold = 5,
    };

    gcfg.f_init = game_curseminer_init;
    gcfg.f_update = game_curseminer_update;
    gcfg.f_exit = game_curseminer_free;
    *(gcfgs + 0) = gcfg;

    gcfg.f_init = game_other_init;
    gcfg.f_update = game_other_update;
    gcfg.f_exit = game_other_free;
    *(gcfgs + 1) = gcfg;

    qu_next(GLOBALS.games_qu);

    log_debug("=== Heap Info ===");
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    GLOBALS.world = world_init(10, 10, 4 * PAGE_SIZE);
    GameContext *gctx = game_init(&gcfg, GLOBALS.world);
    GLOBALS.game = gctx;

    schedule_cb(g_runqueue, 0, 0, game_update, NULL, cb_exit);
    schedule_run(GLOBALS.runqueue_list);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
