#pragma once

/* Classic ESP32 DevKit V1 (ESP-WROOM-32).
 *
 * Unlike the S3, GPIO 6-11 are the internal flash interface and must NOT
 * be used, and GPIO 1/3 are the UART0 serial console. The wiring below
 * keeps well clear of all of those. This is the hand-wired ILI9341
 * retrofit board — the I2C management interface is optional. */

#define BOARD_NAME "ESP32 DevKit V1"

/* VSPI default pins: SCK 18, MOSI 23, MISO 19. */
#define LEFT_EYE_CS 17  /* UART2_TX label */
#define RIGHT_EYE_CS 16 /* UART2_RX label */
#define SHARED_RST 21
#define SHARED_DC 22
#define SHARED_SDA 23   /* MOSI */
#define SHARED_SCL 18   /* SCLK */
#define SHARED_MISO 19  /* unused, but keep off UART0 (GPIO1/3) */

/* Not used by the panels. */
#define I2C_SDA 4
#define I2C_SCL 15
#define BOOT_BUTTON 0   /* GPIO0 is the BOOT strap on DevKit V1 */
#define LED_PIN 2       /* onboard blue LED */

/* Classic ESP32 uses a CP2102 UART bridge — serial is instantly available. */
#define SERIAL_BOOT_DELAY_MS 0

/* Classic ESP32 internal RAM is two disjoint heap regions: the largest
   single allocatable block is ~128KB, but a full 240x320 ILI9341 frame is
   153,600 B. The renderer must split each frame into vertical bands and
   use a partial-height buffer. (GC9A01's 240x240 always fits.) */
#define BOARD_BANDED_FRAMEBUFFER 1