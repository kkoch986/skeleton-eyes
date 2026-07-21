#include "wifi_manager.h"
#include "ota_display.h"

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

static bool last_boot_button_state = HIGH;
static unsigned long last_boot_button_change = 0;

static void load_wifi_credentials() {
  if (wifi_ssid.length() == 0) {
    Preferences prefs;
    prefs.begin("eye", true);
    wifi_ssid = prefs.getString("wifi_ssid", "");
    wifi_pass = prefs.getString("wifi_pass", "");
    prefs.end();
  }
}

void wifi_init() {
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

void wifi_update() {
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
}

void boot_button_update() {
  uint32_t now = millis();
  bool boot_button_state = digitalRead(BOOT_BUTTON);

  if (boot_button_state == LOW && last_boot_button_state == HIGH && now - last_boot_button_change > 200) {
    last_boot_button_change = now;
    if (wifi_connected || wifi_connecting) {
      Serial.println("Boot button: disconnecting WiFi");
      wifi_should_connect = false;
      wifi_connecting = false;
      wifi_connected = false;
      waiting_display_shown = false;
      if (WiFi.isConnected()) {
        WiFi.disconnect(true, true);
      }
    } else {
      Serial.println("Boot button: connecting WiFi");
      load_wifi_credentials();
      if (wifi_ssid.length() > 0) {
        wifi_should_connect = true;
      } else {
        Serial.println("Boot button: no WiFi credentials stored");
      }
    }
  }
  last_boot_button_state = boot_button_state;
}
