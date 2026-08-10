#pragma once

#include "globals.h"

void display_bus_init();
void display_bus_lock();
void display_bus_unlock();

void send_command(uint8_t cmd);
void send_data(uint8_t data);
void send_frame_buffer(uint8_t cs_pin, uint16_t *buffer);
void select_display(uint8_t cs_pin);
void deselect_all();
void init_gc9a01();
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void clear_frame_buffer(uint16_t *buffer, uint16_t color);
