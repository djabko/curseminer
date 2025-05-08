#include <string.h>
#include <dirent.h>

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_spiffs.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#include "curseminer/globals.h"
#include "curseminer/stack64.h"
#include "curseminer/frontend.h"
#include "curseminer/frontends/esp32s3-waveshare.h"

/* Memory allocation functions just for STB_Image */
void *my_alloc (size_t size) {
    log_debug("Allocating with %zuB", size);

    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

    return ptr;
}

void *my_realloc(void *old, size_t new) {
    log_debug("Reallocating %p with %zuB", old, new);
    
    free(old);
    void *ptr = heap_caps_malloc(new, MALLOC_CAP_SPIRAM);

    return ptr;
}

void my_free(void *ptr) {
    log_debug("Freeing %p", ptr);
    free(ptr);
}

#define STBI_MALLOC(sz)             my_alloc(sz)
#define STBI_REALLOC(p,newsz)       my_realloc(p,newsz)
#define STBI_FREE(p)                my_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#ifndef NAME_MAX
#define NAME_MAX 64
#endif

#define g_screen_w 240
#define g_screen_h 280

#define g_spi_freq (40 * 1000 * 1000)
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
#define g_sprite_w 32
#define g_sprite_h 32
#define g_spritesheet_size 32 * 224 * 4
#define WAIT(ms) vTaskDelay((ms) / (portTICK_PERIOD_MS))


typedef uint8_t imu_enable_t;
const imu_enable_t IMU_ENABLE_SYNCSMPL           = 0b10000000;
const imu_enable_t IMU_ENABLE_SYS_HS             = 0b01000000;
const imu_enable_t IMU_ENABLE_RESERVED           = 0b00100000; // Do not use
const imu_enable_t IMU_ENABLE_SNOOZE_MODE        = 0b00010000;
const imu_enable_t IMU_ENABLE_ATTITUDE_ENGINE    = 0b00001000;
const imu_enable_t IMU_ENABLE_MAGNETOMETER       = 0b00000100;
const imu_enable_t IMU_ENABLE_GYROSCOPE          = 0b00000010;
const imu_enable_t IMU_ENABLE_ACCELEROMETER      = 0b00000001;

int g_counter = 0;

typedef struct Resolution {
    int w, h;
} Resolution;

Resolution g_resolution = {.w = g_screen_w, .h = g_screen_h};
uint16_t *g_framebuffer;
static void *g_spritesheet;
static Queue64 *g_available_spritesheets;

esp_lcd_panel_handle_t g_lcd_panel;
static uint16_t *g_draw_buffer;
static bool g_render_sprites = false;

static Queue64 *scan_available_spritesheets(const char *path, const char *prefix) {
    Queue64 *qu = NULL;

    DIR *dir = opendir(path);

    if (!dir) {
        log_debug("Failed to open dir '%s'", path);
        return NULL;
    }

    struct dirent *dp;

    qu = qu_init(1);

    while ((dp = readdir(dir)) != NULL) {

        if (0 == strncmp(dp->d_name, prefix, strlen(prefix))) {

            char *data = calloc(NAME_MAX, 1);

            int plen = strlen(path);
            memcpy(data, path, plen);

            *(data + plen) = '/';

            memcpy(data + plen + 1, dp->d_name, strlen(dp->d_name));

            log_debug("Found '%s'", data);

            qu_enqueue(qu, (uint64_t) data);
        }
    }

    if (qu_empty(qu)) {
        free(qu);
        qu = NULL;
        log_debug("Found no files with prefix '%s' in '%s'", prefix, path);
    }

    log_debug("Queue count: %d", qu->count);

    closedir(dir);

    return qu;
}

static uint8_t *load_rgb(char *path, size_t size) {
    void *img = NULL;

    FILE *file = fopen(path, "r");

    if (file == NULL)
        log_debug("File '%s' does not exist!", path);

    else {

        img = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

        size_t _size = fread(img, 1, size, file);

        if (_size != size) log_debug("Read %zu bytes, expected %zu", _size, size);

        if (!_size) {
            free(img);
            img = NULL;
        }

        fclose(file);
    }

    return img;
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint8_t *image_shrink(uint8_t *img, int w, int h, int depth, int nw, int nh) {
    if (w <= nw || h <= nh) return NULL;

    uint8_t *new_img = heap_caps_malloc(nw * nh * depth, MALLOC_CAP_SPIRAM);

    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {

            int ox = x * w / nw;
            int oy = y * h / nh;
            int off_old = depth * (oy * w + ox);
            int off_new = depth * (y * nw + x); 

            for (int bit = 0; bit < depth; bit++)
                *(new_img + off_new + bit) = img[off_old + bit];
        }
    }

    return new_img;
}

static void draw_sprite(uint8_t *spritesheet, int tx, int ty, int depth, int offset, int w, int h) {
    if (!g_framebuffer || !spritesheet) return;

    uint8_t *data = spritesheet + w * h * offset * depth;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            uint8_t r = data[depth * (y * w + x) + 0];
            uint8_t g = data[depth * (y * w + x) + 1];
            uint8_t b = data[depth * (y * w + x) + 2];

            uint16_t color = rgb888_to_rgb565(r, g, b);
            int idx = (ty + y) * g_resolution.w + (tx + x);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            color = (color << 8) | (color >> 8);
#endif

            if (color != 0x0000) g_framebuffer[idx] = color;

        }
    }
}

static void draw_rect(int x1, int y1, int x2, int y2, uint16_t color) {
    if (!g_framebuffer || x2 < x1 || y2 < y1) return;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    color = (color << 8) | (color >> 8);
#endif

    for (int y = y1; y < y2; y++)
        for (int x = x1; x < x2; x++)
            g_framebuffer[y * g_resolution.w + x] = color;
}

static void draw_square(int x1, int y1, int len, uint16_t color) {
    draw_rect(x1, y1, x1 + len, y1 + len, color);
}

static void lcd_flush() {
    if (!g_framebuffer) return;

    esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, 0, g_resolution.w, g_resolution.h, g_framebuffer);
}

typedef const uint8_t reg_t;
static reg_t g_imu_ctrl1 = 0x02;
static reg_t g_imu_ctrl2 = 0x03;
static reg_t g_imu_ctrl3 = 0x04;
//static reg_t g_imu_ctrl4 = 0x05;
static reg_t g_imu_ctrl7 = 0x08;
static reg_t g_imu_ctrl9 = 0x0A;
static i2c_master_dev_handle_t g_imu_handle;

static void read_imu_data(reg_t reg_start, int16_t *x, int16_t *y, int16_t *z) {
    uint8_t wbuf[1];
    uint8_t rbuf[6];
    memset(wbuf, 0x00, 1);
    memset(rbuf, 0x00, 6);

    // Reset FIFO
    wbuf[0] = g_imu_ctrl9;
    wbuf[1] = 0x04;
    i2c_master_transmit(g_imu_handle, wbuf, 1, 1000);


    wbuf[0] = 0x2F; // STATUS1 register
    ESP_ERROR_CHECK(
            i2c_master_transmit_receive(g_imu_handle, wbuf, 1, rbuf, 6, 1000));

    wbuf[0] = reg_start; // First out of 6 registers
    ESP_ERROR_CHECK(
            i2c_master_transmit_receive(g_imu_handle, wbuf, 1, rbuf, 6, 1000));

    *x = (int16_t)(rbuf[0]) + ((int16_t)(rbuf[1]) << 8);
    *y = (int16_t)(rbuf[2]) + ((int16_t)(rbuf[3]) << 8);
    *z = (int16_t)(rbuf[4]) + ((int16_t)(rbuf[5]) << 8);
}

static void read_accel_data(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z) {
    read_imu_data(0x35, accel_x, accel_y, accel_z);
}

static void read_gyro_data(int16_t *angle_x, int16_t *angle_y, int16_t *angle_z) {
    read_imu_data(0x3B, angle_x, angle_y, angle_z);
}

static void read_magno_data(int16_t *magno_x, int16_t *magno_y, int16_t *magno_z) {
    read_imu_data(0x65, magno_x, magno_y, magno_z);
}

static esp_err_t init_imu(imu_enable_t enable_flags) {
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
    wbuf[1] = enable_flags;
    log_debug("Sending {0x%.02X 0x%.02X} to 0x%.02X", wbuf[0], wbuf[1], imu_cfg.device_address);
    i2c_master_transmit(g_imu_handle, wbuf, 2, 1000);

    WAIT(50);

    return ESP_OK;
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
        .max_transfer_sz = g_resolution.h * g_draw_buf_height * sizeof(uint16_t),
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

    esp_lcd_panel_invert_color(g_lcd_panel, true);
    esp_lcd_panel_disp_on_off(g_lcd_panel, true);
    esp_lcd_panel_mirror(g_lcd_panel, false, false);
    esp_lcd_panel_set_gap(g_lcd_panel, 0, 20);

    ESP_ERROR_CHECK(
            gpio_set_level(g_gpio_lcd_bl, 1));

    size_t size = g_resolution.w * g_resolution.h  * panel_cfg.bits_per_pixel;
    g_framebuffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

    assert_log(g_framebuffer != NULL,
            "FATAL ERROR: Failed to allocate %zuB for g_framebuffer", size);

    draw_rect(0, 0, g_screen_w, g_screen_h, 0x00);
    lcd_flush();

    return err;
}

static void exit_panel() {
    // TODO
}

static void draw_tile(Skin *skin, int x, int y) {
    int tile_w =  g_screen_w / GLOBALS.view_port_maxx;
    int tile_h =  tile_w;
    uint16_t color = rgb888_to_rgb565(skin->fg_r, skin->fg_g, skin->fg_b);

    if (g_render_sprites && g_available_spritesheets && 0 < skin->glyph) {

        if (!g_spritesheet) {
            const char *path = (const char*) qu_peek(g_available_spritesheets);

            log_debug("Loading '%s'", path);

            uint8_t *tmp = load_rgb(path, g_spritesheet_size);
            g_spritesheet = image_shrink(tmp, g_sprite_w, 7 * g_sprite_h, 4, tile_w, 7 * tile_h);
            free(tmp);
        }

        draw_sprite(g_spritesheet, x * tile_w, y * tile_h, 4, skin->glyph - 1, tile_w, tile_h);

    } else
        draw_square(x * tile_w, y * tile_h, tile_w, color);

    game_set_dirty(GLOBALS.game, x, y, 0);
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

                        draw_tile(skin, x, y);
                    }
                }
            }
        }


    // Update all tiles
    } else if (df->command == -1) {

        skin = GLOBALS.game->entity_types->default_skin;

        uint16_t bg = rgb888_to_rgb565(skin->fg_r, skin->fg_g, skin->fg_b);

        draw_rect(0, 0, g_resolution.w, g_resolution.h, bg);

        for (int y = 0; y < GLOBALS.view_port_maxy; y++) {
            for (int x = 0; x < GLOBALS.view_port_maxx; x++) {

                skin = game_world_getxy(GLOBALS.game, x, y);

                draw_tile(skin, x, y);
            }
        }
    }

    lcd_flush();

    df->command = 0;
    return 0;
}

static const uint8_t I_STILL = 0;
static const uint8_t I_UP    = 1;
static const uint8_t I_DOWN  = 2;
static const uint8_t I_LEFT  = 3;
static const uint8_t I_RIGHT = 4;
static uint8_t g_intent = I_STILL;
static uint8_t g_move = I_STILL;

static int job_imu_input(Task *task, Stack64 *st) {
    static const int thld = 10000;
    int16_t x, y, z;

    read_accel_data(&x, &y, &z);

    if      (x > thld)  g_intent = I_DOWN;
    else if (x < -thld) g_intent = I_UP;
    else if (y > thld)  g_intent = I_LEFT;
    else if (y < -thld) g_intent = I_RIGHT;
    else                g_intent = I_STILL;

    event_ctx_t ctx = E_CTX_GAME;
    InputEvent ie = {
        .type = E_KB_DOWN,
        .mods = E_NOMOD,
    };

    event_t kb_down = E_NULL;
    event_t kb_up = E_NULL;

    if (g_move != g_intent) {
        g_move = g_intent;

        if (g_move == I_LEFT) {
            ie.state = ES_UP;
            ie.id = E_KB_W;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_S;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_D;
            frontend_dispatch_event(ctx, &ie);

            ie.state = ES_DOWN;
            ie.id = E_KB_A;
            frontend_dispatch_event(ctx, &ie);
        }

        else if (g_move== I_RIGHT) {
            ie.state = ES_UP;
            ie.id = E_KB_W;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_S;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_A;
            frontend_dispatch_event(ctx, &ie);

            ie.state = ES_DOWN;
            ie.id = E_KB_D;
            frontend_dispatch_event(ctx, &ie);
        }

        else if (g_move == I_UP) {
            ie.state = ES_UP;
            ie.id = E_KB_S;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_A;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_D;
            frontend_dispatch_event(ctx, &ie);

            ie.state = ES_DOWN;
            ie.id = E_KB_W;
            frontend_dispatch_event(ctx, &ie);
        }

        else if (g_move == I_DOWN) {
            ie.state = ES_UP;
            ie.id = E_KB_W;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_A;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_D;
            frontend_dispatch_event(ctx, &ie);

            ie.state = ES_DOWN;
            ie.id = E_KB_S;
            frontend_dispatch_event(ctx, &ie);

        } else if (g_move == I_STILL) {
            ie.state = ES_UP;
            ie.id = E_KB_W;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_A;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_S;
            frontend_dispatch_event(ctx, &ie);
            ie.id = E_KB_D;
            frontend_dispatch_event(ctx, &ie);
        }
    }

    return 0;
}


/* External API Functions */
static bool set_glyphset(const char* name) {
    return false;
}

void lcd_test() {
    int t = 3;
    int len = g_resolution.w / t;

    draw_rect(0, 0, g_resolution.w, g_resolution.h, 0x0000);
    lcd_flush();
    WAIT(1000);
    draw_rect(0, 0, g_resolution.w, g_resolution.h, 0xFFFF);
    lcd_flush();
    WAIT(1000);
    draw_rect(0, 0, g_resolution.w, g_resolution.h, 0x8080);
    lcd_flush();
    WAIT(1000);

    int s = 50;
    draw_square(0, g_resolution.h-1*s, s, 0x7BCF);
    draw_square(0, g_resolution.h-2*s, s, 0x7CCF);
    draw_square(0, g_resolution.h-3*s, s, 0x7DCF);
    lcd_flush();
    WAIT(1000);

    const uint16_t a = 0b0000000000011111;
    const uint16_t b = 0b1111100000000000;
    const uint16_t c = 0b0001111100000000;
    const uint16_t d = 0b0000000011111000;
    const uint16_t _c = (a>>8) | (a<<8);
    const uint16_t _d = (b>>8) | (b<<8);

    static uint16_t colors[] = {
        0x0000,
        a,
        b,

        0xFFFF,
        c,
        d,

        _c,
        _d,
        0x0000,
    };

    for (int i = 0; i < t*t; i++) {
        int x = (i % t) * len;
        int y = (i / t) * len;

        //uint16_t c = (colors[i] << 8) | (colors[i] >> 8);
        uint16_t c = colors[i]; 
        draw_square(x, y, len, c);
    }

    lcd_flush();
    WAIT(1000);
}

void btn_isr_render_toggle(void *args) {
    if (g_render_sprites && g_available_spritesheets) {
        qu_next(g_available_spritesheets);
        g_spritesheet = NULL;
    }

    g_render_sprites = true;// !g_render_sprites;
    GLOBALS.game->cache_dirty_flags->command = -1;
}

void btn_isr_break_tile(void *args) {
    InputEvent ie = {
        .id = E_KB_Z,
        .type = E_TYPE_KB,
        .state = ES_DOWN,
        .mods = E_NOMOD,
    };

    frontend_dispatch_event(E_CTX_GAME, &ie);

    ie.state = ES_UP;
    frontend_dispatch_event(E_CTX_GAME, &ie);
}

static esp_err_t init_btn(gpio_num_t gpio, void (*handler)(void*)) {

    ESP_RETURN_ON_ERROR(gpio_reset_pin(gpio),
            TAG, "Failed to reset gpio pin %d", gpio);

    gpio_config_t bcfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&bcfg),
            TAG, "Failed to configure gpio %d", gpio);

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(gpio, handler, NULL),
            TAG, "Failed to add ISR handler %p for gpio %d", handler, gpio);

    log_debug("Initialized isr for gpio %d", gpio);
    log_debug_nl();

    return ESP_OK;
}

int frontend_esp32s3_ui_init(Frontend* fr, const char *title) {
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = "assets",
        .max_files = 3,
        .format_if_mount_failed = false,
    };

    esp_vfs_spiffs_register(&config);

    // Accelerometer requires gyro mode enabled
    imu_enable_t flags = IMU_ENABLE_ACCELEROMETER | IMU_ENABLE_GYROSCOPE;

    ESP_ERROR_CHECK(init_panel());
    ESP_ERROR_CHECK(init_imu(flags));

    fr->f_set_glyphset = set_glyphset;

    schedule(GLOBALS.runqueue, 0, 0, job_render, NULL);

    uint8_t *tmp = load_rgb("/spiffs/splash.raw", 128*64*3);

    draw_rect(0, 0, g_resolution.w, g_resolution.h, 0x2020);
    draw_sprite(tmp, g_resolution.w/2-128/2, g_resolution.h/2-64/2, 3, 0, 128, 64);

    lcd_flush();
    WAIT(2000);

    free(tmp);

    g_available_spritesheets = scan_available_spritesheets("/spiffs", "spr_");

    ESP_RETURN_ON_ERROR(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1),
            TAG, "Failed to install ISR for gpio");

    ESP_ERROR_CHECK(init_btn(GPIO_NUM_18, btn_isr_break_tile));
    ESP_ERROR_CHECK(init_btn(GPIO_NUM_40, btn_isr_render_toggle));

    return 0;
}

void frontend_esp32s3_ui_exit() {
    esp_vfs_spiffs_unregister(NULL);
    exit_panel();
    free(g_spritesheet);
    free(g_framebuffer);
}

int frontend_esp32s3_input_init(Frontend*, const char*) {
    schedule(GLOBALS.runqueue, 0, 0, job_imu_input, NULL);
    return 0;
}

void frontend_esp32s3_input_exit() {}
