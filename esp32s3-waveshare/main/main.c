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

#include "globals.h"
#include "games/curseminer.h"
#include "frontends/esp32s3-waveshare.h"

#define g_update_rate 10

struct Globals GLOBALS = {
    .player = NULL,
    .input_context = E_CTX_0,
    .view_port_maxx = 20,
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

    frontend_init_t fuii = frontend_esp32s3_ui_init;
    frontend_exit_t fuie = frontend_esp32s3_ui_exit;
    frontend_init_t fini = frontend_esp32s3_input_init;
    frontend_exit_t fine = frontend_esp32s3_input_exit;

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

    GameContextCFG gcfg = {
        .skins_max = 12,
        .entity_types_max = 12,
        .scroll_threshold = 2,

        .f_init = game_curseminer_init,
        .f_update = game_curseminer_update,
        .f_free = game_curseminer_free,
    };

    World *world = world_init(10, 10, 4 * 4096);
    GameContext *gctx = game_init(&gcfg, world);
    GLOBALS.game = gctx;

    Stack64 *gst = st_init(1);
    st_push(gst, (uint64_t) gctx);
    schedule_cb(g_runqueue, 0, 0, game_update, gst, cb_exit);
    schedule_run(GLOBALS.runqueue_list);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
