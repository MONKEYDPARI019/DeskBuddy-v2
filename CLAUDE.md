# DeskBuddy v2

ESP32 desk companion with an animated face called Mochi.
Framework: PlatformIO + Arduino (espressif32@6.9.0 = Arduino-ESP32 2.0.17), C++.
This is v2, a rebuild of the ESP8266 version (kept in its own repo, github.com/MONKEYDPARI019/DeskBuddy). Do not port v1 code back.

## Hardware
- ESP32 WROOM DevKit (plain ESP32, NOT S3 or C3)
- SSD1306 0.96" OLED 128x64, I2C, SDA=21 SCL=22, U8g2 library
  (CLAUDE.md used to say SH1106; the module on hand is an SSD1306)
- 3 push buttons: GPIO 32, 33, 27, wired to GND, INPUT_PULLUP
- Capacitive touch pad: GPIO 4
- LEDs: 16 red (silent), 17 yellow (unread), 18 green (net)
- Audio out: built-in DAC on GPIO 25/26 (I2S0 DAC mode) into a PAM8403; optional buzzer on 19
- Mic: INMP441 on I2S1 — SCK 14, WS 13, SD 34
- Reserved, do not use: GPIO 6-11 (flash). GPIO 34-39 are input-only.
- All pins are defined only in src/config.h

## File layout
main.cpp (tasks + app logic), config.h/cpp, events.h/cpp, cli.cpp,
display.cpp, face.cpp, sensors.cpp, audio.cpp, mic.cpp, net.cpp
docs/PROTOCOL.md (app protocol), android/ (companion app), tools/ (PC test client)

## Build phases
0. Port to ESP32, OLED unchanged                       done
1. Settings struct + Preferences + serial CLI + OTA    done
2. FreeRTOS task split with an event queue             done
3. Face state machine                                  done
4. Buttons and touch                                   done
5. Audio out (chimes and spoken replies, no music)     done
6. Microphone                                          done (push-to-talk streaming)
7. WebSocket protocol + Android app                    done: retro Compose app (android/) tested on phone
Tested on hardware: OLED, face, CLI, WiFi, weather, app link + notifications.
Not yet tested: buttons, touch, LEDs, speaker, INMP441 mic (parts not wired / mic not bought yet).

## Architecture rules
- Only the app task (main.cpp) consumes g_eventQueue; other modules post events.
- Face/display state is protected by the UI lock (UiLock in display.h).
- Never call net* send functions while holding the UI lock (lock-order: WS lock may take UI lock, not the reverse).
- Audio functions only enqueue work for the audio task.

## Rules
- One phase at a time. Do not build ahead.
- All settings live in one struct in config.h. No scattered globals.
- No secrets in source: API keys are set at runtime via `set owm_key ...`.
- Explain what you changed and why after each task.
