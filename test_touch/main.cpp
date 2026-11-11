// Touch test + calibration. Prints raw touch values and draws a dot where you press.
#include <Arduino.h>
#include <TFT_eSPI.h>
TFT_eSPI tft;
uint16_t calData[5];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[TT] touch test start");
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Press and hold the screen", 20, 140, 4);
  uint32_t t0 = millis();
  while (millis() - t0 < 6000) {
    uint16_t rx = 0, ry = 0;
    uint16_t z = tft.getTouchRawZ();
    tft.getTouchRaw(&rx, &ry);
    Serial.printf("[TT] raw x=%u y=%u z=%u\n", rx, ry, z);
    delay(200);
  }
  tft.fillScreen(TFT_BLACK);
  tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
  Serial.printf("[TT] CALDATA {%u, %u, %u, %u, %u}\n",
                calData[0], calData[1], calData[2], calData[3], calData[4]);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Calibrated. Touch to draw.", 20, 10, 4);
}

void loop() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y, 200)) {
    tft.fillCircle(x, y, 3, TFT_GREEN);
    Serial.printf("[TT] x=%u y=%u\n", x, y);
  }
}