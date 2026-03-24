#include "http.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "ssr.h"

static const char* TAG = "http";

static httpd_handle_t server = NULL;
static bool running = false;

static void reboot_task(void* pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

static esp_err_t reboot_get_handler(httpd_req_t* req) {
    httpd_resp_sendstr(req, "Rebooting...");
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t set_get_handler(httpd_req_t* req) {
    char query[128];
    char mode_str[16] = {0};
    char p_active_str[16] = {0};

    if (httpd_req_get_url_query_len(req) >= sizeof(query) ||
        httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Bad request: GENERIC");
        return ESP_OK;
    }

    bool mode_ok = httpd_query_key_value(query, "mode", mode_str, sizeof(mode_str)) == ESP_OK;
    bool p_active_ok =
        httpd_query_key_value(query, "p_active", p_active_str, sizeof(p_active_str)) == ESP_OK;

    if (!mode_ok) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Bad request: !MODE_OK");
        return ESP_OK;
    }

    ssr_mode_t mode = parse_mode(mode_str);

    if (mode == MODE_INVALID) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Bad request: MODE_INVALID");
        return ESP_OK;
    }

    if (mode != MODE_OFF && !p_active_ok) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Bad request: !P_ACTIVE_OK");
        return ESP_OK;
    }

    float p_active = p_active_ok ? strtof(p_active_str, NULL) : 0.0f;

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");

    float ssr_lvl_curr = ssr_lvl;
    ssr_control_msg_t control_msg = {.p_active = p_active, .mode = mode};
    xQueueOverwrite(ssr_control_queue, &control_msg);

    uint16_t retries = 25; // 25 * 20 = 500ms
    while (ssr_lvl_curr == ssr_lvl && retries--) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    float ssr_lvl_new = ssr_lvl;

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"mode\":\"%s\",\"p_active\":%.2f,\"ssr_lvl\":%.2f}", mode_str,
             p_active, ssr_lvl_new);

    httpd_resp_sendstr(req, resp);

    return ESP_OK;
}

static const httpd_uri_t uri_reboot = {
    .uri = "/reboot", .method = HTTP_GET, .handler = reboot_get_handler, .user_ctx = NULL};

static const httpd_uri_t uri_set = {
    .uri = "/set", .method = HTTP_GET, .handler = set_get_handler, .user_ctx = NULL};

static httpd_handle_t start_server_internal(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t new_server = NULL;

    ESP_LOGI(TAG, "Starting HTTP server...");

    if (httpd_start(&new_server, &config) == ESP_OK) {
        httpd_register_uri_handler(new_server, &uri_reboot);
        httpd_register_uri_handler(new_server, &uri_set);

        return new_server;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

esp_err_t http_server_start(void) {
    if (running) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    server = start_server_internal();
    if (server == NULL) {
        return ESP_FAIL;
    }

    running = true;
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

esp_err_t http_server_stop(void) {
    if (!running) {
        ESP_LOGW(TAG, "HTTP server already stopped");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping HTTP server...");

    esp_err_t err = httpd_stop(server);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop HTTP server");
        return err;
    }

    server = NULL;
    running = false;

    ESP_LOGI(TAG, "HTTP server stopped");
    return ESP_OK;
}