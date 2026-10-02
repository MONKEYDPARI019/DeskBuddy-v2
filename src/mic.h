#ifndef MIC_H
#define MIC_H

// Phase 6 — INMP441 I2S microphone (optional, enable with `set mic_en on`).
//
// Wiring: SCK->GPIO14, WS->GPIO13, SD->GPIO34, L/R->GND (left channel), VDD->3V3.
// While push-to-talk is held, 16 kHz 16-bit mono PCM is streamed to the
// connected app as binary WebSocket frames (see docs/PROTOCOL.md).

#include <Arduino.h>

#define MIC_SAMPLE_RATE 16000

bool     micInit();              // installs I2S1 + task; false if disabled or failed
bool     micAvailable();
void     micStartCapture();      // begin streaming to the app
void     micStopCapture();
bool     micIsCapturing();
uint16_t micPeakLevel();         // 0..32767 peak of the last block (for `test mic`)

#endif // MIC_H
