/* ============================================================================
 * include/ssd1306.h
 * ----------------------------------------------------------------------------
 * Minimal SSD1306 OLED driver (128x64, I2C) for ESP-IDF v6.1+ using the NEW
 * esp_driver_i2c master API.
 *
 * SPDX-License-Identifier: MIT
 * ==========================================================================*/

#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH         128
#define SSD1306_PAGES         8
#define SSD1306_HEIGHT        64
#define SSD1306_FRAMEBUF_SIZE (SSD1306_WIDTH * SSD1306_PAGES)

/* 6 px per character (5 px glyph + 1 px spacing) */
#define SSD1306_CHAR_WIDTH 6

/**
 * @brief Create the I2C master bus (if needed) and the SSD1306 device, then
 *        send the panel init sequence and clear the framebuffer.
 *
 * Idempotent: any previously created bus/device created by this driver is
 * removed before a new one is created.
 *
 * @param sda_gpio   SDA GPIO number
 * @param scl_gpio   SCL GPIO number
 * @param i2c_addr   7-bit I2C address of the panel (typically 0x3C or 0x3D)
 *
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t ssd1306_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr);

/** @brief Clear (zero) the internal framebuffer. Does not push to the panel. */
void ssd1306_clear(void);

/**
 * @brief Draw monospace 5x7 text into the internal framebuffer.
 *
 * @param x_px    Left pixel column, 0..127
 * @param page_y  Page row (8 px pages), 0..7
 * @param text    NUL-terminated string; bytes outside ASCII 32..126 are
 *                skipped safely (never drawn, never a crash, UTF-8 safe)
 * @param invert  false = OR the glyph pixels, true = XOR (inverted) pixels
 */
void ssd1306_draw_text(int x_px, int page_y, const char *text, bool invert);

/** @brief Flush the internal framebuffer to the panel (128 bytes per page). */
void ssd1306_show(void);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */