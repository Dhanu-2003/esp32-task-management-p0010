// TaskDeck board wiring — single place to rewire for your ESP32 board.
// Defaults: SSD1306 128x64 I2C OLED + 3 active-low buttons with internal pull-ups.
#pragma once

// I2C OLED display
#define TASK_OLED_SDA_GPIO      21
#define TASK_OLED_SCL_GPIO      22
#define TASK_OLED_I2C_ADDR      0x3C
#define TASK_OLED_WIDTH         128
#define TASK_OLED_HEIGHT        64

// Buttons (active-low, INPUT_PULLUP)
#define TASK_BTN_UP_GPIO        15
#define TASK_BTN_DOWN_GPIO      2
#define TASK_BTN_SELECT_GPIO    4

// Behaviour
#define TASK_BTN_DEBOUNCE_MS    50
#define TASK_BTN_LONG_PRESS_MS  800
