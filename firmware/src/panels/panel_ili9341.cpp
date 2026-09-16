#include "display.h"

#if defined(PANEL_ILI9341)
/*
 * ILI9341 init sequence (standard Adafruit/ILI9341 datasheet sequence),
 * portrait 240x320. This module family is 3.3V SPI and
 * shares the exact framing commands (CASET/RASET/RAMWR) with the GC9A01, so
 * it drops into the same bus without library changes.
 *
 * Each command and its parameters are sent in one continuous SPI transaction.
 * Splitting command and data into separate transactions works on some panels
 * but causes others to miss the parameter bytes, so the helper keeps CS/CLK
 * live while D/C is toggled.
 */
void panel_driver_init(bool right) {
  send_command(PANEL_SWRESET);  /* software reset */
  delay(150);

  send_command(PANEL_SLPOUT);  /* exit sleep */
  delay(150);

  /* Portrait: 240 x 320, RGB order (our RGB565 is R-first; the BGR bit
   * swaps red/blue). The physically upside-down panel gets a 180-degree
   * controller rotation (MADCTL MX|MY = 0xC0) so it net-presents frames
   * upright on screen; the other panel stays 0x00. Which panel that is
   * comes from PANEL_ILI9341.h's PANEL_ROTATE_RIGHT_180. No software
   * canvas flips are needed. */
  {
    uint8_t d = 0x00;
    if (right == PANEL_ROTATE_RIGHT_180) {
      d = 0xC0;
    }
    send_command_with_data(PANEL_MADCTL, &d, 1);
  }

  /* 18-bit RGB666 pixel format — this panel family reads 3 bytes per
   * pixel. */
  {
    uint8_t d = 0x66;
    send_command_with_data(PANEL_COLMOD, &d, 1);
  }

  /* Column/row address slices. */
  {
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};  /* 0..239 */
    send_command_with_data(PANEL_CASET, caset, sizeof(caset));
  }
  {
    uint8_t raset[] = {0x00, 0x00, 0x01, 0x3F};  /* 0..319 */
    send_command_with_data(PANEL_RASET, raset, sizeof(raset));
  }

  /* Power control A/B. */
  {
    uint8_t d[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    send_command_with_data(0xCB, d, sizeof(d));
  }
  {
    uint8_t d[] = {0x00, 0xC1, 0x30};
    send_command_with_data(0xCF, d, sizeof(d));
  }

  /* Driver timing control A/B. */
  {
    uint8_t d[] = {0x85, 0x00, 0x78};
    send_command_with_data(0xE8, d, sizeof(d));
  }
  {
    uint8_t d[] = {0x00, 0x00};
    send_command_with_data(0xEA, d, sizeof(d));
  }

  /* Power-on sequence + pump ratio. */
  {
    uint8_t d[] = {0x64, 0x03, 0x12, 0x81};
    send_command_with_data(0xED, d, sizeof(d));
  }
  {
    uint8_t d = 0x20;
    send_command_with_data(0xF7, &d, 1);
  }

  /* Power control 1/2, VCOM 1/2. */
  {
    uint8_t d = 0x23;
    send_command_with_data(0xC0, &d, 1);
  }
  {
    uint8_t d = 0x10;
    send_command_with_data(0xC1, &d, 1);
  }
  {
    uint8_t d[] = {0x3E, 0x28};
    send_command_with_data(0xC5, d, sizeof(d));
  }
  {
    uint8_t d = 0x86;
    send_command_with_data(0xC7, &d, 1);
  }

  /* Frame rate (79Hz) + display function control. */
  {
    uint8_t d[] = {0x00, 0x18};
    send_command_with_data(0xB1, d, sizeof(d));
  }
  {
    uint8_t d[] = {0x08, 0x82, 0x27};
    send_command_with_data(0xB6, d, sizeof(d));
  }

  /* 3G disable, gamma curve 1. */
  {
    uint8_t d = 0x00;
    send_command_with_data(0xF2, &d, 1);
  }
  {
    uint8_t d = 0x01;
    send_command_with_data(0x26, &d, 1);
  }

  /* Positive / negative gamma. */
  {
    uint8_t d[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E,
                   0x08, 0x4E, 0xF1, 0x37, 0x07,
                   0x10, 0x03, 0x0E, 0x09, 0x00};
    send_command_with_data(0xE0, d, sizeof(d));
  }
  {
    uint8_t d[] = {0x00, 0x0E, 0x14, 0x03, 0x11,
                   0x07, 0x31, 0xC1, 0x48, 0x08,
                   0x0F, 0x0C, 0x31, 0x36, 0x0F};
    send_command_with_data(0xE1, d, sizeof(d));
  }

  send_command(PANEL_DISPON);
  delay(50);
}
#endif  /* PANEL_ILI9341 */
