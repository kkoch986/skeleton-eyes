#pragma once

/*
 * 2.4" ILI9341 SPI TFT (240x320) - GODIYMODULES retrofit eye module.
 * Two of these replace the round GC9A01 panels for a 12ft skeleton build.
 *
 * Portrait orientation (MADCTL 0x00, 240 wide x 320 tall). The two panels
 * are physically mounted opposite each other (one upside down, mirroring
 * the non-retrofit round layout), so panel_driver_init(right) rotates the
 * OUTPUT of the flipped panel 180 degrees in the controller (MADCTL
 * MX|MY = 0xC0). The flipped panel is selected by PANEL_ROTATE_RIGHT_180
 * (0 = left is flipped, matching the round build; 1 = right). With the
 * controller rotation the flipped panel presents frames upright like the
 * other, so the software stays symmetric: both eyes drawn identically, no
 * canvas flips and no mirror flag. 240x240 sprites are centered
 * vertically: SPRITE_BAND_Y.
 */

#define PANEL_DRIVER_NAME "ILI9341"
#define PANEL_WIDTH 240
#define PANEL_HEIGHT 320
#define PANEL_ROUND 0
#define PANEL_REVERSED_BUF 0
/* Which panel is physically mounted upside down. 0 = left (like the
 * non-retrofit round build), 1 = right. Only that panel gets the 180-degree
 * controller rotation above; the other stays 0x00. */
#define PANEL_ROTATE_RIGHT_180 0
/* Both panels present frames upright after the controller rotation, so the
 * right eye is drawn identically to the left (no mirror flag). The round
 * build needs PANEL_RIGHT_EYE_MIRROR=1 instead because its right panel
 * remains 180-rotated on screen. */
#define PANEL_RIGHT_EYE_MIRROR 0

/* 156KB x 2 of framebuffer exceeds the S3's internal DRAM. Draw and push
   each eye sequentially through one shared buffer instead. Banding (see
   globals.h) further shrinks that shared buffer on the classic ESP32. */
#define PANEL_SINGLE_BUFFER 1

/* These ILI9341 clones are 18-bit controllers: at COLMOD 0x66 the panel
 * reads 3 bytes per pixel, so the push path converts RGB565 on the fly
 * (PANEL_18BIT). One row per window keeps the clone column-counter quirks
 * at bay. */
#define PANEL_18BIT 1
#define PANEL_ROW_WINDOW_PUSH 1

/* Sprite band: sprites are square 240x240, centered inside the 320-tall panel. */
#define SPRITE_BAND_W 240
#define SPRITE_BAND_H 240
#define SPRITE_BAND_X 0
#define SPRITE_BAND_Y ((PANEL_HEIGHT - PANEL_WIDTH) / 2)

/* Procedural eye geometry (pixels). */
#define EYE_SOCKET_RADIUS 118
#define EYE_SCLERA_RADIUS 104
#define EYE_IRIS_RADIUS 42
#define EYE_PUPIL_RADIUS 20

/* Eye scale factor: multiplies all procedural eye radii around the panel
 * center. These panels are 240x320, taller than the round 240x240 originals,
 * so scaling the eye up (>1.0) fills the extra height - the eye then clips
 * against the panel edges, which is the intent for the retrofit. Adjust this
 * value to taste; the round GC9A01 build keeps 1.0. */
#define EYE_SCALE 1.35f

/* ILI9341 controller command set (shared framing commands match GC9A01). */
#define PANEL_SWRESET 0x01
#define PANEL_SLPOUT 0x11
#define PANEL_DISPON 0x29
#define PANEL_CASET 0x2A
#define PANEL_RASET 0x2B
#define PANEL_RAMWR 0x2C
#define PANEL_MADCTL 0x36
#define PANEL_COLMOD 0x3A

void panel_driver_init(bool right);