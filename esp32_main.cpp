#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdint.h>
TFT_eSPI pd_tft = TFT_eSPI();
uint32_t pd_held = 0, pd_prev = 0; uint16_t pd_col = 0;
void pd_init(); void pd_update(uint32_t); void pd_draw(uint32_t);
static const int PINS[8] = {32, 33, 25, 26, 27, 14, 12, 13};   // UP DOWN LEFT RIGHT A B X Y (edit to match your wiring)
void setup() {
  for (int i = 0; i < 8; i++) pinMode(PINS[i], INPUT_PULLUP);
  pd_tft.init(); pd_tft.setRotation(0); pd_tft.fillScreen(TFT_BLACK);
  pd_init();
}
void loop() {
  static uint32_t last = 0, tick = 0;
  if (millis() - last < 25) return;
  last = millis(); pd_prev = pd_held; pd_held = 0;
  for (int i = 0; i < 8; i++) if (digitalRead(PINS[i]) == LOW) pd_held |= (1u << i);
  pd_update(tick); pd_draw(tick++);
}
