idf_component_register(
    SRCS "status_led.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_timer led_strip
)