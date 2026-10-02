# DeskBuddy Roadmap

This roadmap tracks where DeskBuddy has been and where it's headed. It's a living document — if you're picking up a [good first issue](../GOOD_FIRST_ISSUES.md) or proposing something new, feel free to open a PR against this file too.

Status legend: ✅ Shipped · 🚧 In Progress · 📋 Planned

---

## v2.0 — ESP32 rewrite *(in progress, branch `v2`)*

- ✅ Port to ESP32 WROOM + PlatformIO
- ✅ Settings in NVS, serial CLI, OTA updates
- ✅ FreeRTOS task split with an event queue
- ✅ Face state machine (listening / thinking / speaking / sleep)
- ✅ Debounced buttons + capacitive touch pad
- ✅ Speaker out via built-in DAC + PAM8403 (chimes and spoken replies)
- ✅ INMP441 microphone, push-to-talk streaming
- ✅ WebSocket protocol + mDNS discovery
- 🚧 Android companion app (scaffold written, needs first build + testing)
- 🚧 Hardware bring-up and testing of phases 2–7
- 📋 Speech-to-text + assistant behind the app's voice loop

---

## v1.0 — Core Experience *(current)*

The baseline: a reliable desk companion that replaces phone glances for time, weather, and notifications.

- ✅ Clock screen with NTP sync and minute progress bar
- ✅ Weather screen via OpenWeatherMap, with pixel-art icons
- ✅ Mochi face with 15 moods, physics-based eyes, and idle animation
- ✅ MQTT notifications over TLS (HiveMQ Cloud) with exponential-backoff reconnection
- ✅ HTTP notification endpoint as an MQTT-free alternative
- ✅ Web config portal (WiFi, weather, MQTT, HTTP settings) backed by LittleFS
- ✅ Status LEDs for silent mode, unread notifications, and WiFi/MQTT state
- ✅ Buzzer feedback for boot, screen changes, moods, and notifications
- ✅ Silent / DND mode

---

## v1.1 — Polish & Reliability

Small, high-value improvements that make the existing feature set feel finished.

- 📋 Particle effects refinement (sparkles, hearts, sleep "Z"s) across all moods
- 📋 Mochi physics tuning — smoother saccades, more natural blink timing
- 📋 Notification popup animation (slide-in / fade instead of hard cut)
- 📋 Config portal input validation (reject malformed MQTT ports, empty required fields)
- 📋 MQTT and WiFi edge-case testing (broker unreachable, malformed JSON payloads, WiFi flapping)
- 📋 Battery-friendly display dimming after inactivity
- 📋 Improved Serial diagnostics for first-time setup debugging

---

## v2.x — Standalone Hardware

Moving DeskBuddy from "wired to a breadboard" to a purpose-built, self-contained device.

- 📋 Custom single-sided PCB (KiCad), fabricated on standard copper-clad board with no wire bridges
- 📋 Battery monitoring and optional battery-powered operation
- ✅ OTA firmware updates — no USB required after initial flash (v2.0)
- 📋 3D-printable enclosure design
- 📋 Optional buzzer volume / mute hardware switch

---

## v3.0 — Platform Expansion

Growing beyond a single-board, single-integration project.

- ✅ ESP32 support (done in v2.0)
- 📋 Additional notification sources beyond MacroDroid: Telegram bot, Discord webhook
- 🚧 Companion mobile app (android/ in v2.0)
- 📋 Multi-device sync (shared notification state across more than one DeskBuddy)
- 📋 Plugin-style screen system so community members can contribute new display modes without touching core firmware

---

## Not Planned (for now)

To keep DeskBuddy focused, the following are intentionally out of scope unless community demand changes that:

- Full smartwatch-style app integrations (calendar, email clients, etc.)
- Touchscreen support
- Voice assistant integration

If you disagree with something on this list, open an issue — roadmaps are meant to be argued with.
