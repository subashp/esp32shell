idf_component_register(SRCS "main.cpp" "ssh_server.cpp" INCLUDE_DIRS "." REQUIRES wolfssh wolfssl nvs_flash esp_event esp_wifi)
