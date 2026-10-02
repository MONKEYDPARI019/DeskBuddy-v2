# DeskBuddy v2 — Carrier PCB schematic (rev A)

This PCB is a carrier board. The ESP32 DevKit, OLED, PAM8403 and INMP441 are modules that plug into headers. Buttons, LEDs, resistors and capacitors solder directly to the board. The speaker connects through a 2-pin connector.

Every GPIO below matches `src/config.h`, so the firmware needs **no changes**.

> ⚠️ Pin **order** on the OLED, PAM8403 and INMP441 modules varies by seller. Draw each header in the order printed on **your** module.
> ESP32 DevKit boards come in 30-pin and 38-pin versions with different pin positions. Match GPIO **names**, not pin numbers.

---

## 1. Parts list (schematic symbols)

| Ref | Part | Value / type | KiCad symbol (suggestion) | Footprint idea |
|---|---|---|---|---|
| U1 | ESP32 DevKit (WROOM-32) | 30 or 38 pin | `Connector_Generic:Conn_01x15` ×2 (or ×19) | 2× female header 2.54 mm, rows 22.86 mm / 25.4 mm apart (measure yours) |
| J1 | OLED 0.96" SSD1306 I2C | 4-pin | `Conn_01x04` | female header 2.54 mm |
| J2 | PAM8403 module, input + power side | 5 pads (L, GND, R, +5V, GND) | `Conn_01xNN` | custom: pads + module outline |
| J3 | PAM8403 module, output side | 4 pads (L+, L−, R+, R−) | `Conn_01x04` | custom, same module outline |
| J4 | Speaker connector | 2-pin | `Conn_01x02` | JST-PH 2.0 mm or 2-pin screw terminal |
| J5 | INMP441 mic module | 6-pin | `Conn_01x06` | female header 2.54 mm |
| SW1–SW3 | Tactile push button | 6×6 or 12×12 mm | `Switch:SW_Push` | `Button_Switch_THT:SW_PUSH_6mm` |
| D1 | LED red (silent) | 3 or 5 mm | `Device:LED` | THT 3 mm or 5 mm |
| D2 | LED yellow (unread) | 3 or 5 mm | `Device:LED` | THT |
| D3 | LED green (network) | 3 or 5 mm | `Device:LED` | THT |
| R1–R3 | LED resistors | 330 Ω (220–470 ok) | `Device:R` | THT or 0805 |
| R4, R5 | Audio series resistors | 10 kΩ | `Device:R` | THT or 0805 |
| R6, R7 | I2C pull-ups (optional, DNP) | 4.7 kΩ | `Device:R` | 0805; only fit if your OLED has no pull-ups |
| C1 | Amp bulk capacitor | 470 µF ≥10 V electrolytic | `Device:C_Polarized` | radial, 2.5 or 3.5 mm pitch |
| C2 | Amp decoupling | 100 nF | `Device:C` | 0805 / THT |
| C3 | 3V3 decoupling (OLED + mic) | 10 µF | `Device:C` | 0805 / THT |
| C4 | 3V3 decoupling | 100 nF | `Device:C` | 0805 / THT |
| TP1 | Touch pad | copper pad ~15×15 mm | `Connector:TestPoint` or custom | custom: solder-mask-free copper pad, "PAT ME" silkscreen |
| JP1 | Mic L/R select | 3-pad solder jumper | `Jumper:SolderJumper_3_Open` | default: bridge L/R to GND |
| BZ1 | Passive buzzer (optional, DNP) | 12 mm | `Device:Buzzer` | THT 12 mm |
| R8 | Buzzer series (optional, DNP) | 100 Ω | `Device:R` | THT / 0805 |
| J6 | USB-C power breakout (power in) | VBUS, GND (+CC1/CC2 if exposed) | `Conn_01x02` (or `Conn_01x04`) | custom: the breakout's pin holes + outline, at the board edge |
| D4 | Power OR-ing / reverse protection | SS14 or 1N5819 Schottky | `Device:D_Schottky` | SMA or THT DO-41 |
| F1 | Resettable fuse (optional) | 1 A PTC | `Device:Polyfuse` | 1206 or THT |
| C5 | USB input capacitor | 10 µF | `Device:C` | 0805 / THT |
| R9, R10 | CC pull-downs (only if your breakout lacks them) | 5.1 kΩ | `Device:R` | 0805 |
| H1–H4 | Mounting holes | M3 | `Mechanical:MountingHole_Pad` | M3, pad tied to GND |

---

## 2. Net list (connect these)

### Power
| Net | Connects |
|---|---|
| **+5V** | U1 `VIN`/`5V` pin → J2 `+5V` (PAM8403 VCC) → C1 (+) → C2 |
| **+3V3** | U1 `3V3` pin → J1 `VCC` (OLED) → J5 `VDD` (mic) → C3 → C4 → JP1 side "3V3" |
| **GND** | U1 `GND` (all GND pins) → J1 `GND` → J2 `GND` (both) → J5 `GND` → C1 (−) → C2 → C3 → C4 → SW1/SW2/SW3 pin 2 → D1/D2/D3 cathodes → JP1 side "GND" → H1–H4 |

> The ESP32 DevKit powers everything from USB. **Don't** feed 5 V into the board anywhere else at the same time as USB.

### USB-C power input (J6)
| Net | Path |
|---|---|
| VBUS_C | J6 `VBUS` → F1 (optional) → D4 anode |
| +5V | D4 cathode → U1 `VIN`/`5V` (same +5V net as above) |
| GND | J6 `GND` → GND |
| CC1 / CC2 | only if the breakout exposes them **and** has no resistors: each → 5.1 kΩ (R9/R10) → GND |
| C5 | 10 µF from VBUS_C to GND, next to J6 |

> **Power from the USB-C port; program through the DevKit's own USB port, or over WiFi with the `ota` env.**
> D4 stops the two USB sources fighting if both are plugged in at once. After D4 the +5V rail is about 4.7 V, which is fine for the PAM8403 and the DevKit regulator.
> Many cheap USB-C breakouts have **no 5.1 kΩ CC resistors**. Those only get power from an A-to-C cable, not from a C-to-C charger. Look for two tiny resistors marked `512` or `5K1` near the connector.

### Display (I2C)
| Net | ESP32 | Goes to |
|---|---|---|
| I2C_SDA | GPIO **21** | J1 `SDA` (+ R6 to +3V3, optional) |
| I2C_SCL | GPIO **22** | J1 `SCL` (+ R7 to +3V3, optional) |

### Buttons (active low, ESP32 internal pull-ups, no resistors needed)
| Net | ESP32 | Goes to |
|---|---|---|
| BTN1 | GPIO **32** | SW1 pin 1 (pin 2 → GND) |
| BTN2 | GPIO **33** | SW2 pin 1 (pin 2 → GND) |
| BTN3 | GPIO **27** | SW3 pin 1 (pin 2 → GND) |

### Touch
| Net | ESP32 | Goes to |
|---|---|---|
| TOUCH | GPIO **4** | TP1 copper pad (short trace, no ground pour under/around the pad) |

### LEDs
| Net | ESP32 | Path |
|---|---|---|
| LED_RED | GPIO **16** | → R1 330 Ω → D1 anode; D1 cathode → GND |
| LED_YEL | GPIO **17** | → R2 330 Ω → D2 anode; D2 cathode → GND |
| LED_GRN | GPIO **18** | → R3 330 Ω → D3 anode; D3 cathode → GND |

### Audio out
| Net | ESP32 | Path |
|---|---|---|
| DAC_L | GPIO **25** | → R4 10 kΩ → J2 `L` (PAM8403 left in) |
| DAC_R | GPIO **26** | → R5 10 kΩ → J2 `R` (PAM8403 right in) |
| SPK+ | – | J3 `L+` → J4 pin 1 |
| SPK− | – | J3 `L−` → J4 pin 2 |

> PAM8403 outputs are **bridged**. SPK− is **not ground**, so never connect it to GND. J3 `R+`/`R−` can be left unconnected, or add a second 2-pin connector for a right speaker.

### Microphone (I2S)
| Net | ESP32 | Goes to |
|---|---|---|
| MIC_SCK | GPIO **14** | J5 `SCK` |
| MIC_WS | GPIO **13** | J5 `WS` |
| MIC_SD | GPIO **34** | J5 `SD` (GPIO 34 is input-only, which is fine for mic data) |
| MIC_LR | – | J5 `L/R` → JP1 middle pad (default bridge to GND = left channel) |

### Optional
| Net | ESP32 | Path |
|---|---|---|
| BUZZER | GPIO **19** | → R8 100 Ω → BZ1 (+); BZ1 (−) → GND. Leave unfitted if you use the speaker. |

**Leave unconnected:** GPIO 0, 2, 5, 12, 15 (boot strapping pins), GPIO 6–11 (flash), and all other DevKit pins. Put no-connect flags (X) on them in KiCad.

---

## 3. KiCad tips (ERC)

- Add **PWR_FLAG** symbols on `+5V`, `+3V3` and `GND`, because they come from the DevKit header rather than a regulator symbol. Otherwise ERC complains with "power input not driven".
- Use **net labels** (`I2C_SDA`, `BTN1`, …) instead of long wires; the schematic stays readable.
- Put each section (power, display, input, audio, mic) in its own box with a text title.

---

## 4. Layout notes (PCB editor)

1. **ESP32 antenna:** the end of the DevKit with the WROOM module's antenna must **hang over the board edge**, or have no copper on any layer underneath it (keep-out zone). Otherwise WiFi range drops a lot.
2. **Ground pour:** a solid GND pour on the bottom layer, and top too if you like, stitched with vias. Keep it away from the antenna and the touch pad.
3. **Touch pad (TP1):** place it where a finger naturally rests, for example on the front or top edge. Leave a clearance of at least 2 mm with no ground pour around it, and keep the GPIO 4 trace short.
4. **Audio:** put C1 and C2 right next to the PAM8403 power pads. Use **≥0.8 mm traces** for +5V, GND and the speaker lines. Keep the DAC_L/DAC_R traces short and away from the mic lines (GPIO 13, 14, 34).
5. **Mic:** place J5 near a board edge that will face a hole in the case, and away from the speaker connector.
6. **OLED:** put J1 so the screen sits at the front and centre of the case. Check the module's height above the board so it lines up with the case window.
7. **Buttons:** along the top or front edge, spaced ≥10 mm apart, with silkscreen labels: `MOOD`, `SCREEN`, `TALK`.
8. **Silkscreen:** add "DeskBuddy v2 rev A", your name and GitHub handle, the date, and a small Mochi face. It looks great in photos.
9. **Mounting holes:** M3 in the corners, ~3.5 mm from the edges.
10. **Board size:** roughly 70 × 60 mm fits everything. Keep it under 100 × 100 mm for the cheapest fab pricing.

---

## 5. Before ordering: checklist

- [ ] ERC passes (0 errors)
- [ ] DRC passes, using the fab's rules (e.g., 0.2 mm track/space)
- [ ] Module footprints measured against the real modules (print the PCB 1:1 on paper and place the modules on it)
- [ ] DevKit header spacing matches your board
- [ ] OLED pin order matches your module (GND/VCC are sometimes swapped!)
- [ ] Antenna end over the edge or in a keep-out
- [ ] Silkscreen: part labels, pin-1 marks, LED polarity, C1 polarity
- [ ] Gerbers + drill files exported and checked in a Gerber viewer
