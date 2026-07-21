#pragma once

#include "globals.h"

#define I2C_READY_DISPLAY_MS 3000

void ota_display_init();
void ota_display_update(unsigned int progress, unsigned int total);
void show_waiting_display();
void show_i2c_ready_display();
