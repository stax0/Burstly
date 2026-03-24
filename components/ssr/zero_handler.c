#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "ssr.h"
#include "ssr_handlers.h"
#include "status_led.h"

#define ZH_SSR_STEPS 6

static const char* TAG = "zh";

static TickType_t zh_wake;
static float zh_ssr_lvl = 0;
static const uint8_t* zh_pattern = NULL;

static int c_pos = 0;
static int c_neg = 0;

static const uint8_t wave_duration = 10;
const int p_threshold_1_3 = CONFIG_P_BOILER_MAX_NOM * 0.33f;
const int p_threshold_2_3 = CONFIG_P_BOILER_MAX_NOM * 0.66f;

static const uint8_t P_0[ZH_SSR_STEPS] = {0, 0, 0, 0, 0, 0};
static const uint8_t P_33[ZH_SSR_STEPS] = {1, 0, 0, 1, 0, 0};
static const uint8_t P_66[ZH_SSR_STEPS] = {1, 1, 0, 1, 1, 0};
static const uint8_t P_100[ZH_SSR_STEPS] = {1, 1, 1, 1, 1, 1};

static void nzh_enter(void) {
    zh_pattern = P_0;
    zh_ssr_lvl = 0;

    status_led_set(COLOR_GREEN);
    ESP_LOGI(TAG, "NETZERO Mode Started");

    zh_wake = xTaskGetTickCount();
}

static void szh_enter(void) {
    zh_pattern = P_0;
    zh_ssr_lvl = 0;

    status_led_set(COLOR_BLUE);
    ESP_LOGI(TAG, "SOFTZERO Mode Started");

    zh_wake = xTaskGetTickCount();
}

static void zh_run(ssr_control_msg_t* msg, bool is_new) {
    if (is_new || zh_pattern == NULL) {
        float p_eff = msg->p_active + (zh_ssr_lvl / 100.0f) * CONFIG_P_BOILER_MAX_NOM;

        if (p_eff >= (float)CONFIG_P_BOILER_MAX_NOM) {
            zh_pattern = P_100;
            zh_ssr_lvl = 100.0f;
        } else if (p_eff >= p_threshold_2_3) {
            zh_pattern = P_66;
            zh_ssr_lvl = 66.6f;
        } else if (p_eff >= p_threshold_1_3) {
            zh_pattern = P_33;
            zh_ssr_lvl = 33.3f;
        } else if (msg->mode == MODE_SOFTZERO && p_eff >= (p_threshold_1_3 / 2)) {
            zh_pattern = P_33;
            zh_ssr_lvl = 33.3f;
        } else {
            zh_pattern = P_0;
            zh_ssr_lvl = 0.0f;
        }
        ESP_LOGI(TAG, "p_eff: %.2f, ssr level: %.2f", p_eff, zh_ssr_lvl);
        ssr_lvl = zh_ssr_lvl;
    }

    if (c_pos != c_neg) {
        gpio_set_level(CONFIG_SSR_GPIO, 0);
        ESP_LOGE(TAG, "DC Offset detected, blocking!");
        status_led_override(COLOR_WHITE);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    for (int i = 0; i < ZH_SSR_STEPS; i++) {
        if (xTaskDelayUntil(&zh_wake, pdMS_TO_TICKS(wave_duration)) == pdFALSE) {
            zh_wake = xTaskGetTickCount();
            status_led_override(COLOR_YELLOW);
            ESP_LOGW(TAG, "Timing miss!");
        }

        gpio_set_level(CONFIG_SSR_GPIO, zh_pattern[i]);

        if (zh_pattern[i] == 1) {
            if (i % 2 == 0) {
                c_pos++;
            } else {
                c_neg++;
            }
        }
    }
}

static void nzh_exit(void) {
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ESP_LOGI(TAG, "NetZero Mode Stopped");
}

static void szh_exit(void) {
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ESP_LOGI(TAG, "SoftZero Mode Stopped");
}

const ssr_handler_t netzero_handler = {
    .enter = nzh_enter, .run = zh_run, .exit = nzh_exit, .name = "NETZERO"};

const ssr_handler_t softzero_handler = {
    .enter = szh_enter, .run = zh_run, .exit = szh_exit, .name = "SOFTZERO"};