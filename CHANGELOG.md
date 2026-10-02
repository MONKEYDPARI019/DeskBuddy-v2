# Changelog

All notable changes to DeskBuddy are documented here. This project follows [Semantic Versioning](https://semver.org/) where practical, though early pre-1.0 versions were more experimental than strictly versioned.

---

## [v2.0.0-dev] — ESP32 rewrite

The firmware now runs on a plain ESP32 WROOM, split into modules and FreeRTOS tasks. It compiles cleanly (about 1.1 MB of the 1.75 MB OTA slot) and has been brought up on real hardware, except for the microphone and OTA updates, which are still to be verified.

### Phase 0–1 (committed earlier)
- Ported v1 to ESP32 + PlatformIO; OLED driven as SSD1306 on I2C 21/22
- One `Settings` struct in NVS (Preferences), serial CLI, ArduinoOTA, custom OTA partition table

### Phase 2: tasks + event queue
- New `events` module: one 32-slot queue. Input, network, CLI and audio post events; only the app task in `main.cpp` consumes them
- Tasks: `ui` (30 fps render), `app`, `input`, `cli` on core 1; `net`, `audio`, `mic` on core 0
- Recursive UI mutex (`UiLock`) guards face/display state shared between tasks
- `main.cpp` reduced from ~870 lines of v1 code to setup + app logic; duplicate v1 globals and functions removed

### Phase 3: face state machine
- States: boot → idle ⇄ notify / listening → thinking → speaking / sleep, with timed transitions
- Idle mood (15 moods) is kept separate from the state; short "reactions" (pat → love/cute, BTN3 → wink)
- Talking mouth animation while a reply plays; sleep after `sleep_sec` of inactivity dims the OLED

### Phase 4: buttons + touch
- OneButton click/hold handling on 32/33/27; BTN3 hold = push-to-talk
- Touch pad on GPIO 4 with configurable threshold (`touch_th`) and a 2-sample debounce
- LEDs: red = silent, yellow = unread, green blink = WiFi / solid = app connected

### Phase 5: audio out
- I2S0 in built-in-DAC mode on GPIO 25/26 → PAM8403: synthesized chimes with click-free envelopes, volume control
- Streamed speech playback (16-bit PCM, 8–48 kHz) through a 16 KB stream buffer
- Buzzer backend on GPIO 19 kept as an option (`set audio buzzer`)

### Phase 6: microphone
- INMP441 on I2S1 (SCK 14, WS 13, SD 34), 16 kHz; streams to the app while BTN3 is held; `test mic` level meter

### Phase 7: app link
- WebSocket server on `ws://deskbuddy.local:81/` + mDNS `_deskbuddy._tcp`; JSON protocol in `docs/PROTOCOL.md`
- HTTP `/notify` endpoint kept for curl/Tasker
- `tools/deskbuddy_client.py` PC test client (notify, face, say WAV, push-to-talk echo)
- `android/` Kotlin companion app (notification forwarding with a per-app filter, controls, push-to-talk echo, TTS replies)

### Hardware bring-up and polish
- Tested on a 30-pin ESP32 DevKit: OLED, buttons, touch pad, LEDs, WiFi portal, weather, app link, notification forwarding, speaker and buzzer
- Android app rebuilt as a retro Jetpack Compose UI (Home, Moods, Inbox, Voice, Setup), light/dark theme, adaptive launcher icon
- Mochi faces redrawn as parametric shapes (eyes, lids, pupils, mouths, per-mood extras), checked with the PC simulator in `tools/face-sim`
- New setting `emo_buzzer`: face changes, touch pats and notification chimes play on the buzzer while the speaker keeps the screen click, timer, listen beeps and spoken replies
- Setup-portal reminder on the OLED is shown every 30 s instead of every 2 s
- `default_envs = esp32dev`, so the Upload button flashes over USB only
- Added `hardware/WIRING.md` (breadboard guide) and `hardware/PCB_SCHEMATIC.md` (carrier board netlist)
- v1 documents moved to `docs/archive-v1/`

### Changed / removed
- MQTT/HiveMQ removed (replaced by the WebSocket app link); PubSubClient dropped from `lib_deps`
- Web config portal replaced by WiFiManager (WiFi only) + CLI/app settings
- **Removed the OpenWeatherMap API key from the source.** It was committed in the Phase 1 commit, so rotate that key. It is now set at runtime with `set owm_key`
- New settings: `tz`, `ws_port`, `touch_th`; removed `mqtt_*`, `serverHost`, `serverPort`

---

## [v1.0.0] — Core Experience

The first release considered feature-complete and reliable enough for daily use.

### Added
- Web-based configuration portal (WiFi, weather, MQTT, HTTP settings), backed by LittleFS
- HTTP notification endpoint (`/notify?title=&msg=`) as an MQTT-free alternative, running alongside MQTT without conflict
- MQTT reconnection with exponential backoff (1s base delay, 5-minute ceiling, capped retry count)
- Status LED logic: red (silent mode), yellow (unread notifications), green (WiFi/MQTT state)
- Silent / DND mode, toggled via long-press, persisted to flash

### Fixed
- `loop()` had broken brace structure leaving duplicate, unreachable weather-fetch code
- `MOOD_COUNT` was set to 13 while 15 moods were defined, causing mood cycling to skip the last two
- `drawMouth()` was defined but never called from `drawMochi()`, leaving Mochi's face without a mouth
- `startConfigPortal()` was defined but never invoked from `setup()`, so holding BTN2 at boot had no effect
- PubSubClient's default 256-byte buffer was too small for HiveMQ TLS packets, causing dropped/truncated MQTT messages — fixed with `setBufferSize(1024)`
- Blank MQTT credentials previously caused a silent bail-out instead of a validated, logged failure
- BTN2 boot-time config detection existed but wasn't wired into `setup()`

### Changed
- Notification push logic (MQTT and HTTP) consolidated into a single shared function to remove duplicated buffer-handling code
- Buzzer melodies (`buzzIntro`, `buzzScreenChange`, `buzzEmotion`, `buzzNotification`) — previously composed but never called — wired into boot, button handling, and notification events

---

## [v0.5.0] — Notifications & Configuration

The point where DeskBuddy became more than a clock — real-time notifications and persistent configuration arrived.

### Added
- MQTT client (PubSubClient) with TLS support, connecting to a configurable broker
- MQTT callback parsing JSON payloads (`app`, `title`, `body`) into a circular notification buffer
- Fourth screen: notification history, with BTN1 scrolling through recent messages
- Popup overlay shown for a few seconds on incoming notifications
- `Config` struct backed by LittleFS (`/config.json`), replacing hardcoded WiFi/weather/MQTT values
- WiFiManager integration for captive-portal WiFi setup on first boot

### Changed
- Weather city, API key, and units moved from hardcoded constants into the persisted config struct

### Known Issues (carried into later fixes)
- Config portal existed in code but wasn't reachable from normal boot flow
- Several LED and buzzer functions were written but not yet connected to actual events

---

## [v0.1.0] — First Prototype

The initial working sketch — a proof of concept, not yet a product.

### Added
- OLED initialization and rendering (SH1106, 128×64, I2C)
- Clock screen with NTP-synced time, 12-hour format, and date
- Weather screen with basic OpenWeatherMap integration
- Early version of the Mochi face — static eyes, no physics or mood system yet
- Basic button handling for screen cycling
- Hardcoded WiFi credentials and API keys (no persistent config yet)

---

## Versioning Notes

Given DeskBuddy's history as a personal project before becoming open source, versions prior to v1.0 are reconstructed from development notes rather than tagged releases. Going forward, releases will be tagged in GitHub and this file will be updated as part of the release process — see [`CONTRIBUTING.md`](CONTRIBUTING.md).
