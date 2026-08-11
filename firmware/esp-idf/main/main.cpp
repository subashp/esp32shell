#include <stdio.h>

extern "C" {
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssh/ssh.h>
}

extern "C" void app_main() {
    const int result = wolfSSH_Init();
    printf("esp32shell wolfSSH component probe: %s\n",
           result == WS_SUCCESS ? "initialized" : "initialization failed");
    wolfSSH_Cleanup();
}
