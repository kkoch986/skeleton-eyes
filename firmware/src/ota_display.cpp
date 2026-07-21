#include "ota_display.h"
#include "display.h"
#include "sprites.h"
#include <WiFi.h>

bool i2c_just_ready = false;
unsigned long i2c_ready_display_start = 0;

void ota_display_init() {
  ota_display_active = true;
  ota_fill_rows = 0;
  waiting_display_shown = false;
  
  clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
  clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);
  
  int y_ota = (DISPLAY_HEIGHT - 3 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "OTA", 25, y_ota, 2);
  draw_text_vertical_ccw(right_eye_buffer, "OTA", 25, y_ota, 2);
  
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
  
  int y_ota = (DISPLAY_HEIGHT - 3 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "OTA", 25, y_ota, 2);
  draw_text_vertical_ccw(right_eye_buffer, "OTA", 25, y_ota, 2);
  
  int y_wait = (DISPLAY_HEIGHT - 7 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "WAITING", 75, y_wait, 2);
  draw_text_vertical_ccw(right_eye_buffer, "WAITING", 75, y_wait, 2);
  
  int y_ip = (DISPLAY_HEIGHT - ip.length() * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, ip.c_str(), 120, y_ip, 2);
  draw_text_vertical_ccw(right_eye_buffer, ip.c_str(), 120, y_ip, 2);
  
  uint32_t npix = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = left_eye_buffer[i];
    left_eye_buffer[i] = left_eye_buffer[npix - 1 - i];
    left_eye_buffer[npix - 1 - i] = t;
  }
  
  send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
  send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
}

void show_i2c_ready_display() {
  i2c_just_ready = true;
  i2c_ready_display_start = millis();

  clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
  clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);

  int y = (DISPLAY_HEIGHT - 3 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "I2C", 75, y, 2);
  draw_text_vertical_ccw(right_eye_buffer, "I2C", 75, y, 2);

  int y2 = (DISPLAY_HEIGHT - 2 * FONT_H * 2) / 2;
  draw_text_vertical_ccw(left_eye_buffer, "OK", 120, y2, 2);
  draw_text_vertical_ccw(right_eye_buffer, "OK", 120, y2, 2);

  uint32_t npix = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = left_eye_buffer[i];
    left_eye_buffer[i] = left_eye_buffer[npix - 1 - i];
    left_eye_buffer[npix - 1 - i] = t;
    uint16_t r = right_eye_buffer[i];
    right_eye_buffer[i] = right_eye_buffer[npix - 1 - i];
    right_eye_buffer[npix - 1 - i] = r;
  }

  send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
  send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
}
