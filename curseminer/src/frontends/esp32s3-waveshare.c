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
#define WAIT(ms) vTaskDelay((ms) / (portTICK_PERIOD_MS));


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

esp_lcd_panel_handle_t g_lcd_panel;
static uint16_t *g_draw_buffer;

static void print_all_files(const char *path) {
    DIR *dir = opendir(path);

    if (!dir) {
        log_debug("Failed to open dir '%s'", path);
        return;
    }

    struct dirent *dp;

    while ((dp = readdir(dir)) != NULL)
        log_debug("File: '%s'", dp->d_name);

    closedir(dir);
}

static int load_spritesheet(char *path) {
    int err = 0;

    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = "assets",
        .max_files = 3,
        .format_if_mount_failed = false,
    };

    esp_vfs_spiffs_register(&config);

    print_all_files("/spiffs");

    FILE *file = fopen(path, "r");

    if(file == NULL) {
        ESP_LOGE("FILE", "File does not exist!");
        err = -1;

    } else {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (0 < size) {

            void *data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            g_spritesheet = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            int width, height, layers;

            fread(data, 1, size, file);

            g_spritesheet = stbi_load_from_memory(data, size, &width, &height, &layers, 0);

            log_debug("Spritesheet: %p [%dx%dx%d]", g_spritesheet, width, height, layers);

            free(data);

        } else {
            log_debug("Error: couldn't find '%s'", path);
            err = -1;
        }

        fclose(file);
    }

    esp_vfs_spiffs_unregister(NULL);

    return err;
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void draw_sprite(int tx, int ty, int offset, int crop_x, int crop_y) {
    if (!g_framebuffer || !g_spritesheet) return;
    if (g_sprite_w < crop_x || crop_x < 0) crop_x = g_sprite_w;
    if (g_sprite_h < crop_y || crop_y < 0) crop_y = g_sprite_h;

    uint8_t *data = g_spritesheet + (g_sprite_w * g_sprite_h * offset) * 4;

    for (int y = 0; y < crop_y; y++) {
        for (int x = 0; x < crop_x; x++) {

            uint8_t r = data[4 * (y * g_sprite_w + x) + 0];
            uint8_t g = data[4 * (y * g_sprite_w + x) + 1];
            uint8_t b = data[4 * (y * g_sprite_w + x) + 2];

            uint16_t color = rgb888_to_rgb565(r, g, b);
            int idx = (ty + y) * g_resolution.w + (tx + x);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            color = (color << 8) | (color >> 8);
#endif

            g_framebuffer[idx] = color;

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

                        uint16_t color = rgb888_to_rgb565(skin->fg_r, skin->fg_g, skin->fg_b);

                        int tile_w =  g_screen_w / GLOBALS.view_port_maxx;
                        int tile_h =  tile_w;

                        if (skin->glyph == 0)
                            draw_square(x * tile_w, y * tile_h, tile_w, color);
                        else
                            draw_sprite(x * tile_w, y * tile_h, skin->glyph - 1, tile_w, tile_h);

                        game_set_dirty(GLOBALS.game, x, y, 0);
                    }
                }
            }
        }


    // Update all tiles
    } else if (df->command == -1) {
        for (int y = 0; y < GLOBALS.view_port_maxy; y++) {
            for (int x = 0; x < GLOBALS.view_port_maxx; x++) {

                skin = game_world_getxy(GLOBALS.game, x, y);

                uint16_t color = rgb888_to_rgb565(skin->fg_r, skin->fg_g, skin->fg_b);

                int tile_w =  g_screen_w / GLOBALS.view_port_maxx;
                int tile_h =  tile_w;

                if (skin->glyph == 0)
                    draw_square(x * tile_w, y * tile_h, tile_w, color);
                else
                    draw_sprite(x * tile_w, y * tile_h, skin->glyph - 1, tile_w, tile_h);

                game_set_dirty(GLOBALS.game, x, y, 0);
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
    static const int thld = 5000;
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

void btn_isr(void *args) {
    g_counter++;

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

esp_err_t init_btn(gpio_num_t gpio) {

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

    ESP_RETURN_ON_ERROR(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1),
            TAG, "Failed to install ISR for gpio %d", gpio);

    log_debug("Assigning %p as isr handler function", btn_isr);
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(gpio, btn_isr, NULL),
            TAG, "Failed to add ISR handler %p for gpio %d", btn_isr, gpio);

    log_debug("Initialized isr for gpio %d", gpio);
    log_debug_nl();

    return ESP_OK;
}

int frontend_esp32s3_ui_init(Frontend* fr, const char *title) {

    ESP_ERROR_CHECK(init_btn(GPIO_NUM_18));

    // Accelerometer requires gyro mode enabled
    imu_enable_t flags = IMU_ENABLE_ACCELEROMETER | IMU_ENABLE_GYROSCOPE;

    ESP_ERROR_CHECK(init_panel());
    ESP_ERROR_CHECK(init_imu(flags));

    fr->f_set_glyphset = set_glyphset;

    schedule(GLOBALS.runqueue, 0, 0, job_render, NULL);

    if (0 != load_spritesheet("/spiffs/tiles_01.png")) return -1;

    int i = 2;
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 0, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 1, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 2, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 3, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 4, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 5, 0, 0);
    draw_sprite(4 *g_sprite_w, i++ * g_sprite_h, 6, 0, 0);
    lcd_flush();
    WAIT(2000);

    return 0;
}

void frontend_esp32s3_ui_exit() {
    exit_panel();
    free(g_spritesheet);
    free(g_framebuffer);
}

int frontend_esp32s3_input_init(Frontend*, const char*) {
    schedule(GLOBALS.runqueue, 0, 0, job_imu_input, NULL);
    return 0;
}

void frontend_esp32s3_input_exit() {}
