#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t http_server_start(void);
esp_err_t http_server_stop(void);
