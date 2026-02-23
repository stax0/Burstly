idf_component_register(
    SRCS "ssr.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES status_led driver esp_timer
)