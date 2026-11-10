// Pin test: drives ONE display signal pin HIGH at a time (others LOW), 4 s each.
// Measure the matching pin on the DISPLAY header with a multimeter (DC volts, black probe on GND).
// Expected: ~3.3 V on the pin named in the console, ~0 V on the others.
#include <Arduino.h>

struct P { const char *name; int gpio; };
static const P PINS[] = {
    { "CS",         10 },
    { "RESET",       7 },
    { "DC (RS)",    14 },
    { "MOSI (SDI)", 11 },
    { "SCK",        12 },
};
static const int N = sizeof(PINS) / sizeof(PINS[0]);

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("[P] pin test start");
    for (int i = 0; i < N; i++) { pinMode(PINS[i].gpio, OUTPUT); digitalWrite(PINS[i].gpio, LOW); }
}

void loop() {
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) digitalWrite(PINS[k].gpio, (k == i) ? HIGH : LOW);
        Serial.printf("[P] %s (GPIO%d) is HIGH now, measure it on the display\n", PINS[i].name, PINS[i].gpio);
        delay(4000);
    }
}