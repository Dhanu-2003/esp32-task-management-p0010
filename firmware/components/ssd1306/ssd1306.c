// Minimal SSD1306 I2C driver stub — full driver lands in step 4.
#include "ssd1306.h"

esp_err_t ssd1306_init(i2c_master_bus_handle_t bus, unsigned char addr)
{
    (void) bus;
    (void) addr;
    return ESP_OK;
}
