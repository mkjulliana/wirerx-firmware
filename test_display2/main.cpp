// Display test v2 (ILI9486 driver, final pins). Prints a [D2] banner so you can tell it is running.
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

static const uint16_t COLS[]  = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK };
static const char    *NAMES[] = { "RED", "GREEN", "BLUE", "WHITE", "BLACK" };
static const int      N       = 5;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("[D2] ===== DISPLAY TEST v2 =====");
    Serial.println("[D2] ILI9486 driver | CS10 RST7 DC14 MOSI11 SCK12");

    tft.begin();
    Serial.println("[D2] tft.begin ok");
    tft.init();
    Serial.println("[D2] tft.init ok");
    tft.setRotation(1);
    tft.fillScreen(TFT_BLUE);
    tft.fillRect(10, 10, 60, 30, TFT_WHITE);
    tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);
    tft.setTextColor(TFT_YELLOW, TFT_BLUE);
    tft.drawString("DISPLAY TEST v2", 20, 60, 4);
    Serial.printf("[D2] screen %d x %d drawn\n", tft.width(), tft.height());
    delay(3000);
}

void loop() {
    static int i = 0;
    tft.fillScreen(COLS[i]);
    tft.setTextColor((i == 3) ? TFT_BLACK : TFT_WHITE, COLS[i]);
    tft.drawString(NAMES[i], 20, 20, 4);
    Serial.printf("[D2] %s\n", NAMES[i]);
    i = (i + 1) % N;
    delay(2000);
}