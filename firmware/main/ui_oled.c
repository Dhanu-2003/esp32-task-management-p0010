#include "ui_oled.h"

#include <stdlib.h>
#include <string.h>

#include "buttons.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"
#include "ssd1306.h"
#include "task_store.h"

static const char *TAG = "ui";

// 21 chars/line at 6px; rows: 0=header, 1..5=tasks, 7=footer hint.
#define ROWS 5
#define COLS 21

typedef enum {
    UI_LIST,
    UI_MENU,
    UI_CONFIRM_RESET,
    UI_MESSAGE,
} ui_mode_t;

static const char *MENU_ITEMS[] = {"Add quick task", "Delete selected", "Reset all", "Back"};

static int s_cursor = 0;
static int s_menu = 0;
static ui_mode_t s_mode = UI_LIST;
static char s_msg[COLS + 1] = "";

// Copy at most COLS display columns without splitting a UTF-8 sequence.
static void safe_trunc(char *dst, const char *src, size_t max_bytes)
{
    size_t n = strlen(src);
    if (n > max_bytes) {
        n = max_bytes;
        while (n > 0 && (src[n] & 0xC0) == 0x80) {
            n--;
        }
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void show_header(void)
{
    char hdr[COLS + 1];
    int open = 0;
    task_t *tasks = NULL;
    size_t n = 0;
    if (task_store_list(&tasks, &n) == ESP_OK) {
        for (size_t i = 0; i < n; i++) {
            if (!tasks[i].done) {
                open++;
            }
        }
        free(tasks);
    }
    snprintf(hdr, sizeof(hdr), "Tasks %d open", open);
    ssd1306_draw_text(0, 0, hdr, true);
}

static void show_list(void)
{
    show_header();
    task_t *tasks = NULL;
    size_t n = 0;
    if (task_store_list(&tasks, &n) != ESP_OK) {
        ssd1306_draw_text(0, 2, "Store error", false);
    } else if (n == 0) {
        ssd1306_draw_text(0, 2, "No tasks.", false);
        ssd1306_draw_text(0, 3, "Hold SEL: menu", false);
    } else {
        if (s_cursor >= (int)n) {
            s_cursor = (int)n - 1;
        }
        if (s_cursor < 0) {
            s_cursor = 0;
        }
        int top = s_cursor - (ROWS - 1);
        if (top < 0) {
            top = 0;
        }
        for (int r = 0; r < ROWS && (size_t)(top + r) < n; r++) {
            char line[COLS + 1];
            char title[COLS - 5];
            safe_trunc(title, tasks[top + r].title, sizeof(title) - 1);
            const char *mark = tasks[top + r].done ? "[x]" : "[ ]";
            const char *flag = tasks[top + r].priority == 2 ? "!" :
                               tasks[top + r].priority == 0 ? "." : " ";
            snprintf(line, sizeof(line), "%c%s%s%s",
                     (top + r) == s_cursor ? '>' : ' ', mark, flag, title);
            ssd1306_draw_text(0, 1 + r, line, (top + r) == s_cursor);
        }
    }
    free(tasks);
    ssd1306_draw_text(0, 7, "SEL done HLD menu", false);
    ssd1306_show();
}

static void show_menu(void)
{
    ssd1306_draw_text(0, 0, "Menu", true);
    for (int i = 0; i < 4; i++) {
        char line[COLS + 1];
        snprintf(line, sizeof(line), "%c%s", i == s_menu ? '>' : ' ', MENU_ITEMS[i]);
        ssd1306_draw_text(0, 1 + i, line, i == s_menu);
    }
    ssd1306_draw_text(0, 7, "SEL ok UP/DN move", false);
    ssd1306_show();
}

static void show_confirm(void)
{
    ssd1306_draw_text(0, 0, "Reset ALL tasks?", true);
    ssd1306_draw_text(0, 2, "SEL: wipe", false);
    ssd1306_draw_text(0, 3, "other: cancel", false);
    ssd1306_show();
}

static void show_message(void)
{
    ssd1306_draw_text(0, 0, "TaskDeck", true);
    ssd1306_draw_text(0, 3, s_msg, false);
    ssd1306_show();
}

static void flash_message(const char *msg)
{
    strncpy(s_msg, msg, sizeof(s_msg) - 1);
    s_msg[sizeof(s_msg) - 1] = '\0';
    s_mode = UI_MESSAGE;
    show_message();
    vTaskDelay(pdMS_TO_TICKS(1200));
    s_mode = UI_LIST;
}

static void do_menu_action(void)
{
    if (s_menu == 0) {
        char title[32];
        snprintf(title, sizeof(title), "Task %d", task_store_count() + 1);
        if (task_store_add(title, 1, NULL) == ESP_OK) {
            flash_message("Added. Edit online");
        } else {
            flash_message("Store full (100)");
        }
    } else if (s_menu == 1) {
        task_t *tasks = NULL;
        size_t n = 0;
        if (task_store_list(&tasks, &n) == ESP_OK && (size_t)s_cursor < n) {
            uint32_t id = tasks[s_cursor].id;
            free(tasks);
            tasks = NULL;
            if (task_store_delete(id) == ESP_OK) {
                flash_message("Deleted");
            } else {
                flash_message("Delete failed");
            }
        } else {
            flash_message("Nothing to delete");
        }
        free(tasks);
    } else if (s_menu == 2) {
        s_mode = UI_CONFIRM_RESET;
        return;
    }
    s_mode = UI_LIST;
}

static void handle_event(const btn_event_t *ev)
{
    if (s_mode == UI_CONFIRM_RESET) {
        if (ev->id == BTN_SELECT && !ev->long_press) {
            task_store_clear();
            flash_message("All tasks wiped");
        } else {
            s_mode = UI_MENU;
        }
        return;
    }
    if (s_mode == UI_MESSAGE) {
        return;
    }
    if (s_mode == UI_MENU) {
        if (ev->long_press) {
            s_mode = UI_LIST;
        } else if (ev->id == BTN_UP) {
            s_menu = (s_menu + 3) % 4;
        } else if (ev->id == BTN_DOWN) {
            s_menu = (s_menu + 1) % 4;
        } else {
            do_menu_action();
        }
        return;
    }
    // UI_LIST
    if (ev->long_press) {
        if (ev->id == BTN_SELECT) {
            s_menu = 0;
            s_mode = UI_MENU;
        }
        return;
    }
    if (ev->id == BTN_UP) {
        s_cursor--;
    } else if (ev->id == BTN_DOWN) {
        s_cursor++;
    } else {
        task_t *tasks = NULL;
        size_t n = 0;
        if (task_store_list(&tasks, &n) == ESP_OK && (size_t)s_cursor < n) {
            uint32_t id = tasks[s_cursor].id;
            free(tasks);
            tasks = NULL;
            task_store_toggle(id, NULL);
        }
        free(tasks);
    }
}

static void ui_task(void *arg)
{
    (void)arg;
    // Splash + boot-time factory reset: SELECT held ~3 s wipes everything.
    ssd1306_draw_text(0, 0, "TaskDeck", true);
    ssd1306_draw_text(0, 2, "Hold SEL 3s", false);
    ssd1306_draw_text(0, 3, "to factory reset", false);
    ssd1306_show();
    for (int i = 0; i < 30; i++) {
        if (!buttons_select_held()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        if (i == 29) {
            task_store_clear();
            strncpy(s_msg, "Factory reset!", sizeof(s_msg) - 1);
            s_mode = UI_MESSAGE;
            show_message();
            vTaskDelay(pdMS_TO_TICKS(1500));
            s_mode = UI_LIST;
        }
    }

    btn_event_t ev;
    while (1) {
        if (s_mode == UI_LIST) {
            show_list();
        } else if (s_mode == UI_MENU) {
            show_menu();
        } else if (s_mode == UI_CONFIRM_RESET) {
            show_confirm();
        }
        if (buttons_get(&ev, pdMS_TO_TICKS(5000))) {
            handle_event(&ev);
        }
    }
}

void ui_oled_start(void)
{
    if (ssd1306_init(TASK_OLED_SDA_GPIO, TASK_OLED_SCL_GPIO, TASK_OLED_I2C_ADDR) != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed (SDA=%d SCL=%d addr=0x%02x); continuing headless",
                 TASK_OLED_SDA_GPIO, TASK_OLED_SCL_GPIO, TASK_OLED_I2C_ADDR);
        return;
    }
    buttons_init();
    ESP_LOGI(TAG, "OLED UI started");
    xTaskCreate(ui_task, "ui_oled", 4096, NULL, 8, NULL);
}
