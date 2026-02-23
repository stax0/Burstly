#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    WIFI_STATE_INITIALIZING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_OK,
    WIFI_STATE_RECOVERING
} wifi_status_t;

esp_err_t wifi_init(void);
void wifi_coordinator(void* pv);

extern QueueHandle_t wifi_event_queue;
