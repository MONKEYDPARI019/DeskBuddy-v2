#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Firmware identity
// ---------------------------------------------------------------------------
#define FW_NAME     "DeskBuddy"
#define FW_VERSION  "2.0.0-dev"
#define HOSTNAME    "deskbuddy"          // mDNS: deskbuddy.local, OTA hostname

// ---------------------------------------------------------------------------
// Hardware pins — the ONLY place pins are defined (ESP32 WROOM DevKit)
// Reserved: GPIO 6-11 (flash). GPIO 34-39 are input-only.
// ---------------------------------------------------------------------------
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define OLED_I2C_ADDR   0x3C

#define PIN_BTN1        32      // to GND, INPUT_PULLUP
#define PIN_BTN2        33
#define PIN_BTN3        27
#define PIN_TOUCH       4       // T0 capacitive pad

#define PIN_LED_STATUS  16      // red    — silent / DND
#define PIN_LED_NOTIFY  17      // yellow — unread notifications
#define PIN_LED_NET     18      // green  — WiFi / app connection

#define PIN_BUZZER      19      // optional passive buzzer (audio backend 1)
#define PIN_DAC_L       25      // built-in DAC ch1 -> PAM8403 L
#define PIN_DAC_R       26      // built-in DAC ch2 -> PAM8403 R

#define PIN_MIC_SCK     14      // INMP441 BCLK/SCK
#define PIN_MIC_WS      13      // INMP441 WS/LRCL
#define PIN_MIC_SD      34      // INMP441 SD (input-only pin is fine)

// ---------------------------------------------------------------------------
// Settings — every user-tunable value lives here, persisted in NVS
// ---------------------------------------------------------------------------
enum AudioBackend : uint8_t {
    AUDIO_BACKEND_DAC = 0,
    AUDIO_BACKEND_BUZZER = 1
};

struct Settings {
    // Weather (OpenWeatherMap) — key is set via CLI: set owm_key <key>
    char owm_key[40];
    char owm_city[32];
    char owm_units[10];         // metric | imperial

    // Time
    char tz[40];                // POSIX TZ string, e.g. "IST-5:30"

    // HTTP notification endpoint: GET http://deskbuddy.local:<port>/notify?app=&title=&msg=
    bool http_enabled;
    uint16_t http_port;

    // App link (Phase 7): device runs a WebSocket server, phone/PC connect to it
    uint16_t ws_port;           // ws://deskbuddy.local:<ws_port>/

    // Device
    bool silent;                // DND: no sounds
    uint8_t brightness;         // OLED contrast 0-255
    uint8_t volume;             // 0-100
    uint32_t sleepTimeoutSec;   // face goes to sleep + display dims after inactivity (0 = never)
    uint16_t pomodoroMin;       // pomodoro work length
    uint16_t touchThreshold;    // touchRead() below this = touched (calibrate with `test touch`)

    // Audio / mic
    uint8_t audioBackend;       // 0 = DAC (GPIO 25/26), 1 = buzzer (GPIO 19)
    bool micEnabled;            // INMP441 I2S mic present
    bool emoBuzzer;             // mood/emotion chirps play on the buzzer (GPIO 19); everything else on audioBackend
};

extern Settings settings;

void settingsInit();
void settingsLoad();
void settingsSave();
void settingsResetDefaults();
bool settingsSet(const char* key, const char* val);   // returns false for unknown key
void settingsPrint();

#endif // CONFIG_H
