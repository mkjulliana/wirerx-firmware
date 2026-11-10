<<<<<<< HEAD
// Minimal display test: cycles the whole screen through red, green, blue, white, black.
// No LVGL, no ADC, no WiFi. Build with:  pio run -e displaytest
=======
>>>>>>> 222cfaf (Add displaytest env using ILI9486 driver)
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

static const uint16_t COLS[]  = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK };
static const char    *NAMES[] = { "RED", "GREEN", "BLUE", "WHITE", "BLACK" };
static const int      N       = 5;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("[T] display test start");
    tft.begin();
    Serial.println("[T] tft.begin ok");
    tft.setRotation(1);
}

void loop() {
    static int i = 0;
    tft.fillScreen(COLS[i]);
    tft.setTextColor((i == 3) ? TFT_BLACK : TFT_WHITE, COLS[i]);
    tft.drawString(NAMES[i], 20, 20, 4);
    Serial.printf("[T] %s\n", NAMES[i]);
    i = (i + 1) % N;
    delay(1500);
<<<<<<< HEAD
}
=======
}
>>>>>>> 222cfaf (Add displaytest env using ILI9486 driver)
