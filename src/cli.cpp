#include "cli.h"
#include "config.h"
#include "events.h"
#include "display.h"
#include "face.h"
#include "audio.h"
#include "sensors.h"
#include "mic.h"
#include "net.h"
#include <WiFi.h>

// Serial CLI (115200 baud). Runs in its own low-priority task. Commands
// that change what Mochi does are posted as events so the app task stays
// the single place where behaviour is decided.

static char inputBuffer[128];
static size_t inputLen = 0;

void cliInit() {
    inputLen = 0;
    Serial.println(F("\n=================================="));
    Serial.println(F(" DeskBuddy v2 — type 'help'"));
    Serial.println(F("==================================\n"));
}

static void printHelp() {
    Serial.println(F("\n--- DeskBuddy v2 CLI ---"));
    Serial.println(F("status                    system, network and face status"));
    Serial.println(F("get                       print all settings"));
    Serial.println(F("set <key> <value>         change a setting (applies now; 'save' to keep)"));
    Serial.println(F("save | defaults           write settings to flash | restore defaults"));
    Serial.println(F("face <mood|state>         default happy love star wink dizzy angry sad sleepy"));
    Serial.println(F("                          surprised smug nervous cat sleeping cute"));
    Serial.println(F("                          | boot idle listen think speak notify sleep"));
    Serial.println(F("screen <name|next>        clock weather mochi notify pomodoro"));
    Serial.println(F("sound <name>              intro screen notify timer purr happy sad ..."));
    Serial.println(F("notify <text>             fake a notification"));
    Serial.println(F("test <target>             oled leds buzzer speaker buttons touch mic"));
    Serial.println(F("weather                   refresh weather now"));
    Serial.println(F("wifi reset                forget WiFi and reboot into setup portal"));
    Serial.println(F("reboot"));
    Serial.println(F("------------------------\n"));
}

static void printStatus() {
    Serial.println(F("\n--- Status ---"));
    Serial.printf("Firmware:   %s %s\n", FW_NAME, FW_VERSION);
    Serial.printf("Uptime:     %lu s\n", millis() / 1000UL);
    Serial.printf("Heap:       %u free (min %u)\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    const char* ns = netStatus() == NET_CONNECTED ? "connected" : netStatus() == NET_PORTAL ? "setup portal" : "offline";
    Serial.printf("WiFi:       %s\n", ns);
    if (netWifiConnected()) {
        Serial.printf("IP:         %s (%s.local), RSSI %d dBm\n", WiFi.localIP().toString().c_str(), HOSTNAME, WiFi.RSSI());
        Serial.printf("App link:   ws://%s.local:%u  clients=%d\n", HOSTNAME, settings.ws_port, netAppClients());
    }
    {
        UiLock lock;
        Serial.printf("Face:       state=%s mood=%s\n", faceStateName(faceGetState()), faceMoodName(faceGetMood()));
        Serial.printf("Screen:     %s, notifications=%d (unread %d)\n",
                      displayScreenName(displayGetScreen()), displayNotifCount(), (int)unreadNotifs);
    }
    Serial.printf("Audio:      %s, volume %u, silent %s\n",
                  settings.audioBackend == AUDIO_BACKEND_DAC ? "DAC 25/26" : "buzzer 19", settings.volume, settings.silent ? "ON" : "off");
    Serial.printf("Mic:        %s\n", micAvailable() ? "ready" : "off");
    Serial.println(F("--------------\n"));
}

static void runTest(const char* target) {
    if (!strcasecmp(target, "oled")) {
        {
            UiLock lock;              // holding the lock pauses the UI task
            displayDrawTestPattern();
            delay(2000);
        }
        Serial.println(F("[Test] OLED pattern shown for 2 s"));
    } else if (!strcasecmp(target, "leds")) {
        sensorsSetLedOverride(true);
        sensorsLedWrite(HIGH, LOW, LOW);  delay(400);
        sensorsLedWrite(LOW, HIGH, LOW);  delay(400);
        sensorsLedWrite(LOW, LOW, HIGH);  delay(400);
        sensorsLedWrite(HIGH, HIGH, HIGH); delay(400);
        sensorsLedWrite(LOW, LOW, LOW);
        sensorsSetLedOverride(false);
        Serial.println(F("[Test] LEDs: red, yellow, green, all"));
    } else if (!strcasecmp(target, "buzzer")) {
        uint8_t prev = settings.audioBackend;
        settings.audioBackend = AUDIO_BACKEND_BUZZER;
        audioPlaySound(SOUND_INTRO, true);
        delay(600);
        settings.audioBackend = prev;
        Serial.println(F("[Test] buzzer chime on GPIO 19"));
    } else if (!strcasecmp(target, "speaker")) {
        audioTestSpeaker();
    } else if (!strcasecmp(target, "buttons")) {
        Serial.println(F("[Test] press buttons for 5 s (raw levels):"));
        uint32_t end = millis() + 5000;
        bool last[3] = {false, false, false};
        while (millis() < end) {
            for (uint8_t b = 1; b <= 3; b++) {
                bool d = sensorsButtonDown(b);
                if (d != last[b - 1]) { last[b - 1] = d; Serial.printf("  BTN%u %s\n", b, d ? "DOWN" : "up"); }
            }
            delay(10);
        }
    } else if (!strcasecmp(target, "touch")) {
        Serial.printf("[Test] touch GPIO4 for 5 s, threshold=%u (touched = below)\n", settings.touchThreshold);
        uint32_t end = millis() + 5000;
        while (millis() < end) {
            Serial.printf("  touch=%u\n", (unsigned)sensorsReadTouch());
            delay(250);
        }
        Serial.println(F("  tip: set touch_th about halfway between the untouched and touched values"));
    } else if (!strcasecmp(target, "mic")) {
        if (!micAvailable()) {
            Serial.println(F("[Test] mic not running (set mic_en on, save, reboot)"));
            return;
        }
        Serial.println(F("[Test] mic peak level for 5 s — speak or clap:"));
        uint32_t end = millis() + 5000;
        while (millis() < end) {
            uint16_t p = micPeakLevel();
            char bar[33];
            uint8_t n = (uint8_t)min<uint32_t>(32, p / 1024);
            memset(bar, '#', n); bar[n] = '\0';
            Serial.printf("  %5u |%s\n", p, bar);
            delay(200);
        }
    } else {
        Serial.printf("Unknown test '%s' (oled leds buzzer speaker buttons touch mic)\n", target);
    }
}

void cliHandleCommand(const char* line) {
    if (!line || !*line) return;

    char cmd[128];
    strlcpy(cmd, line, sizeof(cmd));
    char* token = strtok(cmd, " \r\n");
    if (!token) return;
    // rest of the line (for commands that take free text)
    char* rest = strtok(nullptr, "\r\n");
    while (rest && *rest == ' ') rest++;

    if (!strcasecmp(token, "help") || !strcmp(token, "?")) {
        printHelp();
    } else if (!strcasecmp(token, "status")) {
        printStatus();
    } else if (!strcasecmp(token, "get")) {
        settingsPrint();
    } else if (!strcasecmp(token, "set")) {
        char* key = rest ? strtok(rest, " ") : nullptr;
        char* val = key ? strtok(nullptr, "\r\n") : nullptr;
        if (!key || !val) { Serial.println(F("Usage: set <key> <value>   (see 'get' for keys)")); return; }
        if (settingsSet(key, val)) {
            postEvent(EVT_SETTING_CHANGED, 0, key);
            Serial.printf("%s = %s  (type 'save' to keep after reboot)\n", key, val);
        } else {
            Serial.printf("Unknown key '%s'\n", key);
        }
    } else if (!strcasecmp(token, "save")) {
        settingsSave();
    } else if (!strcasecmp(token, "defaults")) {
        settingsResetDefaults();
        postEvent(EVT_SETTING_CHANGED, 0, "all");
        Serial.println(F("Defaults restored (not saved yet)"));
    } else if (!strcasecmp(token, "face")) {
        if (!rest) { Serial.println(F("Usage: face <mood|state>")); return; }
        postEvent(EVT_FACE_CMD, 0, rest);
    } else if (!strcasecmp(token, "screen")) {
        ScreenMode s;
        if (!rest || !strcasecmp(rest, "next")) postEvent(EVT_SCREEN_CMD, -1);
        else if (displayScreenByName(rest, &s)) postEvent(EVT_SCREEN_CMD, s);
        else Serial.println(F("Screens: clock weather mochi notify pomodoro"));
    } else if (!strcasecmp(token, "sound")) {
        if (!rest) { Serial.println(F("Usage: sound <name>")); return; }
        postEvent(EVT_SOUND_CMD, 0, rest);
    } else if (!strcasecmp(token, "notify")) {
        { UiLock lock; displayPushNotification("CLI", "Test", rest ? rest : "Hello from serial"); }
        postEvent(EVT_NOTIFICATION, 0, "CLI");
    } else if (!strcasecmp(token, "test")) {
        if (!rest) { Serial.println(F("Usage: test <oled|leds|buzzer|speaker|buttons|touch|mic>")); return; }
        runTest(rest);
    } else if (!strcasecmp(token, "weather")) {
        netRequestWeather();
        Serial.println(F("Weather refresh requested"));
    } else if (!strcasecmp(token, "wifi")) {
        if (rest && !strcasecmp(rest, "reset")) {
            Serial.println(F("Forgetting WiFi, rebooting into portal 'DeskBuddy_Setup'..."));
            netResetWifi();
        } else {
            printStatus();
        }
    } else if (!strcasecmp(token, "reboot") || !strcasecmp(token, "restart")) {
        Serial.println(F("Rebooting..."));
        delay(200);
        ESP.restart();
    } else {
        Serial.printf("Unknown command '%s' — type 'help'\n", token);
    }
}

void cliProcess() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (inputLen > 0) {
                inputBuffer[inputLen] = '\0';
                Serial.printf("> %s\n", inputBuffer);
                cliHandleCommand(inputBuffer);
                inputLen = 0;
            }
        } else if (c == '\b' || c == 127) {
            if (inputLen > 0) inputLen--;
        } else if (inputLen < sizeof(inputBuffer) - 1) {
            inputBuffer[inputLen++] = c;
        }
    }
}
