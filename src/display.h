#ifndef DISPLAY_H
#define DISPLAY_H

// OLED rendering + the UI data it shows (notifications, weather, pomodoro).
//
// Thread safety: the UI task draws ~30 times a second while the app, net
// and CLI tasks change what is shown. Anything that touches `display`, the
// face, or the data below must hold the UI lock:
//     { UiLock lock; faceSetMood(MOOD_HAPPY); }

#include <Arduino.h>
#include <U8g2lib.h>

enum ScreenMode : uint8_t {
    SCREEN_CLOCK = 0,
    SCREEN_WEATHER,
    SCREEN_MOCHI,
    SCREEN_NOTIFY,
    SCREEN_POMODORO,
    SCREEN_COUNT
};

#define MAX_NOTIFS 8
struct Notification {
    char app[16];
    char title[24];
    char body[64];
    uint32_t timestamp;
};

struct WeatherData {
    char  city[24];
    char  desc[32];
    char  icon[4];
    int   tempC;
    int   feelsC;
    int   humid;
    float windMs;
    bool  valid;
};

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;
extern WeatherData g_weather;
extern volatile int unreadNotifs;   // read by the LED code without the lock

// ---- locking ----
void uiLockInit();
void uiLock();
void uiUnlock();
struct UiLock {
    UiLock()  { uiLock(); }
    ~UiLock() { uiUnlock(); }
};

// ---- everything below: caller holds the UI lock ----
void        displayInit();
void        displayApplyContrast();
void        displaySetScreen(ScreenMode s);
void        displayNextScreen();
ScreenMode  displayGetScreen();
const char* displayScreenName(ScreenMode s);
bool        displayScreenByName(const char* name, ScreenMode* out);

void displayPushNotification(const char* app, const char* title, const char* body);
void displayNotifNext();
void displayNotifClear();
int  displayNotifCount();

// Pomodoro timer shown on SCREEN_POMODORO
void pomodoroToggle();          // start / pause / resume
void pomodoroReset();
bool pomodoroTick();            // true once when a work/break period ends
bool pomodoroOnBreak();

void displaySetOtaProgress(int percent);   // -1 = not updating
void displaySetDimmed(bool dim);            // low contrast while Mochi sleeps
void displayShowMessage(const char* title, const char* subtitle);  // splash until next frame
void displayRender();                       // draw one full frame
void displayDrawTestPattern();

#endif // DISPLAY_H
