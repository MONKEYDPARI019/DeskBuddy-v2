/*
 * DeskBuddy v2 — ESP32 desk companion with an animated face called Mochi.
 *
 * Task layout (Phase 2):
 *
 *   core 1  ui     (prio 2)  ~30 fps: faceUpdate() + displayRender()
 *   core 1  app    (prio 3)  the only consumer of g_eventQueue — all behaviour lives here
 *   core 1  input  (prio 3)  buttons / touch / LEDs at 50 Hz        -> events
 *   core 1  cli    (prio 1)  serial commands                         -> events
 *   core 0  net    (prio 2)  WiFi, NTP, OTA, HTTP, WebSocket, weather -> events
 *   core 0  audio  (prio 4)  chimes + streamed speech on the DAC
 *   core 0  mic    (prio 3)  INMP441 capture while push-to-talk      (optional)
 *
 * Locking rules:
 *   - face/display state: hold the UI lock (UiLock) — see display.h
 *   - never call net* send functions while holding the UI lock
 */

#include <Arduino.h>
#include <Wire.h>
#include <time.h>

#include "config.h"
#include "events.h"
#include "display.h"
#include "face.h"
#include "sensors.h"
#include "audio.h"
#include "mic.h"
#include "net.h"
#include "cli.h"

static uint32_t lastActivityMs = 0;
static bool     sleeping = false;
static ScreenMode screenBeforeSleep = SCREEN_CLOCK;

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
static void uiTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        {
            UiLock lock;
            faceUpdate();
            displayRender();
        }
        // sleep mode: 10 fps is plenty for a snoozing face
        vTaskDelayUntil(&last, pdMS_TO_TICKS(sleeping ? 100 : 33));
    }
}

static void cliTask(void*) {
    cliInit();
    for (;;) {
        cliProcess();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ---------------------------------------------------------------------------
// App logic helpers (called from the app task only)
// ---------------------------------------------------------------------------
static void showMochi() { displaySetScreen(SCREEN_MOCHI); }

static void goToSleep() {
    UiLock lock;
    sleeping = true;
    screenBeforeSleep = displayGetScreen();
    showMochi();
    faceSetState(FACE_STATE_SLEEP);
    displaySetDimmed(true);
    Serial.println(F("[App] sleeping (any input wakes)"));
}

// Returns true if the device was asleep (the waking input is then swallowed)
static bool wakeUp() {
    lastActivityMs = millis();
    if (!sleeping) return false;
    UiLock lock;
    sleeping = false;
    displaySetDimmed(false);
    displaySetScreen(screenBeforeSleep);
    faceSetState(FACE_STATE_IDLE);
    Serial.println(F("[App] awake"));
    return true;
}

static void startPushToTalk() {
    if (!micAvailable()) {
        { UiLock lock; displayShowMessage("Mic is off", "set mic_en on"); }
        audioPlaySound(SOUND_ERROR);
        return;
    }
    if (netAppClients() == 0) {
        { UiLock lock; displayShowMessage("No app", "connect the app first"); }
        audioPlaySound(SOUND_ERROR);
        return;
    }
    audioStreamAbort();                       // talking over Mochi interrupts it
    { UiLock lock; showMochi(); faceSetState(FACE_STATE_LISTENING); }
    audioPlaySound(SOUND_LISTEN, true);
    netSendPttStart();
    micStartCapture();
}

static void stopPushToTalk() {
    if (!micIsCapturing()) return;
    micStopCapture();
    netSendPttEnd();
    audioPlaySound(SOUND_LISTEN_END, true);
    UiLock lock;
    faceSetState(FACE_STATE_THINKING);        // app answers with say_start or think:false
}

static void handleButton(uint8_t btn, const char* action) {
    netSendButton(btn, action);

    // any press wakes Mochi; the waking press does nothing else (except PTT release)
    bool wasAsleep = wakeUp();
    if (wasAsleep && strcmp(action, "release") != 0) return;

    ScreenMode screen;
    { UiLock lock; screen = displayGetScreen(); }

    if (btn == 1 && !strcmp(action, "click")) {
        UiLock lock;
        if (screen == SCREEN_NOTIFY) {
            displayNotifNext();
            audioPlaySound(SOUND_SCREEN);
        } else {
            faceNextMood();
            showMochi();
            audioPlaySound(SOUND_MOOD);
        }
    } else if (btn == 1 && !strcmp(action, "long")) {
        { UiLock lock; displayNotifClear(); displayShowMessage("Cleared", "notifications"); }
        audioPlaySound(SOUND_SCREEN);
    } else if (btn == 2 && !strcmp(action, "click")) {
        { UiLock lock; displayNextScreen(); }
        audioPlaySound(SOUND_SCREEN);
    } else if (btn == 2 && !strcmp(action, "long")) {
        settings.silent = !settings.silent;
        settingsSave();
        { UiLock lock; displayShowMessage(settings.silent ? "Silent ON" : "Silent OFF", ""); }
        audioPlaySound(settings.silent ? SOUND_SILENT_ON : SOUND_SILENT_OFF, true);
    } else if (btn == 3 && !strcmp(action, "click")) {
        if (screen == SCREEN_POMODORO) {
            { UiLock lock; pomodoroToggle(); }
            audioPlaySound(SOUND_SCREEN);
        } else if (screen == SCREEN_WEATHER) {
            netRequestWeather();
            audioPlaySound(SOUND_SCREEN);
        } else {
            { UiLock lock; showMochi(); faceReact(MOOD_WINK, 1500); }
            audioPlaySound(SOUND_HAPPY);
        }
    } else if (btn == 3 && !strcmp(action, "long")) {
        if (screen == SCREEN_POMODORO) {
            { UiLock lock; pomodoroReset(); displayShowMessage("Pomodoro", "reset"); }
            audioPlaySound(SOUND_SCREEN);
        } else {
            startPushToTalk();
        }
    } else if (btn == 3 && !strcmp(action, "release")) {
        stopPushToTalk();
    }
}

static void applySetting(const char* key) {
    UiLock lock;
    displayApplyContrast();
    if (!strcasecmp(key, "tz") || !strcasecmp(key, "all")) configTzTime(settings.tz, "pool.ntp.org");
    if (!strncasecmp(key, "owm", 3) || !strcasecmp(key, "all")) netRequestWeather();
}

static void handleEvent(const Event& ev) {
    switch (ev.type) {
        case EVT_BUTTON:
            handleButton((uint8_t)ev.value, ev.text);
            break;

        case EVT_TOUCH:
            netSendTouch();
            if (wakeUp()) break;
            { UiLock lock; showMochi(); faceReact(random(2) ? MOOD_LOVE : MOOD_CUTE, 2500); }
            audioPlaySound(SOUND_PURR);
            break;

        case EVT_NOTIFICATION: {
            wakeUp();
            UiLock lock;
            FaceState s = faceGetState();
            if (s != FACE_STATE_LISTENING && s != FACE_STATE_THINKING && s != FACE_STATE_SPEAKING) {
                faceSetState(FACE_STATE_NOTIFY);
            }
            audioPlaySound(SOUND_NOTIFY);
            break;
        }

        case EVT_FACE_CMD: {
            wakeUp();
            bool ok;
            { UiLock lock; ok = faceApplyName(ev.text); if (ok) showMochi(); }
            if (ok) audioPlaySound(SOUND_MOOD);
            else Serial.printf("[App] unknown face '%s'\n", ev.text);
            break;
        }

        case EVT_SCREEN_CMD:
            wakeUp();
            { UiLock lock; if (ev.value < 0) displayNextScreen(); else displaySetScreen((ScreenMode)ev.value); }
            break;

        case EVT_SOUND_CMD:
            if (!audioPlaySoundByName(ev.text, true)) Serial.printf("[App] unknown sound '%s'\n", ev.text);
            break;

        case EVT_MIC_TRIGGER:
            // push-to-talk from the app (BTN3 on the device goes through EVT_BUTTON)
            if (ev.value) { wakeUp(); startPushToTalk(); }
            else stopPushToTalk();
            break;

        case EVT_THINKING: {
            UiLock lock;
            if (ev.value) { showMochi(); faceSetState(FACE_STATE_THINKING); }
            else if (faceGetState() == FACE_STATE_THINKING) faceSetState(FACE_STATE_IDLE);
            break;
        }

        case EVT_SPEAK_START:
            wakeUp();
            { UiLock lock; showMochi(); faceSetState(FACE_STATE_SPEAKING); }
            break;

        case EVT_SPEAK_END:
            { UiLock lock; if (faceGetState() == FACE_STATE_SPEAKING) faceSetState(FACE_STATE_IDLE); }
            lastActivityMs = millis();
            break;

        case EVT_SETTING_CHANGED:
            applySetting(ev.text);
            break;

        case EVT_NET_STATUS:
            if (ev.value == NET_CONNECTED) {
                UiLock lock;
                displayShowMessage("WiFi connected", HOSTNAME ".local");
            }
            break;

        case EVT_APP_LINK:
            if (ev.value > 0) {
                { UiLock lock; displayShowMessage("App connected", ""); faceReact(MOOD_HAPPY, 2000); }
                audioPlaySound(SOUND_HAPPY);
            } else {
                stopPushToTalk();
                UiLock lock;
                if (faceGetState() == FACE_STATE_THINKING) faceSetState(FACE_STATE_IDLE);
            }
            break;

        default:
            break;
    }

    // keep the app's view in sync (outside any UI lock — see locking rules)
    switch (ev.type) {
        case EVT_BUTTON: case EVT_TOUCH: case EVT_NOTIFICATION: case EVT_FACE_CMD:
        case EVT_SCREEN_CMD: case EVT_THINKING: case EVT_SPEAK_START: case EVT_SPEAK_END:
        case EVT_SETTING_CHANGED: case EVT_MIC_TRIGGER: case EVT_WEATHER_UPDATE:
            netSendState();
            break;
        default:
            break;
    }
}

static void periodic() {
    uint32_t now = millis();

    // pomodoro period finished
    bool pomoDone, onBreak;
    { UiLock lock; pomoDone = pomodoroTick(); onBreak = pomodoroOnBreak(); }
    if (pomoDone) {
        wakeUp();
        { UiLock lock; displayPushNotification("Pomodoro", onBreak ? "Break time!" : "Back to focus", onBreak ? "Stretch for 5 min" : "Next session started"); }
        audioPlaySound(SOUND_TIMER);
    }

    // WiFi setup portal hint: shown briefly every 30 s so the screens stay usable
    static uint32_t lastPortalMsg = 0;
    if (netStatus() == NET_PORTAL && (lastPortalMsg == 0 || now - lastPortalMsg > 30000)) {
        lastPortalMsg = now;
        UiLock lock;
        displayShowMessage("WiFi setup", "Join DeskBuddy_Setup");
    }

    // inactivity -> sleep (not while talking or timing)
    if (!sleeping && settings.sleepTimeoutSec > 0 && now - lastActivityMs > settings.sleepTimeoutSec * 1000UL) {
        FaceState s;
        { UiLock lock; s = faceGetState(); }
        bool busy = s != FACE_STATE_IDLE || audioIsStreaming() || micIsCapturing();
        if (!busy) goToSleep();
        else lastActivityMs = now;
    }
}

// Push state to the app when something changed on its own (notify timeout,
// falling asleep...) and every 10 s as a heartbeat (RSSI, weather).
static void syncAppState() {
    static uint32_t lastSig = 0, lastSend = 0;
    if (netAppClients() == 0) { lastSig = 0; return; }
    uint32_t sig;
    {
        UiLock lock;
        sig = (uint32_t)faceGetState() | ((uint32_t)faceShownMood() << 8) |
              ((uint32_t)displayGetScreen() << 16) | ((uint32_t)(unreadNotifs & 0xFF) << 24);
    }
    uint32_t now = millis();
    if (sig != lastSig || now - lastSend > 10000) {
        lastSig = sig;
        lastSend = now;
        netSendState();
    }
}

static void appTask(void*) {
    Event ev;
    uint32_t lastPeriodic = 0;
    for (;;) {
        if (xQueueReceive(g_eventQueue, &ev, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (ev.type != EVT_OTA) Serial.printf("[Event] %s %ld %s\n", eventName(ev.type), (long)ev.value, ev.text);
            handleEvent(ev);
        }
        if (millis() - lastPeriodic >= 100) {
            lastPeriodic = millis();
            periodic();
            syncAppState();
        }
    }
}

// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[Boot] %s %s\n", FW_NAME, FW_VERSION);

    settingsInit();
    eventsInit();
    uiLockInit();

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);
    {
        UiLock lock;
        displayInit();
        faceInit();
        pomodoroReset();
        displaySetScreen(SCREEN_MOCHI);     // say hello with the face first
    }

    audioInit();
    micInit();
    sensorsInit();
    netInit();

    lastActivityMs = millis();
    xTaskCreatePinnedToCore(uiTask,  "ui",  6144, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(appTask, "app", 6144, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(cliTask, "cli", 4096, nullptr, 1, nullptr, 1);

    audioPlaySound(SOUND_INTRO);
    Serial.println(F("[Boot] all tasks started"));
}

void loop() {
    vTaskDelete(nullptr);   // everything runs in tasks
}
