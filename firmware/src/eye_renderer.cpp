#include "eye_renderer.h"
#include "display.h"
#include "sprites.h"

uint16_t apply_3d_lighting(uint16_t base_color, int32_t dist_sq, int32_t radius_sq,
                           int16_t dx, int16_t dy, lighting_t* lighting) {
  if (dist_sq >= radius_sq) return base_color;
  if (base_color == COLOR_BLACK || base_color == COLOR_SOCKET) return COLOR_BLACK;
  if (dist_sq < 0) return base_color;
  
  if (radius_sq == 0) return base_color;
  
   uint32_t norm_dist_sq_256 = ((uint32_t)dist_sq << 8) / radius_sq;
   if (norm_dist_sq_256 >= 256) return base_color;
   
   if (norm_dist_sq_256 >= 240) {
     norm_dist_sq_256 = 255;
   }
  
   uint32_t reduction = (norm_dist_sq_256 * 192) >> 8;
   uint32_t sphere_factor_256 = 256 - reduction;
   if (sphere_factor_256 < 51) sphere_factor_256 = 51;
  
  uint16_t r = (((base_color >> 11) & 0x1F) * sphere_factor_256) >> 8;
  uint16_t g = (((base_color >> 5) & 0x3F) * sphere_factor_256) >> 8;  
  uint16_t b = ((base_color & 0x1F) * sphere_factor_256) >> 8;
  
  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;
  
  return (r << 11) | (g << 5) | b;
}

uint16_t render_3d_iris(int16_t x, int16_t y, int16_t iris_x, int16_t iris_y,
                       int32_t dist_iris_sq, int32_t iris_radius_sq, bool is_inner) {
  if (dist_iris_sq >= iris_radius_sq) {
    return is_inner ? current_iris_med_color : current_iris_dark_color;
  }
  
  int16_t dx = x - iris_x;
  int16_t dy = y - iris_y;
  uint8_t pattern = (abs(dx) + abs(dy) + (dx ^ dy)) & 0x1F;
  float pattern_brightness = 0.9f + (pattern / 160.0f);
  
  uint16_t base_color = is_inner ? current_iris_med_color : current_iris_dark_color;
  
  uint16_t r = ((base_color >> 11) & 0x1F) * pattern_brightness;
  uint16_t g = ((base_color >> 5) & 0x3F) * pattern_brightness;
  uint16_t b = (base_color & 0x1F) * pattern_brightness;
  
  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;
  
  return (r << 11) | (g << 5) | b;
}

void draw_skeleton_eye_buffered(uint16_t *buffer, int16_t look_x,
                                int16_t look_y, float blink_amount, float squint_amount,
                                bool mirror) {
  int16_t center_x = DISPLAY_WIDTH / 2;
  int16_t center_y = DISPLAY_HEIGHT / 2;
  int16_t socket_radius = 140;
  int16_t sclera_radius = 110;
  int16_t iris_radius = 45;
  int16_t pupil_radius = 22;

  int16_t effective_look_x = mirror ? -look_x : look_x;
  int16_t effective_look_y = mirror ? -look_y : look_y;

  int16_t max_iris_offset = sclera_radius - iris_radius - 5;
  int16_t iris_x =
      center_x + constrain(effective_look_x, -max_iris_offset, max_iris_offset);
  int16_t iris_y =
      center_y + constrain(effective_look_y, -max_iris_offset, max_iris_offset);

  int16_t pupil_x = iris_x;
  int16_t pupil_y = iris_y;

  int32_t socket_radius_sq = (int32_t)socket_radius * socket_radius;
  int32_t sclera_radius_sq = (int32_t)sclera_radius * sclera_radius;
  int32_t iris_radius_sq = (int32_t)iris_radius * iris_radius;
  int32_t pupil_radius_sq = (int32_t)pupil_radius * pupil_radius;
  int32_t inner_iris_radius_sq = (int32_t)(iris_radius - 8) * (iris_radius - 8);
  int32_t reflection_radius_sq = 4 * 4;

    float blink_eyelid = blink_amount * sclera_radius;
   float squint_eyelid = squint_amount * sclera_radius * closure_strength;
   float total_eyelid_closure = blink_eyelid + squint_eyelid;
   if (total_eyelid_closure > sclera_radius) total_eyelid_closure = sclera_radius;

   int16_t visible_left = 0;
   int16_t visible_right = DISPLAY_WIDTH - 1;
   int16_t visible_top = 0;
   int16_t visible_bottom = DISPLAY_HEIGHT - 1;

   if (total_eyelid_closure > 0.0f) {
     int16_t eyelid_closure = (int16_t)total_eyelid_closure;
     
     visible_left = center_x - sclera_radius + eyelid_closure;
     visible_right = center_x + sclera_radius - eyelid_closure;
     
     visible_top = 0;
     visible_bottom = DISPLAY_HEIGHT - 1;

     if (visible_left + 2 >= visible_right) {
        clear_frame_buffer(buffer, COLOR_BLACK);
        return;
      }
    }

   clear_frame_buffer(buffer, COLOR_BLACK);

   uint16_t *pixel_ptr = buffer;

  for (int16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    int32_t dy_center = y - center_y;
    int32_t dy_iris = y - iris_y;
    int32_t dy_pupil = y - pupil_y;
    int32_t dy_reflection = y - (pupil_y - 6);

    int32_t dy_center_sq = dy_center * dy_center;
    int32_t dy_iris_sq = dy_iris * dy_iris;
    int32_t dy_pupil_sq = dy_pupil * dy_pupil;
    int32_t dy_reflection_sq = dy_reflection * dy_reflection;

    for (int16_t x = 0; x < DISPLAY_WIDTH; x++, pixel_ptr++) {
      uint16_t color = COLOR_BLACK;

       bool in_visible_area = true;
       
        if (total_eyelid_closure > 0.0f) {
         int16_t distance_from_center_y = abs(y - center_y);
         float normalized_distance = (float)distance_from_center_y / sclera_radius;
         float curve_factor = 1.0f + (normalized_distance * normalized_distance * curve_falloff);
         if (curve_factor > (1.0f + curve_minimum)) curve_factor = 1.0f + curve_minimum;
          
           int16_t curved_eyelid_closure = (int16_t)(total_eyelid_closure * curve_factor);
         int16_t curved_left = center_x - sclera_radius + curved_eyelid_closure;
         int16_t curved_right = center_x + sclera_radius - curved_eyelid_closure;
         
         in_visible_area = (x >= curved_left && x <= curved_right);
       }

       if (!in_visible_area && total_eyelid_closure > 0.0f) {
        *pixel_ptr = COLOR_BLACK;
        continue;
      }

      int32_t dx_center = x - center_x;
      int32_t dist_center_sq = dx_center * dx_center + dy_center_sq;

      lighting_t effective_lighting = eye_lighting;
      int16_t effective_dx_center = dx_center;
      int16_t effective_dy_center = dy_center;

      uint32_t safe_socket_radius_sq = socket_radius_sq + 50;
      bool is_socket_area = (dist_center_sq <= safe_socket_radius_sq);
      bool is_sclera_area = (dist_center_sq <= sclera_radius_sq);
      
      if (is_socket_area) {
          color = COLOR_BLACK;

          if (is_sclera_area) {
            color = apply_3d_lighting(current_sclera_color, dist_center_sq, sclera_radius_sq, 
                                    effective_dx_center, effective_dy_center, &effective_lighting);

           int32_t dx_iris = x - iris_x;
           int32_t dist_iris_sq = dx_iris * dx_iris + dy_iris_sq;
           int16_t effective_dx_iris = mirror ? -dx_iris : dx_iris;

           if (dist_iris_sq <= iris_radius_sq) {
              bool is_inner = (dist_iris_sq <= inner_iris_radius_sq);
              color = render_3d_iris(x, y, iris_x, iris_y, dist_iris_sq, iris_radius_sq, is_inner);
              color = apply_3d_lighting(color, dist_iris_sq, iris_radius_sq, 
                                       effective_dx_iris, dy_iris, &effective_lighting);

             int32_t dx_pupil = x - pupil_x;
             int32_t dist_pupil_sq = dx_pupil * dx_pupil + dy_pupil_sq;

             if (dist_pupil_sq <= pupil_radius_sq) {
                float pupil_depth = 1.0f - (float)dist_pupil_sq / pupil_radius_sq;
                uint16_t pupil_brightness = (uint16_t)(10 + pupil_depth * 15);
                
                uint16_t pupil_r = (pupil_brightness >> 2) & 0x1F;
                uint16_t pupil_g = (pupil_brightness >> 1) & 0x3F;  
                uint16_t pupil_b = (pupil_brightness >> 2) & 0x1F;
               color = (pupil_r << 11) | (pupil_g << 5) | pupil_b;

               int16_t reflection_offset_x = mirror ? 6 : -6;
               int32_t dx_reflection = x - (pupil_x + reflection_offset_x);
               int32_t dist_reflection_sq = dx_reflection * dx_reflection + dy_reflection_sq;

               if (dist_reflection_sq <= reflection_radius_sq) {
                  color = COLOR_WHITE;
                } else {
                  int16_t reflection2_offset_x = mirror ? -3 : 3;
                  int32_t dx_reflection2 = x - (pupil_x + reflection2_offset_x);
                  int32_t dy_reflection2 = y - (pupil_y + 2);
                  int32_t dist_reflection2_sq = dx_reflection2 * dx_reflection2 + dy_reflection2 * dy_reflection2;
                  
                  if (dist_reflection2_sq <= 4) {
                    uint16_t r = ((color >> 11) & 0x1F) + 8;
                    uint16_t g = ((color >> 5) & 0x3F) + 16;
                    uint16_t b = (color & 0x1F) + 8;
                    
                    r = r > 0x1F ? 0x1F : r;
                    g = g > 0x3F ? 0x3F : g;
                    b = b > 0x1F ? 0x1F : b;
                    
                    color = (r << 11) | (g << 5) | b;
             }
           }
         }
            }
          }
          }

        if (is_sclera_area) {
         uint32_t boundary_threshold = sclera_radius_sq - 50;
         if (dist_center_sq >= boundary_threshold) {
            color = COLOR_BLACK;
          }
        }

        *pixel_ptr = color;
    }
  }
}

void update_eyes() {
  static uint32_t last_blink = 0;
  static uint32_t last_look_change = 0;
  static uint32_t last_frame = 0;
  static uint32_t blink_start = 0;
  static bool blinking = false;
  static bool last_blinking = false;
  static int16_t last_look_x = -999;
  static int16_t last_look_y = -999;
  static float last_blink_amount = -1.0f;

  uint32_t now = millis();

  if (ota_display_active || waiting_display_shown || i2c_just_ready) {
    last_frame = now;
    return;
  }

  if (now - last_frame < 1) {
    return;
  }

  if (sprite_mode) {
    if (auto_blink_enabled && !blinking && (now - last_blink > random(auto_blink_interval / 2, auto_blink_interval * 3 / 2 + 1))) {
      blinking = true;
      blink_start = now;
      last_blink = now;
    }

    float blink_amount = 0.0f;
    if (blinking) {
      uint32_t blink_duration = 500;
      uint32_t blink_elapsed = now - blink_start;

      if (blink_elapsed < blink_duration) {
        float progress = (float)blink_elapsed / blink_duration;
        if (progress < 0.4f) {
          blink_amount = progress / 0.4f;
        } else if (progress < 0.6f) {
          blink_amount = 1.0f;
        } else {
          blink_amount = 1.0f - (progress - 0.6f) / 0.4f;
        }
      } else {
        blinking = false;
        blink_amount = 0.0f;
      }
    }

    if (current_sprite >= 0 && current_sprite < sprite_count) {
      render_sprite_frame(current_sprite, left_eye_buffer, right_eye_buffer);

      if (blink_amount > 0.0f) {
        apply_sprite_blink_overlay(left_eye_buffer, blink_amount);
        apply_sprite_blink_overlay(right_eye_buffer, blink_amount);
      }

      send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
      send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
    }

  } else {
    if (auto_blink_enabled && !blinking && (now - last_blink > random(auto_blink_interval / 2, auto_blink_interval * 3 / 2 + 1))) {
      blinking = true;
      blink_start = now;
      last_blink = now;
    }

    float blink_amount = 0.0f;
    if (blinking) {
      uint32_t blink_duration = 300;
      uint32_t blink_elapsed = now - blink_start;

      if (blink_elapsed < blink_duration) {
        float progress = (float)blink_elapsed / blink_duration;
        if (progress < 0.4f) {
          blink_amount = progress / 0.4f;
        } else if (progress < 0.6f) {
          blink_amount = 1.0f;
        } else {
          blink_amount = 1.0f - (progress - 0.6f) / 0.4f;
        }
      } else {
        blinking = false;
        blink_amount = 0.0f;
      }
    }

    if (i2c_blink_trigger) {
      blink_start = now;
      blinking = true;
      i2c_blink_trigger = false;
    }

    if (!i2c_external_control) {
      if (now - last_look_change > random(3000, 8000)) {
        target_look_x = random(-30, 31);
        target_look_y = random(-25, 26);
        last_look_change = now;
      }

      current_look_x += (target_look_x - current_look_x) * 0.1f;
      current_look_y += (target_look_y - current_look_y) * 0.1f;
    } else {
      current_look_x += (i2c_look_x - current_look_x) * i2c_smoothing;
      current_look_y += (i2c_look_y - current_look_y) * i2c_smoothing;
    }

    bool need_update = false;

    if (abs(current_look_x - last_look_x) > 1 ||
        abs(current_look_y - last_look_y) > 1) {
      need_update = true;
    }

    if (abs(blink_amount - last_blink_amount) > 0.05f) {
      need_update = true;
    }

    if (blinking != last_blinking) {
      need_update = true;
    }

    if (blinking) need_update = true;

    if (i2c_external_control) {
      global_squint = i2c_squint_level;
    }

    if (need_update) {
      draw_skeleton_eye_buffered(left_eye_buffer, current_look_x, current_look_y,
                                 blink_amount, global_squint + left_squint, false);
      draw_skeleton_eye_buffered(right_eye_buffer, current_look_x, current_look_y,
                                 blink_amount, global_squint + right_squint, true);

      send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
      send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);

      last_look_x = current_look_x;
      last_look_y = current_look_y;
      last_blink_amount = blink_amount;
      last_blinking = blinking;
    }
  }

  last_frame = now;
}
