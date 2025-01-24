idf_component_register(SRCS "wifi_comp.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event esp_system esp_http_server)
