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

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "globals.h"
#include "games/curseminer.h"
#include "frontends/headless.h"

#define g_update_rate 1

struct Globals GLOBALS = {
    .player = NULL,
    .input_context = E_CTX_0,
    .view_port_maxx = 5,
    .view_port_maxy = 5,
};

typedef struct Resolution {
    int w, h;
} Resolution;

Resolution g_resolution = {.w = 240, .h = 280};

static RunQueue *g_runqueue;

static void init(const char *title) {
    time_init(g_update_rate);

    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
    GLOBALS.runqueue = g_runqueue;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    frontend_init_t fuii = frontend_headless_ui_init;
    frontend_exit_t fuie = frontend_headless_ui_exit;
    frontend_init_t fini = frontend_headless_input_init;
    frontend_exit_t fine = frontend_headless_input_exit;

    frontend_register_ui(fuii, fuie);
    frontend_register_input(fini, fine);
    frontend_init(title);

    schedule_run(GLOBALS.runqueue_list);

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
    log_debug("1");
    log_debug("2");
    log_debug("3");
    log_debug("4");
    log_debug("5");
    GameContext *gctx = game_init(&gcfg, world);

    Stack64 *gst = st_init(1);
    st_push(gst, (uint64_t) gctx);
    schedule_cb(g_runqueue, 0, 0, game_update, gst, cb_exit);
    schedule_run(GLOBALS.runqueue_list);

    int ctr = 10;
    while (0 < ctr--) {
        printf("Restarting in %d seconds...", ctr);
        vTaskDelay((1000) / (portTICK_PERIOD_MS));
    }

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
