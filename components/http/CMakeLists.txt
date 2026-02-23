idf_component_register(
    SRCS "http.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_server ssr status_led
)