#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    MODE_UNKNOWN = 0,
    MODE_OFF,
    MODE_BURST,
    MODE_DOWNSHIFT,
    MODE_UPSHIFT,
    MODE_SIGMADELTA
} ssr_mode_t;

typedef struct {
    ssr_mode_t mode;
    float p_active;
    float p_boiler;
} ssr_control_msg_t;

extern QueueHandle_t ssr_control_queue;
extern volatile uint8_t ssr_lvl;

void ssr_coordinator(void* pv);
esp_err_t ssr_init();
ssr_mode_t parse_mode(const char* s);