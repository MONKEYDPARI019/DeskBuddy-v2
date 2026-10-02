# DeskBuddy Android app (Phase 7)

This is a small Kotlin app that connects to DeskBuddy over the WebSocket protocol described in [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md).

> **v0.2 — retro UI.** Jetpack Compose screens (Home, Moods, Inbox, Voice, Setup) matching the DeskBuddy Retro App mockup; light/dark/auto theme; pixel fonts Press Start 2P + VT323 (SIL OFL, see `FONTS-OFL.txt`).
>
> **Status:** builds in Android Studio and runs on a Samsung Galaxy A34 (notifications, controls, theme, icon tested).

## What it does

| Feature | Where |
|---|---|
| Connects to `ws://deskbuddy.local:81/` and reconnects automatically with backoff | `DeskBuddyClient.kt` |
| Finds the device on the LAN via mDNS (`_deskbuddy._tcp`) with the **Find** button | `MainActivity.kt` |
| Keeps the link alive in the background as a foreground service | `LinkService.kt` |
| Forwards phone notifications (replaces the v1 Automate/HTTP setup) | `NotifListener.kt` |
| Push-to-talk: receives mic audio from BTN3, saves `last_ptt.wav`, and answers | `VoiceLoop.kt` |
| Makes Mochi speak typed text using Android TTS, streamed as PCM to the DAC speaker | `TtsPcm.kt` |
| Buttons for faces, screens and volume, plus a test notification | `MainActivity.kt` |

## Build and run

1. In Android Studio, choose **File → Open** and select this `android/` folder. It generates the Gradle wrapper and syncs.
2. Run the app on your phone. The phone must be on the same WiFi as DeskBuddy.
3. Tap **Find** (or type the IP shown by `status` on the serial CLI), then tap **Connect**.
4. Tap **Grant notification access** and enable DeskBuddy.
5. For the voice test, turn on **Voice echo test**, hold BTN3 on the device, talk, and release. Mochi repeats what you said. This checks the mic, WiFi and speaker end to end.

## Hooking up a real assistant

`VoiceLoop.answer()` receives the 16 kHz PCM of each utterance. Replace the echo/TTS placeholder with:

1. speech-to-text on the PCM (an on-device model such as Vosk or whisper.cpp, or a cloud API),
2. your assistant or LLM call,
3. `tts.speak(replyText)`, which streams the spoken reply to DeskBuddy.

While step 2 runs, you can send `DeskBuddyClient.think(true)` so Mochi shows the thinking face. The device already switches to thinking when you release BTN3.
