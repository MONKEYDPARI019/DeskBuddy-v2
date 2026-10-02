#pragma once
#include "Arduino.h"
#define U8G2_DRAW_ALL 0x0f
#define U8G2_R0 0
#define U8X8_PIN_NONE 255
extern const uint8_t u8g2_font_4x6_tf[], u8g2_font_5x8_tf[], u8g2_font_6x10_tf[], u8g2_font_7x14B_tf[], u8g2_font_logisoso24_tf[], u8g2_font_logisoso32_tf[];
class U8G2_SSD1306_128X64_NONAME_F_HW_I2C {
public:
  uint8_t buf[64][128]; uint8_t col = 1; int fw = 4;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C(int, int = 0, int = 0, int = 0) { clearBuffer(); }
  void clearBuffer() { memset(buf, 0, sizeof(buf)); }
  void setDrawColor(uint8_t c) { col = c; }
  void px(int x, int y) { if (x < 0 || y < 0 || x > 127 || y > 63) return; if (col == 2) buf[y][x] ^= 1; else buf[y][x] = col; }
  void drawPixel(int x, int y) { px(x, y); }
  void drawHLine(int x, int y, int w) { for (int i = 0; i < w; i++) px(x + i, y); }
  void drawVLine(int x, int y, int h) { for (int i = 0; i < h; i++) px(x, y + i); }
  void drawBox(int x, int y, int w, int h) { for (int j = 0; j < h; j++) drawHLine(x, y + j, w); }
  void drawFrame(int x, int y, int w, int h) { drawHLine(x, y, w); drawHLine(x, y + h - 1, w); drawVLine(x, y, h); drawVLine(x + w - 1, y, h); }
  void drawLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) { px(x0, y0); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
  }
  void drawDisc(int x, int y, int r, int = 0) { for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx * dx + dy * dy <= r * r + r) px(x + dx, y + dy); }
  void drawCircle(int x, int y, int r, int = 0) { for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) { int d = dx * dx + dy * dy; if (d <= r * r + r && d >= r * r - r) px(x + dx, y + dy); } }
  void drawFilledEllipse(int x, int y, int rx, int ry, int = 0) { for (int dy = -ry; dy <= ry; dy++) for (int dx = -rx; dx <= rx; dx++) if ((float)dx*dx/(rx*rx+0.5f) + (float)dy*dy/(ry*ry+0.5f) <= 1.0f) px(x + dx, y + dy); }
  void drawEllipse(int x, int y, int rx, int ry, int = 0) { for (int a = 0; a < 360; a++) px(x + (int)lroundf(rx * cosf(a * PI / 180)), y + (int)lroundf(ry * sinf(a * PI / 180))); }
  void drawRBox(int x, int y, int w, int h, int r) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
      int cx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0), cy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
      if (cx && cy && cx * cx + cy * cy > r * r + r) continue; px(x + i, y + j); }
  }
  void drawRFrame(int x, int y, int w, int h, int r) { drawRBox(x, y, w, h, r); }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2) {
    int minx = std::min({x0,x1,x2}), maxx = std::max({x0,x1,x2}), miny = std::min({y0,y1,y2}), maxy = std::max({y0,y1,y2});
    for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
      float d1 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1), d2 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2), d3 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
      bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0; if (!(neg && pos)) px(x, y); }
  }
  void setFont(const uint8_t* f) { fw = (f == u8g2_font_4x6_tf) ? 4 : 6; }
  int getStrWidth(const char* s) { return strlen(s) * fw; }
  void drawStr(int x, int y, const char* s) { for (int i = 0; s[i]; i++) if (s[i] != ' ') drawBox(x + i * fw, y - fw - 1, fw - 1, fw + 1); }
  void sendBuffer() {}
  void setContrast(int) {}
};
