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

#define g_spi_freq (60 * 1000 * 1000)
#define g_spi_lcd SPI3_HOST
#define g_gpio_lcd_dc GPIO_NUM_4
#define g_gpio_lcd_cs GPIO_NUM_5
#define g_gpio_lcd_sclk GPIO_NUM_6
#define g_gpio_lcd_mosi GPIO_NUM_7
#define g_gpio_lcd_rst GPIO_NUM_8
#define g_gpio_lcd_bl GPIO_NUM_15

#define g_draw_buf_height 50
#define WAIT(ms) vTaskDelay((ms) / (portTICK_PERIOD_MS));

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

#define TAG "Panel"
static int init_panel() {
    int ret = 0;

    gpio_config_t bk_gpio_cfg = {
        .pin_bit_mask = 1ULL << g_gpio_lcd_bl,
        .mode = GPIO_MODE_OUTPUT,
    };

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = g_gpio_lcd_sclk,
        .mosi_io_num = g_gpio_lcd_mosi,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = g_resolution.h * g_draw_buf_height,
    };

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = g_gpio_lcd_dc,
        .cs_gpio_num = g_gpio_lcd_cs,
        .pclk_hz = g_spi_freq,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = g_gpio_lcd_rst,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16
    };

    ESP_ERROR_CHECK(
            gpio_config(&bk_gpio_cfg));

    ESP_RETURN_ON_ERROR(
            spi_bus_initialize(
                g_spi_lcd, &bus_cfg, SPI_DMA_CH_AUTO),
            TAG, "SPI init failed");


    esp_lcd_panel_io_handle_t io_handle = NULL;

    ESP_RETURN_ON_ERROR(
            esp_lcd_new_panel_io_spi(
                (esp_lcd_spi_bus_handle_t) g_spi_lcd,
                &io_cfg,
                &io_handle),
            TAG, "New panel IO failed");

    esp_lcd_panel_handle_t lcd_panel = NULL;

    ESP_RETURN_ON_ERROR(
            esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &lcd_panel),
            TAG, "New panel failed");

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);

    esp_lcd_panel_invert_color(lcd_panel, false);
    esp_lcd_panel_disp_on_off(lcd_panel, true);
    esp_lcd_panel_mirror(lcd_panel, false, false);
    esp_lcd_panel_set_gap(lcd_panel, 0, 0);

    ESP_ERROR_CHECK(
            gpio_set_level(g_gpio_lcd_bl, 1));

    const void *data[32];
    memset(data, 0x80, 32);

    int sx = 0;
    int y = 100;
    for (int ex = sx; ex < 200; ex += 10) {
        esp_lcd_panel_draw_bitmap(lcd_panel, sx, y, ex, y+50, data);
        WAIT(1000);
    }

    return ret;
}

static void init(const char *title) {
    init_panel();
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

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
