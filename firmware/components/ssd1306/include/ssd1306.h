// Minimal SSD1306 I2C driver stub — full driver lands in step 4.
// (Will credit upstream source in step 4; placeholder keeps step-2 CMake valid.)
#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ssd1306_init(i2c_master_bus_handle_t bus, unsigned char addr);

#ifdef __cplusplus
}
#endif
