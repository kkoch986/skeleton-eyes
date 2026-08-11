#include <Wire.h>

#define EYE_I2C_ADDR 0x42

#define I2C_SDA 8
#define I2C_SCL 9

#define CMD_LOOK          0x01
#define CMD_BLINK         0x02
#define CMD_SQUINT        0x03
#define CMD_CURVE_PARAMS  0x06
#define CMD_STATUS        0x07
#define CMD_RESET         0x08
#define CMD_SCLERA_RGB    0x09
#define CMD_IRIS_RGB      0x0A
#define CMD_AUTO_BLINK    0x0B
#define CMD_IDLE          0x0C
#define CMD_JUMP          0x0D
#define CMD_SMOOTHING     0x0E
#define CMD_WIFI_SSID     0x0F
#define CMD_WIFI_PASS     0x10
#define CMD_WIFI_CONNECT  0x11
#define CMD_WIFI_FORGET   0x12
#define CMD_WIFI_STATUS   0x13
#define CMD_RESET_DEVICE  0x14
#define CMD_AUTO_BLINK_SPEED 0x15
#define CMD_SPRITE_MODE  0x16
#define CMD_GET_MODE     0x17
#define CMD_SET_SPRITE   0x18
#define CMD_DISPLAY_TEXT 0x19
#define CMD_CLEAR_TEXT   0x1A
#define CMD_TEXT_COLOR   0x1B
#define CMD_TEXT_BG      0x1C

#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

#define SCLERA_DEFAULT 0xF7DE
#define IRIS_DEFAULT   0x4A49

uint16_t parseRGB(String s) {
  int first = s.indexOf(' ');
  if (first > 0) {
    int second = s.indexOf(' ', first + 1);
    if (second > 0) {
      return RGB565(
        (uint8_t)constrain(s.substring(0, first).toInt(), 0, 255),
        (uint8_t)constrain(s.substring(first + 1, second).toInt(), 0, 255),
        (uint8_t)constrain(s.substring(second + 1).toInt(), 0, 255)
      );
    }
  }
  return (uint16_t)strtol(s.c_str(), NULL, 16);
}

uint8_t parseSmoothing(String s) {
  s.toLowerCase();
  if (s == "fast")   return 127;
  if (s == "medium") return 25;
  if (s == "slow")   return 5;
  return (uint8_t)constrain(s.toInt(), 0, 255);
}

bool eye_connected = false;

#define MAX_ANIM_FRAMES 64
bool animating = false;
uint8_t anim_frames[MAX_ANIM_FRAMES];
uint8_t anim_frame_count = 0;
uint8_t anim_current_index = 0;
uint16_t anim_frame_delay = 0;
unsigned long anim_last_time = 0;

void stop_anim() {
  animating = false;
}

void setSpriteIndex(int idx, bool silent = false);

bool probe_eye() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  byte err = Wire.endTransmission();
  eye_connected = (err == 0);
  return eye_connected;
}

void i2c_scan() {
  Serial.println("Scanning I2C bus...");
  int count = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  Found device at 0x%02X", addr);
      if (addr == EYE_I2C_ADDR) Serial.print(" <-- eye board");
      Serial.println();
      count++;
    }
  }
  Serial.printf("Scan complete. %d device(s) found.\n", count);
}

void printHelp() {
  Serial.println();
  Serial.println("--- Eye Controller Commands ---");
  Serial.println("look <x> <y> [s]     Smooth gaze to position; optional smoothing");
  Serial.println("                       (fast/medium/slow or 0-255)");
  Serial.println("look center|left|right|up|down");
  Serial.println("jump <x> <y>         Instant gaze to position");
  Serial.println("jump center|left|right|up|down");
  Serial.println("blink [ms]           Trigger blink (default 200ms)");
  Serial.println("squint <0-255>       Squint level");
  Serial.println("unsquint             Squint = 0");
  Serial.println("sclera <r g b|hex>   Set sclera: 3 bytes (0-255) or 4-digit hex");
  Serial.println("iris <r g b|hex>     Set iris: 3 bytes (0-255) or 4-digit hex");
  Serial.println("curve <f> <m> <s>    Eyelid curve (falloff min strength 0-255)");
  Serial.println("autoblink on|off      Enable/disable autonomous blinking");
  Serial.println("autoblinkspeed <ms>    Set interval between auto-blinks (ms)");
  Serial.println("smoothing <val>      Smoothing speed: fast|medium|slow or 0-255");
  Serial.println("wifi ssid <name>     Set WiFi SSID");
  Serial.println("wifi pass <pass>     Set WiFi password");
  Serial.println("wifi connect         Connect to WiFi (enables OTA updates)");
  Serial.println("wifi status          Query WiFi connection status");
  Serial.println("wifi forget          Clear stored WiFi credentials");
  Serial.println("idle                  Return to autonomous eye movement");
  Serial.println("emotion name         angry, sleepy, surprised, neutral");
  Serial.println("status               Read eye status");
  Serial.println("probe                Check I2C connection");
  Serial.println("i2cscan              Scan I2C bus");
  Serial.println("demo                 Run demo sequence");
  Serial.println("reset                Return eyes to autonomous mode");
  Serial.println("resetdevice          Reboot the ESP32 eye board");
  Serial.println("sprite on|off        Enter/exit sprite render mode");
  Serial.println("sprite <n>           Display sprite by index (0-8, 255=blank)");
  Serial.println("spritestatus         Query current render mode");
  Serial.println("animate <ms> <i>...  Loop sprite indices at given speed");
  Serial.println("                     e.g. animate 100 0 1 2 3 4 5 6 7 8");
  Serial.println("stop                 Stop sprite animation");
  Serial.println("curve <f> <m> <s>    Eyelid curve (falloff min strength 0-255)");
  Serial.println("text <string>        Display text on eye screen (max 62 chars)");
  Serial.println("text off             Clear displayed text");
  Serial.println("textcolor <r g b|hex> Set text color (3 bytes or 4-digit hex)");
  Serial.println("textbg <r g b|hex>   Set text background color");
  Serial.println("help / ?             This help");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Skeleton Eye I2C Controller");

  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  Serial.printf("I2C pins: SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  for (int i = 0; i < 5; i++) {
    if (probe_eye()) break;
    delay(200);
  }

  if (eye_connected) {
    Serial.println("Eye board detected!");
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    Serial.println("WARNING: No eye board found on I2C bus! Type 'help' for commands.");
  }

  Serial.println("Commands: look, blink, squint, sclera, iris, emotion, reset, status, probe, i2cscan, help");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    stop_anim();
    processSerialCommand(cmd);
  }

  if (animating && millis() - anim_last_time >= anim_frame_delay) {
    anim_last_time = millis();
    setSpriteIndex(anim_frames[anim_current_index], true);
    anim_current_index++;
    if (anim_current_index >= anim_frame_count) {
      anim_current_index = 0;
    }
  }
}

void processSerialCommand(String cmd) {
  if (cmd == "jump center")       { jumpDirection(0, 0); }
  else if (cmd == "jump left")    { jumpDirection(0, -40); }
  else if (cmd == "jump right")   { jumpDirection(0, 40); }
  else if (cmd == "jump up")      { jumpDirection(30, 0); }
  else if (cmd == "jump down")    { jumpDirection(-30, 0); }
  else if (cmd == "look center")  { lookDirection(0, 0); }
  else if (cmd == "look left")    { lookDirection(0, -40); }
  else if (cmd == "look right")   { lookDirection(0, 40); }
  else if (cmd == "look up")      { lookDirection(30, 0); }
  else if (cmd == "look down")    { lookDirection(-30, 0); }
  else if (cmd == "blink")        { triggerBlink(200); }
  else if (cmd == "squint")       { setSquintLevel(150); }
  else if (cmd == "unsquint")     { setSquintLevel(0); }
  else if (cmd == "reset")        { resetEyes(); }
  else if (cmd == "status")       { readEyeStatus(); }
  else if (cmd == "resetdevice")  { resetDevice(); }
  else if (cmd.startsWith("look ")) {
  int space1 = cmd.indexOf(' ', 5);
    if (space1 > 0) {
      int space2 = cmd.indexOf(' ', space1 + 1);
      if (space2 > 0) {
        int16_t x = cmd.substring(5, space1).toInt();
        int16_t y = cmd.substring(space1 + 1, space2).toInt();
        uint8_t s = parseSmoothing(cmd.substring(space2 + 1));
        setSmoothing(s);
        lookDirection(x, y);
      } else {
        int16_t x = cmd.substring(5, space1).toInt();
        int16_t y = cmd.substring(space1 + 1).toInt();
        lookDirection(x, y);
      }
    }
  }
  else if (cmd.startsWith("jump ")) {
    int space = cmd.indexOf(' ', 5);
    if (space > 0) {
      int16_t x = cmd.substring(5, space).toInt();
      int16_t y = cmd.substring(space + 1).toInt();
      jumpDirection(x, y);
    }
  }
  else if (cmd.startsWith("sclera ")) {
    setScleraRGB(parseRGB(cmd.substring(7)));
  }
  else if (cmd.startsWith("iris ")) {
    setIrisRGB(parseRGB(cmd.substring(5)));
  }
  else if (cmd.startsWith("blink ")) {
    uint16_t dur = (uint16_t)cmd.substring(6).toInt();
    triggerBlink(dur);
  }
  else if (cmd.startsWith("squint ")) {
    uint8_t level = (uint8_t)constrain(cmd.substring(7).toInt(), 0, 255);
    setSquintLevel(level);
  }
  else if (cmd == "autoblink on")     { setAutoBlink(true); }
  else if (cmd == "autoblink off")    { setAutoBlink(false); }
  else if (cmd.startsWith("autoblinkspeed ")) {
    uint16_t ms = (uint16_t)constrain(cmd.substring(15).toInt(), 100, 60000);
    setAutoBlinkSpeed(ms);
  }
  else if (cmd.startsWith("smoothing ")) {
    setSmoothing(parseSmoothing(cmd.substring(10)));
  }
  else if (cmd.startsWith("curve ")) {
    int f1 = cmd.indexOf(' ', 6);
    if (f1 > 0) {
      int f2 = cmd.indexOf(' ', f1 + 1);
      if (f2 > 0) {
        uint8_t f = (uint8_t)constrain(cmd.substring(6, f1).toInt(), 0, 255);
        uint8_t m = (uint8_t)constrain(cmd.substring(f1 + 1, f2).toInt(), 0, 255);
        uint8_t s = (uint8_t)constrain(cmd.substring(f2 + 1).toInt(), 0, 255);
        setCurveParams(f, m, s);
      } else {
        Serial.println("usage: curve <falloff> <min> <strength>");
      }
    }
  }
  else if (cmd == "text off")             { clearText(); }
  else if (cmd.startsWith("textcolor "))     { setTextColor(parseRGB(cmd.substring(10))); }
  else if (cmd.startsWith("textbg "))        { setTextBG(parseRGB(cmd.substring(7))); }
  else if (cmd.startsWith("text "))          { displayText(cmd.substring(5)); }
  else if (cmd == "idle")             { idleEyes(); }
  else if (cmd.startsWith("wifi ssid "))   { setWiFiSSID(cmd.substring(10)); }
  else if (cmd.startsWith("wifi pass "))   { setWiFiPass(cmd.substring(10)); }
  else if (cmd == "wifi connect")          { connectWiFi(); }
  else if (cmd == "wifi status")           { wifiStatus(); }
  else if (cmd == "wifi forget")           { forgetWiFi(); }
  else if (cmd == "angry" || cmd == "emotion angry")        { showEmotion("angry"); }
  else if (cmd == "sleepy" || cmd == "emotion sleepy")       { showEmotion("sleepy"); }
  else if (cmd == "surprised" || cmd == "emotion surprised")    { showEmotion("surprised"); }
  else if (cmd == "neutral" || cmd == "emotion neutral")      { showEmotion("neutral"); }
  else if (cmd == "demo")         { demoSequence(); }
  else if (cmd == "probe")        { Serial.println(probe_eye() ? "Eye connected" : "No response"); }
  else if (cmd == "i2cscan")      { i2c_scan(); }
  else if (cmd.startsWith("sprite ") && cmd.charAt(7) >= '0' && cmd.charAt(7) <= '9') {
    int idx = cmd.substring(7).toInt();
    setSpriteIndex(idx);
  }
  else if (cmd == "sprite on")    { setSpriteMode(true); }
  else if (cmd == "sprite off")   { setSpriteMode(false); }
  else if (cmd == "spritestatus") { getSpriteMode(); }
  else if (cmd == "stop")         { stop_anim(); Serial.println("Animation stopped"); }
  else if (cmd.startsWith("animate ")) {
    String rest = cmd.substring(8);
    int first_space = rest.indexOf(' ');
    if (first_space > 0) {
      uint16_t delay_ms = (uint16_t)rest.substring(0, first_space).toInt();
      String indices = rest.substring(first_space + 1);
      anim_frame_count = 0;
      int pos = 0;
      while (pos >= 0 && anim_frame_count < MAX_ANIM_FRAMES) {
        int next = indices.indexOf(' ', pos);
        String token = (next >= 0) ? indices.substring(pos, next) : indices.substring(pos);
        int idx = token.toInt();
        if (idx >= 0 && idx <= 254) {
          anim_frames[anim_frame_count++] = (uint8_t)idx;
        }
        pos = (next >= 0) ? next + 1 : -1;
      }
      if (anim_frame_count > 0) {
        anim_frame_delay = delay_ms;
        anim_current_index = 0;
        anim_last_time = millis();
        setSpriteMode(true);
        animating = true;
        Serial.printf("Animating %d frames at %dms\n", anim_frame_count, delay_ms);
      }
    }
  }
  else if (cmd == "help" || cmd == "?") { printHelp(); }
  else { Serial.println("Unknown command. Try 'help'."); }
}

bool send_i2c() {
  byte err = Wire.endTransmission();
  if (err == 0) {
    eye_connected = true;
    digitalWrite(LED_BUILTIN, HIGH);
    return true;
  }
  eye_connected = false;
  digitalWrite(LED_BUILTIN, LOW);
  Serial.printf("I2C error: ");
  switch (err) {
    case 1: Serial.println("data too long"); break;
    case 2: Serial.println("NACK on address"); break;
    case 3: Serial.println("NACK on data"); break;
    default: Serial.println("unknown"); break;
  }
  return false;
}

void lookDirection(int16_t x, int16_t y) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_LOOK);
  Wire.write((uint8_t)(x & 0xFF));
  Wire.write((uint8_t)((x >> 8) & 0xFF));
  Wire.write((uint8_t)(y & 0xFF));
  Wire.write((uint8_t)((y >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Look: (%d, %d)\n", x, y);
}

void jumpDirection(int16_t x, int16_t y) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_JUMP);
  Wire.write((uint8_t)(x & 0xFF));
  Wire.write((uint8_t)((x >> 8) & 0xFF));
  Wire.write((uint8_t)(y & 0xFF));
  Wire.write((uint8_t)((y >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Jump: (%d, %d)\n", x, y);
}

void triggerBlink(uint16_t duration_ms) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_BLINK);
  Wire.write((uint8_t)(duration_ms & 0xFF));
  Wire.write((uint8_t)((duration_ms >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Blink: %dms\n", duration_ms);
}

void setSquintLevel(uint8_t level) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_SQUINT);
  Wire.write(level);
  if (send_i2c()) Serial.printf("Squint: %d\n", level);
}

void displayText(String text) {
  if (text.length() > 62) text = text.substring(0, 62);
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_DISPLAY_TEXT);
  for (unsigned int i = 0; i < text.length(); i++) {
    Wire.write(text[i]);
  }
  if (send_i2c()) Serial.printf("Text: \"%s\"\n", text.c_str());
}

void clearText() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_CLEAR_TEXT);
  if (send_i2c()) Serial.println("Text cleared");
}

void setTextColor(uint16_t rgb) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_TEXT_COLOR);
  Wire.write((uint8_t)(rgb & 0xFF));
  Wire.write((uint8_t)((rgb >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Text color: #%04X\n", rgb);
}

void setTextBG(uint16_t rgb) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_TEXT_BG);
  Wire.write((uint8_t)(rgb & 0xFF));
  Wire.write((uint8_t)((rgb >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Text background: #%04X\n", rgb);
}

void setScleraRGB(uint16_t rgb) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_SCLERA_RGB);
  Wire.write((uint8_t)(rgb & 0xFF));
  Wire.write((uint8_t)((rgb >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Sclera RGB: #%04X\n", rgb);
  triggerBlink(100);
}

void setIrisRGB(uint16_t rgb) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_IRIS_RGB);
  Wire.write((uint8_t)(rgb & 0xFF));
  Wire.write((uint8_t)((rgb >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Iris RGB: #%04X\n", rgb);
  triggerBlink(100);
}

void setCurveParams(uint8_t falloff, uint8_t minimum, uint8_t strength) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_CURVE_PARAMS);
  Wire.write(falloff);
  Wire.write(minimum);
  Wire.write(strength);
  if (send_i2c()) Serial.printf("Curve params: falloff=%d min=%d strength=%d\n", falloff, minimum, strength);
}

void setSmoothing(uint8_t level) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_SMOOTHING);
  Wire.write(level);
  if (send_i2c()) Serial.printf("Smoothing: %d (%.2f)\n", level, level / 255.0f);
}

void setAutoBlink(bool on) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_AUTO_BLINK);
  Wire.write(on ? 1 : 0);
  if (send_i2c()) Serial.printf("Auto blink: %s\n", on ? "ON" : "OFF");
}

void setAutoBlinkSpeed(uint16_t interval_ms) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_AUTO_BLINK_SPEED);
  Wire.write((uint8_t)(interval_ms & 0xFF));
  Wire.write((uint8_t)((interval_ms >> 8) & 0xFF));
  if (send_i2c()) Serial.printf("Auto blink interval: %dms\n", interval_ms);
}

void idleEyes() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_IDLE);
  if (send_i2c()) Serial.println("Autonomous eye movement");
}

void setWiFiSSID(String ssid) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_WIFI_SSID);
  for (unsigned int i = 0; i < ssid.length(); i++) {
    Wire.write(ssid[i]);
  }
  Wire.write(0);
  if (send_i2c()) Serial.printf("WiFi SSID set: %s\n", ssid.c_str());
}

void setWiFiPass(String pass) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_WIFI_PASS);
  for (unsigned int i = 0; i < pass.length(); i++) {
    Wire.write(pass[i]);
  }
  Wire.write(0);
  if (send_i2c()) Serial.println("WiFi password set");
}

void connectWiFi() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_WIFI_CONNECT);
  if (send_i2c()) Serial.println("Connecting to WiFi...");
}

void wifiStatus() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_WIFI_STATUS);
  Wire.endTransmission(false);
  if (Wire.requestFrom(EYE_I2C_ADDR, 5) == 5) {
    uint8_t s = Wire.read();
    if (s == 2) {
      uint8_t ip[4] = { Wire.read(), Wire.read(), Wire.read(), Wire.read() };
      Serial.printf("WiFi: Connected %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    } else {
      Wire.read(); Wire.read(); Wire.read(); Wire.read();
      Serial.printf("WiFi: %s\n", s == 1 ? "Connecting..." : "Disconnected");
    }
  } else {
    Serial.println("WiFi status read failed");
  }
}

void forgetWiFi() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_WIFI_FORGET);
  if (send_i2c()) Serial.println("WiFi credentials cleared");
}

void resetEyes() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_RESET);
  if (send_i2c()) Serial.println("Reset to autonomous mode");
}

void resetDevice() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_RESET_DEVICE);
  if (send_i2c()) Serial.println("Resetting ESP32 device...");
}

void setSpriteMode(bool on) {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_SPRITE_MODE);
  Wire.write(on ? 1 : 0);
  if (send_i2c()) Serial.printf("Sprite mode: %s\n", on ? "ON" : "OFF");
}

void setSpriteIndex(int idx, bool silent) {
  if (idx > 255) idx = 255;
  if (idx < 0) idx = 0;
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_SET_SPRITE);
  Wire.write((uint8_t)idx);
  if (send_i2c() && !silent) {
    if (idx == 255) Serial.println("Sprite cleared");
    else Serial.printf("Sprite index: %d\n", idx);
  }
}

void getSpriteMode() {
  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_GET_MODE);
  Wire.endTransmission(false);
  if (Wire.requestFrom(EYE_I2C_ADDR, 1) == 1) {
    Serial.printf("Render mode: %s\n", Wire.read() ? "Sprite" : "Procedural");
  } else {
    Serial.println("Mode read failed");
  }
}

void readEyeStatus() {
  if (!probe_eye()) {
    Serial.println("Eye not connected!");
    return;
  }

  Wire.beginTransmission(EYE_I2C_ADDR);
  Wire.write(CMD_STATUS);
  Wire.endTransmission(false);

  if (Wire.requestFrom(EYE_I2C_ADDR, 31) == 31) {
    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);
    uint8_t squint = Wire.read();
    uint8_t external = Wire.read();
    uint8_t mode = Wire.read();
    uint8_t autonomous = Wire.read();
    uint8_t ota = Wire.read();
    uint8_t smoothing = Wire.read();
    uint8_t auto_blink = Wire.read();
    uint16_t blink_interval = Wire.read() | (Wire.read() << 8);
    uint8_t sprite_ix = Wire.read();
    uint16_t sclera = Wire.read() | (Wire.read() << 8);
    uint16_t iris_med = Wire.read() | (Wire.read() << 8);
    uint16_t iris_dark = Wire.read() | (Wire.read() << 8);
    uint8_t falloff = Wire.read();
    uint8_t curve_min = Wire.read();
    uint8_t closure = Wire.read();
    uint8_t i2c_init = Wire.read();
    uint8_t master = Wire.read();
    uint16_t blink_dur = Wire.read() | (Wire.read() << 8);
    int16_t cur_x = Wire.read() | (Wire.read() << 8);
    int16_t cur_y = Wire.read() | (Wire.read() << 8);
    Serial.printf("Status - target X:%d Y:%d render X:%d Y:%d\n", x, y, cur_x, cur_y);
    Serial.printf("  squint:%d blink:%dms autoblink:%s interval:%dms\n",
                  squint, blink_dur, auto_blink ? "on" : "off", blink_interval);
    Serial.printf("  smoothing:%d ext_ctrl:%s mode:%s autonomous:%s sprite:%d\n",
                  smoothing, external ? "yes" : "no",
                  mode ? "sprite" : "procedural",
                  autonomous ? "yes" : "no",
                  sprite_ix == 255 ? -1 : sprite_ix);
    Serial.printf("  sclera:#%04X iris:#%04X/%04X curve:%d/%d/%d\n",
                  sclera, iris_med, iris_dark, falloff, curve_min, closure);
    Serial.printf("  i2c_init:%s master:%s text:%s ota:%s\n",
                  i2c_init ? "yes" : "no", master ? "yes" : "no",
                  ota & 4 ? "active" : "none",
                  ota & 1 ? "updating" : (ota & 2 ? "waiting" : "none"));
  } else {
    Serial.println("Status read failed");
  }
}

void showEmotion(const String& emotion) {
  if (emotion == "angry") {
    setSquintLevel(180);
    setCurveParams(30, 80, 255);
    setScleraRGB(0xF800);
    setIrisRGB(0xF800);
    lookDirection(5, -10);
  } else if (emotion == "sleepy") {
    setSquintLevel(120);
    setCurveParams(10, 200, 150);
    triggerBlink(800);
    lookDirection(-10, 0);
  } else if (emotion == "surprised") {
    setSquintLevel(0);
    setCurveParams(5, 50, 255);
    lookDirection(15, 0);
  } else if (emotion == "neutral") {
    setSquintLevel(0);
    setCurveParams(13, 153, 255);
    setScleraRGB(0xF7DE);
    setIrisRGB(0x4A49);
    lookDirection(0, 0);
  }
  Serial.printf("Emotion: %s\n", emotion.c_str());
}

void demoSequence() {
  Serial.println("Starting demo sequence...");

  lookDirection(0, 0);
  delay(1000);

  setIrisRGB(0x4D9F);
  setScleraRGB(0xFFFF);
  delay(500);

  lookDirection(0, 40);
  delay(800);
  lookDirection(0, -40);
  delay(800);
  lookDirection(25, 0);
  delay(800);
  lookDirection(-25, 0);
  delay(800);
  lookDirection(0, 0);
  delay(500);

  showEmotion("angry");
  delay(2000);

  showEmotion("surprised");
  delay(2000);

  showEmotion("sleepy");
  delay(2000);

  triggerBlink(150);
  delay(500);
  triggerBlink(150);
  delay(500);

  setIrisRGB(0x87E0);
  setSquintLevel(0);
  lookDirection(10, 20);
  delay(1500);

  setIrisRGB(0x8C51);
  delay(1500);

  setCurveParams(15, 120, 200);
  delay(1000);

  readEyeStatus();

  showEmotion("neutral");
  Serial.println("Demo complete");
}
