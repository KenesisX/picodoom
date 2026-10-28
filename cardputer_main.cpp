// Arduino entry point: runs the PicoDoom engine (../../main.cpp) on the Cardputer at 40 Hz.
#include <M5Cardputer.h>
#include <stdint.h>
M5Canvas pd_canvas(&M5Cardputer.Display);
uint32_t pd_held = 0, pd_prev = 0; uint16_t pd_col = 0;
void pd_init(); void pd_update(uint32_t); void pd_draw(uint32_t);
static bool k(char c) { return M5Cardputer.Keyboard.isKeyPressed(c); }
void setup() {
  auto cfg = M5.config(); M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1); M5Cardputer.Display.fillScreen(0);
  pd_canvas.createSprite(240, 135); pd_canvas.fillScreen(0);
  pd_init();
}
void loop() {
  static uint32_t last = 0, tick = 0;
  M5Cardputer.update();
  if (millis() - last < 25) return;
  last = millis();
  pd_prev = pd_held; pd_held = 0;
  if (k(';') || k('w')) pd_held |= 1;      // UP    (walk)
  if (k('.') || k('s')) pd_held |= 2;      // DOWN
  if (k(',') || k('a')) pd_held |= 4;      // LEFT  (turn)
  if (k('/') || k('d')) pd_held |= 8;      // RIGHT
  if (k(' ') || k('f')) pd_held |= 16;     // A  fire
  if (k('b') || k('z')) pd_held |= 32;     // B  strafe modifier
  if (k('x') || k('q')) pd_held |= 64;     // X  switch weapon
  if (k('m') || k('y')) pd_held |= 128;    // Y  minimap
  pd_update(tick); pd_draw(tick++);
  pd_canvas.pushSprite(0, 0);
}
