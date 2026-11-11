// Raw XPT2046 test. No libraries. Shows IRQ state and raw X/Y/Z.
#include <Arduino.h>
#define T_CS   1
#define T_CLK  12
#define T_DIN  11
#define T_DO   13
#define T_IRQ  2
#define LCD_CS 10

static uint16_t xpt(uint8_t cmd) {
  digitalWrite(T_CS, LOW);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(T_DIN, (cmd >> i) & 1);
    digitalWrite(T_CLK, HIGH); delayMicroseconds(3);
    digitalWrite(T_CLK, LOW);  delayMicroseconds(3);
  }
  digitalWrite(T_CLK, HIGH); delayMicroseconds(3);   // busy bit
  digitalWrite(T_CLK, LOW);  delayMicroseconds(3);
  uint16_t v = 0;
  for (int i = 0; i < 12; i++) {
    digitalWrite(T_CLK, HIGH); delayMicroseconds(3);
    digitalWrite(T_CLK, LOW);  delayMicroseconds(3);
    v = (v << 1) | digitalRead(T_DO);
  }
  for (int i = 0; i < 3; i++) {
    digitalWrite(T_CLK, HIGH); delayMicroseconds(3);
    digitalWrite(T_CLK, LOW);  delayMicroseconds(3);
  }
  digitalWrite(T_CS, HIGH);
  return v;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);   // keep display off the bus
  pinMode(T_CS, OUTPUT);   digitalWrite(T_CS, HIGH);
  pinMode(T_CLK, OUTPUT);  digitalWrite(T_CLK, LOW);
  pinMode(T_DIN, OUTPUT);  digitalWrite(T_DIN, LOW);
  pinMode(T_DO, INPUT);
  pinMode(T_IRQ, INPUT_PULLUP);
  Serial.println("[T2] raw touch test start");
}

void loop() {
  int irq = digitalRead(T_IRQ);
  uint16_t x = xpt(0xD0), y = xpt(0x90), z = xpt(0xB0);
  Serial.printf("[T2] irq=%d x=%u y=%u z=%u do_idle=%d\n", irq, x, y, z, digitalRead(T_DO));
  delay(300);
}