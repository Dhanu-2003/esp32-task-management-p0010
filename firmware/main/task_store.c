#include "task_store.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "time_util.h"

static const char *TAG = "task_store";
static const char *NVS_NS = "tasks";
static const char *NVS_KEY = "db_v1";
static const char *NVS_KEY_BAK = "db_bak";

// Writes are coalesced to limit NVS wear: mark dirty, flush 500 ms later.
#define SAVE_DELAY_US 500000

static SemaphoreHandle_t s_lock;
static task_t s_tasks[TASK_STORE_MAX];
static size_t s_count = 0;
static uint32_t s_seq = 0;
static esp_timer_handle_t s_save_timer;
static bool s_save_pending = false;

static void save_now_locked(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "seq", s_seq);
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < s_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s_tasks[i].id);
        cJSON_AddStringToObject(o, "title", s_tasks[i].title);
        cJSON_AddBoolToObject(o, "done", s_tasks[i].done);
        cJSON_AddNumberToObject(o, "priority", s_tasks[i].priority);
        cJSON_AddNumberToObject(o, "created", (double)s_tasks[i].created);
        cJSON_AddNumberToObject(o, "updated", (double)s_tasks[i].updated);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "tasks", arr);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        ESP_LOGE(TAG, "OOM serialising tasks");
        nvs_close(h);
        return;
    }
    esp_err_t err = nvs_set_blob(h, NVS_KEY, json, strlen(json) + 1);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %s (tasks=%u, bytes=%u)",
                 esp_err_to_name(err), (unsigned)s_count, (unsigned)strlen(json));
    }
    free(json);
    nvs_close(h);
}

static void save_timer_cb(void *arg)
{
    (void)arg;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        s_save_pending = false;
        save_now_locked();
        xSemaphoreGive(s_lock);
    }
}

static void mark_dirty_locked(void)
{
    if (!s_save_pending) {
        s_save_pending = true;
        esp_timer_start_once(s_save_timer, SAVE_DELAY_US);
    }
}

static bool parse_db(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    cJSON *seq = cJSON_GetObjectItemCaseSensitive(root, "seq");
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "tasks");
    if (!cJSON_IsNumber(seq) || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return false;
    }
    size_t n = 0;
    uint32_t max_id = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr)
    {
        if (n >= TASK_STORE_MAX) {
            break;
        }
        cJSON *jid = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *jtitle = cJSON_GetObjectItemCaseSensitive(item, "title");
        cJSON *jdone = cJSON_GetObjectItemCaseSensitive(item, "done");
        cJSON *jprio = cJSON_GetObjectItemCaseSensitive(item, "priority");
        cJSON *jcreated = cJSON_GetObjectItemCaseSensitive(item, "created");
        cJSON *jupdated = cJSON_GetObjectItemCaseSensitive(item, "updated");
        if (!cJSON_IsNumber(jid) || !cJSON_IsString(jtitle) || jtitle->valuestring == NULL) {
            continue;
        }
        task_t *t = &s_tasks[n];
        t->id = (uint32_t)jid->valuedouble;
        strncpy(t->title, jtitle->valuestring, TASK_TITLE_MAX);
        t->title[TASK_TITLE_MAX] = '\0';
        if (t->title[0] == '\0') {
            continue;
        }
        t->done = cJSON_IsTrue(jdone);
        int prio = cJSON_IsNumber(jprio) ? (int)jprio->valuedouble : 1;
        t->priority = (prio < 0 || prio > 2) ? 1 : (uint8_t)prio;
        t->created = cJSON_IsNumber(jcreated) ? (int64_t)jcreated->valuedouble : 0;
        t->updated = cJSON_IsNumber(jupdated) ? (int64_t)jupdated->valuedouble : t->created;
        if (t->id > max_id) {
            max_id = t->id;
        }
        n++;
    }
    s_count = n;
    s_seq = (uint32_t)seq->valuedouble;
    if (s_seq < max_id) {
        s_seq = max_id;
    }
    cJSON_Delete(root);
    return true;
}

static int find_locked(uint32_t id)
{
    for (size_t i = 0; i < s_count; i++) {
        if (s_tasks[i].id == id) {
            return (int)i;
        }
    }
    return -1;
}

esp_err_t task_store_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    const esp_timer_create_args_t args = {
        .callback = save_timer_cb,
        .name = "task_save",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_save_timer));

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no task db yet, starting empty");
        return ESP_OK;
    }
    size_t len = 0;
    bool loaded = false;
    if (nvs_get_blob(h, NVS_KEY, NULL, &len) == ESP_OK && len > 1 && len < 128 * 1024) {
        char *buf = malloc(len);
        if (buf && nvs_get_blob(h, NVS_KEY, buf, &len) == ESP_OK) {
            loaded = parse_db(buf);
        }
        free(buf);
    }
    if (!loaded) {
        // Try the backup copy before giving up.
        len = 0;
        if (nvs_get_blob(h, NVS_KEY_BAK, NULL, &len) == ESP_OK && len > 1 && len < 128 * 1024) {
            char *buf = malloc(len);
            if (buf && nvs_get_blob(h, NVS_KEY_BAK, buf, &len) == ESP_OK) {
                loaded = parse_db(buf);
                ESP_LOGW(TAG, "primary db unreadable, restored backup");
            }
            free(buf);
        }
    }
    nvs_close(h);
    if (!loaded) {
        ESP_LOGE(TAG, "task db corrupt or missing, starting empty");
        s_count = 0;
        s_seq = 0;
    } else {
        ESP_LOGI(TAG, "loaded %u tasks (seq=%lu)", (unsigned)s_count, (unsigned long)s_seq);
    }
    return ESP_OK;
}

int task_store_count(void)
{
    return (int)s_count;
}

esp_err_t task_store_list(task_t **out, size_t *n)
{
    task_t *copy = malloc(sizeof(task_t) * (s_count ? s_count : 1));
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        free(copy);
        return ESP_FAIL;
    }
    memcpy(copy, s_tasks, sizeof(task_t) * s_count);
    *out = copy;
    *n = s_count;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_add(const char *title, uint8_t priority, task_t *created_out)
{
    if (!title || !title[0] || strlen(title) > TASK_TITLE_MAX || priority > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_count >= TASK_STORE_MAX) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NO_MEM; // at cap: 100 tasks
    }
    task_t *t = &s_tasks[s_count++];
    t->id = ++s_seq;
    strncpy(t->title, title, TASK_TITLE_MAX);
    t->title[TASK_TITLE_MAX] = '\0';
    t->done = false;
    t->priority = priority;
    t->created = time_util_now();
    t->updated = t->created;
    if (created_out) {
        *created_out = *t;
    }
    mark_dirty_locked();
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_update(uint32_t id, const char *title, const bool *done,
                            const uint8_t *priority)
{
    if (title && (!title[0] || strlen(title) > TASK_TITLE_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (priority && *priority > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    int i = find_locked(id);
    if (i < 0) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    if (title) {
        strncpy(s_tasks[i].title, title, TASK_TITLE_MAX);
        s_tasks[i].title[TASK_TITLE_MAX] = '\0';
    }
    if (done) {
        s_tasks[i].done = *done;
    }
    if (priority) {
        s_tasks[i].priority = *priority;
    }
    s_tasks[i].updated = time_util_now();
    mark_dirty_locked();
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_toggle(uint32_t id, task_t *out)
{
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    int i = find_locked(id);
    if (i < 0) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    s_tasks[i].done = !s_tasks[i].done;
    s_tasks[i].updated = time_util_now();
    if (out) {
        *out = s_tasks[i];
    }
    mark_dirty_locked();
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_delete(uint32_t id)
{
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    int i = find_locked(id);
    if (i < 0) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    memmove(&s_tasks[i], &s_tasks[i + 1], sizeof(task_t) * (s_count - (size_t)i - 1));
    s_count--;
    mark_dirty_locked();
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_to_json(char **out_json)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    for (size_t i = 0; i < s_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s_tasks[i].id);
        cJSON_AddStringToObject(o, "title", s_tasks[i].title);
        cJSON_AddBoolToObject(o, "done", s_tasks[i].done);
        cJSON_AddNumberToObject(o, "priority", s_tasks[i].priority);
        cJSON_AddNumberToObject(o, "created", (double)s_tasks[i].created);
        cJSON_AddNumberToObject(o, "updated", (double)s_tasks[i].updated);
        cJSON_AddItemToArray(arr, o);
    }
    xSemaphoreGive(s_lock);
    cJSON_AddItemToObject(root, "tasks", arr);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    *out_json = json;
    return ESP_OK;
}

esp_err_t task_store_flush(void)
{
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_save_pending) {
        esp_timer_stop(s_save_timer);
        s_save_pending = false;
        save_now_locked();
    }
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t task_store_clear(void)
{
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    s_count = 0;
    if (s_save_pending) {
        esp_timer_stop(s_save_timer);
        s_save_pending = false;
    }
    save_now_locked();
    xSemaphoreGive(s_lock);
    ESP_LOGW(TAG, "all tasks cleared");
    return ESP_OK;
}
