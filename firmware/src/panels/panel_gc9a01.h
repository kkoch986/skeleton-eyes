#pragma once

/*
 * 1.28" round GC9A01 SPI display (original Skeleton Eye panel).
 * 240x240, round aperture, MADCTL 0xC8 (180 degrees) baked into the driver
 * for both panels. The left panel is also mounted upside down, so its
 * controller+physical rotation cancels and it reads upright; the right
 * panel stays 180-rotated on screen, so the right eye is drawn with the
 * mirror flag and that rotation flips it back to match the left.
 * PANEL_REVERSED_BUF restores text/OTA frames for that arrangement.
 */

#define PANEL_DRIVER_NAME "GC9A01"
#define PANEL_WIDTH 240
#define PANEL_HEIGHT 240
#define PANEL_ROUND 1
#define PANEL_REVERSED_BUF 1
/* Right eye is the 180-rotated panel on screen, so it is drawn with the
 * mirror flag: the buffer mirror is re-cancelled by that panel rotation.
 * The retrofit keeps 0 (its rotated panel is controller-fixed instead). */
#define PANEL_RIGHT_EYE_MIRROR 1

/* 240x240 x2 (112KB) fits internal DRAM, so GC9A01 keeps two buffers
   and draws both eyes before pushing either. */
#define PANEL_SINGLE_BUFFER 0

/* GC9A01 handles multi-row windows correctly; keep the fast banded push. */
#define PANEL_ROW_WINDOW_PUSH 0

/* Sprite band: sprites are square 240x240 and cover the whole panel. */
#define SPRITE_BAND_W 240
#define SPRITE_BAND_H 240
#define SPRITE_BAND_X 0
#define SPRITE_BAND_Y 0

/* Procedural eye geometry (pixels). */
#define EYE_SOCKET_RADIUS 140
#define EYE_SCLERA_RADIUS 110
#define EYE_IRIS_RADIUS 45
#define EYE_PUPIL_RADIUS 22

/* Eye scale: the round panels fill their own 240x240 aperture, so the
 * standard build keeps the eye at 1.0 (explicit; see panel_ili9341.h for
 * the scaled retrofit variant). */
#define EYE_SCALE 1.0f

/* GC9A01 controller command set (shared framing commands match ILI9341). */
#define PANEL_SWRESET 0x01
#define PANEL_SLPOUT 0x11
#define PANEL_DISPON 0x29
#define PANEL_CASET 0x2A
#define PANEL_RASET 0x2B
#define PANEL_RAMWR 0x2C
#define PANEL_MADCTL 0x36
#define PANEL_COLMOD 0x3A

void panel_driver_init(bool right);