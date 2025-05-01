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

#define g_update_rate 10
#define g_screen_w 240
#define g_screen_h 280

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
    .view_port_maxx = 10,
    .view_port_maxy = 10,
};

typedef struct Resolution {
    int w, h;
} Resolution;

Resolution g_resolution = {.w = 240, .h = 280};

static RunQueue *g_runqueue;

esp_lcd_panel_handle_t g_lcd_panel;
static uint16_t *g_draw_buffer;

static void draw_square(int x, int y, int len, uint16_t color) {
    log_debug("Drawing %d square at (%d, %d)", len, x, y);
    size_t size = len * len * sizeof(uint16_t);
    uint16_t *data = malloc(size);

    memset(data, color, size);
    esp_lcd_panel_draw_bitmap(g_lcd_panel, x, y, x + len, y + len, data);

    free(data);
}

#define TAG "Panel"
static esp_err_t init_panel() {
    esp_err_t err = 0;

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

    ESP_RETURN_ON_ERROR(
            esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &g_lcd_panel),
            TAG, "New panel failed");

    esp_lcd_panel_reset(g_lcd_panel);
    esp_lcd_panel_init(g_lcd_panel);

    esp_lcd_panel_invert_color(g_lcd_panel, false);
    esp_lcd_panel_disp_on_off(g_lcd_panel, true);
    esp_lcd_panel_mirror(g_lcd_panel, false, false);
    esp_lcd_panel_set_gap(g_lcd_panel, 0, 0);

    ESP_ERROR_CHECK(
            gpio_set_level(g_gpio_lcd_bl, 1));

    draw_square(0, 0, g_screen_w, 0x80);

    return err;
}

static bool set_glyphset(const char* name) { return false; }

static int ui_init(Frontend* fr, const char *title) {
    fr->f_set_glyphset = set_glyphset;

    init_panel();

    return 0;
}

static void ui_exit() {}

static void init(const char *title) {
    ESP_ERROR_CHECK(init_panel());

    time_init(g_update_rate);
    GLOBALS.runqueue_list = scheduler_init();
    g_runqueue = scheduler_new_rq(GLOBALS.runqueue_list);
    GLOBALS.runqueue = g_runqueue;

    assert_log(GLOBALS.runqueue_list && g_runqueue,
            "failed to initialize main RunQueue");

    frontend_init_t fuii = ui_init;
    frontend_exit_t fuie = ui_exit;
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

static int job_render(Task *task, Stack64 *st) {
    Skin* skin;
    DirtyFlags *df = GLOBALS.game->cache_dirty_flags;

    // No tiles to draw
    if (df->command == 0) {
        return 0;

    // Draw only dirty tiles
    } else if (df->command == 1) {

        size_t s = df->stride;
        int maxx = GLOBALS.view_port_maxx;

        for (int i = 0; i < s; i++) {
            if (df->groups[i]) {

                for (int j = 0; j < s; j++) {

                    int index = i * s + j;
                    byte_t flag = df->flags[index];

                    if (flag == 1) {

                        int x = index % maxx;
                        int y = index / maxx;

                        skin = game_world_getxy(GLOBALS.game, x, y);

                        uint16_t color = (skin->fg_r >> 3) + (skin->fg_b >> 2) + (skin->fg_b >> 3);

                        int tile_w =  g_screen_w / GLOBALS.view_port_maxx;
                        int tile_h =  tile_w;
                        draw_square(x * tile_w, y * tile_h, tile_w, color);
                        game_set_dirty(GLOBALS.game, x, y, 0);
                    }
                }
            }
        }


    // Update all tiles
    } else if (df->command == -1) {
        for (int y = 0; y < GLOBALS.view_port_maxx; y++) {
            for (int x = 0; x < GLOBALS.view_port_maxy; x++) {

                skin = game_world_getxy(GLOBALS.game, x, y);

                uint16_t color = (skin->fg_r >> 3) + (skin->fg_b >> 2) + (skin->fg_b >> 3);
                int tile_w =  g_screen_w / GLOBALS.view_port_maxx;
                int tile_h =  tile_w;
                draw_square(x * tile_w, y * tile_h, tile_w, color);
                game_set_dirty(GLOBALS.game, x, y, 0);
            }
        }
    }

    df->command = 0;
    return 0;
}

void app_main(void)
{
    init("Curseminer!");

    draw_square(20, 20, 100, 0x8000);

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
    schedule_cb(g_runqueue, 0, 0, job_render, NULL, cb_exit);
    schedule_run(GLOBALS.runqueue_list);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
