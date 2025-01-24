idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES wifi_component esp_driver_gpio nvs_flash)
