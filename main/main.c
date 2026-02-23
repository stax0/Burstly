#include "freertos/idf_additions.h"
#include "ssr.h"
#include "status_led.h"
#include "wifi.h"

void app_main(void)

{
    ESP_ERROR_CHECK(status_led_init());
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(ssr_init());

    status_led_clear_override();
    status_led_set(COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(2000));

    xTaskCreatePinnedToCore(wifi_coordinator, "wifi_coord", 4096, NULL, 5, NULL, 0);

    xTaskCreatePinnedToCore(ssr_coordinator, "ssr_coord", 4096, NULL, 20, NULL, 0);
}