#include "audio.h"
#include "config.h"
#include "events.h"
#include <driver/i2s.h>
#include <freertos/stream_buffer.h>
#include <math.h>

#define AUDIO_I2S_PORT     I2S_NUM_0      // only I2S0 can drive the built-in DAC
#define TONE_SAMPLE_RATE   16000
#define STREAM_BUF_BYTES   (16 * 1024)    // ~0.5 s of 16 kHz speech
#define FRAMES_PER_CHUNK   256
#define BUZZER_LEDC_CH     7

struct Note { uint16_t freq; uint16_t ms; };   // freq 0 = rest

// ---------------------------------------------------------------------------
// Sound library — short chirps, deliberately no music
// ---------------------------------------------------------------------------
static const Note S_INTRO[]      = {{659,80},{0,20},{784,80},{0,20},{880,160}};
static const Note S_SCREEN[]     = {{784,30},{0,10},{1047,50}};
static const Note S_NOTIFY[]     = {{880,60},{0,15},{1175,100}};
static const Note S_TIMER[]      = {{1047,150},{0,80},{1047,150},{0,80},{1319,250}};
static const Note S_PURR[]       = {{523,50},{554,50},{587,50},{622,60}};
static const Note S_MOOD[]       = {{988,40},{0,15},{1319,60}};
static const Note S_HAPPY[]      = {{784,50},{988,50},{1175,90}};
static const Note S_SAD[]        = {{659,120},{587,120},{523,200}};
static const Note S_SURPRISED[]  = {{880,40},{1760,90}};
static const Note S_LISTEN[]     = {{1319,60}};
static const Note S_LISTEN_END[] = {{988,60}};
static const Note S_ERROR[]      = {{311,120},{0,40},{233,200}};
static const Note S_SILENT_ON[]  = {{880,60},{659,90}};
static const Note S_SILENT_OFF[] = {{659,60},{880,90}};

struct SoundDef { const char* name; const Note* notes; uint8_t count; };
#define SND(n, arr) {n, arr, (uint8_t)(sizeof(arr) / sizeof(Note))}
static const SoundDef SOUNDS[SOUND_COUNT] = {
    {"none", nullptr, 0},
    SND("intro", S_INTRO), SND("screen", S_SCREEN), SND("notify", S_NOTIFY),
    SND("timer", S_TIMER), SND("purr", S_PURR), SND("mood", S_MOOD),
    SND("happy", S_HAPPY), SND("sad", S_SAD), SND("surprised", S_SURPRISED),
    SND("listen", S_LISTEN), SND("listen_end", S_LISTEN_END), SND("error", S_ERROR),
    SND("silent_on", S_SILENT_ON), SND("silent_off", S_SILENT_OFF),
};

// ---------------------------------------------------------------------------
// Task plumbing
// ---------------------------------------------------------------------------
enum AudioCmdType : uint8_t { CMD_SOUND, CMD_TONE, CMD_STREAM_BEGIN, CMD_STREAM_END, CMD_STREAM_ABORT, CMD_TEST };
struct AudioCmd { AudioCmdType type; uint32_t a; uint32_t b; };

static QueueHandle_t audioQueue = nullptr;
static StreamBufferHandle_t streamBuf = nullptr;
static volatile bool streaming = false;       // app is sending / we are playing speech
static volatile bool streamEnding = false;    // app said "end", drain then stop
static bool dacReady = false;
static uint32_t currentRate = TONE_SAMPLE_RATE;

static int16_t sineTable[256];
static uint16_t frameBuf[FRAMES_PER_CHUNK * 2];   // stereo frames for I2S

// ---------------------------------------------------------------------------
// DAC output helpers (built-in DAC reads the high byte of each 16-bit slot)
// ---------------------------------------------------------------------------
static void dacSetRate(uint32_t rate) {
    if (!dacReady || rate == currentRate) return;
    i2s_set_sample_rates(AUDIO_I2S_PORT, rate);
    currentRate = rate;
}

static inline uint16_t toDac(int32_t s16) {
    int32_t v = (s16 * settings.volume) / 100;          // volume
    v = (v >> 8) + 128;                                  // -> unsigned 8-bit
    if (v < 0) v = 0; else if (v > 255) v = 255;
    return (uint16_t)(v << 8);
}

static void dacWriteFrames(const int16_t* mono, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint16_t d = toDac(mono[i]);
        frameBuf[2 * i] = d;
        frameBuf[2 * i + 1] = d;
    }
    size_t written;
    i2s_write(AUDIO_I2S_PORT, frameBuf, n * 4, &written, portMAX_DELAY);
}

static void dacSilence(uint32_t ms) {
    static int16_t zeros[FRAMES_PER_CHUNK] = {0};
    uint32_t frames = currentRate * ms / 1000;
    while (frames) {
        size_t n = frames > FRAMES_PER_CHUNK ? FRAMES_PER_CHUNK : frames;
        dacWriteFrames(zeros, n);
        frames -= n;
    }
}

static void dacTone(uint16_t freq, uint16_t ms) {
    dacSetRate(TONE_SAMPLE_RATE);
    if (freq == 0) { dacSilence(ms); return; }
    int16_t chunk[FRAMES_PER_CHUNK];
    uint32_t total = (uint32_t)TONE_SAMPLE_RATE * ms / 1000;
    uint32_t phase = 0;
    uint32_t step = (uint32_t)(((uint64_t)freq << 32) / TONE_SAMPLE_RATE);  // phase: top 8 bits index the table
    uint32_t ramp = TONE_SAMPLE_RATE / 200;                              // 5 ms fade in/out: no clicks
    uint32_t done = 0;
    while (done < total) {
        size_t n = (total - done) > FRAMES_PER_CHUNK ? FRAMES_PER_CHUNK : (total - done);
        for (size_t i = 0; i < n; i++, done++) {
            int32_t s = sineTable[(phase >> 24) & 0xFF];
            phase += step;
            if (done < ramp)              s = s * (int32_t)done / (int32_t)ramp;
            else if (total - done < ramp) s = s * (int32_t)(total - done) / (int32_t)ramp;
            chunk[i] = (int16_t)s;
        }
        dacWriteFrames(chunk, n);
    }
}

// ---------------------------------------------------------------------------
// Buzzer backend
// ---------------------------------------------------------------------------
static void buzzerTone(uint16_t freq, uint16_t ms) {
    if (freq) ledcWriteTone(BUZZER_LEDC_CH, freq);
    else      ledcWrite(BUZZER_LEDC_CH, 0);
    vTaskDelay(pdMS_TO_TICKS(ms));
    ledcWrite(BUZZER_LEDC_CH, 0);
}

static void playNote(uint16_t freq, uint16_t ms) {
    if (settings.audioBackend == AUDIO_BACKEND_BUZZER) buzzerTone(freq, ms);
    else if (dacReady)                                 dacTone(freq, ms);
}

// Sounds routed to the buzzer when emo_buzzer is on: Mochi face changes (BTN1, app face icons),
// touch pat, happy/sad/surprised, and the notification chime.
static bool isEmotionSound(SoundId id) {
    return id == SOUND_MOOD || id == SOUND_PURR || id == SOUND_HAPPY ||
           id == SOUND_SAD  || id == SOUND_SURPRISED || id == SOUND_NOTIFY;
}

static void playSound(SoundId id) {
    if (id == SOUND_NONE || id >= SOUND_COUNT) return;
    const SoundDef& s = SOUNDS[id];

    if (settings.emoBuzzer && isEmotionSound(id)) {
        // buzzer only: the speaker/amp stays completely silent for these
        for (uint8_t i = 0; i < s.count; i++) buzzerTone(s.notes[i].freq, s.notes[i].ms);
        return;
    }

    for (uint8_t i = 0; i < s.count; i++) playNote(s.notes[i].freq, s.notes[i].ms);
    if (settings.audioBackend == AUDIO_BACKEND_DAC && dacReady) dacSilence(20);
}

// ---------------------------------------------------------------------------
// Streaming speech
// ---------------------------------------------------------------------------
static void pumpStream() {
    uint8_t raw[FRAMES_PER_CHUNK * 2];
    size_t got = xStreamBufferReceive(streamBuf, raw, sizeof(raw), pdMS_TO_TICKS(20));
    if (got >= 2) {
        if (settings.audioBackend == AUDIO_BACKEND_DAC && dacReady) {
            dacWriteFrames((const int16_t*)raw, got / 2);   // ESP32 is little-endian
        }
        // buzzer backend cannot play speech: data is simply drained
    } else if (streamEnding && xStreamBufferIsEmpty(streamBuf)) {
        streaming = false;
        streamEnding = false;
        if (dacReady) dacSilence(30);
        dacSetRate(TONE_SAMPLE_RATE);
        postEvent(EVT_SPEAK_END, 0);
    }
}

static void audioTask(void*) {
    AudioCmd cmd;
    for (;;) {
        // while speech is playing, poll quickly; otherwise sleep until a command arrives
        TickType_t wait = streaming ? 0 : portMAX_DELAY;
        if (xQueueReceive(audioQueue, &cmd, wait) == pdTRUE) {
            switch (cmd.type) {
                case CMD_SOUND: if (!streaming) playSound((SoundId)cmd.a); break;
                case CMD_TONE:  if (!streaming) playNote((uint16_t)cmd.a, (uint16_t)cmd.b); break;
                case CMD_STREAM_BEGIN:
                    // buffer is already empty (drained or reset when the last stream ended);
                    // not resetting here keeps any bytes that raced ahead of this command
                    dacSetRate(cmd.a);
                    streamEnding = false;
                    streaming = true;
                    break;
                case CMD_STREAM_END:
                    if (streaming) streamEnding = true;
                    break;
                case CMD_STREAM_ABORT:
                    if (streaming) {
                        streaming = false;
                        streamEnding = false;
                        xStreamBufferReset(streamBuf);
                        dacSetRate(TONE_SAMPLE_RATE);
                        postEvent(EVT_SPEAK_END, 1);
                    }
                    break;
                case CMD_TEST:
                    Serial.println(F("[Audio] speaker test: 300 Hz -> 3 kHz sweep"));
                    for (uint16_t f = 300; f <= 3000; f += 150) dacTone(f, 60);
                    dacSilence(20);
                    Serial.println(F("[Audio] speaker test done"));
                    break;
            }
        }
        if (streaming) pumpStream();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void audioInit() {
    for (int i = 0; i < 256; i++) sineTable[i] = (int16_t)(sinf(i * 2.0f * PI / 256.0f) * 30000.0f);

    audioQueue = xQueueCreate(12, sizeof(AudioCmd));
    streamBuf = xStreamBufferCreate(STREAM_BUF_BYTES, 1);

    // Buzzer is always prepared so the backend can be switched at runtime
    ledcSetup(BUZZER_LEDC_CH, 2000, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CH);
    ledcWrite(BUZZER_LEDC_CH, 0);

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = TONE_SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 6;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;     // underrun outputs silence, not a buzz

    if (i2s_driver_install(AUDIO_I2S_PORT, &cfg, 0, nullptr) == ESP_OK) {
        i2s_set_pin(AUDIO_I2S_PORT, nullptr);             // built-in DAC: no external pins
        i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);        // GPIO25 + GPIO26
        dacReady = true;
    } else {
        Serial.println(F("[Audio] I2S DAC init failed, speaker disabled"));
    }

    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 4, nullptr, 0);
    Serial.printf("[Audio] ready, backend=%s\n", settings.audioBackend == AUDIO_BACKEND_DAC ? "dac" : "buzzer");
}

static void send(AudioCmdType t, uint32_t a = 0, uint32_t b = 0) {
    if (!audioQueue) return;
    AudioCmd c = {t, a, b};
    xQueueSend(audioQueue, &c, pdMS_TO_TICKS(5));
}

void audioPlaySound(SoundId sound, bool force) {
    if (settings.silent && !force) return;
    send(CMD_SOUND, sound);
}

bool audioPlaySoundByName(const char* name, bool force) {
    for (uint8_t i = 1; i < SOUND_COUNT; i++) {
        if (!strcasecmp(name, SOUNDS[i].name)) { audioPlaySound((SoundId)i, force); return true; }
    }
    return false;
}

const char* audioSoundName(SoundId s) { return s < SOUND_COUNT ? SOUNDS[s].name : "?"; }

void audioPlayTone(uint16_t freq, uint16_t durationMs, bool force) {
    if (settings.silent && !force) return;
    send(CMD_TONE, freq, durationMs);
}

bool audioStreamBegin(uint32_t sampleRate) {
    if (sampleRate < 8000 || sampleRate > 48000) return false;
    send(CMD_STREAM_BEGIN, sampleRate);
    return true;
}

size_t audioStreamWrite(const uint8_t* data, size_t len) {
    if (!streamBuf) return 0;
    // wait a little for space so a fast sender is paced by playback
    return xStreamBufferSend(streamBuf, data, len, pdMS_TO_TICKS(200));
}

void audioStreamEnd()   { send(CMD_STREAM_END); }
void audioStreamAbort() { send(CMD_STREAM_ABORT); }
bool audioIsStreaming() { return streaming; }
void audioTestSpeaker() { send(CMD_TEST); }
