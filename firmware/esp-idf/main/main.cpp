#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "ssh_server.h"
#include "wifi_station.h"

extern "C" {
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssh/ssh.h>
}

extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                [](void*, esp_event_base_t,
                                                   int32_t, void*) {
                                                  esp32shell_ssh_start_after_wifi();
                                                }, nullptr));
    printf("esp32shell wolfSSH server ready; waiting for Wi-Fi IP\n");
    if (!esp32shell_wifi_start()) {
        printf("esp32shell Wi-Fi not configured; SSH remains disabled\n");
    }
}
