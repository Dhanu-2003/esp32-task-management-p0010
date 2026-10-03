#include "http_server.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "task_store.h"
#include "wifi_apsta.h"

static const char *TAG = "httpd";

#define BODY_MAX 2048

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_json(httpd_req_t *req, int status, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status == 200 ? "200 OK" :
                              status == 201 ? "201 Created" :
                              status == 400 ? "400 Bad Request" :
                              status == 404 ? "404 Not Found" :
                              status == 413 ? "413 Payload Too Large" :
                              "500 Internal Server Error");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_error(httpd_req_t *req, int status, const char *msg)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    return send_json(req, status, buf);
}

// Reads the whole request body (capped). Caller frees *out. Missing body -> *out=NULL.
static esp_err_t read_body(httpd_req_t *req, char **out)
{
    *out = NULL;
    if (req->content_len <= 0) {
        return ESP_OK;
    }
    if (req->content_len > BODY_MAX) {
        return ESP_ERR_NO_MEM;
    }
    char *buf = malloc((size_t)req->content_len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    size_t got = 0;
    while (got < (size_t)req->content_len) {
        int r = httpd_req_recv(req, buf + got, (size_t)req->content_len - got);
        if (r <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        got += (size_t)r;
    }
    buf[got] = '\0';
    *out = buf;
    return ESP_OK;
}

static esp_err_t on_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    size_t len = (size_t)(index_html_end - index_html_start);
    return httpd_resp_send(req, index_html_start, len);
}

static esp_err_t on_tasks_get(httpd_req_t *req)
{
    char *json = NULL;
    if (task_store_to_json(&json) != ESP_OK) {
        return send_error(req, 500, "store failed");
    }
    esp_err_t r = send_json(req, 200, json);
    free(json);
    return r;
}

static esp_err_t on_tasks_post(httpd_req_t *req)
{
    char *body = NULL;
    if (read_body(req, &body) != ESP_OK) {
        return send_error(req, 413, "body too large");
    }
    if (!body) {
        return send_error(req, 400, "missing body");
    }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_error(req, 400, "invalid JSON");
    }
    cJSON *jtitle = cJSON_GetObjectItemCaseSensitive(root, "title");
    cJSON *jprio = cJSON_GetObjectItemCaseSensitive(root, "priority");
    uint8_t prio = 1;
    esp_err_t r;
    if (!cJSON_IsString(jtitle) || !jtitle->valuestring || !jtitle->valuestring[0] ||
        strlen(jtitle->valuestring) > TASK_TITLE_MAX) {
        r = send_error(req, 400, "title required (1..120 chars)");
    } else {
        if (cJSON_IsNumber(jprio)) {
            int p = (int)jprio->valuedouble;
            if (p < 0 || p > 2) {
                cJSON_Delete(root);
                return send_error(req, 400, "priority must be 0..2");
            }
            prio = (uint8_t)p;
        }
        task_t created;
        esp_err_t err = task_store_add(jtitle->valuestring, prio, &created);
        if (err == ESP_ERR_NO_MEM) {
            r = send_error(req, 413, "task limit reached (100)");
        } else if (err != ESP_OK) {
            r = send_error(req, 400, "invalid task");
        } else {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddNumberToObject(o, "id", created.id);
            cJSON_AddStringToObject(o, "title", created.title);
            cJSON_AddBoolToObject(o, "done", false);
            cJSON_AddNumberToObject(o, "priority", created.priority);
            cJSON_AddNumberToObject(o, "created", (double)created.created);
            cJSON_AddNumberToObject(o, "updated", (double)created.updated);
            char *one = cJSON_PrintUnformatted(o);
            cJSON_Delete(o);
            r = one ? send_json(req, 201, one) : send_error(req, 500, "store failed");
            free(one);
        }
    }
    cJSON_Delete(root);
    return r;
}

// URI forms: /api/tasks/<id>  or  /api/tasks/<id>/toggle
static bool parse_id(const char *uri, uint32_t *id, bool *is_toggle)
{
    const char *p = strstr(uri, "/api/tasks/");
    if (!p) {
        return false;
    }
    p += strlen("/api/tasks/");
    char *end = NULL;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p || v == 0 || v > UINT32_MAX) {
        return false;
    }
    *id = (uint32_t)v;
    *is_toggle = (strcmp(end, "/toggle") == 0);
    return *end == '\0' || *is_toggle;
}

static esp_err_t on_task_item(httpd_req_t *req)
{
    uint32_t id = 0;
    bool is_toggle = false;
    if (!parse_id(req->uri, &id, &is_toggle)) {
        return send_error(req, 404, "unknown task route");
    }
    if (req->method == HTTP_POST) {
        if (!is_toggle) {
            return send_error(req, 404, "unknown task route");
        }
        task_t t;
        if (task_store_toggle(id, &t) == ESP_ERR_NOT_FOUND) {
            return send_error(req, 404, "task not found");
        }
        char *json = NULL;
        task_store_to_json(&json);
        esp_err_t r = json ? send_json(req, 200, json) : send_error(req, 500, "store failed");
        free(json);
        return r;
    }
    if (req->method == HTTP_DELETE) {
        if (is_toggle) {
            return send_error(req, 404, "unknown task route");
        }
        if (task_store_delete(id) == ESP_ERR_NOT_FOUND) {
            return send_error(req, 404, "task not found");
        }
        return send_json(req, 200, "{\"ok\":true}");
    }
    if (req->method != HTTP_PUT) {
        return send_error(req, 404, "unknown task route");
    }
    if (is_toggle) {
        return send_error(req, 404, "unknown task route");
    }
    char *body = NULL;
    if (read_body(req, &body) != ESP_OK) {
        return send_error(req, 413, "body too large");
    }
    if (!body) {
        return send_error(req, 400, "missing body");
    }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_error(req, 400, "invalid JSON");
    }
    const char *title = NULL;
    bool done_val = false;
    const bool *done_ptr = NULL;
    uint8_t prio_val = 1;
    const uint8_t *prio_ptr = NULL;
    cJSON *jtitle = cJSON_GetObjectItemCaseSensitive(root, "title");
    cJSON *jdone = cJSON_GetObjectItemCaseSensitive(root, "done");
    cJSON *jprio = cJSON_GetObjectItemCaseSensitive(root, "priority");
    if (jtitle) {
        if (!cJSON_IsString(jtitle) || !jtitle->valuestring || !jtitle->valuestring[0] ||
            strlen(jtitle->valuestring) > TASK_TITLE_MAX) {
            cJSON_Delete(root);
            return send_error(req, 400, "title must be 1..120 chars");
        }
        title = jtitle->valuestring;
    }
    if (jdone) {
        if (!cJSON_IsBool(jdone)) {
            cJSON_Delete(root);
            return send_error(req, 400, "done must be boolean");
        }
        done_val = cJSON_IsTrue(jdone);
        done_ptr = &done_val;
    }
    if (jprio) {
        if (!cJSON_IsNumber(jprio) || (int)jprio->valuedouble < 0 ||
            (int)jprio->valuedouble > 2) {
            cJSON_Delete(root);
            return send_error(req, 400, "priority must be 0..2");
        }
        prio_val = (uint8_t)(int)jprio->valuedouble;
        prio_ptr = &prio_val;
    }
    esp_err_t err = task_store_update(id, title, done_ptr, prio_ptr);
    cJSON_Delete(root);
    if (err == ESP_ERR_NOT_FOUND) {
        return send_error(req, 404, "task not found");
    }
    if (err != ESP_OK) {
        return send_error(req, 400, "invalid task");
    }
    char *json = NULL;
    task_store_to_json(&json);
    esp_err_t r = json ? send_json(req, 200, json) : send_error(req, 500, "store failed");
    free(json);
    return r;
}

static esp_err_t on_wifi_get(httpd_req_t *req)
{
    char *json = NULL;
    if (wifi_apsta_status_json(&json) != ESP_OK) {
        return send_error(req, 500, "wifi failed");
    }
    esp_err_t r = send_json(req, 200, json);
    free(json);
    return r;
}

static esp_err_t on_wifi_post(httpd_req_t *req)
{
    char *body = NULL;
    if (read_body(req, &body) != ESP_OK) {
        return send_error(req, 413, "body too large");
    }
    if (!body) {
        return send_error(req, 400, "missing body");
    }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_error(req, 400, "invalid JSON");
    }
    cJSON *jssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *jpass = cJSON_GetObjectItemCaseSensitive(root, "pass");
    esp_err_t r;
    if (!cJSON_IsString(jssid) || !jssid->valuestring || !jssid->valuestring[0] ||
        strlen(jssid->valuestring) > 32) {
        r = send_error(req, 400, "ssid required (1..32 chars)");
    } else if (jpass && (!cJSON_IsString(jpass) || !jpass->valuestring ||
                        strlen(jpass->valuestring) > 64)) {
        r = send_error(req, 400, "pass must be 0..64 chars");
    } else {
        const char *pass = (jpass && jpass->valuestring) ? jpass->valuestring : "";
        r = (wifi_apsta_set_sta(jssid->valuestring, pass) == ESP_OK)
                ? send_json(req, 200, "{\"ok\":true}")
                : send_error(req, 500, "could not save wifi");
    }
    cJSON_Delete(root);
    return r;
}

static esp_err_t on_reset_post(httpd_req_t *req)
{
    (void)req;
    task_store_clear();
    return send_json(req, 200, "{\"ok\":true}");
}

static const char *method_name(httpd_method_t m)
{
    switch (m) {
    case HTTP_GET:
        return "GET";
    case HTTP_POST:
        return "POST";
    case HTTP_PUT:
        return "PUT";
    case HTTP_DELETE:
        return "DELETE";
    default:
        return "?";
    }
}

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;
    // HTTPD_DEFAULT_CONFIG allows only 8 handlers; we register 9.
    config.max_uri_handlers = 16;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = on_index},
        {.uri = "/api/tasks", .method = HTTP_GET, .handler = on_tasks_get},
        {.uri = "/api/tasks", .method = HTTP_POST, .handler = on_tasks_post},
        {.uri = "/api/tasks/*", .method = HTTP_PUT, .handler = on_task_item},
        {.uri = "/api/tasks/*", .method = HTTP_DELETE, .handler = on_task_item},
        {.uri = "/api/tasks/*", .method = HTTP_POST, .handler = on_task_item},
        {.uri = "/api/wifi", .method = HTTP_GET, .handler = on_wifi_get},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = on_wifi_post},
        {.uri = "/api/reset", .method = HTTP_POST, .handler = on_reset_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) {
            // Do not abort the device over one missing route: log it and carry on
            // with the rest (e.g. ESP_ERR_HTTPD_HANDLERS_FULL if the configured
            // handler limit is too low for the route table).
            ESP_LOGE(TAG, "route %s %s not registered: %s", routes[i].uri,
                     method_name(routes[i].method), esp_err_to_name(err));
        }
    }
    ESP_LOGI(TAG, "web UI on http://192.168.4.1/");
    return ESP_OK;
}
