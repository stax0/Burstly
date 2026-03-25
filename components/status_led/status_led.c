#include "status_led.h"

#include "esp_check.h"
#include "led_strip.h"

#include <stdbool.h>
#include <stdint.h>

#define STATUS_LED_PIN 38
static const char* TAG = "status_led";

static status_color_t current_status_color = COLOR_OFF;
static led_strip_handle_t led_strip;
static volatile bool led_override_active = false;

static void color_to_rgb(status_color_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    switch (color) {
    case COLOR_RED:
        *r = 50;
        *g = 0;
        *b = 0;
        break;
    case COLOR_GREEN:
        *r = 0;
        *g = 50;
        *b = 0;
        break;
    case COLOR_BLUE:
        *r = 0;
        *g = 0;
        *b = 50;
        break;
    case COLOR_YELLOW:
        *r = 50;
        *g = 35;
        *b = 0;
        break;
    case COLOR_MAGENTA:
        *r = 50;
        *g = 0;
        *b = 40;
        break;
    case COLOR_CYAN:
        *r = 0;
        *g = 40;
        *b = 50;
        break;
    case COLOR_WHITE:
        *r = 30;
        *g = 30;
        *b = 30;
        break;
    case COLOR_OFF:
    default:
        *r = 0;
        *g = 0;
        *b = 0;
        break;
    }
}

esp_err_t status_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = STATUS_LED_PIN,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip), TAG,
                        "led_strip_new_rmt_device()");

    ESP_RETURN_ON_ERROR(led_strip_clear(led_strip), TAG, "led_strip_clear()");
    return ESP_OK;
}

void status_led_set(status_color_t color) {
    if (led_override_active) {
        current_status_color = color;
        return;
    }
    current_status_color = color;
    uint8_t r, g, b;
    color_to_rgb(color, &r, &g, &b);

    led_strip_set_pixel(led_strip, 0, g, r, b);
    led_strip_refresh(led_strip);
}

void status_led_override(status_color_t color) {
    led_override_active = true;

    uint8_t r, g, b;
    color_to_rgb(color, &r, &g, &b);

    led_strip_set_pixel(led_strip, 0, g, r, b);
    led_strip_refresh(led_strip);
}

void status_led_clear_override(void) {
    led_override_active = false;
    status_led_set(current_status_color);
}
