#pragma once

/*
 * Board / GPIO selection
 * ----------------------
 * Default target is the ESP32-S3 (LOLIN S3 Mini / SuperMini) — the original
 * Skeleton Eye board. Build the classic ESP32 DevKit V1 with:
 *     pio run -e esp32-devkitc-v1-ili9341
 * which defines `-DBOARD_ESP32DEV=1` (the classic ESP32 has different usable
 * GPIOs: 6-11 are flash straps, so the S3 pin map won't work there).
 *
 * Every board-specific constant (display SPI pins, I2C pins, LED, boot
 * button) lives in the selected board header.
 */
#if defined(BOARD_ESP32DEV)
  #include "boards/board_esp32dev.h"
#else
  #include "boards/board_lolin_s3.h"
#endif