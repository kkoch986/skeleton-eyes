# Skeleton Eyes - ESP32-S3 Animatronic Display

ESP32-S3 project for dual 1.28" TFT displays creating realistic animated eyes with I2C external control support.

## Hardware

| Component | Connection |
|-----------|------------|
| Left Eye CS  | IO13 |
| Right Eye CS | IO9 |
| Shared RST   | IO12 |
| Shared DC    | IO8 |
| Shared SDA   | IO11 (MOSI) |
| Shared SCL   | IO10 (SCLK) |
| I2C SDA      | IO4 |
| I2C SCL      | IO7 |

## Display Panels

The standard configuration drives the original pair of 1.28" round GC9A01
displays.

| Build (`pio run -e …`) | Board | Panel | Res | Orientation |
|------------------------|-------|-------|-----|-------------|
| `esp32-s3-devkitc-1`      | ESP32-S3 | 1.28" GC9A01 round | 240×240 | left panel mounted upside-down, firmware rotates 180° |

Build and flash with `make` / `make upload`. A non-standard configuration
for the 12 ft Home Depot skeleton is documented at the end of this file.

## Quick Start

```bash
make              # Build
make upload       # Upload to device
make monitor      # View serial output
```

### Serial Commands (USB)

| Key | Action |
|-----|--------|
| `s` | Toggle sprite mode |
| `b` | Trigger blink |
| `l`/`r`/`u`/`d` | Look left/right/up/down |
| `[`/`]` | Squint open/close |
| `h` | Print help |
| `w`/`B`/`L`/`G`/`H`/`R` | Sclera white/blue/light/green/red/iris red |

## Features

### Dual Rendering Modes

- **Procedural**: Real-time mathematical eye rendering with 3D lighting, eyelids, and reflections
- **Sprite**: Pre-rendered compressed sprite animations with procedural blink overlay

### I2C Slave (Address 0x42)

The eye board acts as an I2C slave. Commands override autonomous mode until a reset command.

| Cmd | Name | Params | Description |
|-----|------|--------|-------------|
| 0x01 | `LOOK` | x (i16), y (i16) | Set gaze direction |
| 0x02 | `BLINK` | duration (u16) | Trigger blink (ms) |
| 0x03 | `SQUINT` | level (u8) | 0 (open) — 255 (closed) |
| 0x06 | `CURVE_PARAMS` | falloff (u8), min (u8), strength (u8) | Eyelid curve (0-255 each → 0.0-1.0) |
| 0x07 | `STATUS` | (read 31 bytes) | Full status block — gaze, squint, mode, colors, curve, blink, current position |
| 0x08 | `RESET` | — | Return to autonomous mode |
| 0x09 | `SCLERA_RGB` | color (u16 le) | Sclera color as RGB565 |
| 0x0A | `IRIS_RGB` | color (u16 le) | Iris color as RGB565 (dark variant derived) |
| 0x0B | `AUTO_BLINK` | enable (u8) | 1=on, 0=off |
| 0x0C | `IDLE` | — | Release I2C control, resume autonomous movement |
| 0x0D | `JUMP` | x (i16), y (i16) | Instant position (no smoothing) |
| 0x0E | `SMOOTHING` | factor (u8) | 0-255, default ~25 |
| 0x0F | `WIFI_SSID` | null-terminated string | Set WiFi SSID |
| 0x10 | `WIFI_PASS` | null-terminated string | Set WiFi password |
| 0x11 | `WIFI_CONNECT` | — | Connect to WiFi (enables OTA) |
| 0x12 | `WIFI_FORGET` | — | Clear stored WiFi credentials |
| 0x13 | `WIFI_STATUS` | (read) | Returns 1 byte: 0=disconnected, 1=connecting, 2=connected |
| 0x14 | `RESET_DEVICE` | — | Reboot ESP32 |
| 0x15 | `AUTO_BLINK_SPEED` | interval (u16 le) | ms between auto-blinks |
| 0x16 | `SPRITE_MODE` | enable (u8) | 1=sprite, 0=procedural |
| 0x17 | `GET_MODE` | (read) | Returns 1 byte: current render mode |
| 0x18 | `SET_SPRITE` | index (u8) | 0-63, 255 = none |
| 0x19 | `DISPLAY_TEXT` | string (max 62) | Display text wrapped on the eye screen |
| 0x1A | `CLEAR_TEXT` | — | Hide text overlay |
| 0x1B | `TEXT_COLOR` | color (u16 le) | Text color as RGB565 |
| 0x1C | `TEXT_BG` | color (u16 le) | Text background color as RGB565 |

## Sprite Development

1. Place 240×240 PNGs in `sprites/` — sorted alphabetically, indexed 0, 1, 2, ...
2. `make sprites` — convert to RLE-compressed sprite data at full resolution
3. `make sprites-half` — convert at 120×120, upscaled 2× at runtime (~4× less flash)
4. Select by index via I2C command `0x18` or the management board's `sprite on` command

## Performance

- **Frame rate**: 30 FPS
- **SPI speed**: 80 MHz
- **RAM**: ~86%, **Flash**: ~64%

## Project Structure

```
firmware/
├── src/main.cpp              # Main application + I2C slave
├── src/generated_sprites.h   # Sprite data (generated)
├── sprites/                  # Source images (240×240 PNG)
├── convert_sprites.py        # Image → sprite data converter
├── platformio.ini            # Build configuration
└── Makefile                  # Build automation
```

---

## Retrofitting the 12 ft Home Depot Skeleton (non-standard)

The setup above is the standard product. This section documents a
**non-standard, DIY configuration built for one specific project**:
replacing the eyes of a **12 ft Home Depot skeleton** (Home Accents
"12 Foot Animated Skeleton") with a pair of **2.4" SPI TFT LCD 240×320
(ILI9341)** displays.

The 12 ft skull's eye sockets are much larger than the 24 mm aperture the
round panels were sized for, so bigger eyeballs fill them properly. This is
*not* a general drop-in panel option — `-DPANEL_ILI9341=1` swaps the driver,
panel geometry, and frame-buffer strategy at compile time, and it was
verified on this one skeleton build only. Any other application needs the
geometry, mounting, and lensing re-checked against your own hardware.

Builds (same wiring/rules as the round build, different panel module):

| Build (`pio run -e …`) | Board | Panel | Res | Notes |
|------------------------|-------|-------|-----|-------|
| `esp32-s3-devkitc-1-ili9341` | ESP32-S3 | 2.4" ILI9341 (GODIYMODULES) | 240×320 | as standard, SPI 80 MHz |
| `esp32-devkitc-v1-ili9341` | classic ESP32 DevKit V1 | 2.4" ILI9341 | 240×320 | SPI capped 40 MHz (hand-wired) |

```bash
pio run -e esp32-s3-devkitc-1-ili9341 -t upload       # S3
pio run -e esp32-devkitc-v1-ili9341 -t upload         # classic DevKit V1
make upload ENVIRONMENT=esp32-devkitc-v1-ili9341      # via Makefile
```

**Panel behaviour and memory**

- The two panels mount **physically opposite each other** — one upside down,
  mirroring the round build's layout. `panel_driver_init(right)` rotates the
  flipped panel's *output* 180° in the controller (`MADCTL MX|MY = 0xC0`),
  selected by `PANEL_ROTATE_RIGHT_180` in `src/panels/panel_ili9341.h`
  (0 = left is flipped, matching the round build; 1 = right). Both eyes are
  then drawn **identically** (`PANEL_RIGHT_EYE_MIRROR 0`) — no software
  canvas flips — and the 240×240 sprite/text area is centered vertically.
  All wires to the two panels share one bus; each panel gets its own CS line.
- The rendered eye is scaled up 1.35× (`EYE_SCALE` in `src/panels/panel_ili9341.h`)
  so it fills the taller 320-pixel panel — the eye clips against the panel
  edges by design, matching the round panels' full-aperture look. On the round
  GC9A01 build `EYE_SCALE` is 1.0.
- 240×320 full-screen RGB565 frames are 153KB each, so the ILI9341 build
  shares a **single framebuffer**: each eye is rendered and pushed to the
  screen before the next one is drawn. `PANEL_SINGLE_BUFFER` in
  `src/panels/panel_ili9341.h` gates this; the GC9A01 build keeps two
  buffers and draws-both-push-both. The classic ESP32 can't even hold one
  full frame contiguously, so that build splits each frame into two vertical
  bands (see `src/boards/board_esp32dev.h`).
- These ILI9341 clones are **18-bit controllers**: the push path converts
  RGB565 to RGB666 (`PANEL_18BIT`, COLMOD 0x66) and uses one row per window
  as a workaround for clone column-counter quirks.

**Wiring (panel ⬅ ESP32-S3):**

| Panel pin | ESP32-S3 pin |
|-----------|--------------|
| VCC / 3.3V | 3V3   |
| GND        | GND   |
| SCL / SCK  | IO10  |
| SDA / MOSI | IO11  |
| DC (A0)    | IO8   |
| RST        | IO12  |
| CS (left eye)  | IO13  |
| CS (right eye) | IO9   |

**External control (I2C slave, address 0x42 ⬅ management board):**

| Pin | ESP32-S3 pin |
|-----|--------------|
| I2C SDA | IO4  |
| I2C SCL | IO7  |

**Wiring (panel ⬅ classic ESP32 DevKit V1):**

| Panel pin | ESP32 DevKit V1 pin |
|-----------|--------------|
| VCC / 3.3V | 3V3   |
| GND        | GND   |
| SCL / SCK  | GPIO 18 |
| SDA / MOSI | GPIO 23 |
| DC (A0)    | GPIO 22 |
| RST        | GPIO 21 |
| CS (left eye)  | GPIO 17 |
| CS (right eye) | GPIO 16 |

**External control (I2C slave, address 0x42 ⬅ management board):**

| Pin | ESP32 DevKit V1 pin |
|-----|---------------------|
| I2C SDA | GPIO 4 |
| I2C SCL | GPIO 15 |

Join the two panels' SCL→SCK, SDA→MOSI, DC→DC, RST→RST, VCC→3V3, GND→GND
together; only the CS pins differ per panel. The backlight LED pin on the
module can go straight to 3V3 for full brightness (or PWM-capable IO2 for a
dimming hookup if you add one).

> **Orientation note:** ILI9341 init uses `MADCTL 0x00` for the upright
> panel. The physically flipped panel gets `MADCTL 0xC0` (`MX|MY`, 180°)
> via `PANEL_ROTATE_RIGHT_180` and the `right` flag passed to
> `panel_driver_init(right)`. If the image comes out sideways on your
> module revision, adjust the MADCTL data bytes in
> `src/panels/panel_ili9341.cpp` (the `d` in the `PANEL_MADCTL` block),
> e.g. swap which value applies to the flipped panel. `PANEL_MADCTL` itself
> is the 0x36 command opcode, not the orientation value.

**Performance (ILI9341 builds)**

- S3: ~63% RAM (single shared framebuffer); push-bound frame rate
- Classic DevKit V1: single banded framebuffer (~62 KB); SPI 40 MHz
