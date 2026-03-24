#include "ssr.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
#include "ssr_handlers.h"

#include <stdint.h>

static const char* TAG = "ssr";

QueueHandle_t ssr_control_queue = NULL;
volatile float ssr_lvl = 0;

static const ssr_handler_t* handlers[] = {
    [MODE_OFF] = &off_handler,
    [MODE_BURST] = &burst_handler,
    [MODE_NETZERO] = &netzero_handler,
    [MODE_SOFTZERO] = &softzero_handler,
};

void ssr_coordinator(void* pv) {
    ssr_control_msg_t msg = {.mode = MODE_OFF, .p_active = 0};
    ssr_mode_t current_mode = MODE_INVALID;
    uint64_t last_recv_us = esp_timer_get_time();
    const uint64_t timeout_us = (uint64_t)CONFIG_SSR_WATCHDOG_S * 1000000ULL;

    for (;;) {
        bool recv = (xQueueReceive(ssr_control_queue, &msg, 0) == pdTRUE);

        if (recv) {
            last_recv_us = esp_timer_get_time();
        } else {
            if (esp_timer_get_time() - last_recv_us > timeout_us) {
                if (msg.mode != MODE_OFF) {
                    msg.mode = MODE_OFF;
                    recv = true;
                    ESP_LOGW(TAG, "Watchdog timeout! Safety OFF.");
                }
            }
        }

        if (msg.mode != current_mode) {
            if (handlers[current_mode] && handlers[current_mode]->exit) {
                handlers[current_mode]->exit();
            }

            current_mode = msg.mode;

            if (handlers[current_mode]->enter) {
                handlers[current_mode]->enter();
            }
        }

        if (handlers[current_mode]->run) {
            handlers[current_mode]->run(&msg, recv);
        }
    }
}

esp_err_t ssr_init() {
    ssr_control_queue = xQueueCreate(1, sizeof(ssr_control_msg_t));
    if (!ssr_control_queue) {
        ESP_LOGE(TAG, "Failed to create SSR Control Queue");
        return ESP_FAIL;
    }

    gpio_config_t io_conf = {.pin_bit_mask = 1ULL << CONFIG_SSR_GPIO,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config()");
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_SSR_GPIO, 0), TAG, "gpio_set_level()");

    ssr_lvl = 0;

    return ESP_OK;
}

ssr_mode_t parse_mode(const char* s) {
    if (!s)
        return MODE_INVALID;

    if (strcasecmp(s, "mode_off") == 0)
        return MODE_OFF;
    if (strcasecmp(s, "mode_burst") == 0)
        return MODE_BURST;
    if (strcasecmp(s, "mode_netzero") == 0)
        return MODE_NETZERO;
    if (strcasecmp(s, "mode_softzero") == 0)
        return MODE_SOFTZERO;

    return MODE_INVALID;
}