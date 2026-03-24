#pragma once
#include "esp_err.h"

#include <stdbool.h>

typedef enum {
    COLOR_OFF = 0,
    COLOR_RED, // OFF
    COLOR_GREEN, // NZ
    COLOR_BLUE, // GZ
    COLOR_YELLOW, // Err: TIMING
    COLOR_MAGENTA, // BURST
    COLOR_CYAN, // Err: INIT
    COLOR_WHITE, // Err: DC Off
} status_color_t;

esp_err_t status_led_init(void);
void status_led_set(status_color_t color);
void status_led_override(status_color_t color);
void status_led_clear_override(void);
