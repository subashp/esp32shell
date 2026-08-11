// esp32shell Phase 0/1 bring-up sketch.
// Board: ESP32S3 Dev Module, 32MB Flash, OPI PSRAM.

String readLine;

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help         Show this help");
  Serial.println("  version      Show firmware version");
  Serial.println("  device-info  Show chip information");
  Serial.println("  uptime       Show uptime in milliseconds");
  Serial.println("  heap         Show free heap");
}

void handleCommand(const String& command) {
  if (command == "help") {
    printHelp();
  } else if (command == "version") {
    Serial.println("esp32shell 0.1.0-serial");
  } else if (command == "device-info") {
    Serial.printf("chip=%s cores=%d cpu_mhz=%u\n", ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz());
    Serial.printf("flash=%u bytes psram=%u bytes\n", ESP.getFlashChipSize(), ESP.getPsramSize());
  } else if (command == "uptime") {
    Serial.printf("%lu ms\n", millis());
  } else if (command == "heap") {
    Serial.printf("free_heap=%u min_free_heap=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  } else if (command.length() > 0) {
    Serial.println("error: unknown command; try 'help'");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("esp32shell serial bring-up");
  Serial.println("Type 'help' for commands.");
  Serial.print("esp32shell> ");
}

void loop() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (readLine.length() > 0) {
        handleCommand(readLine);
        readLine = "";
      }
      Serial.print("esp32shell> ");
    } else if (readLine.length() < 96) {
      readLine += c;
    }
  }
}
