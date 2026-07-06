# I2C Controller for Skeleton Eye

Controls the skeleton-eye animatronic eye displays via I2C from an ESP32-C3 (or any Arduino-compatible I2C master).

## I2C Protocol

The eye board acts as an I2C slave at address **`0x42`** (100kHz). All multi-byte values are little-endian.

See the [firmware README](../firmware/README.md#i2c-slave-address-0x42) for the full I2C command table.

## Build & Upload

```bash
make build      # compile
make upload     # flash to ESP32-C3
make monitor    # serial console
make run        # build + upload + monitor
```

## Wiring

- Controller SDA (GPIO8) → Eye board SDA (GPIO4)
- Controller SCL (GPIO9) → Eye board SCL (GPIO7)
- Common GND

## Serial Commands

| Command | Description |
|---------|-------------|
| `look <x> <y> [s]` / `look center\|left\|right\|up\|down` | Smooth gaze |
| `jump <x> <y>` / `jump center\|left\|right\|up\|down` | Instant position |
| `blink [ms]` | Trigger blink |
| `squint <0-255>` | Set squint level |
| `unsquint` | Reset squint to 0 |
| `sclera <r g b\|hex>` | Sclera color (3×0-255 or 4-digit hex) |
| `iris <r g b\|hex>` | Iris color |
| `curve <f> <m> <s>` | Eyelid curve params (0-255) |
| `autoblink on\|off` | Enable/disable auto blink |
| `autoblinkspeed <ms>` | Set auto-blink interval |
| `smoothing fast\|medium\|slow\|<0-255>` | Gaze smoothing speed |
| `wifi ssid\|pass\|connect\|status\|forget` | WiFi/OTA control |
| `idle` | Release I2C control |
| `emotion <name>` | angry, sleepy, surprised, neutral |
| `sprite on\|off` | Toggle render mode |
| `status` | Read eye status |
| `probe` | Check I2C connection |
| `i2cscan` | Scan I2C bus |
| `demo` | Run demo sequence |
| `reset` | Reset to autonomous mode |
| `resetdevice` | Reboot ESP32 eye board |
| `help` / `?` | Show help |
