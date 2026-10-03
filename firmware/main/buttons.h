// Push-buttons: UP / DOWN / SELECT, active-low with internal pull-ups.
// Poll task with debounce; delivers short-press (on release) and long-press events.
#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"

typedef enum {
    BTN_UP,
    BTN_DOWN,
    BTN_SELECT,
} btn_id_t;

typedef struct {
    btn_id_t id;
    bool long_press; // true = held >= LONG threshold (sent once); false = short press
} btn_event_t;

// Starts the poll task. Safe to call once from app_main.
void buttons_init(void);
// Block up to timeout for the next button event. Returns false on timeout.
bool buttons_get(btn_event_t *ev, TickType_t timeout);
// True while the SELECT button is physically held (for boot-time factory reset).
bool buttons_select_held(void);
