#include "freertos/idf_additions.h"
#include "ssr.h"
#include "status_led.h"
#include "wifi.h"

void app_main(void) {
    ESP_ERROR_CHECK(status_led_init());
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(ssr_init());

    status_led_clear_override();

    // WiFi and API on core 0, the real-time SSR loop gets core 1 for itself
    BaseType_t ok;
    ok = xTaskCreatePinnedToCore(wifi_coordinator, "wifi_coord", 4096, NULL, 5, NULL, 0);
    configASSERT(ok == pdPASS);
    ok = xTaskCreatePinnedToCore(ssr_coordinator, "ssr_coord", 4096, NULL, 20, NULL, 1);
    configASSERT(ok == pdPASS);
}