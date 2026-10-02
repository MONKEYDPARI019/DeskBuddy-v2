#include "display.h"
#include "face.h"
#include "config.h"
#include <time.h>
#include <freertos/semphr.h>

// SSD1306 128x64 over hardware I2C (the module on hand is an SSD1306, not an SH1106)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

WeatherData g_weather = {};
volatile int unreadNotifs = 0;

static SemaphoreHandle_t uiMutex = nullptr;

static ScreenMode currentScreen = SCREEN_CLOCK;

static Notification notifBuf[MAX_NOTIFS];
static int notifHead = 0;
static int notifCount = 0;
static int selectedNotifIdx = 0;

static bool popupActive = false;
static uint32_t popupStartMs = 0;
static char popupTitle[24];
static char popupBody[64];

static uint32_t minuteStartMs = 0;
static int otaPercent = -1;
static bool dimmed = false;

static char msgTitle[24] = "";
static char msgSub[32] = "";
static uint32_t msgUntil = 0;

// Pomodoro
static bool     pomoRunning = false;
static bool     pomoBreak = false;
static uint32_t pomoRemainMs = 0;     // valid while paused
static uint32_t pomoEndMs = 0;        // valid while running
static const uint32_t POMO_BREAK_MS = 5UL * 60UL * 1000UL;

static const char* const SCREEN_NAMES[SCREEN_COUNT] = {"clock", "weather", "mochi", "notify", "pomodoro"};

// ---------------------------------------------------------------------------
// Lock
// ---------------------------------------------------------------------------
void uiLockInit() {
    if (!uiMutex) uiMutex = xSemaphoreCreateRecursiveMutex();
}
void uiLock()   { xSemaphoreTakeRecursive(uiMutex, portMAX_DELAY); }
void uiUnlock() { xSemaphoreGiveRecursive(uiMutex); }

// ---------------------------------------------------------------------------
static void centerStr(const char* s, uint8_t y) {
    display.drawStr(64 - display.getStrWidth(s) / 2, y, s);
}

void displayInit() {
    display.setI2CAddress(OLED_I2C_ADDR * 2);
    display.begin();
    displayApplyContrast();
}

void displayApplyContrast() {
    display.setContrast(dimmed ? 1 : settings.brightness);
}

void displaySetDimmed(bool dim) {
    dimmed = dim;
    displayApplyContrast();
}

void displaySetScreen(ScreenMode s) {
    if (s < SCREEN_COUNT) currentScreen = s;
    if (s == SCREEN_NOTIFY) { unreadNotifs = 0; selectedNotifIdx = 0; }
}
void displayNextScreen() { displaySetScreen((ScreenMode)((currentScreen + 1) % SCREEN_COUNT)); }
ScreenMode displayGetScreen() { return currentScreen; }
const char* displayScreenName(ScreenMode s) { return s < SCREEN_COUNT ? SCREEN_NAMES[s] : "?"; }
bool displayScreenByName(const char* name, ScreenMode* out) {
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
        if (!strcasecmp(name, SCREEN_NAMES[i])) { *out = (ScreenMode)i; return true; }
    }
    return false;
}

void displayPushNotification(const char* app, const char* title, const char* body) {
    Notification& n = notifBuf[notifHead];
    n.timestamp = millis();
    strlcpy(n.app,   (app && *app) ? app : "Phone",          sizeof(n.app));
    strlcpy(n.title, (title && *title) ? title : "Notification", sizeof(n.title));
    strlcpy(n.body,  body ? body : "",                        sizeof(n.body));

    notifHead = (notifHead + 1) % MAX_NOTIFS;
    if (notifCount < MAX_NOTIFS) notifCount++;
    if (currentScreen != SCREEN_NOTIFY) unreadNotifs++;
    selectedNotifIdx = 0;

    strlcpy(popupTitle, n.title, sizeof(popupTitle));
    strlcpy(popupBody,  n.body,  sizeof(popupBody));
    popupActive = true;
    popupStartMs = millis();
}

void displayNotifNext() {
    if (notifCount > 0) selectedNotifIdx = (selectedNotifIdx + 1) % notifCount;
}
void displayNotifClear() {
    notifCount = 0; notifHead = 0; selectedNotifIdx = 0; unreadNotifs = 0;
}
int displayNotifCount() { return notifCount; }

// ---------------------------------------------------------------------------
// Pomodoro
// ---------------------------------------------------------------------------
static uint32_t pomoWorkMs() { return (uint32_t)settings.pomodoroMin * 60UL * 1000UL; }

void pomodoroReset() {
    pomoRunning = false;
    pomoBreak = false;
    pomoRemainMs = pomoWorkMs();
}

void pomodoroToggle() {
    if (pomoRunning) {
        int32_t left = (int32_t)(pomoEndMs - millis());
        pomoRemainMs = left > 0 ? left : 0;
        pomoRunning = false;
    } else {
        if (pomoRemainMs == 0) pomoRemainMs = pomoBreak ? POMO_BREAK_MS : pomoWorkMs();
        pomoEndMs = millis() + pomoRemainMs;
        pomoRunning = true;
    }
}

bool pomodoroTick() {
    if (!pomoRunning) return false;
    if ((int32_t)(pomoEndMs - millis()) > 0) return false;
    // period over: switch work <-> break and keep running
    pomoBreak = !pomoBreak;
    pomoRemainMs = pomoBreak ? POMO_BREAK_MS : pomoWorkMs();
    pomoEndMs = millis() + pomoRemainMs;
    return true;
}

bool pomodoroOnBreak() { return pomoBreak; }

static void drawPomodoro() {
    uint32_t left = pomoRunning ? (uint32_t)max<int32_t>(0, (int32_t)(pomoEndMs - millis())) : pomoRemainMs;
    if (!pomoRunning && left == 0) left = pomoWorkMs();

    display.setFont(u8g2_font_6x10_tf);
    centerStr(pomoBreak ? "BREAK" : "FOCUS", 10);
    display.drawHLine(0, 12, 128);

    display.setFont(u8g2_font_logisoso24_tf);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(left / 60000UL), (unsigned long)((left / 1000UL) % 60UL));
    centerStr(buf, 44);

    uint32_t total = pomoBreak ? POMO_BREAK_MS : pomoWorkMs();
    uint8_t w = total ? (uint8_t)(126UL * (total - min(left, total)) / total) : 0;
    display.drawFrame(0, 48, 128, 4);
    display.drawHLine(1, 49, w);
    display.drawHLine(1, 50, w);

    display.setFont(u8g2_font_4x6_tf);
    centerStr(pomoRunning ? "BTN3: pause   BTN3 hold: reset" : "BTN3: start   BTN3 hold: reset", 62);
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
static void drawClock() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    display.setFont(u8g2_font_6x10_tf);
    char date[20];
    strftime(date, sizeof(date), "%a, %d %b %Y", &t);
    centerStr(date, 10);

    for (uint8_t x = 0; x < 128; x += 4) display.drawPixel(x, 13);

    display.setFont(u8g2_font_logisoso32_tf);
    char hhmm[6];
    uint8_t hr = t.tm_hour % 12;
    if (hr == 0) hr = 12;
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", hr, t.tm_min);
    uint8_t tw = display.getStrWidth(hhmm);
    display.drawStr(64 - tw / 2 - 10, 50, hhmm);

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(64 + tw / 2 - 8, 40, t.tm_hour < 12 ? "AM" : "PM");

    if (t.tm_sec == 0) minuteStartMs = millis();
    if (minuteStartMs == 0) minuteStartMs = millis() - (uint32_t)t.tm_sec * 1000;
    uint8_t barW = (uint8_t)((millis() - minuteStartMs) * 126UL / 60000UL);
    if (barW > 126) barW = 126;
    display.drawHLine(1, 62, barW);
}

static void wxSeg(char ch, int ox, int oy, uint8_t sw = 10, uint8_t sh = 8, uint8_t t = 2) {
    const bool S[10][7] = {
        {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1}, {0,1,1,0,0,1,1},
        {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0}, {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
    };
    if (ch < '0' || ch > '9') return;
    const bool* s = S[ch - '0'];
    if (s[0]) display.drawBox(ox + t, oy, sw - 2 * t, t);
    if (s[1]) display.drawBox(ox + sw - t, oy + t, t, sh - t);
    if (s[2]) display.drawBox(ox + sw - t, oy + sh + 1, t, sh - t);
    if (s[3]) display.drawBox(ox + t, oy + 2 * sh - t, sw - 2 * t, t);
    if (s[4]) display.drawBox(ox, oy + sh + 1, t, sh - t);
    if (s[5]) display.drawBox(ox, oy + t, t, sh - t);
    if (s[6]) display.drawBox(ox + t, oy + sh - 1, sw - 2 * t, t);
}

static void wxBigNum(const char* s, int ox, int oy) {
    int cx = ox;
    for (; *s; s++) {
        wxSeg(*s, cx, oy);
        cx += 12;
    }
}

static void wxIconSun(int cx, int cy) {
    display.drawDisc(cx, cy, 7, U8G2_DRAW_ALL);
    display.setDrawColor(0);
    display.drawDisc(cx, cy, 4, U8G2_DRAW_ALL);
    display.setDrawColor(1);
    for (int a = 0; a < 8; a++) {
        float ang = a * 3.14159f / 4;
        display.drawLine(cx + (int)(9 * cosf(ang)), cy + (int)(9 * sinf(ang)),
                         cx + (int)(12 * cosf(ang)), cy + (int)(12 * sinf(ang)));
    }
}

static void wxIconCloud(int cx, int cy) {
    display.drawDisc(cx - 4, cy + 2, 4, U8G2_DRAW_ALL);
    display.drawDisc(cx + 3, cy + 2, 3, U8G2_DRAW_ALL);
    display.drawDisc(cx, cy - 1, 4, U8G2_DRAW_ALL);
    display.drawBox(cx - 7, cy + 3, 14, 3);
}

static void wxIconRain(int cx, int cy) {
    wxIconCloud(cx, cy - 3);
    for (int i = 0; i < 3; i++)
        display.drawLine(cx - 4 + i * 4, cy + 4, cx - 6 + i * 4, cy + 9);
}

static void wxIconThunder(int cx, int cy) {
    wxIconCloud(cx, cy - 3);
    display.drawLine(cx + 2, cy + 4, cx - 1, cy + 8);
    display.drawLine(cx - 1, cy + 8, cx + 1, cy + 8);
    display.drawLine(cx + 1, cy + 8, cx - 2, cy + 11);
}

static void wxIconSnow(int cx, int cy) {
    wxIconCloud(cx, cy - 3);
    for (int i = 0; i < 3; i++) {
        int sx = cx - 4 + i * 4;
        display.drawPixel(sx, cy + 5);
        display.drawPixel(sx - 1, cy + 6);
        display.drawPixel(sx + 1, cy + 6);
        display.drawPixel(sx, cy + 7);
    }
}

static void wxIconMist(int cx, int cy) {
    for (int i = 0; i < 4; i++) {
        uint8_t w = 10 + (i % 2) * 4, x = cx - w / 2;
        display.drawHLine(x, cy - 3 + i * 3, w);
    }
}

static void drawWeather() {
    if (!g_weather.valid) {
        display.setFont(u8g2_font_6x10_tf);
        if (settings.owm_key[0] == '\0') {
            centerStr("No weather API key", 28);
            display.setFont(u8g2_font_5x8_tf);
            centerStr("serial: set owm_key <key>", 44);
        } else {
            centerStr("Fetching weather...", 32);
        }
        return;
    }
    #define CSTR(str, zcx, y) display.drawStr((zcx) - display.getStrWidth(str) / 2, (y), (str))
    display.setFont(u8g2_font_4x6_tf);
    display.drawHLine(0, 0, 128); display.drawHLine(0, 9, 128);
    display.drawHLine(0, 37, 128); display.drawHLine(0, 63, 128);
    display.drawVLine(42, 9, 28); display.drawVLine(85, 9, 28); display.drawVLine(63, 37, 26);

    char city[16];
    strncpy(city, g_weather.city, 15);
    city[15] = '\0';
    for (uint8_t i = 0; city[i]; i++) city[i] = toupper((unsigned char)city[i]);
    uint8_t cw = display.getStrWidth(city), hx = 64 - cw / 2;
    display.drawStr(hx, 7, city);
    if (hx > 4) display.drawHLine(2, 4, hx - 4);
    if (hx + cw + 3 < 126) display.drawHLine(hx + cw + 3, 4, 126 - (hx + cw + 3));

    int iconCode = atoi(g_weather.icon);
    if      (iconCode <= 1)  wxIconSun(21, 23);
    else if (iconCode <= 2)  { wxIconSun(16, 19); wxIconCloud(22, 24); }
    else if (iconCode <= 4)  wxIconCloud(21, 23);
    else if (iconCode <= 10) wxIconRain(21, 22);
    else if (iconCode == 11) wxIconThunder(21, 22);
    else if (iconCode == 13) wxIconSnow(21, 22);
    else                     wxIconMist(21, 23);

    char tmp[5];
    snprintf(tmp, sizeof(tmp), "%d", g_weather.tempC);
    uint8_t digits = strlen(tmp), tempW = digits * 12 - 2;
    uint8_t groupW = tempW + 3 + 9, tempX = 63 - groupW / 2;
    wxBigNum(tmp, tempX, 12);
    uint8_t ax = tempX + tempW + 3;
    display.drawDisc(ax + 1, 12, 2, U8G2_DRAW_ALL);
    display.setDrawColor(0);
    display.drawDisc(ax + 1, 12, 1, U8G2_DRAW_ALL);
    display.setDrawColor(1);
    display.drawHLine(ax + 4, 12, 4);
    display.drawHLine(ax + 4, 16, 4);
    display.drawVLine(ax + 4, 12, 5);

    char fl[9];
    snprintf(fl, sizeof(fl), "FL %dC", g_weather.feelsC);
    CSTR(fl, 106, 17);
    display.drawHLine(87, 20, 38);

    char desc[10];
    strncpy(desc, g_weather.desc, 9);
    desc[9] = '\0';
    desc[0] = toupper((unsigned char)desc[0]);
    for (uint8_t i = 1; desc[i]; i++) desc[i] = tolower((unsigned char)desc[i]);
    CSTR(desc, 106, 30);

    char humline[12];
    snprintf(humline, sizeof(humline), "HUM %d%%", g_weather.humid);
    CSTR(humline, 31, 45);
    uint8_t hbw = (uint8_t)((uint16_t)g_weather.humid * 57U / 100U);
    display.drawHLine(2, 49, 59); display.drawHLine(2, 52, 59);
    display.drawVLine(2, 49, 4);  display.drawVLine(60, 49, 4);
    display.drawBox(3, 50, hbw, 2);

    char wd[9];
    snprintf(wd, sizeof(wd), "%dm/s", (int)g_weather.windMs);
    CSTR("WIND", 95, 45);
    CSTR(wd, 95, 57);
    #undef CSTR
}

static void drawNotifications() {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, "NOTIFICATIONS");
    display.drawHLine(0, 12, 128);

    if (notifCount == 0) {
        display.setFont(u8g2_font_5x8_tf);
        centerStr("No notifications yet", 36);
        return;
    }

    int idx = (notifHead - 1 - selectedNotifIdx + MAX_NOTIFS * 2) % MAX_NOTIFS;
    const Notification& n = notifBuf[idx];

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 24, n.app);
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(0, 36, n.title);
    display.drawHLine(0, 40, 128);

    // body: wrap onto two lines of 25 chars
    char line[26];
    strlcpy(line, n.body, sizeof(line));
    display.drawStr(0, 50, line);
    if (strlen(n.body) > 25) {
        strlcpy(line, n.body + 25, sizeof(line));
        display.drawStr(0, 59, line);
    }

    char countStr[10];
    snprintf(countStr, sizeof(countStr), "%d/%d", selectedNotifIdx + 1, notifCount);
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(128 - display.getStrWidth(countStr), 10, countStr);
}

static void drawPopupOverlay() {
    if (!popupActive) return;
    if (millis() - popupStartMs > 4000) {
        popupActive = false;
        return;
    }
    display.setDrawColor(0);
    display.drawBox(0, 42, 128, 22);
    display.setDrawColor(1);
    display.drawFrame(0, 42, 128, 22);

    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(4, 51, popupTitle);
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(4, 60, popupBody);
}

static void drawOta() {
    display.setFont(u8g2_font_7x14B_tf);
    centerStr("OTA UPDATE", 22);
    display.setFont(u8g2_font_5x8_tf);
    centerStr("Flashing firmware...", 38);
    display.drawFrame(10, 46, 108, 10);
    display.drawBox(12, 48, (104 * constrain(otaPercent, 0, 100)) / 100, 6);
}

void displaySetOtaProgress(int percent) { otaPercent = percent; }

void displayShowMessage(const char* title, const char* subtitle) {
    strlcpy(msgTitle, title ? title : "", sizeof(msgTitle));
    strlcpy(msgSub, subtitle ? subtitle : "", sizeof(msgSub));
    msgUntil = millis() + 2500;
}

void displayDrawTestPattern() {
    display.clearBuffer();
    display.setFont(u8g2_font_7x14B_tf);
    display.drawStr(10, 20, "OLED TEST");
    display.drawFrame(0, 0, 128, 64);
    display.drawBox(10, 30, 40, 20);
    display.drawDisc(80, 40, 10, U8G2_DRAW_ALL);
    display.sendBuffer();
}

void displayRender() {
    display.clearBuffer();
    display.setDrawColor(1);

    if (otaPercent >= 0) {
        drawOta();
    } else if (msgUntil && (int32_t)(msgUntil - millis()) > 0) {
        display.setFont(u8g2_font_7x14B_tf);
        centerStr(msgTitle, 28);
        display.setFont(u8g2_font_6x10_tf);
        centerStr(msgSub, 46);
    } else {
        msgUntil = 0;
        switch (currentScreen) {
            case SCREEN_CLOCK:    drawClock(); break;
            case SCREEN_WEATHER:  drawWeather(); break;
            case SCREEN_MOCHI:    faceDraw(); break;
            case SCREEN_NOTIFY:   drawNotifications(); break;
            case SCREEN_POMODORO: drawPomodoro(); break;
            default: break;
        }
        drawPopupOverlay();
    }
    display.sendBuffer();
}
