#pragma once

/*
 * Panel driver selection
 * ----------------------
 * Build the original dual-GC9A01 round displays with no special flag.
 * Build the 2.4" ILI9341 SPI TFT retrofit pair with:
 *     pio run -e esp32-s3-devkitc-1-ili9341
 * which defines `-DPANEL_ILI9341=1`.
 *
 * The active panel header below defines every panel-specific constant
 * (size, roundness, orientation, eye geometry) plus panel_driver_init().
 */
#if defined(PANEL_ILI9341)
  #include "panels/panel_ili9341.h"
#else
  #include "panels/panel_gc9a01.h"
#endif