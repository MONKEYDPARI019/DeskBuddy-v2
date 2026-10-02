#include "mic.h"
#include "config.h"
#include "net.h"
#include <driver/i2s.h>

#define MIC_I2S_PORT   I2S_NUM_1      // I2S0 is busy driving the DAC
#define MIC_BLOCK      256            // samples per read (16 ms)
#define MIC_GAIN_SHIFT 14             // 32-bit INMP441 word -> 16-bit PCM (~+6 dB over >>16 is fine for speech)

static bool ready = false;
static volatile bool capturing = false;
static volatile uint16_t peak = 0;

static void micTask(void*) {
    static int32_t raw[MIC_BLOCK];
    static int16_t pcm[MIC_BLOCK];
    for (;;) {
        size_t bytes = 0;
        if (i2s_read(MIC_I2S_PORT, raw, sizeof(raw), &bytes, pdMS_TO_TICKS(100)) != ESP_OK || bytes == 0) continue;
        size_t n = bytes / 4;
        uint16_t p = 0;
        for (size_t i = 0; i < n; i++) {
            int32_t s = raw[i] >> MIC_GAIN_SHIFT;
            if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
            pcm[i] = (int16_t)s;
            uint16_t a = (uint16_t)abs(s);
            if (a > p) p = a;
        }
        peak = p;
        if (capturing) netSendAudio((const uint8_t*)pcm, n * 2);
    }
}

bool micInit() {
    if (!settings.micEnabled) {
        Serial.println(F("[Mic] disabled (set mic_en on + reboot to enable)"));
        return false;
    }
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = MIC_SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = MIC_BLOCK;
    cfg.use_apll = false;

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = PIN_MIC_SCK;
    pins.ws_io_num = PIN_MIC_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = PIN_MIC_SD;

    if (i2s_driver_install(MIC_I2S_PORT, &cfg, 0, nullptr) != ESP_OK ||
        i2s_set_pin(MIC_I2S_PORT, &pins) != ESP_OK) {
        Serial.println(F("[Mic] I2S init failed"));
        return false;
    }
    ready = true;
    xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 3, nullptr, 0);
    Serial.println(F("[Mic] INMP441 ready on I2S1 (SCK 14, WS 13, SD 34)"));
    return true;
}

bool     micAvailable()   { return ready; }
void     micStartCapture(){ if (ready) capturing = true; }
void     micStopCapture() { capturing = false; }
bool     micIsCapturing() { return capturing; }
uint16_t micPeakLevel()   { return peak; }
