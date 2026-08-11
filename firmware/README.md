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
- **SPI speed**: 80MHz
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
