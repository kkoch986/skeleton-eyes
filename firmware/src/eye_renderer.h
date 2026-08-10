#pragma once

#include "globals.h"

typedef struct {
  int16_t  look_x;
  int16_t  look_y;
  float    blink;
  float    squint_left;
  float    squint_right;
  uint16_t sclera;
  uint16_t iris_med;
  uint16_t iris_dark;
  float    falloff;
  float    minimum;
  float    strength;
  bool     sprite;
  int8_t   sprite_index;
} eye_frame_t;

uint16_t apply_3d_lighting(uint16_t base_color, int32_t dist_sq, int32_t radius_sq,
                           int16_t dx, int16_t dy, lighting_t* lighting);
uint16_t render_3d_iris(int16_t x, int16_t y, int16_t iris_x, int16_t iris_y,
                        int32_t dist_iris_sq, int32_t iris_radius_sq, bool is_inner,
                        uint16_t med_color, uint16_t dark_color);
void draw_skeleton_eye_buffered(uint16_t *buffer, const eye_frame_t *f,
                                float squint, bool mirror);
void renderer_init();
void update_eyes();
