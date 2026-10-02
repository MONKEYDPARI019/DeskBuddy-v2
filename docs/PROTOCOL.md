# DeskBuddy v2 — App Protocol (Phase 7)

The ESP32 runs a **WebSocket server**. The Android app (or `tools/deskbuddy_client.py`) connects to it.

```
ws://deskbuddy.local:81/          (port = setting ws_port, default 81)
```

The device also advertises the mDNS service `_deskbuddy._tcp`, so the app can find it without typing an IP address.

Why the device is the server: phones change IP addresses all the time and Android kills background servers, but a phone can always open an outgoing connection. It also means any number of clients (the phone, a laptop script) can be connected at the same time.

## Frames

| Frame | Direction | Meaning |
|---|---|---|
| **Text** | both | One JSON object with a `"type"` field |
| **Binary** app → device | after `say_start` | Speech to play: **16-bit signed little-endian mono PCM** at the announced rate |
| **Binary** device → app | after `ptt_start` | Mic audio: **16 kHz, 16-bit signed LE mono PCM**, about 512 bytes per frame |

## App → device

| type | fields | effect |
|---|---|---|
| `hello` | `name`? | Device replies with `hello` + `state` |
| `ping` | – | Device replies `{"type":"pong"}` |
| `notify` | `app`, `title`, `body` | Stores the notification, shows the popup, Mochi looks surprised, chime plays |
| `face` | `name` | Mood (`happy`, `love`, `sad`, …) or state (`idle`, `think`, `sleep`, …) |
| `screen` | `name` | `clock` `weather` `mochi` `notify` `pomodoro` or `next` |
| `sound` | `name` | `intro` `screen` `notify` `timer` `purr` `mood` `happy` `sad` `surprised` `listen` `listen_end` `error` `silent_on` `silent_off` |
| `ptt` | `on` (bool) | App's hold-to-talk button, same as holding BTN3 (`true` on press, `false` on release) |
| `think` | `on` (bool) | Show / stop the thinking face while the app works on a reply |
| `say_start` | `rate` (8000–48000, default 16000) | Begin streaming a spoken reply; face goes to *speaking* |
| *(binary)* | PCM | Speech samples. Send at roughly real time; the device buffers about 0.5 s |
| `say_end` | – | No more audio; the device plays what is buffered and then sends `state` |
| `say_abort` | – | Stop speaking immediately |
| `set` | `key`, `value`, `save`? | Change a setting (same keys as the serial `set` command). `save:true` writes it to flash |
| `get` | – | Device replies with `state` |

Example:

```json
{"type":"notify","app":"WhatsApp","title":"Mom","body":"Dinner is ready"}
{"type":"set","key":"volume","value":60,"save":true}
```

## Device → app

| type | fields | when |
|---|---|---|
| `hello` | `device`, `fw`, `mic`, `audio:{rate,format}` | On connect, and in reply to `hello` |
| `state` | `face`, `mood`, `shown` (expression drawn right now), `screen`, `unread`, `silent`, `volume`, `bright`, `sleep`, `mic`, `rssi`, `ip`, `fw`, `owm_city`, plus `temp`, `city`, `wx` once weather is known | After anything that changes it, and every 10 s while an app is connected |
| `button` | `btn` (1–3), `action` (`click` `long` `release`) | Every button event |
| `touch` | – | Touch pad pat |
| `ptt_start` | `rate`, `format` | BTN3 held: binary mic frames follow |
| `ptt_end` | – | BTN3 released: the utterance is complete, the device shows *thinking* |
| `error` | `msg` | Bad JSON, unknown type, unknown setting, … |
| `pong` | – | Reply to `ping` |

## Voice round trip

```
user holds BTN3
  device -> {"type":"ptt_start","rate":16000,"format":"s16le"}
  device -> binary PCM ... binary PCM
user releases BTN3
  device -> {"type":"ptt_end"}                  (face: THINKING, gives up after 20 s)
app: speech-to-text -> assistant -> text-to-speech
  app    -> {"type":"say_start","rate":16000}   (face: SPEAKING)
  app    -> binary PCM ... binary PCM
  app    -> {"type":"say_end"}                  (face back to IDLE when playback finishes)
```

If the app has no answer, it sends `{"type":"think","on":false}` to end the thinking face.

## HTTP (kept from v1)

```
GET  http://deskbuddy.local/notify?app=Gmail&title=New%20mail&msg=Hello
GET  http://deskbuddy.local/          -> {"device":..,"fw":..,"ip":..,"ws_port":..,"clients":..,"heap":..}
```
