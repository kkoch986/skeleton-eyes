#include "i2c_commands.h"
#include "ota_display.h"
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>

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
