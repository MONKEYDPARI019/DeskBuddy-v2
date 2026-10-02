# Wiring: DeskBuddy v2 on a breadboard

For an ESP32 WROOM **30-pin DevKit**. On this board the pins are printed as `D21`, `D22` and so on, which are the GPIO numbers. Two are labelled differently: **GPIO16 is `RX2`** and **GPIO17 is `TX2`**. The full pin list with notes is in [PINOUT.md](PINOUT.md).

Everything runs from the board's USB power. Join all the grounds together on one GND rail.

## 1. OLED (SSD1306, I2C)

| OLED pin | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | D21 |
| SCL | D22 |

Check with `test oled`. If the screen stays blank, the I2C address may differ (the firmware expects 0x3C).

## 2. Buttons (3)

Each button joins its pin to GND. The firmware turns on the internal pull-ups, so no resistors are needed.

| Button | ESP32 |
|---|---|
| BTN1 | D32, other leg to GND |
| BTN2 | D33, other leg to GND |
| BTN3 | D27, other leg to GND |

Check with `test buttons`.

## 3. LEDs (3)

Long leg (anode) goes to the resistor, short leg (cathode) goes to GND. Use about 330 Ω.

| LED | ESP32 | Meaning |
|---|---|---|
| Red | RX2 (GPIO16) | silent mode |
| Yellow | TX2 (GPIO17) | unread notifications |
| Green | D18 | network |

Check with `test leds`: red, yellow, green, then all on, then off.

## 4. Touch pad

Wire one bare conductor to **D4**: copper tape, a coin, or a paper clip. Nothing else is needed. Tune the sensitivity with `test touch` and `set touch_th <value>`.

## 5. Speaker (PAM8403 amplifier)

| PAM8403 pin | Connect to |
|---|---|
| +5V | ESP32 **VIN** (5 V from USB) |
| GND (power side) | ESP32 GND, the pin next to VIN |
| L (input) | ESP32 **D25**, through a 10 kΩ to 22 kΩ resistor |
| G (input ground) | ESP32 GND, with a short wire |
| L+ and L− | the speaker's two wires |

- Leave R+, R− and the R input empty if you use one speaker.
- Never connect the speaker's minus wire to GND. The amplifier outputs are bridged.
- A 0.5 W speaker can be overdriven. Keep `set volume` at 60 or lower, or use a larger input resistor and a higher volume.
- For a hum or buzz, put a 100 to 470 µF capacitor across the PAM8403 5 V and GND.
- For hiss, use a larger input resistor (22 kΩ to 47 kΩ) and raise the volume to compensate.

Check with `test speaker` (a 300 Hz to 3 kHz sweep).

## 6. Buzzer (optional, for emotion sounds)

Use a **passive** buzzer. An active one plays only a single fixed tone.

| Buzzer | ESP32 |
|---|---|
| + | D19, through a resistor (100 Ω to 470 Ω) |
| − | GND |

Check with `test buzzer`. Then turn on emotion routing:

```
set emo_buzzer on
save
```

Face changes, touch pats and notification chimes then play on the buzzer. The speaker is used for the screen click, timer, listen beeps and spoken replies.

## 7. Microphone (optional, INMP441)

| INMP441 pin | ESP32 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SCK | D14 |
| WS | D13 |
| SD | D34 |
| L/R | GND (left channel) |

Enable it, then reboot:

```
set mic_en on
save
reboot
```

Check with `test mic`, then hold BTN3 to talk.

## Power notes

- VIN is about 4.5 to 4.8 V when powered from USB, which the PAM8403 accepts.
- If the board resets or the OLED flickers at high volume, use a stronger USB port or charger, lower the volume, or add a 470 µF capacitor on the amplifier supply.
- For a permanent build, see [PCB_SCHEMATIC.md](PCB_SCHEMATIC.md) (USB-C power input, parts list).
