#pragma once

#include "globals.h"

uint16_t apply_3d_lighting(uint16_t base_color, int32_t dist_sq, int32_t radius_sq,
                           int16_t dx, int16_t dy, lighting_t* lighting);
uint16_t render_3d_iris(int16_t x, int16_t y, int16_t iris_x, int16_t iris_y,
                        int32_t dist_iris_sq, int32_t iris_radius_sq, bool is_inner);
void draw_skeleton_eye_buffered(uint16_t *buffer, int16_t look_x,
                                int16_t look_y, float blink_amount, float squint_amount,
                                bool mirror);
void update_eyes();
