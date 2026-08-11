idf_component_register(SRCS "main.cpp" "ssh_server.cpp" "wifi_station.cpp" "littlefs_mount.cpp" INCLUDE_DIRS "." REQUIRES wolfssh wolfssl littlefs nvs_flash esp_event esp_netif esp_wifi esp_system)
