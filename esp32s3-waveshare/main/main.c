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
#include "driver/i2c_master.h"

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

#define g_gpio_i2c_master_scl GPIO_NUM_10
#define g_gpio_i2c_master_sda GPIO_NUM_11
#define g_i2c_clk_src I2C_CLK_SRC_DEFAULT
#define g_gpio_imu GPIO_NUM_38
#define g_i2c_freq 100 * 1000

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
    size_t size = len * len * sizeof(uint16_t);
    uint16_t *data = malloc(size);

    for (int i = 0; i < len * len; i++) data[i] = color;

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
    esp_lcd_panel_set_gap(g_lcd_panel, 0, 20);

    ESP_ERROR_CHECK(
            gpio_set_level(g_gpio_lcd_bl, 1));

    draw_square(0, 0, g_screen_w, 0x00);

    return err;
}

typedef const uint8_t reg_t;
static reg_t g_imu_ctrl1 = 0x02;
static reg_t g_imu_ctrl2 = 0x03;
static reg_t g_imu_ctrl3 = 0x04;
static reg_t g_imu_ctrl7 = 0x08;
static reg_t g_imu_ctrl9 = 0x0A;
static i2c_master_dev_handle_t g_imu_handle;

static void read_gyro_data(int16_t *angle_x, int16_t *angle_y, int16_t *angle_z) {
    uint8_t wbuf[1];
    uint8_t rbuf[6];
    memset(wbuf, 0x00, 1);
    memset(rbuf, 0x00, 6);

    // Reset FIFO
    wbuf[0] = g_imu_ctrl9;
    wbuf[1] = 0x04;
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);


    wbuf[0] = 0x2F; // STATUS1 register
    ESP_ERROR_CHECK(
            i2c_master_transmit_receive(g_imu_handle, wbuf, 1, rbuf, 6, 1000));

    wbuf[0] = 0x3B; // First out of 6 acceleration registers
    ESP_ERROR_CHECK(
            i2c_master_transmit_receive(g_imu_handle, wbuf, 1, rbuf, 6, 1000));

    *angle_x = (int16_t)(rbuf[0]) + ((int16_t)(rbuf[1]) << 8);
    *angle_y = (int16_t)(rbuf[2]) + ((int16_t)(rbuf[3]) << 8);
    *angle_z = (int16_t)(rbuf[4]) + ((int16_t)(rbuf[5]) << 8);
}

static esp_err_t init_imu() {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = g_gpio_i2c_master_sda,
        .scl_io_num = g_gpio_i2c_master_scl,
        .clk_source = g_i2c_clk_src,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = g_gpio_imu,
        .scl_speed_hz = g_i2c_freq,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    i2c_device_config_t imu_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = g_i2c_freq,
        .device_address = 0x6B,
    };

    log_debug("Initialized i2c at %dHz", g_i2c_freq);

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &imu_cfg, &g_imu_handle));

    int devices_available = 0;
    for (uint8_t addr = 1; addr < 128; addr++) {
        esp_err_t err = i2c_master_probe(bus_handle, addr, 300);

        if (err == ESP_OK) {
            printf("Found I2C device at 0x%02X\n", addr);
            devices_available++;
        }
        if (addr==127) printf("DONE\n");
    }

    size_t size = 8;
    uint8_t wbuf[size];
    uint8_t rbuf[size];
    memset(wbuf, 0x00, size);
    memset(rbuf, 0x00, size);

    // Enable sensors and set little endian mode
    wbuf[0] = g_imu_ctrl1;
    wbuf[1] = 0b01000000;
    log_debug("Sending {0x%.02X 0x%.02X} to 0x%.02X", wbuf[0], wbuf[1], imu_cfg.device_address);
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);

    WAIT(50);

    // Configure accelerometer
    wbuf[0] = g_imu_ctrl2;
    wbuf[1] = 0b00000000;
    log_debug("Sending {0x%.02X 0x%.02X} to 0x%.02X", wbuf[0], wbuf[1], imu_cfg.device_address);
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);

    WAIT(50);

    // Configure gyro
    wbuf[0] = g_imu_ctrl3;
    wbuf[1] = 0b01110000;
    log_debug("Sending {0x%.02X 0x%.02X} to 0x%.02X", wbuf[0], wbuf[1], imu_cfg.device_address);
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);

    WAIT(50);

    // Enable gyro and accel
    wbuf[0] = g_imu_ctrl7;
    wbuf[1] = 0b00000011;
    log_debug("Sending {0x%.02X 0x%.02X} to 0x%.02X", wbuf[0], wbuf[1], imu_cfg.device_address);
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);

    WAIT(50);

    return ESP_OK;
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
    ESP_ERROR_CHECK(init_imu());

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

static int job_input(Task *task, Stack64 *st) {
    int16_t x, y, z;

    read_gyro_data(&x, &y, &z);

    event_ctx_t ctx = E_CTX_GAME;
    InputEvent ie = {
        .type = E_KB_DOWN,
        .state = ES_DOWN,
        .mods = E_NOMOD,
    };

    int thld = 2000;

    if (x > thld) {
        ie.id = E_KB_W;
        frontend_dispatch_event(ctx, &ie);
    } else if (x < -thld) {
        ie.id = E_KB_S;
        frontend_dispatch_event(ctx, &ie);
    }

    if (z > thld) {
        ie.id = E_KB_D;
        frontend_dispatch_event(ctx, &ie);
    } else if (z < -thld) {
        ie.id = E_KB_A;
        frontend_dispatch_event(ctx, &ie);
    }

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
    schedule_cb(g_runqueue, 0, 0, job_input, NULL, cb_exit);
    schedule_run(GLOBALS.runqueue_list);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
