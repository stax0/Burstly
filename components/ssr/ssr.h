#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum { MODE_INVALID, MODE_OFF, MODE_BURST, MODE_NETZERO, MODE_SOFTZERO } ssr_mode_t;

typedef struct {
    ssr_mode_t mode;
    float p_active;
} ssr_control_msg_t;

extern QueueHandle_t ssr_control_queue;
extern volatile float ssr_lvl;

void ssr_coordinator(void* pv);
esp_err_t ssr_init();
ssr_mode_t parse_mode(const char* s);