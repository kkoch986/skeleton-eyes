#include "display.h"
/* Panel constants and panel_driver_init() come from the single header
 * selected by panel_config.h.  Including both panel headers here let the
 * second one redefine PANEL_WIDTH/HEIGHT/BUFFER_ROWS for this TU only,
 * which made this file push windows and byte counts that disagreed with
 * every other TU. */

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

/* ------------------------------------------------------------------ */
/*  SPI framing (shared by every panel driver).                       */
/* ------------------------------------------------------------------ */

void send_command(uint8_t cmd) {
  digitalWrite(SHARED_DC, LOW);
  hspi->beginTransaction(spi_settings);
  hspi->transferBytes(&cmd, nullptr, 1);
  hspi->endTransaction();
}

void send_data(uint8_t data) {
  digitalWrite(SHARED_DC, HIGH);
  hspi->beginTransaction(spi_settings);
  hspi->transferBytes(&data, nullptr, 1);
  hspi->endTransaction();
}

void send_command_with_data(uint8_t cmd, const uint8_t *data, size_t len) {
  hspi->beginTransaction(spi_settings);

  digitalWrite(SHARED_DC, LOW);
  hspi->transferBytes(&cmd, nullptr, 1);

  digitalWrite(SHARED_DC, HIGH);
  if (len) {
    hspi->transferBytes((uint8_t *)data, nullptr, len);
  }

  hspi->endTransaction();
}

void send_frame_buffer(uint8_t cs_pin, uint16_t *buffer, uint16_t band_top) {
  select_display(cs_pin);

#if PANEL_ROW_WINDOW_PUSH
  /* One continuous SPI transaction for the whole band: every row's
   * CASET/RASET/RAMWR + pixel data, D/C toggled in place.  Per-row
   * windows keep the clone column-counter quirk at bay; a single
   * transaction avoids 160 begin/end round-trips per band. */
#if PANEL_18BIT
  uint8_t srcRow18[PANEL_WIDTH * 3];
#endif
  hspi->beginTransaction(spi_settings);
  for (uint16_t row = 0; row < PANEL_BUFFER_ROWS; row++) {
    uint16_t r = band_top + row;
    uint8_t col_data[] = {0x00, 0x00, 0x00, (uint8_t)(PANEL_WIDTH - 1)};
    uint8_t row_data[] = {(uint8_t)(r >> 8), (uint8_t)r,
                          (uint8_t)(r >> 8), (uint8_t)r};

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_CASET);
    digitalWrite(SHARED_DC, HIGH);
    hspi->transferBytes(col_data, nullptr, 4);

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_RASET);
    digitalWrite(SHARED_DC, HIGH);
    hspi->transferBytes(row_data, nullptr, 4);

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_RAMWR);
    digitalWrite(SHARED_DC, HIGH);

#if PANEL_18BIT
    for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
      uint16_t c = buffer[(uint32_t)row * PANEL_WIDTH + x];
      uint8_t r6 = (uint8_t)(((c >> 11) & 0x1F) << 3);
      uint8_t g6 = (uint8_t)(((c >> 5) & 0x3F) << 2);
      uint8_t b6 = (uint8_t)((c & 0x1F) << 3);
      r6 |= r6 >> 5;
      g6 |= g6 >> 6;
      b6 |= b6 >> 5;
      uint8_t *p = srcRow18 + x * 3;
      p[0] = r6;
      p[1] = g6;
      p[2] = b6;
    }
    hspi->transferBytes(srcRow18, nullptr, PANEL_WIDTH * 3);
#else
    hspi->transferBytes((uint8_t *)buffer + (uint32_t)row * PANEL_WIDTH * 2,
                        nullptr, PANEL_WIDTH * 2);
#endif
  }
  hspi->endTransaction();
#else
  {
    uint16_t x0 = 0;
    uint16_t x1 = PANEL_WIDTH - 1;
    uint8_t col_data[] = {(uint8_t)(x0 >> 8), (uint8_t)x0,
                          (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t row_data[] = {(uint8_t)(band_top >> 8), (uint8_t)band_top,
                          (uint8_t)((band_top + PANEL_BUFFER_ROWS - 1) >> 8),
                          (uint8_t)(band_top + PANEL_BUFFER_ROWS - 1)};

    hspi->beginTransaction(spi_settings);

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_CASET);
    digitalWrite(SHARED_DC, HIGH);
    hspi->transferBytes(col_data, nullptr, 4);

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_RASET);
    digitalWrite(SHARED_DC, HIGH);
    hspi->transferBytes(row_data, nullptr, 4);

    digitalWrite(SHARED_DC, LOW);
    hspi->transfer(PANEL_RAMWR);

    digitalWrite(SHARED_DC, HIGH);
    hspi->writePixels(buffer, (uint32_t)PANEL_WIDTH * PANEL_BUFFER_ROWS * 2);

    hspi->endTransaction();
  }
#endif

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

/*
 * Initialise both panels on the same SPI bus. panel_driver_init(right)
 * comes from the panel selected by panel_config.h.
 */
void panel_init_both() {
  select_display(LEFT_EYE_CS);
  panel_driver_init(false);
  deselect_all();

  select_display(RIGHT_EYE_CS);
  panel_driver_init(true);
  deselect_all();
}

/* ------------------------------------------------------------------ */
/*  Frame orientation helpers                                          */
/* ------------------------------------------------------------------ */

/*
 * 180-degree rotate a finished buffer in place. The round GC9A01 boards
 * rely on this to present text/OTA frames upright with the panels'
 * physical mounting; rectangular panels mount straight so it's a no-op.
 */
void orient_frame_buffer(uint16_t *buffer) {
#if PANEL_REVERSED_BUF
  uint32_t npix = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = buffer[i];
    buffer[i] = buffer[npix - 1 - i];
    buffer[npix - 1 - i] = t;
  }
#else
  (void)buffer;
#endif
}

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  uint8_t col_data[] = {(uint8_t)((x0 >> 8) & 0xFF), (uint8_t)(x0 & 0xFF), 
                        (uint8_t)((x1 >> 8) & 0xFF), (uint8_t)(x1 & 0xFF)};
  uint8_t row_data[] = {(uint8_t)((y0 >> 8) & 0xFF), (uint8_t)(y0 & 0xFF), 
                        (uint8_t)((y1 >> 8) & 0xFF), (uint8_t)(y1 & 0xFF)};

  hspi->beginTransaction(spi_settings);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(PANEL_CASET);
  digitalWrite(SHARED_DC, HIGH);
  hspi->transferBytes(col_data, nullptr, 4);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(PANEL_RASET);
  digitalWrite(SHARED_DC, HIGH);
  hspi->transferBytes(row_data, nullptr, 4);

  digitalWrite(SHARED_DC, LOW);
  hspi->transfer(PANEL_RAMWR);

  hspi->endTransaction();
}

void clear_frame_buffer(uint16_t *buffer, uint16_t color) {
  uint32_t color32 = (uint32_t(color) << 16) | color;
  uint32_t *buffer32 = reinterpret_cast<uint32_t *>(buffer);
  uint32_t count = ((uint32_t)PANEL_WIDTH * PANEL_BUFFER_ROWS) / 2;

  for (uint32_t i = 0; i < count; i++) {
    buffer32[i] = color32;
  }
}