#ifndef AUDIO_H
#define AUDIO_H

// Phase 5 — audio out: chimes and spoken replies (no music).
//
// Backend 0 (default): ESP32 built-in DAC on GPIO 25/26, driven by I2S0 in
//   DAC mode, into a PAM8403. Plays synthesized chimes and streamed speech.
// Backend 1: passive buzzer on GPIO 19 (chimes only, like v1).
//
// Everything runs in the audio task; the public functions only queue work,
// so they are safe to call from any task and never block for long.

#include <Arduino.h>

enum SoundId : uint8_t {
    SOUND_NONE = 0,
    SOUND_INTRO,
    SOUND_SCREEN,
    SOUND_NOTIFY,
    SOUND_TIMER,
    SOUND_PURR,
    SOUND_MOOD,         // generic mood chirp
    SOUND_HAPPY,
    SOUND_SAD,
    SOUND_SURPRISED,
    SOUND_LISTEN,       // push-to-talk start
    SOUND_LISTEN_END,   // push-to-talk stop
    SOUND_ERROR,
    SOUND_SILENT_ON,
    SOUND_SILENT_OFF,
    SOUND_COUNT
};

void        audioInit();
void        audioPlaySound(SoundId sound, bool force = false);  // force = play even in silent mode
bool        audioPlaySoundByName(const char* name, bool force = false);
const char* audioSoundName(SoundId s);
void        audioPlayTone(uint16_t freq, uint16_t durationMs, bool force = false);

// Streaming speech from the app: 16-bit signed little-endian mono PCM
bool   audioStreamBegin(uint32_t sampleRate);
size_t audioStreamWrite(const uint8_t* data, size_t len);   // returns bytes accepted
void   audioStreamEnd();       // play what is buffered, then post EVT_SPEAK_END
void   audioStreamAbort();
bool   audioIsStreaming();

void   audioTestSpeaker();     // 1 kHz sweep on the DAC

#endif // AUDIO_H
