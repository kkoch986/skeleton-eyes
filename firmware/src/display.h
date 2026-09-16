#pragma once

#include "globals.h"
#include "panel_config.h"

void display_bus_init();
void display_bus_lock();
void display_bus_unlock();

void send_command(uint8_t cmd);
void send_data(uint8_t data);
void send_command_with_data(uint8_t cmd, const uint8_t *data, size_t len);
/* Push PANEL_BUFFER_ROWS stored at `buffer` into the panel starting at
   row `band_top`. Non-banded builds call this once with band_top=0 and the
   buffer holds the whole frame; banded builds push each band in turn. */
void send_frame_buffer(uint8_t cs_pin, uint16_t *buffer, uint16_t band_top);
void select_display(uint8_t cs_pin);
void deselect_all();
void panel_init_both();
void orient_frame_buffer(uint16_t *buffer);
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void clear_frame_buffer(uint16_t *buffer, uint16_t color);
