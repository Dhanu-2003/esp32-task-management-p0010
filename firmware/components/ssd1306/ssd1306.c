/* ============================================================================
 * ssd1306.c
 * ----------------------------------------------------------------------------
 * SSD1306 128x64 I2C driver, ESP-IDF v6.1 new i2c master driver.
 *
 * SPDX-License-Identifier: MIT
 * ==========================================================================*/

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ssd1306.h"

static const char *TAG = "ssd1306";

#define SSD1306_I2C_PORT I2C_NUM_0
#define SSD1306_I2C_HZ         400000
#define SSD1306_I2C_TIMEOUT_MS 5000

#define SSD1306_CTRL_CMD  0x00
#define SSD1306_CTRL_DATA 0x40

/* Font glyph: 5 columns, 8 rows, LSB = top row. */
#define FONT_FIRST 0x20
#define FONT_LAST  0x7E
#define FONT_W     5

static uint8_t s_fb[SSD1306_FRAMEBUF_SIZE];
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

/* Classic 5x7 ASCII font, characters 0x20 (' ') .. 0x7E ('~'). */
static const uint8_t s_font5x7[(FONT_LAST - FONT_FIRST + 1) * FONT_W] = {
    0x00, 0x00, 0x00, 0x00, 0x00, /*   */
    0x00, 0x00, 0x5F, 0x00, 0x00, /* ! */
    0x00, 0x07, 0x00, 0x07, 0x00, /* " */
    0x14, 0x7F, 0x14, 0x7F, 0x14, /* # */
    0x24, 0x2A, 0x7F, 0x2A, 0x12, /* $ */
    0x23, 0x13, 0x08, 0x64, 0x62, /* % */
    0x36, 0x49, 0x55, 0x22, 0x50, /* & */
    0x00, 0x05, 0x03, 0x00, 0x00, /* ' */
    0x00, 0x1C, 0x22, 0x41, 0x00, /* ( */
    0x00, 0x41, 0x22, 0x1C, 0x00, /* ) */
    0x14, 0x08, 0x3E, 0x08, 0x14, /* * */
    0x08, 0x08, 0x3E, 0x08, 0x08, /* + */
    0x00, 0x50, 0x30, 0x00, 0x00, /* , */
    0x08, 0x08, 0x08, 0x08, 0x08, /* - */
    0x00, 0x60, 0x60, 0x00, 0x00, /* . */
    0x20, 0x10, 0x08, 0x04, 0x02, /* / */
    0x3E, 0x51, 0x49, 0x45, 0x3E, /* 0 */
    0x00, 0x42, 0x7F, 0x40, 0x00, /* 1 */
    0x42, 0x61, 0x51, 0x49, 0x46, /* 2 */
    0x21, 0x41, 0x45, 0x4B, 0x31, /* 3 */
    0x18, 0x14, 0x12, 0x7F, 0x10, /* 4 */
    0x27, 0x45, 0x45, 0x45, 0x39, /* 5 */
    0x3C, 0x4A, 0x49, 0x49, 0x30, /* 6 */
    0x01, 0x71, 0x09, 0x05, 0x03, /* 7 */
    0x36, 0x49, 0x49, 0x49, 0x36, /* 8 */
    0x06, 0x49, 0x49, 0x29, 0x1E, /* 9 */
    0x00, 0x36, 0x36, 0x00, 0x00, /* : */
    0x00, 0x56, 0x36, 0x00, 0x00, /* ; */
    0x08, 0x14, 0x22, 0x41, 0x00, /* < */
    0x14, 0x14, 0x14, 0x14, 0x14, /* = */
    0x00, 0x41, 0x22, 0x14, 0x08, /* > */
    0x02, 0x01, 0x51, 0x09, 0x06, /* ? */
    0x32, 0x49, 0x79, 0x41, 0x3E, /* @ */
    0x7E, 0x11, 0x11, 0x11, 0x7E, /* A */
    0x7F, 0x49, 0x49, 0x49, 0x36, /* B */
    0x3E, 0x41, 0x41, 0x41, 0x22, /* C */
    0x7F, 0x41, 0x41, 0x22, 0x1C, /* D */
    0x7F, 0x49, 0x49, 0x49, 0x41, /* E */
    0x7F, 0x09, 0x09, 0x09, 0x01, /* F */
    0x3E, 0x41, 0x49, 0x49, 0x7A, /* G */
    0x7F, 0x08, 0x08, 0x08, 0x7F, /* H */
    0x00, 0x41, 0x7F, 0x41, 0x00, /* I */
    0x20, 0x40, 0x41, 0x3F, 0x01, /* J */
    0x7F, 0x08, 0x14, 0x22, 0x41, /* K */
    0x7F, 0x04, 0x08, 0x10, 0x7F, /* L */
    0x7F, 0x02, 0x0C, 0x02, 0x7F, /* M */
    0x7F, 0x04, 0x08, 0x10, 0x7F, /* N */
    0x3E, 0x41, 0x41, 0x41, 0x3E, /* O */
    0x7F, 0x09, 0x09, 0x09, 0x06, /* P */
    0x3E, 0x41, 0x51, 0x21, 0x5E, /* Q */
    0x7F, 0x09, 0x19, 0x29, 0x46, /* R */
    0x46, 0x49, 0x49, 0x49, 0x31, /* S */
    0x01, 0x01, 0x7F, 0x01, 0x01, /* T */
    0x3F, 0x40, 0x40, 0x40, 0x3F, /* U */
    0x1F, 0x20, 0x40, 0x20, 0x1F, /* V */
    0x3F, 0x40, 0x38, 0x40, 0x3F, /* W */
    0x63, 0x14, 0x08, 0x14, 0x63, /* X */
    0x07, 0x08, 0x70, 0x08, 0x07, /* Y */
    0x61, 0x51, 0x49, 0x45, 0x43, /* Z */
    0x00, 0x7F, 0x41, 0x41, 0x00, /* [ */
    0x02, 0x04, 0x08, 0x10, 0x20, /* \ */
    0x00, 0x41, 0x41, 0x7F, 0x00, /* ] */
    0x04, 0x02, 0x01, 0x02, 0x04, /* ^ */
    0x40, 0x40, 0x40, 0x40, 0x40, /* _ */
    0x00, 0x01, 0x02, 0x04, 0x00, /* ` */
    0x20, 0x54, 0x54, 0x54, 0x78, /* a */
    0x7F, 0x48, 0x44, 0x44, 0x38, /* b */
    0x38, 0x44, 0x44, 0x44, 0x20, /* c */
    0x38, 0x44, 0x44, 0x48, 0x7F, /* d */
    0x38, 0x54, 0x54, 0x54, 0x18, /* e */
    0x08, 0x7E, 0x09, 0x01, 0x02, /* f */
    0x0C, 0x52, 0x52, 0x52, 0x3E, /* g */
    0x7F, 0x08, 0x04, 0x04, 0x78, /* h */
    0x00, 0x44, 0x7D, 0x40, 0x00, /* i */
    0x20, 0x40, 0x44, 0x3D, 0x00, /* j */
    0x7F, 0x10, 0x28, 0x44, 0x00, /* k */
    0x00, 0x41, 0x7F, 0x40, 0x00, /* l */
    0x7C, 0x04, 0x18, 0x04, 0x78, /* m */
    0x7C, 0x08, 0x04, 0x04, 0x78, /* n */
    0x38, 0x44, 0x44, 0x44, 0x38, /* o */
    0x7C, 0x14, 0x14, 0x14, 0x08, /* p */
    0x08, 0x14, 0x14, 0x18, 0x7C, /* q */
    0x7C, 0x08, 0x04, 0x04, 0x08, /* r */
    0x48, 0x54, 0x54, 0x54, 0x20, /* s */
    0x04, 0x3F, 0x44, 0x40, 0x20, /* t */
    0x3C, 0x40, 0x40, 0x20, 0x7C, /* u */
    0x1C, 0x20, 0x40, 0x20, 0x1C, /* v */
    0x3C, 0x40, 0x30, 0x40, 0x3C, /* w */
    0x44, 0x28, 0x10, 0x28, 0x44, /* x */
    0x0C, 0x50, 0x50, 0x50, 0x3C, /* y */
    0x44, 0x64, 0x54, 0x4C, 0x44, /* z */
    0x00, 0x08, 0x36, 0x41, 0x00, /* { */
    0x00, 0x00, 0x7F, 0x00, 0x00, /* | */
    0x00, 0x41, 0x36, 0x08, 0x00, /* } */
    0x10, 0x08, 0x08, 0x10, 0x08, /* ~ */
};

static esp_err_t write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {SSD1306_CTRL_CMD, cmd};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), pdMS_TO_TICKS(SSD1306_I2C_TIMEOUT_MS));
}

static esp_err_t write_cmds(const uint8_t *cmds, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        esp_err_t err = write_cmd(cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t write_data(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    uint8_t *buf = malloc(len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    buf[0] = SSD1306_CTRL_DATA;
    memcpy(buf + 1, data, len);
    esp_err_t err = i2c_master_transmit(s_dev, buf, len + 1, pdMS_TO_TICKS(SSD1306_I2C_TIMEOUT_MS));
    free(buf);
    return err;
}

static esp_err_t deinit_existing(void)
{
    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus) {
        i2c_master_bus_rm_device(NULL); // ensure no pending
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    return ESP_OK;
}

esp_err_t ssd1306_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr)
{
    esp_err_t err = deinit_existing();
    if (err != ESP_OK) {
        return err;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = SSD1306_I2C_PORT,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = SSD1306_I2C_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
        return err;
    }

    /* Init sequence per SSD1306 datasheet */
    static const uint8_t init_seq[] = {
        0xAE,       // display off
        0xD5, 0x80, // set display clock divide ratio/oscillator frequency
        0xA8, 0x3F, // set multiplex ratio (1/64)
        0xD3, 0x00, // set display offset
        0x40,       // set start line
        0x8D, 0x14, // charge pump setting
        0x20, 0x02, // memory addressing mode (page)
        0xA1,       // segment re-map (col 127 mapped to SEG0)
        0xC8,       // com output scan direction remapped
        0xDA, 0x12, // set com pins hardware configuration
        0x81, 0xCF, // set contrast control
        0xD9, 0xF1, // set pre-charge period
        0xDB, 0x40, // set VCOMH deselect level
        0xA4,       // entire display ON (resume)
        0xA6,       // set normal display (not inverted)
        0xAF        // display ON
    };
    err = write_cmds(init_seq, sizeof(init_seq));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init sequence failed: %s", esp_err_to_name(err));
        i2c_master_bus_rm_device(s_dev);
        i2c_del_master_bus(s_bus);
        s_dev = NULL;
        s_bus = NULL;
        return err;
    }

    memset(s_fb, 0, sizeof(s_fb));
    ESP_LOGI(TAG, "SSD1306 ready (sda=%d scl=%d addr=0x%02x)", sda_gpio, scl_gpio, i2c_addr);
    return ESP_OK;
}

void ssd1306_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

void ssd1306_draw_text(int x_px, int page_y, const char *text, bool invert)
{
    if (page_y < 0 || page_y >= SSD1306_PAGES || x_px < 0 || x_px >= SSD1306_WIDTH) {
        return;
    }
    int x = x_px;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        unsigned char ch = *p;
        if (ch < FONT_FIRST || ch > FONT_LAST) {
            continue;
        }
        if (x + SSD1306_CHAR_WIDTH > SSD1306_WIDTH) {
            break;
        }
        const uint8_t *glyph = &s_font5x7[(ch - FONT_FIRST) * FONT_W];
        for (int col = 0; col < FONT_W; col++) {
            uint8_t col_bits = glyph[col];
            int fb_idx = page_y * SSD1306_WIDTH + x + col;
            if (invert) {
                s_fb[fb_idx] ^= col_bits;
            } else {
                s_fb[fb_idx] |= col_bits;
            }
        }
        x += SSD1306_CHAR_WIDTH;
    }
}

void ssd1306_show(void)
{
    for (int page = 0; page < SSD1306_PAGES; page++) {
        esp_err_t err = write_cmd((uint8_t)(0xB0 | page)); // set page
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set page %d failed: %s", page, esp_err_to_name(err));
            return;
        }
        err = write_cmd(0x00); // lower column start
        if (err != ESP_OK) {
            return;
        }
        err = write_cmd(0x10); // higher column start
        if (err != ESP_OK) {
            return;
        }
        err = write_data(&s_fb[page * SSD1306_WIDTH], SSD1306_WIDTH);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "write page %d failed: %s", page, esp_err_to_name(err));
            return;
        }
    }
}