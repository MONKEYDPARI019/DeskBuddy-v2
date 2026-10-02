# Pinout — DeskBuddy v2 (ESP32 WROOM DevKit)

All pins are defined in one place: `src/config.h`. The v1 ESP8266 pinout is in the [v1 repository](https://github.com/MONKEYDPARI019/DeskBuddy).

| GPIO | Signal | Direction | Notes |
|---|---|---|---|
| 21 | I2C SDA | I/O | SSD1306 OLED at 0x3C, bus at 400 kHz |
| 22 | I2C SCL | Out | |
| 32 | BTN1 | In, `INPUT_PULLUP` | Button to GND. Mood / notification scroll; hold to clear |
| 33 | BTN2 | In, `INPUT_PULLUP` | Button to GND. Next screen; hold for silent mode |
| 27 | BTN3 | In, `INPUT_PULLUP` | Button to GND. Action; hold for push-to-talk |
| 4 | TOUCH (T0) | Touch | Bare pad or copper tape. Calibrate with `test touch` / `set touch_th` |
| 16 | LED red | Out | Silent / DND |
| 17 | LED yellow | Out | Unread notifications |
| 18 | LED green | Out | Blinking = WiFi only, solid = app connected |
| 25 | DAC1 | Analog out | → PAM8403 L in (through ~1 µF cap is fine) |
| 26 | DAC2 | Analog out | → PAM8403 R in |
| 19 | Buzzer | Out (LEDC PWM) | Optional passive buzzer, `set audio buzzer` |
| 14 | MIC SCK | Out | INMP441 BCLK |
| 13 | MIC WS | Out | INMP441 LRCL |
| 34 | MIC SD | In | INMP441 data (34 is input-only, which is fine here) |

INMP441: VDD → 3V3, GND → GND, L/R → GND (left channel).

## Pins to avoid

* **6–11**: connected to the SPI flash. Never use them.
* **34–39**: input-only, with no internal pull-ups.
* **0, 2, 5, 12, 15**: strapping pins. They are left free on purpose. GPIO 12 held high at boot selects 1.8 V flash and stops the board from booting.
* **ADC2 pins** (0, 2, 4, 12–15, 25–27) can't be used as analog inputs while WiFi is on. DeskBuddy only uses them digitally, for touch or for the DAC, which works fine.

## Power

A single USB 5 V supply powers everything. The PAM8403 runs from 5 V, and the OLED and mic run from 3V3. If the speaker makes the OLED flicker or the ESP32 reset at high volume, add a 470 µF capacitor across the PAM8403 supply.
