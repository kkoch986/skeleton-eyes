#pragma once

/* ESP32-S3 (LOLIN S3 Mini / SuperMini) — original Skeleton Eye board.
 *
 * The S3 has all GPIOs freely mappable; the original wiring uses its low
 * pins for the display bus. Native USB-serial (CDC), not a UART bridge. */

#define BOARD_NAME "ESP32-S3"

#define LEFT_EYE_CS 13
#define RIGHT_EYE_CS 9
#define SHARED_RST 12
#define SHARED_DC 8
#define SHARED_SDA 11   /* MOSI */
#define SHARED_SCL 10   /* SCLK */
/* MISO is unused (write-only panels). GPIO 1 is free on the S3. */
#define SHARED_MISO 1

#define I2C_SDA 4
#define I2C_SCL 7
#define BOOT_BUTTON 0
#define LED_PIN 15

/* Native USB-CDC needs time to enumerate so the host catches the boot log. */
#define SERIAL_BOOT_DELAY_MS 5000

/* S3 internal RAM is a single contiguous heap region (~300KB): a full
   240x320 ILI9341 frame (153,600 B) fits, so no banded rendering. */
#define BOARD_BANDED_FRAMEBUFFER 0