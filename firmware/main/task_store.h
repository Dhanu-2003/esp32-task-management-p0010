// Shared task model + persistent store (NVS JSON blob, mutex-guarded).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define TASK_TITLE_MAX 120
#define TASK_STORE_MAX 100

typedef struct {
    uint32_t id;
    char title[TASK_TITLE_MAX + 1];
    bool done;
    uint8_t priority; // 0 = low, 1 = normal, 2 = high
    int64_t created;
    int64_t updated;
} task_t;

// Load from NVS (tolerates missing/corrupt data). Call once from app_main.
esp_err_t task_store_init(void);
int task_store_count(void);
// Caller frees *out with free(). *n = number of tasks (creation order).
esp_err_t task_store_list(task_t **out, size_t *n);
esp_err_t task_store_add(const char *title, uint8_t priority, task_t *created_out);
esp_err_t task_store_update(uint32_t id, const char *title /*NULL=keep*/,
                            const bool *done /*NULL=keep*/,
                            const uint8_t *priority /*NULL=keep*/);
esp_err_t task_store_toggle(uint32_t id, task_t *out);
esp_err_t task_store_delete(uint32_t id);
// Caller frees *out_json with free().
esp_err_t task_store_to_json(char **out_json);
// Force any coalesced write to NVS now.
esp_err_t task_store_flush(void);
// Delete all tasks (factory reset). Persists immediately.
esp_err_t task_store_clear(void);
