// Shows a 480x320 picture on the display (no LVGL, no ADC, no WiFi).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pic.h"
TFT_eSPI tft;
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[PIC] show picture");
  tft.begin();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, PIC_W, PIC_H, pic_data);
}
void loop() { delay(1000); }