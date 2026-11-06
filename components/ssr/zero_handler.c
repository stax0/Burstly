#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
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
static int c_up = 0;
static int current_idx = 0;

static const uint8_t wave_duration = 10;
const int p_threshold_1_3 = CONFIG_P_BOILER_MAX_NOM * 0.33f;
const int p_threshold_2_3 = CONFIG_P_BOILER_MAX_NOM * 0.66f;

static const float LEVELS[] = {0.0f, 33.3f, 66.6f, 100.0f};

static const uint8_t P_0[ZH_SSR_STEPS] = {0, 0, 0, 0, 0, 0};
static const uint8_t P_33[ZH_SSR_STEPS] = {1, 0, 0, 1, 0, 0};
static const uint8_t P_66[ZH_SSR_STEPS] = {1, 1, 0, 1, 1, 0};
static const uint8_t P_100[ZH_SSR_STEPS] = {1, 1, 1, 1, 1, 1};
static const uint8_t* PATTERNS[] = {P_0, P_33, P_66, P_100};

static void nzh_enter(void) {
    zh_pattern = P_0;
    zh_ssr_lvl = 0;
    current_idx = 0;

    status_led_set(COLOR_GREEN);
    ESP_LOGI(TAG, "NETZERO Mode Started");

    zh_wake = xTaskGetTickCount();
}

static void nzch_enter(void) {
    zh_pattern = P_0;
    zh_ssr_lvl = 0;
    current_idx = 0;

    status_led_set(COLOR_GREEN);
    ESP_LOGI(TAG, "NETZERO (cap) Mode Started");

    zh_wake = xTaskGetTickCount();
}

static void szh_enter(void) {
    zh_pattern = P_0;
    zh_ssr_lvl = 0;
    current_idx = 0;

    status_led_set(COLOR_BLUE);
    ESP_LOGI(TAG, "SOFTZERO Mode Started");

    zh_wake = xTaskGetTickCount();
}

static void zh_run(ssr_control_msg_t* msg, bool is_new) {
    if (is_new || zh_pattern == NULL) {
        float p_eff = msg->p_active + (zh_ssr_lvl / 100.0f) * CONFIG_P_BOILER_MAX_NOM;

        int target_idx = 0;
        if (p_eff >= (float)CONFIG_P_BOILER_MAX_NOM && msg->mode != MODE_NETZERO_CAP) {
            target_idx = 3;
        } else if (p_eff >= p_threshold_2_3) {
            target_idx = 2;
        } else if (p_eff >= p_threshold_1_3) {
            target_idx = 1;
        } else if (msg->mode == MODE_SOFTZERO && p_eff >= (p_threshold_1_3 / 6)) {
            target_idx = 1;
        } else {
            target_idx = 0;
        }

        if (target_idx > current_idx) {
            c_up++;
            if (c_up >= CONFIG_SSR_UPSHIFT_DEBOUNCE) {
                current_idx++;
                c_up = 0;
            }
        } else {
            current_idx = target_idx;
            c_up = 0;
        }

        zh_ssr_lvl = LEVELS[current_idx];
        zh_pattern = PATTERNS[current_idx];

        ESP_LOGI(TAG, "p_eff: %.2f, ssr_lvl: %.2f, c_up: %d", p_eff, zh_ssr_lvl, c_up);
        ssr_lvl = zh_ssr_lvl;
    }

    if (c_pos != c_neg) {
        gpio_set_level(CONFIG_SSR_GPIO, 0);
        ESP_LOGE(TAG, "DC Offset detected, blocking!");
        status_led_override(COLOR_WHITE);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    if (c_pos > 30000 || c_neg > 30000) {
        c_pos = 0;
        c_neg = 0;
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

static void nzch_exit(void) {
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ESP_LOGI(TAG, "NetZero (cap) Mode Stopped");
}

static void szh_exit(void) {
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ESP_LOGI(TAG, "SoftZero Mode Stopped");
}

const ssr_handler_t netzero_handler = {
    .enter = nzh_enter, .run = zh_run, .exit = nzh_exit, .name = "NETZERO"};

const ssr_handler_t netzero_cap_handler = {
    .enter = nzch_enter, .run = zh_run, .exit = nzch_exit, .name = "NETZERO_CAP"};

const ssr_handler_t softzero_handler = {
    .enter = szh_enter, .run = zh_run, .exit = szh_exit, .name = "SOFTZERO"};