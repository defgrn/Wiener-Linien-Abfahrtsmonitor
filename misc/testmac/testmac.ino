#include "WiFi.h"

// Programm zum MAC Adresse rausfinden
// auf ESP32 getter hochladen

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.println();
  Serial.print("MAC Adresse: ");
  Serial.println(WiFi.macAddress()); // die dann im sender einbauen
}

void loop() {
  
}