#include "net.h"
#include "config.h"
#include "events.h"
#include "display.h"
#include "face.h"
#include "audio.h"
#include "mic.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <freertos/semphr.h>
#include <time.h>

static WiFiManager wm;
static WebServer* http = nullptr;
static WebSocketsServer* ws = nullptr;
static SemaphoreHandle_t wsMutex = nullptr;     // arduinoWebSockets is not thread-safe

static volatile NetStatus status = NET_OFFLINE;
static bool servicesStarted = false;
static volatile int appClients = 0;
static uint32_t wxLastFetch = 0;
static volatile bool wxRequested = true;
static bool wasConnected = false;

static const uint32_t WEATHER_PERIOD_MS = 10UL * 60UL * 1000UL;
static const uint32_t WEATHER_RETRY_MS  = 60UL * 1000UL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
struct WsLock {
    WsLock()  { xSemaphoreTakeRecursive(wsMutex, portMAX_DELAY); }
    ~WsLock() { xSemaphoreGiveRecursive(wsMutex); }
};

NetStatus netStatus()        { return status; }
bool      netWifiConnected() { return WiFi.status() == WL_CONNECTED; }
int       netAppClients()    { return appClients; }
void      netRequestWeather(){ wxRequested = true; }

static void setStatus(NetStatus s) {
    if (s == status) return;
    status = s;
    postEvent(EVT_NET_STATUS, s);
}

static void storeNotification(const char* app, const char* title, const char* body) {
    {
        UiLock lock;
        displayPushNotification(app, title, body);
    }
    postEvent(EVT_NOTIFICATION, 0, app);
}

// ---------------------------------------------------------------------------
// Device -> app
// ---------------------------------------------------------------------------
void netSendJson(const char* json) {
    if (!ws || appClients == 0) return;
    WsLock lock;
    ws->broadcastTXT(json);
}

void netSendButton(uint8_t btn, const char* action) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"type\":\"button\",\"btn\":%u,\"action\":\"%s\"}", btn, action);
    netSendJson(buf);
}

void netSendTouch() { netSendJson("{\"type\":\"touch\"}"); }

void netSendPttStart() {
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"type\":\"ptt_start\",\"rate\":%d,\"format\":\"s16le\"}", MIC_SAMPLE_RATE);
    netSendJson(buf);
}

void netSendPttEnd() { netSendJson("{\"type\":\"ptt_end\"}"); }

void netSendState() {
    if (!ws || appClients == 0) return;
    StaticJsonDocument<640> doc;
    {
        UiLock lock;
        doc["type"]   = "state";
        doc["face"]   = faceStateName(faceGetState());
        doc["mood"]   = faceMoodName(faceGetMood());
        doc["shown"]  = faceMoodName(faceShownMood());   // expression drawn right now (for the app's live mirror)
        doc["screen"] = displayScreenName(displayGetScreen());
        doc["unread"] = (int)unreadNotifs;
        if (g_weather.valid) {
            doc["temp"] = g_weather.tempC;
            doc["city"] = g_weather.city;
            doc["wx"]   = g_weather.desc;
        }
    }
    doc["silent"] = settings.silent;
    doc["volume"] = settings.volume;
    doc["bright"] = settings.brightness;
    doc["sleep"]  = settings.sleepTimeoutSec;
    doc["owm_city"] = settings.owm_city;
    doc["mic"]    = micAvailable();
    doc["rssi"]   = WiFi.RSSI();
    doc["ip"]     = WiFi.localIP().toString();
    doc["fw"]     = FW_VERSION;
    char buf[640];
    serializeJson(doc, buf, sizeof(buf));
    netSendJson(buf);
}

void netSendAudio(const uint8_t* pcm, size_t len) {
    if (!ws || appClients == 0) return;
    WsLock lock;
    ws->broadcastBIN(pcm, len);
}

// ---------------------------------------------------------------------------
// App -> device (JSON text frames, binary = speech PCM)
// ---------------------------------------------------------------------------
static void sendHello(uint8_t num) {
    StaticJsonDocument<256> doc;
    doc["type"]  = "hello";
    doc["device"]= FW_NAME;
    doc["fw"]    = FW_VERSION;
    doc["mic"]   = micAvailable();
    JsonObject a = doc.createNestedObject("audio");
    a["rate"]    = MIC_SAMPLE_RATE;
    a["format"]  = "s16le";
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    ws->sendTXT(num, buf);
}

static void sendError(uint8_t num, const char* msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"error\",\"msg\":\"%s\"}", msg);
    ws->sendTXT(num, buf);
}

static void handleAppMessage(uint8_t num, const char* text, size_t len) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, text, len)) { sendError(num, "bad json"); return; }
    const char* type = doc["type"] | "";

    if (!strcmp(type, "hello")) {
        sendHello(num);
        netSendState();
    } else if (!strcmp(type, "ping")) {
        ws->sendTXT(num, "{\"type\":\"pong\"}");
    } else if (!strcmp(type, "notify")) {
        storeNotification(doc["app"] | "Phone", doc["title"] | "Notification", doc["body"] | "");
    } else if (!strcmp(type, "face")) {
        postEvent(EVT_FACE_CMD, 0, doc["name"] | "");
    } else if (!strcmp(type, "screen")) {
        ScreenMode s;
        const char* name = doc["name"] | "next";
        if (!strcmp(name, "next")) postEvent(EVT_SCREEN_CMD, -1);
        else if (displayScreenByName(name, &s)) postEvent(EVT_SCREEN_CMD, s);
        else sendError(num, "unknown screen");
    } else if (!strcmp(type, "sound")) {
        postEvent(EVT_SOUND_CMD, 0, doc["name"] | "");
    } else if (!strcmp(type, "ptt")) {
        // app's hold-to-talk button: same as holding BTN3 on the device
        postEvent(EVT_MIC_TRIGGER, (doc["on"] | false) ? 1 : 0);
    } else if (!strcmp(type, "think")) {
        postEvent(EVT_THINKING, (doc["on"] | true) ? 1 : 0);
    } else if (!strcmp(type, "say_start")) {
        if (audioStreamBegin(doc["rate"] | 16000)) postEvent(EVT_SPEAK_START, 0);
        else sendError(num, "bad sample rate");
    } else if (!strcmp(type, "say_end")) {
        audioStreamEnd();
    } else if (!strcmp(type, "say_abort")) {
        audioStreamAbort();
    } else if (!strcmp(type, "set")) {
        const char* key = doc["key"] | "";
        char val[48];
        if (doc["value"].is<const char*>()) strlcpy(val, doc["value"].as<const char*>(), sizeof(val));
        else if (doc["value"].is<bool>())   strlcpy(val, doc["value"].as<bool>() ? "1" : "0", sizeof(val));
        else                                snprintf(val, sizeof(val), "%ld", (long)(doc["value"] | 0L));
        if (settingsSet(key, val)) {
            if (doc["save"] | false) settingsSave();
            postEvent(EVT_SETTING_CHANGED, 0, key);
        } else {
            sendError(num, "unknown setting");
        }
    } else if (!strcmp(type, "get")) {
        netSendState();
    } else {
        sendError(num, "unknown type");
    }
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED:
            appClients = ws->connectedClients();
            Serial.printf("[WS] client %u connected (%d total)\n", num, (int)appClients);
            sendHello(num);
            postEvent(EVT_APP_LINK, appClients);
            break;
        case WStype_DISCONNECTED:
            appClients = ws->connectedClients();
            Serial.printf("[WS] client %u left (%d total)\n", num, (int)appClients);
            postEvent(EVT_APP_LINK, appClients);
            if (appClients == 0 && audioIsStreaming()) audioStreamAbort();
            break;
        case WStype_TEXT:
            handleAppMessage(num, (const char*)payload, len);
            break;
        case WStype_BIN:
            if (audioIsStreaming()) audioStreamWrite(payload, len);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// HTTP (kept from v1 so Tasker/Automate/curl still work)
//   GET /notify?app=WhatsApp&title=Mom&msg=Dinner
//   GET /        -> small JSON status
// ---------------------------------------------------------------------------
static void handleNotify() {
    if (!settings.http_enabled) { http->send(403, "text/plain", "HTTP notifications disabled"); return; }
    String app   = http->hasArg("app")   ? http->arg("app")   : "HTTP";
    String title = http->hasArg("title") ? http->arg("title") : "Notification";
    String msg   = http->hasArg("msg")   ? http->arg("msg")   : (http->hasArg("body") ? http->arg("body") : "");
    storeNotification(app.c_str(), title.c_str(), msg.c_str());
    http->send(200, "text/plain", "OK");
}

static void handleRoot() {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"device\":\"%s\",\"fw\":\"%s\",\"ip\":\"%s\",\"ws_port\":%u,\"clients\":%d,\"heap\":%u}",
             FW_NAME, FW_VERSION, WiFi.localIP().toString().c_str(), settings.ws_port, (int)appClients, ESP.getFreeHeap());
    http->send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
static void fetchWeather() {
    wxRequested = false;
    wxLastFetch = millis();
    if (!netWifiConnected() || settings.owm_key[0] == '\0') return;

    WiFiClient client;
    HTTPClient req;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=";
    url += settings.owm_city;
    url += "&appid=";
    url += settings.owm_key;
    url += "&units=";
    url += settings.owm_units;

    req.setTimeout(5000);
    req.begin(client, url);
    int code = req.GET();
    bool ok = false;
    if (code == 200) {
        StaticJsonDocument<256> filter;
        filter["name"] = true;
        filter["weather"][0]["description"] = true;
        filter["weather"][0]["icon"] = true;
        filter["main"]["temp"] = true;
        filter["main"]["feels_like"] = true;
        filter["main"]["humidity"] = true;
        filter["wind"]["speed"] = true;

        StaticJsonDocument<512> doc;
        if (!deserializeJson(doc, req.getStream(), DeserializationOption::Filter(filter))) {
            WeatherData w = {};
            strlcpy(w.city, doc["name"] | settings.owm_city, sizeof(w.city));
            strlcpy(w.desc, doc["weather"][0]["description"] | "", sizeof(w.desc));
            strlcpy(w.icon, doc["weather"][0]["icon"] | "01d", sizeof(w.icon));
            w.tempC  = (int)roundf(doc["main"]["temp"] | 0.0f);
            w.feelsC = (int)roundf(doc["main"]["feels_like"] | 0.0f);
            w.humid  = doc["main"]["humidity"] | 0;
            w.windMs = doc["wind"]["speed"] | 0.0f;
            w.valid  = true;
            { UiLock lock; g_weather = w; }
            ok = true;
        }
    } else {
        Serial.printf("[Weather] HTTP %d\n", code);
    }
    req.end();
    if (!ok) wxLastFetch = millis() - WEATHER_PERIOD_MS + WEATHER_RETRY_MS;   // retry in a minute
    postEvent(EVT_WEATHER_UPDATE, ok ? 1 : 0);
}

// ---------------------------------------------------------------------------
// OTA
// ---------------------------------------------------------------------------
static void setupOta() {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.onStart([]() {
        audioStreamAbort();
        postEvent(EVT_OTA, 0);
        UiLock lock;
        displaySetOtaProgress(0);
        displayRender();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int last = -1;
        int pct = total ? (int)(progress * 100ULL / total) : 0;
        if (pct == last) return;
        last = pct;
        UiLock lock;
        displaySetOtaProgress(pct);
        displayRender();          // the UI task may be starved during flashing
    });
    ArduinoOTA.onEnd([]() {
        UiLock lock;
        displayShowMessage("OTA done", "Rebooting...");
        displaySetOtaProgress(-1);
        displayRender();
    });
    ArduinoOTA.onError([](ota_error_t e) {
        Serial.printf("[OTA] error %u\n", e);
        { UiLock lock; displaySetOtaProgress(-1); displayShowMessage("OTA failed", "see serial log"); }
        postEvent(EVT_OTA, -1);
    });
    ArduinoOTA.begin();
}

// ---------------------------------------------------------------------------
// Service startup once WiFi is up
// ---------------------------------------------------------------------------
static void startServices() {
    if (servicesStarted) return;
    servicesStarted = true;

    configTzTime(settings.tz, "pool.ntp.org", "time.google.com");

    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", settings.http_port);
        MDNS.addService("deskbuddy", "tcp", settings.ws_port);   // app discovers _deskbuddy._tcp
    }

    setupOta();

    http = new WebServer(settings.http_port);
    http->on("/", HTTP_GET, handleRoot);
    http->on("/notify", HTTP_GET, handleNotify);
    http->on("/notify", HTTP_POST, handleNotify);
    http->begin();

    ws = new WebSocketsServer(settings.ws_port);
    ws->onEvent(onWsEvent);
    ws->enableHeartbeat(15000, 3000, 2);     // drop dead phone connections
    ws->begin();

    Serial.printf("[Net] http://%s.local:%u  ws://%s.local:%u  (%s)\n",
                  HOSTNAME, settings.http_port, HOSTNAME, settings.ws_port, WiFi.localIP().toString().c_str());
}

void netResetWifi() {
    wm.resetSettings();
    delay(200);
    ESP.restart();
}

static void netTask(void*) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);                       // lower latency for audio streaming

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(0);
    wm.setConnectTimeout(20);
    if (wm.autoConnect("DeskBuddy_Setup")) {
        setStatus(NET_CONNECTED);
    } else {
        Serial.println(F("[Net] no saved WiFi: join AP 'DeskBuddy_Setup' and open 192.168.4.1"));
        setStatus(NET_PORTAL);
    }

    for (;;) {
        if (status == NET_PORTAL) {
            wm.process();
            if (netWifiConnected()) {
                // free port 80 and the AP before our own servers start
                if (wm.getConfigPortalActive()) wm.stopConfigPortal();
                WiFi.mode(WIFI_STA);
                setStatus(NET_CONNECTED);
            }
        }

        bool connected = netWifiConnected();
        if (connected && !servicesStarted) startServices();
        if (connected != wasConnected) {
            wasConnected = connected;
            if (status != NET_PORTAL) setStatus(connected ? NET_CONNECTED : NET_OFFLINE);
            if (connected) wxRequested = true;
        }

        if (servicesStarted) {
            ArduinoOTA.handle();
            http->handleClient();
            {
                WsLock lock;
                ws->loop();
            }
            if (connected && (wxRequested || millis() - wxLastFetch > WEATHER_PERIOD_MS)) fetchWeather();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void netInit() {
    wsMutex = xSemaphoreCreateRecursiveMutex();
    xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 2, nullptr, 0);
}
