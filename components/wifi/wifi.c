#include "wifi.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "http.h"
#include "nvs_flash.h"
#include "status_led.h"

static const char* TAG = "wifi";

typedef struct {
    esp_event_base_t base;
    int32_t id;
} wifi_evt_t;

QueueHandle_t wifi_event_queue = NULL;

wifi_status_t wifi_state = WIFI_STATE_OK;

void wifi_coordinator(void* pv) {
    wifi_evt_t evt;
    wifi_status_t current_state = WIFI_STATE_INITIALIZING;
    int retry_count = 0;

    for (;;) {
        if (xQueueReceive(wifi_event_queue, &evt, pdMS_TO_TICKS(1000))) {
            if (evt.base == WIFI_EVENT) {
                switch (evt.id) {
                case WIFI_EVENT_STA_START:
                case WIFI_EVENT_STA_DISCONNECTED:
                    ESP_LOGW(TAG, "WiFi lost/starting. Reconnecting...");
                    current_state = WIFI_STATE_CONNECTING;
                    http_server_stop();
                    esp_wifi_connect();
                    break;

                case WIFI_EVENT_STA_CONNECTED:
                    ESP_LOGI(TAG, "Layer 2 connected");
                    break;
                }
            } else if (evt.base == IP_EVENT && evt.id == IP_EVENT_STA_GOT_IP) {
                ESP_LOGI(TAG, "Got IP!");
                current_state = WIFI_STATE_OK;
                retry_count = 0;
                http_server_start();
                status_led_clear_override();
            }
        }

        if (current_state == WIFI_STATE_CONNECTING) {
            retry_count++;
            status_led_override(COLOR_ORANGE);

            if (retry_count > 10) {
                ESP_LOGE(TAG, "Recovery: Total reset of WiFi stack");
                current_state = WIFI_STATE_RECOVERING;
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_wifi_start();
                retry_count = 0;
            }
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    wifi_evt_t evt = {.base = base, .id = id};
    xQueueSendFromISR(wifi_event_queue, &evt, NULL);
}

esp_err_t wifi_init(void) {
    status_led_set(COLOR_BLUE);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase()");
        ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "nvs_flash_init()");
    }

    wifi_event_queue = xQueueCreate(10, sizeof(wifi_evt_t));
    if (!wifi_event_queue) {
        ESP_LOGE(TAG, "Failed to create WiFi event queue");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init()");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default()");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init()");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG,
        "wifi_event_handler_register()");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG,
        "ip_event_handler_register()");

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,
                .scan_method = WIFI_FAST_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold.rssi = -78,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "esp_wifi_set_ps()");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode()");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
                        "esp_wifi_set_config()");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start()");

    return ESP_OK;
}
