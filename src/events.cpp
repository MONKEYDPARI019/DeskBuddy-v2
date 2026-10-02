#include "events.h"

QueueHandle_t g_eventQueue = nullptr;

void eventsInit() {
    g_eventQueue = xQueueCreate(32, sizeof(Event));
    if (g_eventQueue == nullptr) {
        Serial.println(F("[Events] FATAL: could not create event queue"));
    }
}

bool postEvent(EventType type, int32_t value, const char* text) {
    if (g_eventQueue == nullptr) return false;
    Event ev = {type, value, ""};
    if (text) strlcpy(ev.text, text, sizeof(ev.text));
    // Never block a producer for long: a full queue means the app task is
    // stuck, and dropping an input event is better than freezing the caller.
    bool ok = xQueueSend(g_eventQueue, &ev, pdMS_TO_TICKS(10)) == pdPASS;
    if (!ok) Serial.printf("[Events] queue full, dropped %s\n", eventName(type));
    return ok;
}

bool postEventFromISR(EventType type, int32_t value, const char* text) {
    if (g_eventQueue == nullptr) return false;
    Event ev = {type, value, ""};
    if (text) strlcpy(ev.text, text, sizeof(ev.text));
    BaseType_t woken = pdFALSE;
    bool ok = xQueueSendFromISR(g_eventQueue, &ev, &woken) == pdPASS;
    if (woken) portYIELD_FROM_ISR();
    return ok;
}

const char* eventName(EventType type) {
    switch (type) {
        case EVT_BUTTON:          return "BUTTON";
        case EVT_TOUCH:           return "TOUCH";
        case EVT_NOTIFICATION:    return "NOTIFICATION";
        case EVT_FACE_CMD:        return "FACE_CMD";
        case EVT_SCREEN_CMD:      return "SCREEN_CMD";
        case EVT_SOUND_CMD:       return "SOUND_CMD";
        case EVT_MIC_TRIGGER:     return "MIC_TRIGGER";
        case EVT_WEATHER_UPDATE:  return "WEATHER_UPDATE";
        case EVT_NET_STATUS:      return "NET_STATUS";
        case EVT_APP_LINK:        return "APP_LINK";
        case EVT_SPEAK_START:     return "SPEAK_START";
        case EVT_SPEAK_END:       return "SPEAK_END";
        case EVT_THINKING:        return "THINKING";
        case EVT_SETTING_CHANGED: return "SETTING_CHANGED";
        case EVT_OTA:             return "OTA";
        default:                  return "NONE";
    }
}
