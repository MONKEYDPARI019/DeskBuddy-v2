#include "face.h"
#include "display.h"
unsigned long g_ms = 10000;
SerialMock Serial;
const uint8_t u8g2_font_4x6_tf[1] = {0}, u8g2_font_5x8_tf[1] = {0}, u8g2_font_6x10_tf[1] = {0}, u8g2_font_7x14B_tf[1] = {0}, u8g2_font_logisoso24_tf[1] = {0}, u8g2_font_logisoso32_tf[1] = {0};
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(0);
// stubs for anything else face.cpp may reference
volatile int unreadNotifs = 0;
static FILE* out;
static void snap(const char* label) {
  display.clearBuffer(); faceDraw();
  char l[32] = {0}; strncpy(l, label, 31); fwrite(l, 1, 32, out);
  fwrite(display.buf, 1, sizeof(display.buf), out);
}
static void run(unsigned long ms) { for (unsigned long t = 0; t < ms; t += 33) { g_ms += 33; faceUpdate(); } }
int main(int argc, char** argv) {
  srand(7);
  out = fopen(argc > 1 ? argv[1] : "frames.bin", "wb");
  faceInit(); run(2000);
  const char* moods[] = {"default","happy","love","star","wink","dizzy","angry","sad","sleepy","surprised","smug","nervous","cat","sleeping","cute"};
  int frames = argc > 2 ? atoi(argv[2]) : 1; unsigned long stepms = argc > 3 ? atoi(argv[3]) : 160;
  for (auto m : moods) { faceApplyName(m); run(1200); for (int f = 0; f < frames; f++) { snap(m); run(stepms); } }
  const char* states[] = {"listen","think","speak","notify","sleep"};
  for (auto s : states) { faceApplyName(s); run(700); for (int f = 0; f < frames; f++) { snap(s); run(stepms); } faceApplyName("idle"); run(300); }
  fclose(out);
}
