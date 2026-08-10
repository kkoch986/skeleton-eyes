#include "i2c_commands.h"
#include "ota_display.h"
#include "sprites.h"
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

/* Ring buffer of incoming I2C commands. onReceive (ISR) enqueues every
 * command, process_i2c_command (loop) dequeues them one at a time, so no
 * command is lost while a render is in progress and multi-byte payloads
 * can't be torn by the ISR. */
#define I2C_CMD_QUEUE_SIZE 16
#define I2C_CMD_MAX_LEN    68

typedef struct {
  uint8_t data[I2C_CMD_MAX_LEN];
  uint8_t len;
} i2c_cmd_t;

static i2c_cmd_t i2c_cmd_queue[I2C_CMD_QUEUE_SIZE];
static volatile uint8_t i2c_cmd_head = 0;
static volatile uint8_t i2c_cmd_tail = 0;
static volatile uint8_t i2c_cmd_count = 0;
static portMUX_TYPE i2c_mux = portMUX_INITIALIZER_UNLOCKED;

void init_i2c_system() {
  while (!i2c_initialized) {
    i2c_initialized = init_i2c_safe();
    if (!i2c_initialized) {
      Serial.println("I2C init failed - retrying in 2s (waiting for bus clear)");
      delay(2000);
    }
  }

  if (i2c_initialized) {
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.printf("I2C slave ready on address 0x%02X (SDA=%d, SCL=%d)\n",
                  I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    digitalWrite(LED_PIN, HIGH);
    show_i2c_ready_display();
  }
}

bool init_i2c_safe() {
  Wire.end();
  delay(50);
  bool ok = Wire.begin((uint8_t)I2C_SLAVE_ADDRESS, (int)I2C_SDA, (int)I2C_SCL, (uint32_t)100000);
  if (ok) { Serial.println("I2C: slave init OK (direct)"); return true; }
  Serial.println("I2C: direct slave init failed, trying master-first...");

  Wire.end();
  delay(50);
  Wire.begin((int)I2C_SDA, (int)I2C_SCL, (uint32_t)100000);
  Wire.end();
  delay(50);
  ok = Wire.begin((uint8_t)I2C_SLAVE_ADDRESS, (int)I2C_SDA, (int)I2C_SCL, (uint32_t)100000);
  if (ok) { Serial.println("I2C: slave init OK (master-first)"); return true; }
  Serial.println("I2C: master-first failed, trying default pins...");

  Wire.end();
  delay(50);
  ok = Wire.begin((uint8_t)I2C_SLAVE_ADDRESS);
  if (ok) { Serial.println("I2C: slave init OK (default pins)"); return true; }
  Serial.println("I2C: all init attempts failed");

  return false;
}

void onReceive(int num_bytes) {
  if (num_bytes == 0) return;

  if (!i2c_master_detected) {
    Serial.println("I2C: master detected for the first time");
  }
  i2c_master_detected = true;

  uint8_t cmd = Wire.read();
  num_bytes--;

  uint8_t tail = i2c_cmd_tail;
  if (i2c_cmd_count >= I2C_CMD_QUEUE_SIZE) {
    /* queue full: drop the oldest command to keep the newest */
    i2c_cmd_head = (i2c_cmd_head + 1) % I2C_CMD_QUEUE_SIZE;
    i2c_cmd_count--;
  }

  i2c_cmd_t *e = &i2c_cmd_queue[tail];
  e->len = 0;
  e->data[e->len++] = cmd;
  while (Wire.available() && e->len < I2C_CMD_MAX_LEN && num_bytes > 0) {
    e->data[e->len++] = Wire.read();
    num_bytes--;
  }
  /* drain any leftovers so the master isn't NAKed mid-transaction */
  while (Wire.available()) Wire.read();

  i2c_cmd_tail = (tail + 1) % I2C_CMD_QUEUE_SIZE;
  i2c_cmd_count++;

  /* keep the last command visible for onRequest */
  i2c_buffer[0] = cmd;
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
  uint8_t ota_flags = (ota_display_active ? 1 : 0) | (waiting_display_shown ? 2 : 0) | (display_text_active ? 4 : 0);
  Wire.write(ota_flags);

  Wire.write((uint8_t)(i2c_smoothing * 255));
  Wire.write(auto_blink_enabled ? 1 : 0);
  Wire.write((uint8_t)(auto_blink_interval & 0xFF));
  Wire.write((uint8_t)((auto_blink_interval >> 8) & 0xFF));
  Wire.write(current_sprite < 0 ? 255 : (uint8_t)current_sprite);
  Wire.write((uint8_t)(current_sclera_color & 0xFF));
  Wire.write((uint8_t)((current_sclera_color >> 8) & 0xFF));
  Wire.write((uint8_t)(current_iris_med_color & 0xFF));
  Wire.write((uint8_t)((current_iris_med_color >> 8) & 0xFF));
  Wire.write((uint8_t)(current_iris_dark_color & 0xFF));
  Wire.write((uint8_t)((current_iris_dark_color >> 8) & 0xFF));
  Wire.write((uint8_t)(curve_falloff * 255));
  Wire.write((uint8_t)(curve_minimum * 255));
  Wire.write((uint8_t)(closure_strength * 255));
  Wire.write(i2c_initialized ? 1 : 0);
  Wire.write(i2c_master_detected ? 1 : 0);
  Wire.write((uint8_t)(i2c_blink_duration & 0xFF));
  Wire.write((uint8_t)((i2c_blink_duration >> 8) & 0xFF));
  Wire.write((uint8_t)(current_look_x & 0xFF));
  Wire.write((uint8_t)((current_look_x >> 8) & 0xFF));
  Wire.write((uint8_t)(current_look_y & 0xFF));
  Wire.write((uint8_t)((current_look_y >> 8) & 0xFF));
}

void process_i2c_command() {
  if (!i2c_initialized) return;

  /* pop one command atomically into a local buffer */
  uint8_t local[I2C_CMD_MAX_LEN];
  uint8_t len;

  portENTER_CRITICAL(&i2c_mux);
  if (i2c_cmd_count == 0) {
    portEXIT_CRITICAL(&i2c_mux);
    i2c_command_received = false;
    return;
  }
  uint8_t head = i2c_cmd_head;
  i2c_cmd_head = (head + 1) % I2C_CMD_QUEUE_SIZE;
  i2c_cmd_count--;
  len = i2c_cmd_queue[head].len;
  if (len > I2C_CMD_MAX_LEN) len = I2C_CMD_MAX_LEN;
  memcpy(local, i2c_cmd_queue[head].data, len);
  portEXIT_CRITICAL(&i2c_mux);

  if (len == 0) return;

  uint8_t cmd = local[0];

  switch (cmd) {
    case I2C_CMD_LOOK:
      if (len >= 5) {
        i2c_look_x = (int16_t)((local[2] << 8) | local[1]);
        i2c_look_y = (int16_t)((local[4] << 8) | local[3]);
      }
      break;
      
    case I2C_CMD_BLINK:
      if (len >= 3) {
        i2c_blink_duration = (uint16_t)((local[2] << 8) | local[1]);
        i2c_blink_trigger = true;
      }
      break;
      
    case I2C_CMD_SQUINT:
      if (len >= 2) {
        i2c_squint_level = local[1] / 255.0f;
      }
      break;
      
    case I2C_CMD_CURVE_PARAMS:
      if (len >= 4) {
        curve_falloff = local[1] / 255.0f;
        curve_minimum = local[2] / 255.0f;
        closure_strength = local[3] / 255.0f;
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
      if (len >= 3) {
        current_sclera_color = (uint16_t)(local[1] | (local[2] << 8));
      }
      break;
      
    case I2C_CMD_IRIS_RGB:
      if (len >= 3) {
        uint16_t rgb = (uint16_t)(local[1] | (local[2] << 8));
        current_iris_med_color = rgb;
        uint8_t r = ((rgb >> 11) & 0x1F) * 0.6f;
        uint8_t g = ((rgb >> 5) & 0x3F) * 0.6f;
        uint8_t b = (rgb & 0x1F) * 0.6f;
        current_iris_dark_color = (r << 11) | (g << 5) | b;
      }
      break;
      
    case I2C_CMD_AUTO_BLINK:
      if (len >= 2) {
        auto_blink_enabled = local[1] != 0;
      }
      break;

    case I2C_CMD_AUTO_BLINK_SPEED:
      if (len >= 3) {
        auto_blink_interval = (uint16_t)(local[1] | (local[2] << 8));
      }
      break;

    case I2C_CMD_SPRITE_MODE:
      if (len >= 2) {
        bool prev = sprite_mode;
        sprite_mode = local[1] != 0;
        if (prev && !sprite_mode) force_eye_repaint = true;
      }
      break;

    case I2C_CMD_GET_MODE:
      break;

    case I2C_CMD_SET_SPRITE:
      if (len >= 2) {
        if (local[1] == 255) {
          current_sprite = -1;
        } else if (local[1] < sprite_count) {
          current_sprite = local[1];
        }
      }
      break;

    case I2C_CMD_IDLE:
      i2c_external_control = false;
      break;
      
    case I2C_CMD_JUMP:
      if (len >= 5) {
        i2c_look_x = (int16_t)((local[2] << 8) | local[1]);
        i2c_look_y = (int16_t)((local[4] << 8) | local[3]);
        current_look_x = i2c_look_x;
        current_look_y = i2c_look_y;
      }
      break;
      
    case I2C_CMD_SMOOTHING:
      if (len >= 2) {
        float level = local[1] / 255.0f;
        i2c_smoothing = 1.0f - level;
        if (i2c_smoothing < 0.02f) i2c_smoothing = 0.02f;
      }
      break;
      
    case I2C_CMD_WIFI_SSID: {
      if (len > 1) {
        wifi_ssid = String((const char*)(local + 1));
        Preferences prefs;
        prefs.begin("eye", false);
        prefs.putString("wifi_ssid", wifi_ssid);
        prefs.end();
      }
      break;
    }
    
    case I2C_CMD_WIFI_PASS: {
      if (len > 1) {
        wifi_pass = String((const char*)(local + 1));
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

    case I2C_CMD_DISPLAY_TEXT: {
      if (len > 1) {
        char text[63];
        uint8_t tlen = len - 1;
        if (tlen > 62) tlen = 62;
        memcpy(text, (const void*)(local + 1), tlen);
        text[tlen] = '\0';
        display_text_active = true;
        draw_text_display(text);
      }
      break;
    }

    case I2C_CMD_CLEAR_TEXT:
      display_text_active = false;
      break;

    case I2C_CMD_TEXT_COLOR:
      if (len >= 3) {
        display_text_color = (uint16_t)(local[1] | (local[2] << 8));
      }
      break;

    case I2C_CMD_TEXT_BG:
      if (len >= 3) {
        display_text_bg = (uint16_t)(local[1] | (local[2] << 8));
      }
      break;
  }
}
