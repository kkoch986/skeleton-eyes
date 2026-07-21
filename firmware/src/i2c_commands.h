#pragma once

#include "globals.h"

bool init_i2c_safe();
void init_i2c_system();
void onReceive(int num_bytes);
void onRequest();
void process_i2c_command();
