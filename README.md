# Skeleton Eye

Dual 1.28" TFT animatronic eyes with an I2C-controlled management board.

## Repository Layout

```
skeleton-eye2/
├── firmware/               # ESP32-S3 eye display controller
│   ├── src/main.cpp        # Procedural + sprite rendering, I2C slave
│   ├── sprites/            # Custom eye images (240x240 PNG)
│   ├── convert_sprites.py  # Image → RLE-compressed sprite data
│   └── platformio.ini      # PlatformIO build config
├── management-board/       # ESP32-C3 I2C master controller
│   └── src/eye_controller.ino  # Serial → I2C bridge
└── eye.sh                  # Send commands via USB serial
```

## Hardware

| Component | Board | Role |
|-----------|-------|------|
| Eye display controller | ESP32-S3 (LOLIN S3 Mini) | Drives 2× GC9A01 displays, I2C slave at `0x42` |
| Management board | ESP32-C3 (LOLIN C3 Mini) | I2C master, serial command interface |

**Note:** The left eye display is physically mounted upside down. The firmware handles the 180-degree rotation automatically.

### Wiring

![Wiring diagram](wiring.svg)

See [`firmware/README.md`](firmware/README.md) for display pin connections and [`management-board/README.md`](management-board/README.md) for I2C wiring details.

## Bill of Materials

| Part | Qty | Link |
|------|:---:|------|
| ESP32 S3 Supermini | 1 | [AliExpress](https://www.aliexpress.us/item/3256807337873344.html) |
| 24.8mm Round Lens | 2 | [AliExpress](https://www.aliexpress.us/item/3256803507484638.html) |
| 1.28" Round TFT display module | 2 | [AliExpress](https://www.aliexpress.us/item/3256811824930283.html) |
| M2 Standoffs 10mm tall | 4 | — |
| M2 Nuts | 8 | — |
| M2 Screws | 4 | — |
| 4-pin JST Cable (10cm) | 1 | [AliExpress](https://www.aliexpress.us/item/3256812395686145.html) |
| Custom PCB | 1 | — |
| 7-pin Female header | 2 | [AliExpress](https://www.aliexpress.us/item/3256812035604365.html) |
| 7-pin Male header | 1 | [AliExpress](https://www.aliexpress.us/item/2255800801798474.html) |
| 4-pin Female JST 2.54mm connector | 2 | [AliExpress](https://www.aliexpress.us/item/3256806894018733.html) |

## Quick Start

### Prerequisites

- PlatformIO CLI (`pip install platformio`)
- Python 3 with Pillow (`pip install Pillow`) for custom sprite conversion

### Build and Upload

```bash
# Build and upload firmware
cd firmware && make upload

# Build and upload management board (if used)
cd management-board && make upload
```

## Features

- **Dual rendering modes**: Procedural 3D eye rendering with lighting, eyelids, and reflections, or pre-rendered sprite animations
- **I2C external control**: Full remote control over I2C bus with smooth gaze, blink, squint, and color commands
- **WiFi OTA updates**: Over-the-air firmware updates with progress display on the eyes
- **C client library**: Drop-in C library for integrating eye control into your own firmware projects
- **Serial interface**: Interactive command shell via USB serial or the included `eye.sh` script

See the subdirectory READMEs for serial commands, I2C protocol details, and sprite development.
