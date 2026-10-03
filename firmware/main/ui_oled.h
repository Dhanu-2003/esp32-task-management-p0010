// On-device UI: SSD1306 OLED + 3 buttons mirror the task list, no phone needed.
// LIST: UP/DOWN scroll, SELECT toggles done, long-SELECT opens the menu.
// MENU: Add quick task / Delete selected / Reset all / Back.
#pragma once

// Starts the UI task. Call after task_store_init(); never returns (own task).
void ui_oled_start(void);
