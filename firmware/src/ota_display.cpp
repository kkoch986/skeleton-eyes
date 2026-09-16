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

  display_bus_lock();

  int y_ota = (PANEL_HEIGHT - 3 * FONT_H * 2) / 2;
  for (uint16_t band_top = 0; band_top < PANEL_HEIGHT; band_top += PANEL_BUFFER_ROWS) {
    clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
    clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);

    draw_text_vertical_ccw(left_eye_buffer, "OTA", 3, 25, y_ota, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, "OTA", 3, 25, y_ota, 2, band_top);

    orient_frame_buffer(left_eye_buffer);
    orient_frame_buffer(right_eye_buffer);

    send_frame_buffer(LEFT_EYE_CS, left_eye_buffer, band_top);
    send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer, band_top);
  }

  display_bus_unlock();
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
  int ox = (PANEL_WIDTH - tw) / 2;
  int oy = (PANEL_HEIGHT - th) / 2;

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
    bool rev = (e == 0) && PANEL_REVERSED_BUF;
    int wx = rev ? (PANEL_WIDTH - ox - tw) : ox;
    int wy = rev ? (PANEL_HEIGHT - oy - th) : oy;
    select_display(cs);
    set_window(wx, wy, wx + tw - 1, wy + th - 1);
    digitalWrite(SHARED_DC, HIGH);
    hspi->beginTransaction(spi_settings);
    if (rev) {
      for (int i = tw * th - 1; i >= 0; i--) {
        hspi->write16(area[i]);
      }
    } else {
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
  int ly = (PANEL_HEIGHT - lh) / 2;

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
    bool rev = (e == 0) && PANEL_REVERSED_BUF;
    int wx = rev ? (PANEL_WIDTH - lx - lw) : lx;
    int wy = rev ? (PANEL_HEIGHT - ly - lh) : ly;
    select_display(cs);
    set_window(wx, wy, wx + lw - 1, wy + lh - 1);
    digitalWrite(SHARED_DC, HIGH);
    hspi->beginTransaction(spi_settings);
    if (rev) {
      for (int i = lw * lh - 1; i >= 0; i--) {
        hspi->write16(area[i]);
      }
    } else {
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
  int target_rows = (int)(((float)progress / total) * PANEL_HEIGHT);
  if (target_rows <= ota_fill_rows) return;

  display_bus_lock();

  int fill_top = PANEL_HEIGHT - target_rows;
  int fill_bot = PANEL_HEIGHT - ota_fill_rows - 1;
  if (fill_top > fill_bot) return;
  
  for (int e = 0; e < 2; e++) {
    uint8_t cs = (e == 0) ? LEFT_EYE_CS : RIGHT_EYE_CS;
    select_display(cs);
    set_window(0, fill_top, PANEL_WIDTH - 1, fill_bot);
    digitalWrite(SHARED_DC, HIGH);
    hspi->beginTransaction(spi_settings);
    for (int i = 0; i < PANEL_WIDTH * (fill_bot - fill_top + 1); i++) {
      hspi->write16(OTA_COLOR_FILL);
    }
    hspi->endTransaction();
    deselect_all();
  }
  ota_fill_rows = target_rows;

  ota_draw_percentage((progress * 100) / total);
  ota_redraw_label();

  display_bus_unlock();
}

void show_waiting_display() {
  if (ota_display_active) return;
  waiting_display_shown = true;

  display_bus_lock();

  String ip = WiFi.localIP().toString();

  int y_ota = (PANEL_HEIGHT - 3 * FONT_H * 2) / 2;
  int y_wait = (PANEL_HEIGHT - 7 * FONT_H * 2) / 2;
  int y_ip = (PANEL_HEIGHT - ip.length() * FONT_H * 2) / 2;

  for (uint16_t band_top = 0; band_top < PANEL_HEIGHT; band_top += PANEL_BUFFER_ROWS) {
    clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
    clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);

    draw_text_vertical_ccw(left_eye_buffer, "OTA", 3, 25, y_ota, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, "OTA", 3, 25, y_ota, 2, band_top);

    draw_text_vertical_ccw(left_eye_buffer, "WAITING", 7, 75, y_wait, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, "WAITING", 7, 75, y_wait, 2, band_top);

    draw_text_vertical_ccw(left_eye_buffer, ip.c_str(), strlen(ip.c_str()), 120, y_ip, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, ip.c_str(), strlen(ip.c_str()), 120, y_ip, 2, band_top);

    orient_frame_buffer(left_eye_buffer);
    orient_frame_buffer(right_eye_buffer);

    send_frame_buffer(LEFT_EYE_CS, left_eye_buffer, band_top);
    send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer, band_top);
  }

  display_bus_unlock();
}

void show_i2c_ready_display() {
  i2c_just_ready = true;
  i2c_ready_display_start = millis();

  display_bus_lock();

  int y = (PANEL_HEIGHT - 3 * FONT_H * 2) / 2;
  int y2 = (PANEL_HEIGHT - 2 * FONT_H * 2) / 2;

  for (uint16_t band_top = 0; band_top < PANEL_HEIGHT; band_top += PANEL_BUFFER_ROWS) {
    clear_frame_buffer(left_eye_buffer, OTA_COLOR_BLACK);
    clear_frame_buffer(right_eye_buffer, OTA_COLOR_BLACK);

    draw_text_vertical_ccw(left_eye_buffer, "I2C", 3, 75, y, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, "I2C", 3, 75, y, 2, band_top);

    draw_text_vertical_ccw(left_eye_buffer, "OK", 2, 120, y2, 2, band_top);
    draw_text_vertical_ccw(right_eye_buffer, "OK", 2, 120, y2, 2, band_top);

    orient_frame_buffer(left_eye_buffer);
    orient_frame_buffer(right_eye_buffer);

    send_frame_buffer(LEFT_EYE_CS, left_eye_buffer, band_top);
    send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer, band_top);
  }

  display_bus_unlock();
}