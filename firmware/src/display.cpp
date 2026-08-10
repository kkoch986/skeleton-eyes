#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * Serialises access to the display buffers and the SPI bus between the
 * async renderer task (core 0) and the loop task (core 1, OTA / text /
 * I2C-triggered draws). Every top-level SPI operation must hold this lock.
 */
static SemaphoreHandle_t display_mutex = NULL;

void display_bus_init() {
  if (!display_mutex) {
    display_mutex = xSemaphoreCreateMutex();
  }
}

void display_bus_lock() {
  if (!display_mutex) display_mutex = xSemaphoreCreateMutex();
  xSemaphoreTake(display_mutex, portMAX_DELAY);
}

void display_bus_unlock() {
  if (display_mutex) xSemaphoreGive(display_mutex);
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
  send_data(0xC8);
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
