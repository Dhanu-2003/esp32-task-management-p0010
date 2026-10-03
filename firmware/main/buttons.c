#include "buttons.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "pins.h"

static const char *TAG = "buttons";

#define POLL_MS 10

static QueueHandle_t s_queue;
static bool s_gpio_done = false;

static void config_gpios(void)
{
    if (s_gpio_done) {
        return;
    }
    s_gpio_done = true;
    const int gpios[3] = {TASK_BTN_UP_GPIO, TASK_BTN_DOWN_GPIO, TASK_BTN_SELECT_GPIO};
    for (int i = 0; i < 3; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << gpios[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));
    }
}

typedef struct {
    btn_id_t id;
    int gpio;
    bool last_level; // true = pressed (active low => level 0)
    uint32_t press_ms;
    bool long_sent;
} btn_state_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * 1000 / configTICK_RATE_HZ);
}

static void poll_task(void *arg)
{
    (void)arg;
    btn_state_t st[3] = {
        {BTN_UP, TASK_BTN_UP_GPIO, false, 0, false},
        {BTN_DOWN, TASK_BTN_DOWN_GPIO, false, 0, false},
        {BTN_SELECT, TASK_BTN_SELECT_GPIO, false, 0, false},
    };
    btn_event_t ev;
    while (1) {
        uint32_t now = now_ms();
        for (int i = 0; i < 3; i++) {
            bool pressed = (gpio_get_level(st[i].gpio) == 0);
            if (pressed != st[i].last_level) {
                // Possible edge: require stable level for debounce window.
                vTaskDelay(pdMS_TO_TICKS(TASK_BTN_DEBOUNCE_MS));
                bool stable = (gpio_get_level(st[i].gpio) == 0);
                if (stable != pressed) {
                    continue;
                }
                st[i].last_level = pressed;
                if (pressed) {
                    st[i].press_ms = now_ms();
                    st[i].long_sent = false;
                } else {
                    if (!st[i].long_sent) {
                        ev.id = st[i].id;
                        ev.long_press = false;
                        xQueueSend(s_queue, &ev, 0);
                    }
                }
            } else if (pressed && !st[i].long_sent &&
                       (now - st[i].press_ms) >= TASK_BTN_LONG_PRESS_MS) {
                st[i].long_sent = true;
                ev.id = st[i].id;
                ev.long_press = true;
                xQueueSend(s_queue, &ev, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void buttons_init(void)
{
    if (s_queue) {
        return;
    }
    s_queue = xQueueCreate(16, sizeof(btn_event_t));
    config_gpios();
    ESP_LOGI(TAG, "buttons on GPIO %d/%d/%d (active-low)",
             TASK_BTN_UP_GPIO, TASK_BTN_DOWN_GPIO, TASK_BTN_SELECT_GPIO);
    xTaskCreate(poll_task, "buttons", 2048, NULL, 10, NULL);
}

bool buttons_get(btn_event_t *ev, TickType_t timeout)
{
    if (!s_queue || !ev) {
        return false;
    }
    return xQueueReceive(s_queue, ev, timeout) == pdTRUE;
}

bool buttons_select_held(void)
{
    config_gpios(); // works before buttons_init() for the boot-time reset check
    return gpio_get_level(TASK_BTN_SELECT_GPIO) == 0;
}
