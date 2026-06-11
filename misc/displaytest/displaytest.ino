#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 
#include <SPI.h>

// Programm um 1,5 zoll display zu testen

#define TFT_CS 15
#define TFT_RST 4 
#define TFT_DC 2
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  tft.init(240, 240);           
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.println("TEST!");
  tft.setCursor(10, 90);
  tft.setTextColor(ST77XX_RED);
  tft.println("Funktioniert");
}

void loop() {
  
}