#include "ssr.h"

#include "driver/gpio.h"
#include "esp_assert.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/projdefs.h"
#include "math.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include "status_led.h"

#include <stdint.h>

ESP_STATIC_ASSERT(CONFIG_INTEGRATION_TIME % 20 == 0, "INTEGRATION_TIME must be a multiple of 20");
#define SSR_STEPS CONFIG_INTEGRATION_TIME / CONFIG_FULL_WAVE

static const char* TAG = "ssr";

uint8_t ssr_debounce_count = 0;
QueueHandle_t ssr_control_queue = NULL;
volatile uint8_t ssr_lvl = 0;

static uint8_t calc_ssr_lvl(const ssr_control_msg_t* msg, uint8_t last_ssr_lvl) {
    float p_boiler_max = CONFIG_P_BOILER_MAX_NOM;
    float p_effective = msg->p_active + msg->p_boiler;
    float p_allowed = (p_effective > 0.0f) ? p_effective : 0.0f;

    if (last_ssr_lvl > 0 && msg->p_boiler > 1) {
        p_boiler_max = msg->p_boiler / (last_ssr_lvl / 100.0f);
    }

    int result = (int)roundf((p_allowed / p_boiler_max) * 100.0f);

    if (result > 100)
        result = 100;
    if (result < CONFIG_SSR_LVL_MIN)
        result = 0;

    bool switching_on = (result > 0) && (last_ssr_lvl == 0);
    bool switching_off = (result == 0) && (last_ssr_lvl > 0);

    bool debounced = false;

    if (switching_on || switching_off) {
        bool bypass = switching_on ? (p_effective > CONFIG_P_HYSTERESE_BYPASS)
                                   : (p_effective < -CONFIG_P_HYSTERESE_BYPASS);

        uint8_t limit = switching_on ? CONFIG_SSR_DEBOUNCE_ON : CONFIG_SSR_DEBOUNCE_OFF;

        if (!bypass && ssr_debounce_count < limit) {
            ssr_debounce_count++;
            result = switching_on ? 0 : CONFIG_SSR_LVL_MIN;
            debounced = true;
        }
    }

    if (!debounced) {
        ssr_debounce_count = 0;
    }

    return (uint8_t)result;
}

static void create_bresenham(uint8_t pattern[SSR_STEPS], uint8_t count) {
    int error = 0;

    for (int i = 0; i < SSR_STEPS; i++) {
        error += count;
        if (error >= SSR_STEPS) {
            pattern[i] = 1;
            error -= SSR_STEPS;
        } else {
            pattern[i] = 0;
        }
    }
}

static void create_sigma_delta(uint8_t pattern[SSR_STEPS], uint8_t requested_lvl, uint8_t n_on_up,
                               uint8_t n_on_down, float* error) {
    float up = (float)n_on_up / SSR_STEPS;
    float down = (float)n_on_down / SSR_STEPS;
    float lvl = (float)requested_lvl / 100;

    *error += lvl;

    float midpoint = (up + down) / 2.0f;
    if (*error >= midpoint) { // if (*error >= 0.5f)
        create_bresenham(pattern, n_on_up);
        *error -= up;
    } else {
        create_bresenham(pattern, n_on_down);
        *error -= down;
    }
}

void ssr_coordinator(void* pv) {
    ssr_control_msg_t msg = {.mode = MODE_OFF, .p_active = 0, .p_boiler = 0};
    ssr_mode_t last_mode = MODE_OFF;
    uint8_t pattern[SSR_STEPS];
    uint8_t n_on_up = 0;
    uint8_t n_on_down = 0;
    float error = 0;

    int64_t last_recv_us = esp_timer_get_time();
    const int64_t watchdog_timeout_us = (int64_t)CONFIG_SSR_WATCHDOG_S * 1000000LL;

    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        if (xQueueReceive(ssr_control_queue, &msg, 0) == pdTRUE) {
            bool recalc = (msg.mode == MODE_DOWNSHIFT || msg.mode == MODE_UPSHIFT ||
                           msg.mode == MODE_SIGMADELTA);

            if (recalc) {
                ssr_lvl = calc_ssr_lvl(&msg, ssr_lvl);
                ESP_LOGI(TAG, "SSR LEVEL %d", ssr_lvl);
                float ratio = (float)ssr_lvl / 100.0f;
                n_on_up = (uint8_t)ceilf((float)SSR_STEPS * ratio);
                n_on_down = (uint8_t)floorf((float)SSR_STEPS * ratio);

                if (msg.mode == MODE_SIGMADELTA && last_mode != MODE_SIGMADELTA) {
                    error = 0;
                }
            }

            last_mode = msg.mode;
            last_recv_us = esp_timer_get_time();

        } else {
            if (esp_timer_get_time() - last_recv_us > watchdog_timeout_us) {
                if (msg.mode != MODE_OFF) {
                    msg.mode = MODE_OFF;
                    msg.p_active = 0;
                    msg.p_boiler = 0;
                }
            }
        }

        switch (msg.mode) {
        case MODE_BURST: {
            float ratio = (float)CONFIG_SSR_LVL_BURST / 100.0f;
            ssr_lvl = CONFIG_SSR_LVL_BURST;
            uint8_t n_on_burst = (uint8_t)ceilf((float)SSR_STEPS * ratio);
            create_bresenham(pattern, n_on_burst);
            break;
        }
        case MODE_DOWNSHIFT:
            create_bresenham(pattern, n_on_down);
            break;
        case MODE_UPSHIFT:
            create_bresenham(pattern, n_on_up);
            break;
        case MODE_SIGMADELTA:
            create_sigma_delta(pattern, ssr_lvl, n_on_up, n_on_down, &error);
            break;
        case MODE_UNKNOWN:
        case MODE_OFF:
        default:
            ssr_lvl = 0;
            create_bresenham(pattern, 0);
            break;
        }

        for (uint8_t i = 0; i < SSR_STEPS; i++) {
            BaseType_t miss = xTaskDelayUntil(&wake, pdMS_TO_TICKS(CONFIG_FULL_WAVE));
            gpio_set_level(CONFIG_SSR_GPIO, pattern[i]);

            status_color_t color = pattern[i] == 1 ? COLOR_GREEN : COLOR_OFF;
            status_led_set(color);

            // for testing to see if calc > 20ms
            if (miss == pdFALSE) {
                status_led_override(COLOR_YELLOW);
                wake = xTaskGetTickCount(); // resync
            }
        }
    }
}

esp_err_t ssr_init() {
    status_led_set(COLOR_MAGENTA);
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
        return MODE_UNKNOWN;

    if (strcasecmp(s, "mode_off") == 0)
        return MODE_OFF;
    if (strcasecmp(s, "mode_burst") == 0)
        return MODE_BURST;
    if (strcasecmp(s, "mode_downshift") == 0)
        return MODE_DOWNSHIFT;
    if (strcasecmp(s, "mode_upshift") == 0)
        return MODE_UPSHIFT;
    if (strcasecmp(s, "mode_sigmadelta") == 0)
        return MODE_SIGMADELTA;

    return MODE_UNKNOWN;
}