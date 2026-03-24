#include "driver/gpio.h"
#include "esp_log.h"
#include "ssr.h"
#include "ssr_handlers.h"
#include "status_led.h"

static const char* TAG = "generic_handler";

static void off_enter(void) {
    status_led_set(COLOR_RED);
    ESP_LOGI(TAG, "Off Mode Started");
}

static void off_run(ssr_control_msg_t* msg, bool is_new) {
    if (is_new) {
        gpio_set_level(CONFIG_SSR_GPIO, 0);
        ssr_lvl = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void off_exit(void) {
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ssr_lvl = 0;
    ESP_LOGI(TAG, "Off Mode Stopped");
}

const ssr_handler_t off_handler = {
    .enter = off_enter, .run = off_run, .exit = off_exit, .name = "OFF"};

static void burst_enter(void) {
    status_led_set(COLOR_MAGENTA);
    ESP_LOGI(TAG, "Burst Mode Started");
}

static void burst_run(ssr_control_msg_t* msg, bool is_new) {
    if (is_new) {
        gpio_set_level(CONFIG_SSR_GPIO, 1);
        ssr_lvl = 100;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void burst_exit(void) {
    ESP_LOGI(TAG, "Off Mode Stopped");
    gpio_set_level(CONFIG_SSR_GPIO, 0);
    ssr_lvl = 100;
}

const ssr_handler_t burst_handler = {
    .enter = burst_enter, .run = burst_run, .exit = burst_exit, .name = "BURST"};
