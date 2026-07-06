/*
 * Skeleton Eye Display Controller
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "generated_sprites.h"

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

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#define LEFT_EYE_CS 13
#define RIGHT_EYE_CS 9
#define SHARED_RST 12
#define SHARED_DC 8
#define SHARED_SDA 11
#define SHARED_SCL 10

#define I2C_SDA 4
#define I2C_SCL 7
#define I2C_SLAVE_ADDRESS 0x42

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

uint16_t left_eye_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
uint16_t right_eye_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

SPIClass *hspi = &SPI;
SPISettings spi_settings(80000000, MSBFIRST, SPI_MODE0);

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

SpriteFrame sprite_frames[MAX_SPRITE_FRAMES];
uint8_t sprite_count = 0;
int8_t current_sprite = -1;
bool sprite_mode = false;

float global_squint = 0.0f;
float left_squint = 0.0f;
float right_squint = 0.0f;

float curve_falloff = 0.05f;
float curve_minimum = 0.6f;
float closure_strength = 1.00f;

uint16_t current_sclera_color = COLOR_SCLERA;
uint16_t current_iris_dark_color = COLOR_IRIS_DARK;
uint16_t current_iris_med_color = COLOR_IRIS_MED;

volatile bool i2c_command_received = false;
volatile uint8_t i2c_buffer[64];
volatile uint8_t i2c_buffer_index = 0;
volatile bool i2c_external_control = false;
int16_t i2c_look_x = 0;
int16_t i2c_look_y = 0;
float i2c_squint_level = 0.0f;
bool i2c_blink_trigger = false;
bool auto_blink_enabled = false;
uint16_t auto_blink_interval = 3500;
int16_t current_look_x = 0;
int16_t current_look_y = 0;
int16_t target_look_x = 0;
int16_t target_look_y = 0;
float i2c_smoothing = 0.1f;
uint32_t i2c_blink_duration = 150;
bool i2c_initialized = false;
bool i2c_master_detected = false;

String wifi_ssid = "";
String wifi_pass = "";
bool wifi_should_connect = false;
bool wifi_connecting = false;
bool wifi_connected = false;
unsigned long wifi_connect_start = 0;

bool ota_display_active = false;
int ota_fill_rows = 0;
bool waiting_display_shown = false;

void select_display(uint8_t cs_pin);
void deselect_all();
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

struct lighting_t {
  int16_t light_x;
  int16_t light_y;
  float ambient;
  float diffuse;
};

lighting_t eye_lighting;

uint16_t apply_3d_lighting(uint16_t base_color, int32_t dist_sq, int32_t radius_sq,
                           int16_t dx, int16_t dy, lighting_t* lighting) {
  if (dist_sq >= radius_sq) return base_color;
  if (base_color == COLOR_BLACK || base_color == COLOR_SOCKET) return COLOR_BLACK;
  if (dist_sq < 0) return base_color;
  
  if (radius_sq == 0) return base_color;
  
   uint32_t norm_dist_sq_256 = ((uint32_t)dist_sq << 8) / radius_sq;
   if (norm_dist_sq_256 >= 256) return base_color;
   
   if (norm_dist_sq_256 >= 240) {
     norm_dist_sq_256 = 255;
   }
  
   uint32_t reduction = (norm_dist_sq_256 * 192) >> 8;
   uint32_t sphere_factor_256 = 256 - reduction;
   if (sphere_factor_256 < 51) sphere_factor_256 = 51;
  
  uint16_t r = (((base_color >> 11) & 0x1F) * sphere_factor_256) >> 8;
  uint16_t g = (((base_color >> 5) & 0x3F) * sphere_factor_256) >> 8;  
  uint16_t b = ((base_color & 0x1F) * sphere_factor_256) >> 8;
  
  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;
  
  return (r << 11) | (g << 5) | b;
}
uint16_t render_3d_iris(int16_t x, int16_t y, int16_t iris_x, int16_t iris_y,
                       int32_t dist_iris_sq, int32_t iris_radius_sq, bool is_inner) {
  if (dist_iris_sq >= iris_radius_sq) {
    return is_inner ? current_iris_med_color : current_iris_dark_color;
  }
  
  int16_t dx = x - iris_x;
  int16_t dy = y - iris_y;
  uint8_t pattern = (abs(dx) + abs(dy) + (dx ^ dy)) & 0x1F;
  float pattern_brightness = 0.9f + (pattern / 160.0f);
  
  uint16_t base_color = is_inner ? current_iris_med_color : current_iris_dark_color;
  
  uint16_t r = ((base_color >> 11) & 0x1F) * pattern_brightness;
  uint16_t g = ((base_color >> 5) & 0x3F) * pattern_brightness;
  uint16_t b = (base_color & 0x1F) * pattern_brightness;
  
  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;
  
  return (r << 11) | (g << 5) | b;
}

bool init_i2c_safe() {
  Wire.end();
  delay(50);
  Wire.begin((uint8_t)I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, 100000);
  return true;
}

void onReceive(int num_bytes) {
  if (num_bytes == 0) return;
  
  i2c_master_detected = true;
  uint8_t cmd = Wire.read();
  num_bytes--;
  
  i2c_buffer_index = 0;
  i2c_buffer[i2c_buffer_index++] = cmd;
  
  while (Wire.available() && i2c_buffer_index < sizeof(i2c_buffer) && num_bytes > 0) {
    i2c_buffer[i2c_buffer_index++] = Wire.read();
    num_bytes--;
  }
  
  i2c_command_received = true;
  
  if (cmd != I2C_CMD_STATUS && cmd != I2C_CMD_WIFI_STATUS &&
      cmd != I2C_CMD_GET_MODE && cmd != I2C_CMD_WIFI_SSID &&
      cmd != I2C_CMD_WIFI_PASS && cmd != I2C_CMD_WIFI_FORGET &&
      cmd != I2C_CMD_WIFI_CONNECT && cmd != I2C_CMD_RESET &&
      cmd != I2C_CMD_IDLE && cmd != I2C_CMD_RESET_DEVICE) {
    i2c_external_control = true;
  }
}

void onRequest() {
  if (!i2c_initialized) return;
  
  uint8_t last_cmd = i2c_buffer[0];
  
  if (last_cmd == I2C_CMD_WIFI_STATUS) {
    uint8_t s = 0;
    if (wifi_connected) s = 2;
    else if (wifi_connecting) s = 1;
    Wire.write(s);
    if (wifi_connected) {
      IPAddress ip = WiFi.localIP();
      Wire.write(ip[0]);
      Wire.write(ip[1]);
      Wire.write(ip[2]);
      Wire.write(ip[3]);
    } else {
      Wire.write(0); Wire.write(0); Wire.write(0); Wire.write(0);
    }
    return;
  }

  if (last_cmd == I2C_CMD_GET_MODE) {
    Wire.write(sprite_mode ? 1 : 0);
    return;
  }
  
  Wire.write((uint8_t)(i2c_look_x & 0xFF));
  Wire.write((uint8_t)((i2c_look_x >> 8) & 0xFF));
  Wire.write((uint8_t)(i2c_look_y & 0xFF));
  Wire.write((uint8_t)((i2c_look_y >> 8) & 0xFF));
  Wire.write((uint8_t)(i2c_squint_level * 255));
  Wire.write(i2c_external_control ? 1 : 0);
  Wire.write(sprite_mode ? 1 : 0);
  bool autonomous = sprite_mode ? false : !i2c_external_control;
  Wire.write(autonomous ? 1 : 0);
  uint8_t ota_flags = (ota_display_active ? 1 : 0) | (waiting_display_shown ? 2 : 0);
  Wire.write(ota_flags);
}

void process_i2c_command() {
  if (!i2c_initialized || !i2c_command_received) return;
  
  uint8_t cmd = i2c_buffer[0];
  
  switch (cmd) {
    case I2C_CMD_LOOK:
      if (i2c_buffer_index >= 5) {
        i2c_look_x = (int16_t)((i2c_buffer[2] << 8) | i2c_buffer[1]);
        i2c_look_y = (int16_t)((i2c_buffer[4] << 8) | i2c_buffer[3]);
      }
      break;
      
    case I2C_CMD_BLINK:
      if (i2c_buffer_index >= 3) {
        i2c_blink_duration = (uint16_t)((i2c_buffer[2] << 8) | i2c_buffer[1]);
        i2c_blink_trigger = true;
      }
      break;
      
    case I2C_CMD_SQUINT:
      if (i2c_buffer_index >= 2) {
        i2c_squint_level = i2c_buffer[1] / 255.0f;
      }
      break;
      
    case I2C_CMD_CURVE_PARAMS:
      if (i2c_buffer_index >= 4) {
        curve_falloff = i2c_buffer[1] / 255.0f;
        curve_minimum = i2c_buffer[2] / 255.0f;
        closure_strength = i2c_buffer[3] / 255.0f;
      }
      break;
      
    case I2C_CMD_RESET:
      i2c_external_control = false;
      i2c_look_x = 0;
      i2c_look_y = 0;
      i2c_squint_level = 0.0f;
      i2c_blink_trigger = false;
      auto_blink_enabled = false;
      sprite_mode = false;
      current_sprite = -1;
      current_sclera_color = COLOR_SCLERA;
      current_iris_dark_color = COLOR_IRIS_DARK;
      current_iris_med_color = COLOR_IRIS_MED;
      curve_falloff = 0.05f;
      curve_minimum = 0.6f;
      closure_strength = 1.00f;
      break;

    case I2C_CMD_RESET_DEVICE:
      esp_restart();
      break;
      
    case I2C_CMD_SCLERA_RGB:
      if (i2c_buffer_index >= 3) {
        current_sclera_color = (uint16_t)(i2c_buffer[1] | (i2c_buffer[2] << 8));
      }
      break;
      
    case I2C_CMD_IRIS_RGB:
      if (i2c_buffer_index >= 3) {
        uint16_t rgb = (uint16_t)(i2c_buffer[1] | (i2c_buffer[2] << 8));
        current_iris_med_color = rgb;
        uint8_t r = ((rgb >> 11) & 0x1F) * 0.6f;
        uint8_t g = ((rgb >> 5) & 0x3F) * 0.6f;
        uint8_t b = (rgb & 0x1F) * 0.6f;
        current_iris_dark_color = (r << 11) | (g << 5) | b;
      }
      break;
      
    case I2C_CMD_AUTO_BLINK:
      if (i2c_buffer_index >= 2) {
        auto_blink_enabled = i2c_buffer[1] != 0;
      }
      break;

    case I2C_CMD_AUTO_BLINK_SPEED:
      if (i2c_buffer_index >= 3) {
        auto_blink_interval = (uint16_t)(i2c_buffer[1] | (i2c_buffer[2] << 8));
      }
      break;

    case I2C_CMD_SPRITE_MODE:
      if (i2c_buffer_index >= 2) {
        sprite_mode = i2c_buffer[1] != 0;
      }
      break;

    case I2C_CMD_GET_MODE:
      break;

    case I2C_CMD_SET_SPRITE:
      if (i2c_buffer_index >= 2) {
        if (i2c_buffer[1] == 255) {
          current_sprite = -1;
        } else if (i2c_buffer[1] < sprite_count) {
          current_sprite = i2c_buffer[1];
        }
      }
      break;

    case I2C_CMD_IDLE:
      i2c_external_control = false;
      break;
      
    case I2C_CMD_JUMP:
      if (i2c_buffer_index >= 5) {
        i2c_look_x = (int16_t)((i2c_buffer[2] << 8) | i2c_buffer[1]);
        i2c_look_y = (int16_t)((i2c_buffer[4] << 8) | i2c_buffer[3]);
        current_look_x = i2c_look_x;
        current_look_y = i2c_look_y;
      }
      break;
      
    case I2C_CMD_SMOOTHING:
      if (i2c_buffer_index >= 2) {
        i2c_smoothing = i2c_buffer[1] / 255.0f;
      }
      break;
      
    case I2C_CMD_WIFI_SSID: {
      if (i2c_buffer_index > 1) {
        wifi_ssid = String((const char*)(i2c_buffer + 1));
        Preferences prefs;
        prefs.begin("eye", false);
        prefs.putString("wifi_ssid", wifi_ssid);
        prefs.end();
      }
      break;
    }
    
    case I2C_CMD_WIFI_PASS: {
      if (i2c_buffer_index > 1) {
        wifi_pass = String((const char*)(i2c_buffer + 1));
        Preferences prefs;
        prefs.begin("eye", false);
        prefs.putString("wifi_pass", wifi_pass);
        prefs.end();
      }
      break;
    }
    
    case I2C_CMD_WIFI_CONNECT:
      if (wifi_ssid.length() == 0) {
        Preferences prefs;
        prefs.begin("eye", true);
        wifi_ssid = prefs.getString("wifi_ssid", "");
        wifi_pass = prefs.getString("wifi_pass", "");
        prefs.end();
      }
      if (wifi_ssid.length() > 0) {
        wifi_should_connect = true;
      }
      break;
      
    case I2C_CMD_WIFI_FORGET: {
      wifi_ssid = "";
      wifi_pass = "";
      wifi_should_connect = false;
      wifi_connecting = false;
      wifi_connected = false;
      Preferences prefs;
      prefs.begin("eye", false);
      prefs.remove("wifi_ssid");
      prefs.remove("wifi_pass");
      prefs.end();
      if (WiFi.isConnected()) {
        WiFi.disconnect(true, true);
      }
      break;
    }
      
    case I2C_CMD_WIFI_STATUS:
      break;
  }
  
  i2c_command_received = false;
}

void send_command(uint8_t cmd) {
  digitalWrite(SHARED_DC, LOW);
  hspi->beginTransaction(spi_settings);
  hspi->transfer(cmd);
  hspi->endTransaction();
}

void send_data(uint8_t data) {
  digitalWrite(SHARED_DC, HIGH);
  hspi->beginTransaction(spi_settings);
  hspi->transfer(data);
  hspi->endTransaction();
}

void send_frame_buffer(uint8_t cs_pin, uint16_t *buffer) {
  select_display(cs_pin);
  set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

  digitalWrite(SHARED_DC, HIGH);
  hspi->beginTransaction(spi_settings);
  hspi->writePixels((uint8_t *)buffer, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
  hspi->endTransaction();

  deselect_all();
  delayMicroseconds(100);
}

void select_display(uint8_t cs_pin) {
  digitalWrite(LEFT_EYE_CS, HIGH);
  digitalWrite(RIGHT_EYE_CS, HIGH);
  delayMicroseconds(50);
  digitalWrite(cs_pin, LOW);
  delayMicroseconds(50);
}

void deselect_all() {
  digitalWrite(LEFT_EYE_CS, HIGH);
  digitalWrite(RIGHT_EYE_CS, HIGH);
  delayMicroseconds(50);
}

void init_gc9a01() {
  send_command(0xEF);
  send_command(0xEB);
  send_data(0x14);
  send_command(0xFE);
  send_command(0xEF);
  send_command(0xEB);
  send_data(0x14);
  send_command(0x84);
  send_data(0x40);
  send_command(0x85);
  send_data(0xFF);
  send_command(0x86);
  send_data(0xFF);
  send_command(0x87);
  send_data(0xFF);
  send_command(0x88);
  send_data(0x0A);
  send_command(0x89);
  send_data(0x21);
  send_command(0x8A);
  send_data(0x00);
  send_command(0x8B);
  send_data(0x80);
  send_command(0x8C);
  send_data(0x01);
  send_command(0x8D);
  send_data(0x01);
  send_command(0x8E);
  send_data(0xFF);
  send_command(0x8F);
  send_data(0xFF);
  send_command(0xB6);
  send_data(0x00);
  send_data(0x20);
  send_command(0x3A);
  send_data(0x05);
  send_command(0x90);
  send_data(0x08);
  send_data(0x08);
  send_data(0x08);
  send_data(0x08);
  send_command(0xBD);
  send_data(0x06);
  send_command(0xBC);
  send_data(0x00);
  send_command(0xFF);
  send_data(0x60);
  send_data(0x01);
  send_data(0x04);
  send_command(0xC3);
  send_data(0x13);
  send_command(0xC4);
  send_data(0x13);
  send_command(0xC9);
  send_data(0x22);
  send_command(0xBE);
  send_data(0x11);
  send_command(0xE1);
  send_data(0x10);
  send_data(0x0E);
  send_command(0xDF);
  send_data(0x21);
  send_data(0x0c);
  send_data(0x02);
  send_command(0xF0);
  send_data(0x45);
  send_data(0x09);
  send_data(0x08);
  send_data(0x08);
  send_data(0x26);
  send_data(0x2A);
  send_command(0xF1);
  send_data(0x43);
  send_data(0x70);
  send_data(0x72);
  send_data(0x36);
  send_data(0x37);
  send_data(0x6F);
  send_command(0xF2);
  send_data(0x45);
  send_data(0x09);
  send_data(0x08);
  send_data(0x08);
  send_data(0x26);
  send_data(0x2A);
  send_command(0xF3);
  send_data(0x43);
  send_data(0x70);
  send_data(0x72);
  send_data(0x36);
  send_data(0x37);
  send_data(0x6F);
  send_command(0xED);
  send_data(0x1B);
  send_data(0x0B);
  send_command(0xAE);
  send_data(0x77);
  send_command(0xCD);
  send_data(0x63);
  send_command(0x70);
  send_data(0x07);
  send_data(0x07);
  send_data(0x04);
  send_data(0x0E);
  send_data(0x0F);
  send_data(0x09);
  send_data(0x07);
  send_data(0x08);
  send_data(0x03);
  send_command(0xE8);
  send_data(0x34);
  send_command(0x62);
  send_data(0x18);
  send_data(0x0D);
  send_data(0x71);
  send_data(0xED);
  send_data(0x70);
  send_data(0x70);
  send_data(0x18);
  send_data(0x0F);
  send_data(0x71);
  send_data(0xEF);
  send_data(0x70);
  send_data(0x70);
  send_command(0x63);
  send_data(0x18);
  send_data(0x11);
  send_data(0x71);
  send_data(0xF1);
  send_data(0x70);
  send_data(0x70);
  send_data(0x18);
  send_data(0x13);
  send_data(0x71);
  send_data(0xF3);
  send_data(0x70);
  send_data(0x70);
  send_command(0x64);
  send_data(0x28);
  send_data(0x29);
  send_data(0xF1);
  send_data(0x01);
  send_data(0xF1);
  send_data(0x00);
  send_data(0x07);
  send_command(0x66);
  send_data(0x3C);
  send_data(0x00);
  send_data(0xCD);
  send_data(0x67);
  send_data(0x45);
  send_data(0x45);
  send_data(0x10);
  send_data(0x00);
  send_data(0x00);
  send_data(0x00);
  send_command(0x67);
  send_data(0x00);
  send_data(0x3C);
  send_data(0x00);
  send_data(0x00);
  send_data(0x00);
  send_data(0x01);
  send_data(0x54);
  send_data(0x10);
  send_data(0x32);
  send_data(0x98);
  send_command(0x74);
  send_data(0x10);
  send_data(0x85);
  send_data(0x80);
  send_data(0x00);
  send_data(0x00);
  send_data(0x4E);
  send_data(0x00);
  send_command(0x98);
  send_data(0x3e);
  send_data(0x07);
  send_command(0x35);
  send_command(0x21);
  send_command(GC9A01_MADCTL);
  send_data(0x08);
  send_command(GC9A01_SLPOUT);
  delay(120);
  send_command(GC9A01_DISPON);
  delay(20);
}

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  uint8_t col_data[] = {(uint8_t)((x0 >> 8) & 0xFF), (uint8_t)(x0 & 0xFF), 
                        (uint8_t)((x1 >> 8) & 0xFF), (uint8_t)(x1 & 0xFF)};
  uint8_t row_data[] = {(uint8_t)((y0 >> 8) & 0xFF), (uint8_t)(y0 & 0xFF), 
                        (uint8_t)((y1 >> 8) & 0xFF), (uint8_t)(y1 & 0xFF)};

  hspi->beginTransaction(spi_settings);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(GC9A01_CASET);
  digitalWrite(SHARED_DC, HIGH);
  hspi->transferBytes(col_data, nullptr, 4);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(GC9A01_RASET);
  digitalWrite(SHARED_DC, HIGH);
  hspi->transferBytes(row_data, nullptr, 4);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(GC9A01_RAMWR);

  hspi->endTransaction();
}

void clear_frame_buffer(uint16_t *buffer, uint16_t color) {
  uint32_t color32 = (uint32_t(color) << 16) | color;
  uint32_t *buffer32 = reinterpret_cast<uint32_t *>(buffer);
  uint32_t count = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 2;

  for (uint32_t i = 0; i < count; i++) {
    buffer32[i] = color32;
  }
}

void draw_skeleton_eye_buffered(uint16_t *buffer, int16_t look_x,
                                int16_t look_y, float blink_amount, float squint_amount,
                                bool mirror) {
  int16_t center_x = DISPLAY_WIDTH / 2;
  int16_t center_y = DISPLAY_HEIGHT / 2;
  int16_t socket_radius = 140;
  int16_t sclera_radius = 110;
  int16_t iris_radius = 45;
  int16_t pupil_radius = 22;

  int16_t effective_look_x = mirror ? -look_x : look_x;
  int16_t effective_look_y = mirror ? -look_y : look_y;

  int16_t max_iris_offset = sclera_radius - iris_radius - 5;
  int16_t iris_x =
      center_x + constrain(effective_look_x, -max_iris_offset, max_iris_offset);
  int16_t iris_y =
      center_y + constrain(effective_look_y, -max_iris_offset, max_iris_offset);

  int16_t pupil_x = iris_x;
  int16_t pupil_y = iris_y;

  int32_t socket_radius_sq = (int32_t)socket_radius * socket_radius;
  int32_t sclera_radius_sq = (int32_t)sclera_radius * sclera_radius;
  int32_t iris_radius_sq = (int32_t)iris_radius * iris_radius;
  int32_t pupil_radius_sq = (int32_t)pupil_radius * pupil_radius;
  int32_t inner_iris_radius_sq = (int32_t)(iris_radius - 8) * (iris_radius - 8);
  int32_t reflection_radius_sq = 4 * 4;

    float blink_eyelid = blink_amount * sclera_radius;
   float squint_eyelid = squint_amount * sclera_radius * closure_strength;
   float total_eyelid_closure = blink_eyelid + squint_eyelid;
   if (total_eyelid_closure > sclera_radius) total_eyelid_closure = sclera_radius;

   int16_t visible_left = 0;
   int16_t visible_right = DISPLAY_WIDTH - 1;
   int16_t visible_top = 0;
   int16_t visible_bottom = DISPLAY_HEIGHT - 1;

   if (total_eyelid_closure > 0.0f) {
     int16_t eyelid_closure = (int16_t)total_eyelid_closure;
     
     visible_left = center_x - sclera_radius + eyelid_closure;
     visible_right = center_x + sclera_radius - eyelid_closure;
     
     visible_top = 0;
     visible_bottom = DISPLAY_HEIGHT - 1;

     if (visible_left + 2 >= visible_right) {
        clear_frame_buffer(buffer, COLOR_BLACK);
        return;
      }
    }

   clear_frame_buffer(buffer, COLOR_BLACK);

   uint16_t *pixel_ptr = buffer;

  for (int16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    int32_t dy_center = y - center_y;
    int32_t dy_iris = y - iris_y;
    int32_t dy_pupil = y - pupil_y;
    int32_t dy_reflection = y - (pupil_y - 6);

    int32_t dy_center_sq = dy_center * dy_center;
    int32_t dy_iris_sq = dy_iris * dy_iris;
    int32_t dy_pupil_sq = dy_pupil * dy_pupil;
    int32_t dy_reflection_sq = dy_reflection * dy_reflection;

    for (int16_t x = 0; x < DISPLAY_WIDTH; x++, pixel_ptr++) {
      uint16_t color = COLOR_BLACK;

       bool in_visible_area = true;
       
        if (total_eyelid_closure > 0.0f) {
         int16_t distance_from_center_y = abs(y - center_y);
         float normalized_distance = (float)distance_from_center_y / sclera_radius;
         float curve_factor = 1.0f + (normalized_distance * normalized_distance * curve_falloff);
         if (curve_factor > (1.0f + curve_minimum)) curve_factor = 1.0f + curve_minimum;
          
           int16_t curved_eyelid_closure = (int16_t)(total_eyelid_closure * curve_factor);
         int16_t curved_left = center_x - sclera_radius + curved_eyelid_closure;
         int16_t curved_right = center_x + sclera_radius - curved_eyelid_closure;
         
         in_visible_area = (x >= curved_left && x <= curved_right);
       }

       if (!in_visible_area && total_eyelid_closure > 0.0f) {
        *pixel_ptr = COLOR_BLACK;
        continue;
      }

      int32_t dx_center = x - center_x;
      int32_t dist_center_sq = dx_center * dx_center + dy_center_sq;

      lighting_t effective_lighting = eye_lighting;
      int16_t effective_dx_center = dx_center;
      int16_t effective_dy_center = dy_center;

      uint32_t safe_socket_radius_sq = socket_radius_sq + 50;
      bool is_socket_area = (dist_center_sq <= safe_socket_radius_sq);
      bool is_sclera_area = (dist_center_sq <= sclera_radius_sq);
      
      if (is_socket_area) {
          color = COLOR_BLACK;

          if (is_sclera_area) {
            color = apply_3d_lighting(current_sclera_color, dist_center_sq, sclera_radius_sq, 
                                    effective_dx_center, effective_dy_center, &effective_lighting);

           int32_t dx_iris = x - iris_x;
           int32_t dist_iris_sq = dx_iris * dx_iris + dy_iris_sq;
           int16_t effective_dx_iris = mirror ? -dx_iris : dx_iris;

           if (dist_iris_sq <= iris_radius_sq) {
              bool is_inner = (dist_iris_sq <= inner_iris_radius_sq);
              color = render_3d_iris(x, y, iris_x, iris_y, dist_iris_sq, iris_radius_sq, is_inner);
              color = apply_3d_lighting(color, dist_iris_sq, iris_radius_sq, 
                                       effective_dx_iris, dy_iris, &effective_lighting);

             int32_t dx_pupil = x - pupil_x;
             int32_t dist_pupil_sq = dx_pupil * dx_pupil + dy_pupil_sq;

             if (dist_pupil_sq <= pupil_radius_sq) {
                float pupil_depth = 1.0f - (float)dist_pupil_sq / pupil_radius_sq;
                uint16_t pupil_brightness = (uint16_t)(10 + pupil_depth * 15);
                
                uint16_t pupil_r = (pupil_brightness >> 2) & 0x1F;
                uint16_t pupil_g = (pupil_brightness >> 1) & 0x3F;  
                uint16_t pupil_b = (pupil_brightness >> 2) & 0x1F;
               color = (pupil_r << 11) | (pupil_g << 5) | pupil_b;

               int16_t reflection_offset_x = mirror ? 6 : -6;
               int32_t dx_reflection = x - (pupil_x + reflection_offset_x);
               int32_t dist_reflection_sq = dx_reflection * dx_reflection + dy_reflection_sq;

               if (dist_reflection_sq <= reflection_radius_sq) {
                  color = COLOR_WHITE;
                } else {
                  int16_t reflection2_offset_x = mirror ? -3 : 3;
                  int32_t dx_reflection2 = x - (pupil_x + reflection2_offset_x);
                  int32_t dy_reflection2 = y - (pupil_y + 2);
                  int32_t dist_reflection2_sq = dx_reflection2 * dx_reflection2 + dy_reflection2 * dy_reflection2;
                  
                  if (dist_reflection2_sq <= 4) {
                    uint16_t r = ((color >> 11) & 0x1F) + 8;
                    uint16_t g = ((color >> 5) & 0x3F) + 16;
                    uint16_t b = (color & 0x1F) + 8;
                    
                    r = r > 0x1F ? 0x1F : r;
                    g = g > 0x3F ? 0x3F : g;
                    b = b > 0x1F ? 0x1F : b;
                    
                    color = (r << 11) | (g << 5) | b;
             }
           }
         }
            }
          }
          }

        if (is_sclera_area) {
         uint32_t boundary_threshold = sclera_radius_sq - 50;
         if (dist_center_sq >= boundary_threshold) {
            color = COLOR_BLACK;
          }
        }

        *pixel_ptr = color;
    }
  }
}

void decompress_rle(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size) {
  uint32_t buffer_pos = 0;
  uint16_t data_pos = 0;
  const uint32_t max_pixels = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  
  while (buffer_pos < max_pixels && data_pos + 2 < size) {
    uint8_t count = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_low = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_high = pgm_read_byte_near(compressed_data + data_pos++);
    uint16_t color = (color_high << 8) | color_low;
    
    uint32_t pixels_to_write = min((uint32_t)count, max_pixels - buffer_pos);
    for (uint32_t i = 0; i < pixels_to_write; i++) {
      buffer[buffer_pos++] = color;
    }
    if (count > pixels_to_write) {
      break;
    }
  }
}

void decompress_rle_upscale(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size) {
  const uint16_t src_w = 120;
  const uint16_t src_h = 120;
  uint16_t src_pos = 0;
  uint16_t data_pos = 0;
  const uint32_t max_src = src_w * src_h;

  while (src_pos < max_src && data_pos + 2 < size) {
    uint8_t count = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_low = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_high = pgm_read_byte_near(compressed_data + data_pos++);
    uint16_t color = (color_high << 8) | color_low;

    uint32_t pixels_to_write = min((uint32_t)count, max_src - src_pos);
    for (uint32_t i = 0; i < pixels_to_write; i++) {
      uint16_t sx = src_pos % src_w;
      uint16_t sy = src_pos / src_w;
      uint16_t dx = sx * 2;
      uint16_t dy = sy * 2;
      buffer[dy * DISPLAY_WIDTH + dx] = color;
      buffer[dy * DISPLAY_WIDTH + dx + 1] = color;
      buffer[(dy + 1) * DISPLAY_WIDTH + dx] = color;
      buffer[(dy + 1) * DISPLAY_WIDTH + dx + 1] = color;
      src_pos++;
    }
    if (count > pixels_to_write) {
      break;
    }
  }
}

void decompress_palette(const uint8_t* compressed_data, const uint8_t* palette_data, 
                       uint16_t* buffer, uint8_t bits_per_pixel, uint16_t size) {
  uint16_t buffer_pos = 0;
  uint16_t data_pos = 0;
  uint8_t pixels_per_byte = 8 / bits_per_pixel;
  uint8_t pixel_mask = (1 << bits_per_pixel) - 1;
  
  while (buffer_pos < DISPLAY_WIDTH * DISPLAY_HEIGHT && data_pos < size) {
    uint8_t packed_pixels = pgm_read_byte_near(compressed_data + data_pos++);
    
    for (uint8_t i = 0; i < pixels_per_byte && buffer_pos < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
      uint8_t pixel_index = (packed_pixels >> (i * bits_per_pixel)) & pixel_mask;
      uint16_t color = pgm_read_word_near(palette_data + pixel_index * 2);
      buffer[buffer_pos++] = color;
    }
  }
}

void decompress_sprite(const SpriteFrame* sprite, uint16_t* buffer) {
  if (!sprite) return;
  
  const SpriteHeader* header = &sprite->header;
  
  clear_frame_buffer(buffer, COLOR_BLACK);
  
  switch (header->compression_type) {
    case SPRITE_UNCOMPRESSED:
      for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        buffer[i] = pgm_read_word_near(sprite->compressed_data + i * 2);
      }
      break;
      
    case SPRITE_RLE_COMPRESSED:
      if (sprite->upscale) {
        decompress_rle_upscale(sprite->compressed_data, buffer, header->compressed_size);
      } else {
        decompress_rle(sprite->compressed_data, buffer, header->compressed_size);
      }
      break;
      
    case SPRITE_PALETTE_4BIT:
      decompress_palette(sprite->compressed_data, sprite->palette_data, 
                        buffer, 4, header->compressed_size);
      break;
      
    case SPRITE_PALETTE_8BIT:
      decompress_palette(sprite->compressed_data, sprite->palette_data, 
                        buffer, 8, header->compressed_size);
      break;
  }
}

void apply_sprite_blink_overlay(uint16_t* buffer, float blink_amount) {
  if (blink_amount <= 0.0) return;
  
  uint16_t center_x = DISPLAY_WIDTH / 2;
  uint16_t blink_width = (uint16_t)(blink_amount * center_x);
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < blink_width; x++) {
      buffer[y * DISPLAY_WIDTH + x] = COLOR_BLACK;
      buffer[y * DISPLAY_WIDTH + (DISPLAY_WIDTH - 1 - x)] = COLOR_BLACK;
    }
  }
}

void render_sprite_frame(uint8_t frame_index, uint16_t* left_buffer, uint16_t* right_buffer) {
  if (frame_index >= sprite_count) return;
  
  const SpriteFrame* sprite = &sprite_frames[frame_index];
  
  decompress_sprite(sprite, left_buffer);
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
      uint16_t src_index = y * DISPLAY_WIDTH + x;
      uint16_t dst_x = y;
      uint16_t dst_y = DISPLAY_WIDTH - 1 - x;
      uint16_t dst_index = dst_y * DISPLAY_WIDTH + dst_x;
      right_buffer[dst_index] = left_buffer[src_index];
    }
  }
  
  for (uint16_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    left_buffer[i] = right_buffer[i];
  }
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
      uint16_t src_index = y * DISPLAY_WIDTH + x;
      uint16_t dst_x = DISPLAY_WIDTH - 1 - x;
      uint16_t dst_y = DISPLAY_HEIGHT - 1 - y;
      uint16_t dst_index = dst_y * DISPLAY_WIDTH + dst_x;
      right_buffer[dst_index] = left_buffer[src_index];
    }
  }
}

void load_sprite_data() {
  sprite_count = 0;
  
  uint8_t n = NUM_SPRITES < MAX_SPRITE_FRAMES ? NUM_SPRITES : MAX_SPRITE_FRAMES;
  for (uint8_t i = 0; i < n; i++) {
    sprite_frames[i].header.compression_type = SPRITE_RLE_COMPRESSED;
    sprite_frames[i].header.bits_per_pixel = 16;
    sprite_frames[i].header.compressed_size = sprite_sizes[i];
    sprite_frames[i].header.palette_size = 0;
    sprite_frames[i].header.frame_id = i;
    sprite_frames[i].palette_data = nullptr;
    sprite_frames[i].compressed_data = sprite_data[i];
    sprite_frames[i].upscale = sprite_upscale[i];
    sprite_count++;
  }
  
  Serial.printf("Loaded %d sprites successfully\n", sprite_count);
}

#define FONT_W 8
#define FONT_H 8

static const uint8_t font_space[FONT_H] = {0};

static const uint8_t font_A[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11111111, 0b11000011, 0b11000011, 0b11000011,
};
static const uint8_t font_O[FONT_H] = {
  0b00111100, 0b01111110, 0b11100111, 0b11000011,
  0b11000011, 0b11100111, 0b01111110, 0b00111100,
};
static const uint8_t font_T[FONT_H] = {
  0b11111111, 0b11111111, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b00011000,
};

static const uint8_t font_a[FONT_H] = {
  0b00000000, 0b00111100, 0b01000010, 0b01111110,
  0b01000010, 0b01000010, 0b00111100, 0b00000000,
};
static const uint8_t font_d[FONT_H] = {
  0b00000010, 0b00000010, 0b00111110, 0b01000010,
  0b01000010, 0b01000010, 0b00111110, 0b00000000,
};
static const uint8_t font_e[FONT_H] = {
  0b00000000, 0b00111100, 0b01000010, 0b01111110,
  0b01000000, 0b01000000, 0b00111100, 0b00000000,
};
static const uint8_t font_f[FONT_H] = {
  0b00001100, 0b00010010, 0b00011110, 0b00010000,
  0b00010000, 0b00010000, 0b00010000, 0b00000000,
};
static const uint8_t font_g[FONT_H] = {
  0b00000000, 0b00111100, 0b01000010, 0b01000010,
  0b00111110, 0b00000010, 0b00111100, 0b00000000,
};
static const uint8_t font_i[FONT_H] = {
  0b00010000, 0b00000000, 0b00010000, 0b00010000,
  0b00010000, 0b00010000, 0b00010000, 0b00000000,
};
static const uint8_t font_n[FONT_H] = {
  0b00000000, 0b01110010, 0b01001010, 0b01001010,
  0b01000010, 0b01000010, 0b01000010, 0b00000000,
};
static const uint8_t font_o[FONT_H] = {
  0b00000000, 0b00111100, 0b01000010, 0b01000010,
  0b01000010, 0b01000010, 0b00111100, 0b00000000,
};
static const uint8_t font_p[FONT_H] = {
  0b00000000, 0b00111100, 0b01000010, 0b01111100,
  0b01000000, 0b01000000, 0b01000000, 0b00000000,
};
static const uint8_t font_r[FONT_H] = {
  0b00000000, 0b01110100, 0b01001100, 0b01000000,
  0b01000000, 0b01000000, 0b01000000, 0b00000000,
};
static const uint8_t font_t[FONT_H] = {
  0b00010000, 0b00010000, 0b01111100, 0b00010000,
  0b00010000, 0b00010000, 0b00001100, 0b00000000,
};
static const uint8_t font_u[FONT_H] = {
  0b00000000, 0b01000010, 0b01000010, 0b01000010,
  0b01000010, 0b01000010, 0b00111110, 0b00000000,
};
static const uint8_t font_w[FONT_H] = {
  0b10000001, 0b11000011, 0b11000011, 0b10100101,
  0b10011001, 0b10011001, 0b10000001, 0b00000000,
};

static const uint8_t font_G[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000000,
  0b11001111, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_I[FONT_H] = {
  0b01111110, 0b00011000, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b01111110,
};
static const uint8_t font_N[FONT_H] = {
  0b10000001, 0b11000001, 0b10100001, 0b10010001,
  0b10001001, 0b10000101, 0b10000011, 0b10000001,
};
static const uint8_t font_W[FONT_H] = {
  0b11000011, 0b11000011, 0b11000011, 0b11011011,
  0b11011011, 0b11100111, 0b11000011, 0b10000001,
};

static const uint8_t font_0[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11000011, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_1[FONT_H] = {
  0b00011000, 0b00111000, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b00111100,
};
static const uint8_t font_2[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b00000110,
  0b00001100, 0b00110000, 0b01100000, 0b11111111,
};
static const uint8_t font_3[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b00000110,
  0b00011100, 0b00000110, 0b11000011, 0b01111110,
};
static const uint8_t font_4[FONT_H] = {
  0b00001100, 0b00011100, 0b00101100, 0b01001100,
  0b11001100, 0b11111111, 0b00001100, 0b00001100,
};
static const uint8_t font_5[FONT_H] = {
  0b11111111, 0b11000000, 0b11000000, 0b11111100,
  0b00000110, 0b00000011, 0b11000011, 0b01111110,
};
static const uint8_t font_6[FONT_H] = {
  0b00011110, 0b00110000, 0b01100000, 0b11111100,
  0b11000110, 0b11000011, 0b11000011, 0b01111110,
};
static const uint8_t font_7[FONT_H] = {
  0b11111111, 0b11111111, 0b00000110, 0b00001100,
  0b00011000, 0b00110000, 0b01100000, 0b01100000,
};
static const uint8_t font_8[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b01111110,
  0b01111110, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_9[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b01111110, 0b00000110, 0b00001100, 0b01111000,
};
static const uint8_t font_pct[FONT_H] = {
  0b00000000, 0b01100010, 0b01100100, 0b00001000,
  0b00010000, 0b00100110, 0b01000110, 0b00000000,
};
static const uint8_t font_dot[FONT_H] = {
  0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00011000, 0b00011000,
};

const uint8_t* get_font_bitmap(char c) {
  switch (c) {
    case ' ': return font_space;
    case 'a': return font_a;
    case 'd': return font_d;
    case 'e': return font_e;
    case 'f': return font_f;
    case 'g': return font_g;
    case 'i': return font_i;
    case 'n': return font_n;
    case 'o': return font_o;
    case 'p': return font_p;
    case 'r': return font_r;
    case 't': return font_t;
    case 'u': return font_u;
    case 'w': return font_w;
    case 'A': return font_A;
    case 'G': return font_G;
    case 'I': return font_I;
    case 'N': return font_N;
    case 'O': return font_O;
    case 'T': return font_T;
    case 'W': return font_W;
    case '%': return font_pct;
    case '.': return font_dot;
    case '0': return font_0;
    case '1': return font_1;
    case '2': return font_2;
    case '3': return font_3;
    case '4': return font_4;
    case '5': return font_5;
    case '6': return font_6;
    case '7': return font_7;
    case '8': return font_8;
    case '9': return font_9;
    default:  return font_space;
  }
}

void draw_char_scaled_rotated(uint16_t *buffer, int x, int y,
                              const uint8_t *bitmap, int scale, bool rotate_ccw) {
  for (int row = 0; row < FONT_H; row++) {
    uint8_t bits = bitmap[row];
    for (int col = 0; col < FONT_W; col++) {
      if (!(bits & (0x80 >> col))) continue;
      int bx, by;
      if (rotate_ccw) {
        bx = row;
        by = FONT_W - 1 - col;
      } else {
        bx = col;
        by = row;
      }
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          int px = x + bx * scale + sx;
          int py = y + by * scale + sy;
          if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
            buffer[py * DISPLAY_WIDTH + px] = OTA_COLOR_TEXT;
          }
        }
      }
    }
  }
}

void draw_text_vertical_ccw(uint16_t *buffer, const char *text, int x, int y_top, int scale) {
  int char_h = FONT_H * scale;
  int len = strlen(text);
  for (int i = 0; i < len; i++) {
    int y = y_top + (len - 1 - i) * char_h;
    draw_char_scaled_rotated(buffer, x, y, get_font_bitmap(text[i]), scale, true);
  }
}

void ota_display_init() {
  ota_display_active = true;
  ota_fill_rows = 0;
  waiting_display_shown = false;
  
  clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
  clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);
  
  // "OTA" rotated 90° CCW, 2x scale, at x=25 (near top of physical display)
  int y_ota = (DISPLAY_HEIGHT - 3 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "OTA", 25, y_ota, 2);
  draw_text_vertical_ccw(right_eye_buffer, "OTA", 25, y_ota, 2);
  
  // Left eye is physically upside down — 180° rotate its buffer
  uint32_t npix = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = left_eye_buffer[i];
    left_eye_buffer[i] = left_eye_buffer[npix - 1 - i];
    left_eye_buffer[npix - 1 - i] = t;
  }
  
  send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
  send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
}

void ota_draw_percentage(int percent) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", percent);
  int len = strlen(buf);
  int scale = 2;
  int pad = 1;
  int cw = FONT_H * scale;
  int ch = FONT_W * scale;
  int tw = cw + 2 * pad;
  int th = len * ch + 2 * pad;
  int ox = (DISPLAY_WIDTH - tw) / 2;
  int oy = (DISPLAY_HEIGHT - th) / 2;

  static const int MAX_PCT_PIXELS = 20 * 96;
  uint16_t area[MAX_PCT_PIXELS];
  if (tw * th > MAX_PCT_PIXELS) return;

  for (int i = 0; i < tw * th; i++) area[i] = OTA_COLOR_BLACK;

  for (int i = 0; i < len; i++) {
    const uint8_t *bm = get_font_bitmap(buf[i]);
    int cy = pad + (len - 1 - i) * ch;
    for (int r = 0; r < FONT_H; r++) {
      uint8_t bits = bm[r];
      for (int c = 0; c < FONT_W; c++) {
        if (!(bits & (0x80 >> c))) continue;
        int bx = r;
        int by = FONT_W - 1 - c;
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            int px = pad + bx * scale + sx;
            int py = cy + by * scale + sy;
            if (px >= 0 && px < tw && py >= 0 && py < th) {
              area[py * tw + px] = OTA_COLOR_TEXT;
            }
          }
        }
      }
    }
  }

  for (int e = 0; e < 2; e++) {
    uint8_t cs = (e == 0) ? LEFT_EYE_CS : RIGHT_EYE_CS;
    select_display(cs);
    if (e == 0) {
      // Left eye is physically upside down:
      // 180° rotate window position and pixel data
      int ox2 = DISPLAY_WIDTH - ox - tw;
      int oy2 = DISPLAY_HEIGHT - oy - th;
      set_window(ox2, oy2, ox2 + tw - 1, oy2 + th - 1);
      digitalWrite(SHARED_DC, HIGH);
      hspi->beginTransaction(spi_settings);
      for (int i = tw * th - 1; i >= 0; i--) {
        hspi->write16(area[i]);
      }
    } else {
      set_window(ox, oy, ox + tw - 1, oy + th - 1);
      digitalWrite(SHARED_DC, HIGH);
      hspi->beginTransaction(spi_settings);
      for (int i = 0; i < tw * th; i++) {
        hspi->write16(area[i]);
      }
    }
    hspi->endTransaction();
    deselect_all();
  }
}

void ota_redraw_label() {
  const char *text = "OTA";
  int len = 3;
  int scale = 2;
  int pad = 1;
  int lw = FONT_H * scale + 2 * pad;
  int lh = len * FONT_W * scale + 2 * pad;
  int lx = 25 - pad;
  int ly = (DISPLAY_HEIGHT - lh) / 2;

  uint16_t area[lw * lh];
  for (int i = 0; i < lw * lh; i++) area[i] = OTA_COLOR_BLACK;

  for (int i = 0; i < len; i++) {
    const uint8_t *bm = get_font_bitmap(text[i]);
    int cy = pad + (len - 1 - i) * FONT_W * scale;
    for (int r = 0; r < FONT_H; r++) {
      uint8_t bits = bm[r];
      for (int c = 0; c < FONT_W; c++) {
        if (!(bits & (0x80 >> c))) continue;
        int bx = r;
        int by = FONT_W - 1 - c;
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            int px = pad + bx * scale + sx;
            int py = cy + by * scale + sy;
            if (px >= 0 && px < lw && py >= 0 && py < lh) {
              area[py * lw + px] = OTA_COLOR_TEXT;
            }
          }
        }
      }
    }
  }

  for (int e = 0; e < 2; e++) {
    uint8_t cs = (e == 0) ? LEFT_EYE_CS : RIGHT_EYE_CS;
    select_display(cs);
    if (e == 0) {
      int lx2 = DISPLAY_WIDTH - lx - lw;
      int ly2 = DISPLAY_HEIGHT - ly - lh;
      set_window(lx2, ly2, lx2 + lw - 1, ly2 + lh - 1);
      digitalWrite(SHARED_DC, HIGH);
      hspi->beginTransaction(spi_settings);
      for (int i = lw * lh - 1; i >= 0; i--) {
        hspi->write16(area[i]);
      }
    } else {
      set_window(lx, ly, lx + lw - 1, ly + lh - 1);
      digitalWrite(SHARED_DC, HIGH);
      hspi->beginTransaction(spi_settings);
      for (int i = 0; i < lw * lh; i++) {
        hspi->write16(area[i]);
      }
    }
    hspi->endTransaction();
    deselect_all();
  }
}

void ota_display_update(unsigned int progress, unsigned int total) {
  if (!ota_display_active) return;
  int target_rows = (int)(((float)progress / total) * DISPLAY_HEIGHT);
  if (target_rows <= ota_fill_rows) return;
  
  int fill_top = DISPLAY_HEIGHT - target_rows;
  int fill_bot = DISPLAY_HEIGHT - ota_fill_rows - 1;
  if (fill_top > fill_bot) return;
  
  for (int e = 0; e < 2; e++) {
    uint8_t cs = (e == 0) ? LEFT_EYE_CS : RIGHT_EYE_CS;
    select_display(cs);
    set_window(0, fill_top, DISPLAY_WIDTH - 1, fill_bot);
    digitalWrite(SHARED_DC, HIGH);
    hspi->beginTransaction(spi_settings);
    for (int i = 0; i < DISPLAY_WIDTH * (fill_bot - fill_top + 1); i++) {
      hspi->write16(OTA_COLOR_FILL);
    }
    hspi->endTransaction();
    deselect_all();
  }
  ota_fill_rows = target_rows;

  ota_draw_percentage((progress * 100) / total);
  ota_redraw_label();
}

void show_waiting_display() {
  if (ota_display_active) return;
  waiting_display_shown = true;
  
  clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
  clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);
  
  String ip = WiFi.localIP().toString();
  
  // Physical display is rotated 90° CW:
  //   framebuffer x → physical vertical (0=top, 239=bottom)
  //   framebuffer y → physical horizontal (0=right, 239=left)
  // 3 separate lines using different framebuffer x values:
  //   OTA:     x=25  → physical Y ≈ 25  (near top, safe from round edges)
  //   WAITING: x=75  → physical Y ≈ 75  (upper area)
  //   IP:      x=120 → physical Y ≈ 120 (middle area)
  // Each is vertically centered in framebuffer y (= physically centered).
  
  int y_ota = (DISPLAY_HEIGHT - 3 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "OTA", 25, y_ota, 2);
  draw_text_vertical_ccw(right_eye_buffer, "OTA", 25, y_ota, 2);
  
  int y_wait = (DISPLAY_HEIGHT - 7 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "WAITING", 75, y_wait, 2);
  draw_text_vertical_ccw(right_eye_buffer, "WAITING", 75, y_wait, 2);
  
  int y_ip = (DISPLAY_HEIGHT - ip.length() * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, ip.c_str(), 120, y_ip, 2);
  draw_text_vertical_ccw(right_eye_buffer, ip.c_str(), 120, y_ip, 2);
  
  // Left eye is physically upside down — 180° rotate its buffer
  uint32_t npix = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = left_eye_buffer[i];
    left_eye_buffer[i] = left_eye_buffer[npix - 1 - i];
    left_eye_buffer[npix - 1 - i] = t;
  }
  
  send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
  send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LEFT_EYE_CS, OUTPUT);
  pinMode(RIGHT_EYE_CS, OUTPUT);
  pinMode(SHARED_RST, OUTPUT);
  pinMode(SHARED_DC, OUTPUT);

  // MISO pin -1 to avoid default pin 13 conflict with LEFT_EYE_CS (IO13)
  hspi->begin(SHARED_SCL, 1, SHARED_SDA, -1);

  digitalWrite(SHARED_DC, LOW);
  digitalWrite(LEFT_EYE_CS, HIGH);
  digitalWrite(RIGHT_EYE_CS, HIGH);

  digitalWrite(SHARED_RST, HIGH);
  delay(10);
  digitalWrite(SHARED_RST, LOW);
  delay(50);
  digitalWrite(SHARED_RST, HIGH);
  delay(150);

  select_display(LEFT_EYE_CS);
  init_gc9a01();
  deselect_all();

  select_display(RIGHT_EYE_CS);
  init_gc9a01();
  deselect_all();

  load_sprite_data();

  randomSeed(analogRead(0));
  
  Serial.println("Eye display system ready!");
  
  i2c_initialized = init_i2c_safe();
  
  if (i2c_initialized) {
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.printf("I2C slave ready on address 0x%02X (SDA=%d, SCL=%d)\n", 
                  I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  } else {
    Serial.println("I2C slave initialization failed - continuing in autonomous mode");
  }
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.printf("OTA Update Start: %s\n", type.c_str());
    ota_display_init();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Update End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress * 100) / total);
    ota_display_update(progress, total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error: %u\n", error);
    ota_display_active = false;
  });
  Serial.println("OTA callbacks registered (started on WiFi connect)");
}

void loop() {
  static uint32_t last_blink = 0;
  static uint32_t last_look_change = 0;
  static uint32_t last_frame = 0;
  static uint32_t blink_start = 0;
  static bool blinking = false;
  static bool last_blinking = false;
  static int16_t last_look_x = -999;
  static int16_t last_look_y = -999;
  static float last_blink_amount = -1.0f;

  uint32_t now = millis();
  
  if (wifi_connected) {
    ArduinoOTA.handle();
  }
  
  if (wifi_should_connect && !wifi_connecting && !wifi_connected) {
    wifi_connecting = true;
    wifi_connect_start = now;
    Serial.printf("Connecting to WiFi: %s\n", wifi_ssid.c_str());
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  }
  if (wifi_connecting && !wifi_connected) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      wifi_connecting = false;
      Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
      ArduinoOTA.begin();
      show_waiting_display();
    } else if (now - wifi_connect_start > 15000) {
      wifi_connecting = false;
      wifi_should_connect = false;
      Serial.println("WiFi connection timeout");
    }
  }
  if (wifi_connected && WiFi.status() != WL_CONNECTED) {
    wifi_connected = false;
    waiting_display_shown = false;
    Serial.println("WiFi disconnected");
  }

  process_i2c_command();

  if (ota_display_active || waiting_display_shown) {
    last_frame = now;
    return;
  }

  if (now - last_frame < 1) { // prevent busy-waiting when idle
    return;
  }

  if (sprite_mode) {
    if (auto_blink_enabled && !blinking && (now - last_blink > random(auto_blink_interval / 2, auto_blink_interval * 3 / 2 + 1))) {
      blinking = true;
      blink_start = now;
      last_blink = now;
    }

    float blink_amount = 0.0f;
    if (blinking) {
      uint32_t blink_duration = 500;
      uint32_t blink_elapsed = now - blink_start;

      if (blink_elapsed < blink_duration) {
        float progress = (float)blink_elapsed / blink_duration;
        if (progress < 0.4f) {
          blink_amount = progress / 0.4f;
        } else if (progress < 0.6f) {
          blink_amount = 1.0f;
        } else {
          blink_amount = 1.0f - (progress - 0.6f) / 0.4f;
        }
      } else {
        blinking = false;
        blink_amount = 0.0f;
      }
    }

    if (current_sprite >= 0 && current_sprite < sprite_count) {
      render_sprite_frame(current_sprite, left_eye_buffer, right_eye_buffer);

      if (blink_amount > 0.0f) {
        apply_sprite_blink_overlay(left_eye_buffer, blink_amount);
        apply_sprite_blink_overlay(right_eye_buffer, blink_amount);
      }

      send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
      send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
    }
    
  } else {
    if (auto_blink_enabled && !blinking && (now - last_blink > random(auto_blink_interval / 2, auto_blink_interval * 3 / 2 + 1))) {
      blinking = true;
      blink_start = now;
      last_blink = now;
    }

    float blink_amount = 0.0f;
    if (blinking) {
      uint32_t blink_duration = 300;
      uint32_t blink_elapsed = now - blink_start;

      if (blink_elapsed < blink_duration) {
        float progress = (float)blink_elapsed / blink_duration;
        if (progress < 0.4f) {
          blink_amount = progress / 0.4f;
        } else if (progress < 0.6f) {
          blink_amount = 1.0f;
        } else {
          blink_amount = 1.0f - (progress - 0.6f) / 0.4f;
        }
      } else {
        blinking = false;
        blink_amount = 0.0f;
      }
    }

    if (i2c_blink_trigger) {
      blink_start = now;
      blinking = true;
      i2c_blink_trigger = false;
    }
    
    if (!i2c_external_control) {
      if (now - last_look_change > random(3000, 8000)) {
        target_look_x = random(-30, 31);
        target_look_y = random(-25, 26);
        last_look_change = now;
      }

      current_look_x += (target_look_x - current_look_x) * 0.1f;
      current_look_y += (target_look_y - current_look_y) * 0.1f;
    } else {
      current_look_x += (i2c_look_x - current_look_x) * i2c_smoothing;
      current_look_y += (i2c_look_y - current_look_y) * i2c_smoothing;
    }

    bool need_update = false;

    if (abs(current_look_x - last_look_x) > 1 ||
        abs(current_look_y - last_look_y) > 1) {
      need_update = true;
    }

    if (abs(blink_amount - last_blink_amount) > 0.05f) {
      need_update = true;
    }

    if (blinking != last_blinking) {
      need_update = true;
    }

    if (blinking) need_update = true;
    
    if (i2c_external_control) {
      global_squint = i2c_squint_level;
    }

    if (need_update) {
      draw_skeleton_eye_buffered(left_eye_buffer, current_look_x, current_look_y,
                                 blink_amount, global_squint + left_squint, false);
      draw_skeleton_eye_buffered(right_eye_buffer, current_look_x, current_look_y,
                                 blink_amount, global_squint + right_squint, true);

      send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
      send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);

      last_look_x = current_look_x;
      last_look_y = current_look_y;
      last_blink_amount = blink_amount;
      last_blinking = blinking;
    }
  }
  
  last_frame = now;
}
