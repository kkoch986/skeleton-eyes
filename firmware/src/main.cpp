/*
 * Skeleton Eye Display Controller
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "globals.h"
#include "display.h"
#include "eye_renderer.h"
#include "sprites.h"
#include "i2c_commands.h"
#include "ota_display.h"
#include "wifi_manager.h"

SPIClass *hspi = &SPI;
SPISettings spi_settings(80000000, MSBFIRST, SPI_MODE0);

uint16_t left_eye_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
uint16_t right_eye_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

SpriteFrame sprite_frames[MAX_SPRITE_FRAMES];
uint8_t sprite_count = 0;
int8_t current_sprite = -1;
bool sprite_mode = false;

lighting_t eye_lighting;
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

bool display_text_active = false;
uint16_t display_text_color = OTA_COLOR_TEXT;
uint16_t display_text_bg = OTA_COLOR_BLACK;

bool force_eye_repaint = false;

void setup() {
  Serial.begin(115200);
  delay(5000);

  pinMode(LEFT_EYE_CS, OUTPUT);
  pinMode(RIGHT_EYE_CS, OUTPUT);
  pinMode(SHARED_RST, OUTPUT);
  pinMode(SHARED_DC, OUTPUT);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  display_bus_init();

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

  renderer_init();
  init_i2c_system();
  wifi_init();
}

void loop() {
  uint32_t now = millis();

  static uint32_t last_heartbeat = 0;
  if (now - last_heartbeat > 5000) {
    last_heartbeat = now;
    Serial.printf("heartbeat i2c_init=%d master_seen=%d cmd_pending=%d\n",
                  i2c_initialized, i2c_master_detected, i2c_command_received);
  }

  wifi_update();
  process_i2c_command();
  boot_button_update();

  if (i2c_just_ready && now - i2c_ready_display_start > I2C_READY_DISPLAY_MS) {
    i2c_just_ready = false;
  }

  update_eyes();

  delay(1);
}
