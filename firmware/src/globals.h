#pragma once

#include <Arduino.h>
#include <SPI.h>

#define SPRITE_UNCOMPRESSED 0
#define SPRITE_RLE_COMPRESSED 1
#define SPRITE_PALETTE_4BIT 2
#define SPRITE_PALETTE_8BIT 3
#define SPRITE_DELTA_COMPRESSED 4

#define I2C_CMD_LOOK 0x01
#define I2C_CMD_BLINK 0x02
#define I2C_CMD_SQUINT 0x03
#define I2C_CMD_CURVE_PARAMS 0x06
#define I2C_CMD_STATUS 0x07
#define I2C_CMD_RESET 0x08
#define I2C_CMD_SCLERA_RGB 0x09
#define I2C_CMD_IRIS_RGB 0x0A
#define I2C_CMD_AUTO_BLINK 0x0B
#define I2C_CMD_IDLE 0x0C
#define I2C_CMD_JUMP 0x0D
#define I2C_CMD_SMOOTHING 0x0E
#define I2C_CMD_WIFI_SSID 0x0F
#define I2C_CMD_WIFI_PASS 0x10
#define I2C_CMD_WIFI_CONNECT 0x11
#define I2C_CMD_WIFI_FORGET 0x12
#define I2C_CMD_WIFI_STATUS 0x13
#define I2C_CMD_RESET_DEVICE 0x14
#define I2C_CMD_AUTO_BLINK_SPEED 0x15
#define I2C_CMD_SPRITE_MODE 0x16
#define I2C_CMD_GET_MODE 0x17
#define I2C_CMD_SET_SPRITE 0x18

#define MAX_SPRITE_FRAMES 64

#define LEFT_EYE_CS 13
#define RIGHT_EYE_CS 9
#define SHARED_RST 12
#define SHARED_DC 8
#define SHARED_SDA 11
#define SHARED_SCL 10

#define I2C_SDA 4
#define I2C_SCL 7
#define I2C_SLAVE_ADDRESS 0x42
#define BOOT_BUTTON 0
#define LED_PIN 15

#define GC9A01_SWRESET 0x01
#define GC9A01_SLPOUT 0x11
#define GC9A01_DISPON 0x29
#define GC9A01_CASET 0x2A
#define GC9A01_RASET 0x2B
#define GC9A01_RAMWR 0x2C
#define GC9A01_MADCTL 0x36
#define GC9A01_COLMOD 0x3A

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_SCLERA 0xF7DE
#define COLOR_IRIS_DARK 0x2945
#define COLOR_IRIS_MED 0x4A49
#define COLOR_PUPIL 0x0000
#define COLOR_SOCKET 0x0000
#define COLOR_RED 0xF800
#define COLOR_BLUE 0x001F

#define OTA_COLOR_BLACK 0x0000
#define OTA_COLOR_FILL  0x07E0
#define OTA_COLOR_TEXT  0xFFFF

#define FONT_W 8
#define FONT_H 8

struct SpriteHeader {
  uint8_t compression_type;
  uint8_t bits_per_pixel;
  uint16_t compressed_size;
  uint16_t palette_size;
  uint8_t frame_id;
} __attribute__((packed));

struct SpriteFrame {
  SpriteHeader header;
  const uint8_t* palette_data;
  const uint8_t* compressed_data;
  bool upscale;
};

struct lighting_t {
  int16_t light_x;
  int16_t light_y;
  float ambient;
  float diffuse;
};

extern SPIClass *hspi;
extern SPISettings spi_settings;
extern uint16_t left_eye_buffer[];
extern uint16_t right_eye_buffer[];

extern SpriteFrame sprite_frames[];
extern uint8_t sprite_count;
extern int8_t current_sprite;
extern bool sprite_mode;

extern lighting_t eye_lighting;
extern float global_squint;
extern float left_squint;
extern float right_squint;
extern float curve_falloff;
extern float curve_minimum;
extern float closure_strength;
extern uint16_t current_sclera_color;
extern uint16_t current_iris_dark_color;
extern uint16_t current_iris_med_color;

extern volatile bool i2c_command_received;
extern volatile uint8_t i2c_buffer[64];
extern volatile uint8_t i2c_buffer_index;
extern volatile bool i2c_external_control;
extern int16_t i2c_look_x;
extern int16_t i2c_look_y;
extern float i2c_squint_level;
extern bool i2c_blink_trigger;
extern bool auto_blink_enabled;
extern uint16_t auto_blink_interval;
extern int16_t current_look_x;
extern int16_t current_look_y;
extern int16_t target_look_x;
extern int16_t target_look_y;
extern float i2c_smoothing;
extern uint32_t i2c_blink_duration;
extern bool i2c_initialized;
extern bool i2c_master_detected;

extern String wifi_ssid;
extern String wifi_pass;
extern bool wifi_should_connect;
extern bool wifi_connecting;
extern bool wifi_connected;
extern unsigned long wifi_connect_start;

extern bool ota_display_active;
extern int ota_fill_rows;
extern bool waiting_display_shown;
extern bool i2c_just_ready;
extern unsigned long i2c_ready_display_start;
