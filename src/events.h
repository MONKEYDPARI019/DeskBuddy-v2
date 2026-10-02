#ifndef EVENTS_H
#define EVENTS_H

// Phase 2 — every module talks to the app logic through ONE queue.
// Producers (sensors, net, cli, audio, mic) post events; the app task in
// main.cpp is the only consumer and decides what the face/display/audio do.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum EventType : uint16_t {
    EVT_NONE = 0,
    EVT_BUTTON,          // value: 1..3, text: "click" | "double" | "long" | "release"
    EVT_TOUCH,           // value: raw touch reading, text: "pat"
    EVT_NOTIFICATION,    // already stored in notif buffer; text: app name
    EVT_FACE_CMD,        // text: mood or state name (from CLI / app)
    EVT_SCREEN_CMD,      // value: ScreenMode, or -1 for "next"
    EVT_SOUND_CMD,       // text: sound name (from CLI / app)
    EVT_MIC_TRIGGER,     // value: 1 = push-to-talk start, 0 = stop
    EVT_WEATHER_UPDATE,  // value: 1 = ok, 0 = failed
    EVT_NET_STATUS,      // value: NetStatus
    EVT_APP_LINK,        // value: number of connected app clients
    EVT_SPEAK_START,     // app started streaming a spoken reply
    EVT_SPEAK_END,       // reply finished playing
    EVT_THINKING,        // app says it is processing (value 1/0)
    EVT_SETTING_CHANGED, // text: key
    EVT_OTA,             // value: percent 0..100, -1 = finished/error
};

struct Event {
    EventType type;
    int32_t value;
    char text[48];
};

extern QueueHandle_t g_eventQueue;

void eventsInit();
bool postEvent(EventType type, int32_t value = 0, const char* text = nullptr);
bool postEventFromISR(EventType type, int32_t value = 0, const char* text = nullptr);
const char* eventName(EventType type);

#endif // EVENTS_H
