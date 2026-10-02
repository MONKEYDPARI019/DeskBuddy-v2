#include "sensors.h"
#include "events.h"
#include "config.h"
#include "display.h"
#include "net.h"
#include <OneButton.h>

static OneButton btn1(PIN_BTN1, true, true);   // active low, internal pull-up
static OneButton btn2(PIN_BTN2, true, true);
static OneButton btn3(PIN_BTN3, true, true);

static bool touched = false;
static uint8_t touchLowCount = 0;
static uint32_t lastPatMs = 0;

static bool ledOverride = false;
static uint32_t lastBlinkMs = 0;
static bool blinkState = false;

static void onBtn1Click()   { postEvent(EVT_BUTTON, 1, "click"); }
static void onBtn1Long()    { postEvent(EVT_BUTTON, 1, "long"); }
static void onBtn2Click()   { postEvent(EVT_BUTTON, 2, "click"); }
static void onBtn2Long()    { postEvent(EVT_BUTTON, 2, "long"); }
static void onBtn3Click()   { postEvent(EVT_BUTTON, 3, "click"); }
static void onBtn3Long()    { postEvent(EVT_BUTTON, 3, "long"); }
static void onBtn3Release() { postEvent(EVT_BUTTON, 3, "release"); }

touch_value_t sensorsReadTouch() { return touchRead(PIN_TOUCH); }

bool sensorsButtonDown(uint8_t btn) {
    uint8_t pin = btn == 1 ? PIN_BTN1 : btn == 2 ? PIN_BTN2 : PIN_BTN3;
    return digitalRead(pin) == LOW;
}

static void handleTouch() {
    touch_value_t val = touchRead(PIN_TOUCH);
    uint32_t now = millis();

    // need 2 consecutive low readings (40 ms) to count as a touch — filters noise
    if (val > 0 && val < settings.touchThreshold) {
        if (touchLowCount < 255) touchLowCount++;
    } else {
        touchLowCount = 0;
        touched = false;
    }

    if (!touched && touchLowCount >= 2) {
        touched = true;
        if (now - lastPatMs > 600) {
            lastPatMs = now;
            postEvent(EVT_TOUCH, (int32_t)val, "pat");
        }
    }
}

void sensorsLedWrite(uint8_t red, uint8_t yellow, uint8_t green) {
    digitalWrite(PIN_LED_STATUS, red);
    digitalWrite(PIN_LED_NOTIFY, yellow);
    digitalWrite(PIN_LED_NET, green);
}

void sensorsSetLedOverride(bool on) { ledOverride = on; }

static void updateLeds() {
    if (ledOverride) return;
    uint32_t now = millis();
    if (now - lastBlinkMs > 500) { lastBlinkMs = now; blinkState = !blinkState; }

    // green: solid = app connected, blinking = WiFi only, off = offline
    uint8_t green = LOW;
    if (netAppClients() > 0)    green = HIGH;
    else if (netWifiConnected()) green = blinkState ? HIGH : LOW;

    sensorsLedWrite(settings.silent ? HIGH : LOW, unreadNotifs > 0 ? HIGH : LOW, green);
}

static void sensorsTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        btn1.tick();
        btn2.tick();
        btn3.tick();
        handleTouch();
        updateLeds();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));   // 50 Hz
    }
}

void sensorsInit() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_NOTIFY, OUTPUT);
    pinMode(PIN_LED_NET, OUTPUT);
    sensorsLedWrite(LOW, LOW, LOW);

    btn1.setPressMs(700);
    btn2.setPressMs(700);
    btn3.setPressMs(350);          // push-to-talk should start quickly

    btn1.attachClick(onBtn1Click);
    btn1.attachLongPressStart(onBtn1Long);
    btn2.attachClick(onBtn2Click);
    btn2.attachLongPressStart(onBtn2Long);
    btn3.attachClick(onBtn3Click);
    btn3.attachLongPressStart(onBtn3Long);
    btn3.attachLongPressStop(onBtn3Release);

    xTaskCreatePinnedToCore(sensorsTask, "input", 3072, nullptr, 3, nullptr, 1);
    Serial.println(F("[Input] buttons 32/33/27, touch GPIO4, LEDs 16/17/18 ready"));
}
