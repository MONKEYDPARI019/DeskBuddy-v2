#include "config.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;
static const char* PREFS_NAMESPACE = "deskbuddy";

// No secrets in source: API keys are entered at runtime (`set owm_key ...`).
static const Settings DEFAULT_SETTINGS = {
    "",             // owm_key
    "Bengaluru",    // owm_city
    "metric",       // owm_units
    "IST-5:30",     // tz
    true,           // http_enabled
    80,             // http_port
    81,             // ws_port
    false,          // silent
    220,            // brightness
    80,             // volume
    300,            // sleepTimeoutSec
    25,             // pomodoroMin
    40,             // touchThreshold
    AUDIO_BACKEND_DAC,
    false,          // micEnabled
    false           // emoBuzzer
};

void settingsResetDefaults() {
    memcpy(&settings, &DEFAULT_SETTINGS, sizeof(Settings));
}

void settingsInit() {
    settingsLoad();
}

void settingsLoad() {
    settingsResetDefaults();

    if (!prefs.begin(PREFS_NAMESPACE, true)) {
        Serial.println(F("[Settings] No saved settings yet, using defaults"));
        return;
    }

    prefs.getString("owm_key", settings.owm_key, sizeof(settings.owm_key));
    prefs.getString("owm_city", settings.owm_city, sizeof(settings.owm_city));
    prefs.getString("owm_units", settings.owm_units, sizeof(settings.owm_units));
    prefs.getString("tz", settings.tz, sizeof(settings.tz));

    settings.http_enabled   = prefs.getBool("http_en", settings.http_enabled);
    settings.http_port      = prefs.getUShort("http_port", settings.http_port);
    settings.ws_port        = prefs.getUShort("ws_port", settings.ws_port);

    settings.silent          = prefs.getBool("silent", settings.silent);
    settings.brightness      = prefs.getUChar("bright", settings.brightness);
    settings.volume          = prefs.getUChar("vol", settings.volume);
    settings.sleepTimeoutSec = prefs.getUInt("sleep_sec", settings.sleepTimeoutSec);
    settings.pomodoroMin     = prefs.getUShort("pomo_min", settings.pomodoroMin);
    settings.touchThreshold  = prefs.getUShort("touch_th", settings.touchThreshold);

    settings.audioBackend = prefs.getUChar("audio_bk", settings.audioBackend);
    settings.micEnabled   = prefs.getBool("mic_en", settings.micEnabled);
    settings.emoBuzzer    = prefs.getBool("emo_bz", settings.emoBuzzer);

    prefs.end();
    Serial.println(F("[Settings] Loaded from NVS"));
}

void settingsSave() {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println(F("[Settings] Failed to open NVS for writing"));
        return;
    }

    prefs.putString("owm_key", settings.owm_key);
    prefs.putString("owm_city", settings.owm_city);
    prefs.putString("owm_units", settings.owm_units);
    prefs.putString("tz", settings.tz);

    prefs.putBool("http_en", settings.http_enabled);
    prefs.putUShort("http_port", settings.http_port);
    prefs.putUShort("ws_port", settings.ws_port);

    prefs.putBool("silent", settings.silent);
    prefs.putUChar("bright", settings.brightness);
    prefs.putUChar("vol", settings.volume);
    prefs.putUInt("sleep_sec", settings.sleepTimeoutSec);
    prefs.putUShort("pomo_min", settings.pomodoroMin);
    prefs.putUShort("touch_th", settings.touchThreshold);

    prefs.putUChar("audio_bk", settings.audioBackend);
    prefs.putBool("mic_en", settings.micEnabled);
    prefs.putBool("emo_bz", settings.emoBuzzer);

    prefs.end();
    Serial.println(F("[Settings] Saved to NVS"));
}

static bool parseBool(const char* v) {
    return !strcasecmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "on") || !strcasecmp(v, "yes");
}

// Single place that knows how to change a setting by name.
// Used by the serial CLI and the WebSocket "set" message.
bool settingsSet(const char* key, const char* val) {
    if (!key || !val) return false;

    if (!strcasecmp(key, "owm_key"))        strlcpy(settings.owm_key, val, sizeof(settings.owm_key));
    else if (!strcasecmp(key, "owm_city"))  strlcpy(settings.owm_city, val, sizeof(settings.owm_city));
    else if (!strcasecmp(key, "owm_units")) strlcpy(settings.owm_units, val, sizeof(settings.owm_units));
    else if (!strcasecmp(key, "tz"))        strlcpy(settings.tz, val, sizeof(settings.tz));
    else if (!strcasecmp(key, "http_en"))   settings.http_enabled = parseBool(val);
    else if (!strcasecmp(key, "http_port")) settings.http_port = (uint16_t)atoi(val);
    else if (!strcasecmp(key, "ws_port"))   settings.ws_port = (uint16_t)atoi(val);
    else if (!strcasecmp(key, "silent"))    settings.silent = parseBool(val);
    else if (!strcasecmp(key, "brightness") || !strcasecmp(key, "bright"))
        settings.brightness = (uint8_t)constrain(atoi(val), 0, 255);
    else if (!strcasecmp(key, "volume") || !strcasecmp(key, "vol"))
        settings.volume = (uint8_t)constrain(atoi(val), 0, 100);
    else if (!strcasecmp(key, "sleep_sec"))    settings.sleepTimeoutSec = (uint32_t)atol(val);
    else if (!strcasecmp(key, "pomodoro_min") || !strcasecmp(key, "pomo_min"))
        settings.pomodoroMin = (uint16_t)constrain(atoi(val), 1, 180);
    else if (!strcasecmp(key, "touch_th"))     settings.touchThreshold = (uint16_t)atoi(val);
    else if (!strcasecmp(key, "audio") || !strcasecmp(key, "audio_backend"))
        settings.audioBackend = (!strcasecmp(val, "buzzer") || !strcmp(val, "1")) ? AUDIO_BACKEND_BUZZER : AUDIO_BACKEND_DAC;
    else if (!strcasecmp(key, "mic") || !strcasecmp(key, "mic_en")) settings.micEnabled = parseBool(val);
    else if (!strcasecmp(key, "emo_buzzer") || !strcasecmp(key, "emo_bz")) settings.emoBuzzer = parseBool(val);
    else return false;

    return true;
}

void settingsPrint() {
    Serial.println(F("\n--- Settings ---"));
    Serial.printf("owm_key       %s\n", strlen(settings.owm_key) ? "(set)" : "(empty)");
    Serial.printf("owm_city      %s\n", settings.owm_city);
    Serial.printf("owm_units     %s\n", settings.owm_units);
    Serial.printf("tz            %s\n", settings.tz);
    Serial.printf("http_en       %s\n", settings.http_enabled ? "true" : "false");
    Serial.printf("http_port     %u\n", settings.http_port);
    Serial.printf("ws_port       %u\n", settings.ws_port);
    Serial.printf("silent        %s\n", settings.silent ? "true" : "false");
    Serial.printf("brightness    %u\n", settings.brightness);
    Serial.printf("volume        %u\n", settings.volume);
    Serial.printf("sleep_sec     %u\n", settings.sleepTimeoutSec);
    Serial.printf("pomodoro_min  %u\n", settings.pomodoroMin);
    Serial.printf("touch_th      %u\n", settings.touchThreshold);
    Serial.printf("audio         %s\n", settings.audioBackend == AUDIO_BACKEND_DAC ? "dac" : "buzzer");
    Serial.printf("mic_en        %s\n", settings.micEnabled ? "true" : "false");
    Serial.printf("emo_buzzer    %s\n", settings.emoBuzzer ? "true" : "false");
    Serial.println(F("----------------\n"));
}
