#include "eye_renderer.h"
#include "display.h"
#include "sprites.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Async renderer task                                               */
/*                                                                   */
/*  Drawing + SPI frame pushes run on core 0 so the loop task (and   */
/*  the I2C slave that feeds it) never blocks on a slow render.      */
/*  Requests are "latest wins": the renderer always draws the newest */
/*  frame that was submitted while it was busy, so intermediate      */
/*  positions are skipped instead of queued.                         */
/* ------------------------------------------------------------------ */

static eye_frame_t pending_frame;
static volatile bool frame_pending = false;
static SemaphoreHandle_t render_sem = NULL;
static portMUX_TYPE render_mux = portMUX_INITIALIZER_UNLOCKED;

static void render_frame(const eye_frame_t *f) {
  display_bus_lock();

  if (f->sprite) {
    if (f->sprite_index >= 0 && f->sprite_index < sprite_count) {
      render_sprite_frame(f->sprite_index, left_eye_buffer, right_eye_buffer);
      if (f->blink > 0.0f) {
        apply_sprite_blink_overlay(left_eye_buffer, f->blink);
        apply_sprite_blink_overlay(right_eye_buffer, f->blink);
      }
      send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
      send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
    }
  } else {
    draw_skeleton_eye_buffered(left_eye_buffer, f, f->squint_left, false);
    draw_skeleton_eye_buffered(right_eye_buffer, f, f->squint_right, true);
    send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
    send_frame_buffer(RIGHT_EYE_CS, right_eye_buffer);
  }

  display_bus_unlock();
}

static void renderer_task(void *arg) {
  (void)arg;
  for (;;) {
    if (!frame_pending) {
      xSemaphoreTake(render_sem, portMAX_DELAY);
      continue;
    }

    eye_frame_t f;
    portENTER_CRITICAL(&render_mux);
    f = pending_frame;
    frame_pending = false;
    portEXIT_CRITICAL(&render_mux);

    render_frame(&f);
  }
}

void renderer_init() {
  if (render_sem) return;
  render_sem = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(renderer_task, "eye_render", 8192, NULL, 3, NULL, 0);
  Serial.println("Renderer: async task started on core 0");
}

static void renderer_request(const eye_frame_t *f) {
  if (!render_sem) return;
  portENTER_CRITICAL(&render_mux);
  pending_frame = *f;
  frame_pending = true;
  portEXIT_CRITICAL(&render_mux);
  xSemaphoreGive(render_sem);
}

/* ------------------------------------------------------------------ */
/*  Iris / lighting helpers                                           */
/* ------------------------------------------------------------------ */

uint16_t apply_3d_lighting(uint16_t base_color, int32_t dist_sq, int32_t radius_sq,
                           int16_t dx, int16_t dy, lighting_t* lighting) {
  (void)dx; (void)dy; (void)lighting;
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
                        int32_t dist_iris_sq, int32_t iris_radius_sq, bool is_inner,
                        uint16_t med_color, uint16_t dark_color) {
  if (dist_iris_sq >= iris_radius_sq) {
    return is_inner ? med_color : dark_color;
  }

  int16_t dx = x - iris_x;
  int16_t dy = y - iris_y;
  uint8_t pattern = (abs(dx) + abs(dy) + (dx ^ dy)) & 0x1F;

  /* pattern brightness in 1/256 units: 0.9 + pattern/160 */
  uint32_t brightness256 = 230 + ((uint32_t)pattern * 8) / 5;

  uint16_t base_color = is_inner ? med_color : dark_color;

  uint16_t r = (((base_color >> 11) & 0x1F) * brightness256) >> 8;
  uint16_t g = (((base_color >> 5) & 0x3F) * brightness256) >> 8;
  uint16_t b = ((base_color & 0x1F) * brightness256) >> 8;

  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;

  return (r << 11) | (g << 5) | b;
}

/* ------------------------------------------------------------------ */
/*  Procedural eye renderer                                           */
/* ------------------------------------------------------------------ */

void draw_skeleton_eye_buffered(uint16_t *buffer, const eye_frame_t *f,
                                float squint, bool mirror) {
  int16_t center_x = DISPLAY_WIDTH / 2;
  int16_t center_y = DISPLAY_HEIGHT / 2;
  int16_t socket_radius = 140;
  int16_t sclera_radius = 110;
  int16_t iris_radius = 45;
  int16_t pupil_radius = 22;

  int16_t effective_look_x = mirror ? -f->look_x : f->look_x;
  int16_t effective_look_y = mirror ? -f->look_y : f->look_y;

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

  float blink_eyelid = f->blink * sclera_radius;
  float squint_eyelid = squint * sclera_radius * f->strength;
  float total_eyelid_closure = blink_eyelid + squint_eyelid;
  if (total_eyelid_closure > sclera_radius) total_eyelid_closure = sclera_radius;

  bool has_eyelid = total_eyelid_closure > 0.0f;

  if (has_eyelid && total_eyelid_closure >= sclera_radius) {
    clear_frame_buffer(buffer, COLOR_BLACK);
    return;
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

    /* curved eyelid bounds for this row (previously per-pixel) */
    int16_t curved_left = center_x - sclera_radius;
    int16_t curved_right = center_x + sclera_radius;
    if (has_eyelid) {
      int16_t distance_from_center_y = abs(y - center_y);
      float normalized_distance = (float)distance_from_center_y / sclera_radius;
      float curve_factor = 1.0f + (normalized_distance * normalized_distance * f->falloff);
      if (curve_factor > (1.0f + f->minimum)) curve_factor = 1.0f + f->minimum;
      int16_t curved_eyelid_closure = (int16_t)(total_eyelid_closure * curve_factor);
      curved_left = center_x - sclera_radius + curved_eyelid_closure;
      curved_right = center_x + sclera_radius - curved_eyelid_closure;
    }

    for (int16_t x = 0; x < DISPLAY_WIDTH; x++, pixel_ptr++) {
      uint16_t color = COLOR_BLACK;

      if (has_eyelid && (x < curved_left || x > curved_right)) {
        *pixel_ptr = color;
        continue;
      }

      int32_t dx_center = x - center_x;
      int32_t dist_center_sq = dx_center * dx_center + dy_center_sq;

      uint32_t safe_socket_radius_sq = socket_radius_sq + 50;
      bool is_socket_area = (dist_center_sq <= safe_socket_radius_sq);
      bool is_sclera_area = (dist_center_sq <= sclera_radius_sq);

      if (is_socket_area) {
        color = COLOR_BLACK;

        if (is_sclera_area) {
          color = apply_3d_lighting(f->sclera, dist_center_sq, sclera_radius_sq,
                                    dx_center, dy_center, &eye_lighting);

          int32_t dx_iris = x - iris_x;
          int32_t dist_iris_sq = dx_iris * dx_iris + dy_iris_sq;
          int16_t effective_dx_iris = mirror ? -dx_iris : dx_iris;

          if (dist_iris_sq <= iris_radius_sq) {
            bool is_inner = (dist_iris_sq <= inner_iris_radius_sq);
            color = render_3d_iris(x, y, iris_x, iris_y, dist_iris_sq, iris_radius_sq,
                                   is_inner, f->iris_med, f->iris_dark);
            color = apply_3d_lighting(color, dist_iris_sq, iris_radius_sq,
                                      effective_dx_iris, dy_iris, &eye_lighting);

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

        if (is_sclera_area) {
          uint32_t boundary_threshold = sclera_radius_sq - 50;
          if (dist_center_sq >= boundary_threshold) {
            color = COLOR_BLACK;
          }
        }
      }

      *pixel_ptr = color;
    }
  }
}

/* ------------------------------------------------------------------ */
/*  Simulation + frame submission (runs on loop/core 1)               */
/* ------------------------------------------------------------------ */

void update_eyes() {
  static uint32_t last_blink = 0;
  static uint32_t last_look_change = 0;
  static uint32_t blink_start = 0;
  static bool blinking = false;
  static uint32_t last_interp = 0;
  static int16_t last_submitted_x = -999;
  static int16_t last_submitted_y = -999;
  static float last_submitted_blink = -1.0f;
  static uint16_t last_submitted_sclera = 0xFFFF;
  static uint16_t last_submitted_iris_dark = 0xFFFF;
  static uint16_t last_submitted_iris_med = 0xFFFF;
  static float last_submitted_squint = -1.0f;
  static int8_t last_submitted_sprite = -2;
  static bool last_submitted_sprite_mode = false;
  static float smooth_x = 0.0f;
  static float smooth_y = 0.0f;
  static bool smooth_init = false;

  uint32_t now = millis();

  if (ota_display_active || waiting_display_shown || i2c_just_ready || display_text_active) {
    last_interp = now;
    return;
  }

  /* frame-rate independent look smoothing so loop speed changes
     don't change the eye's convergence time constant. */
  uint32_t dt = now - last_interp;
  last_interp = now;
  if (dt > 100) dt = 100;
  float k = 1.0f - expf(-(float)dt * i2c_smoothing * 0.5f);

  if (!smooth_init) {
    smooth_x = current_look_x;
    smooth_y = current_look_y;
    smooth_init = true;
  }
  /* pick up hard-set writes to current_look_x/y from i2c_commands.cpp
     (larger than smoothing can produce in one step) */
  if (abs(current_look_x - (int16_t)smooth_x) > 4) smooth_x = current_look_x;
  if (abs(current_look_y - (int16_t)smooth_y) > 4) smooth_y = current_look_y;

  if (!i2c_external_control) {
    if (now - last_look_change > random(3000, 8000)) {
      target_look_x = random(-30, 31);
      target_look_y = random(-25, 26);
      last_look_change = now;
    }
    smooth_x += (target_look_x - smooth_x) * k;
    smooth_y += (target_look_y - smooth_y) * k;
  } else {
    smooth_x += (i2c_look_x - smooth_x) * k;
    smooth_y += (i2c_look_y - smooth_y) * k;
    global_squint = i2c_squint_level;
  }

  current_look_x = (int16_t)smooth_x;
  current_look_y = (int16_t)smooth_y;

  if (auto_blink_enabled && !blinking &&
      (now - last_blink > random(auto_blink_interval / 2, auto_blink_interval * 3 / 2 + 1))) {
    blinking = true;
    blink_start = now;
    last_blink = now;
  }

  if (i2c_blink_trigger) {
    blink_start = now;
    blinking = true;
    i2c_blink_trigger = false;
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

  bool need = false;

  if (last_submitted_sprite_mode != sprite_mode || force_eye_repaint) {
    last_submitted_sprite_mode = sprite_mode;
    force_eye_repaint = false;
    need = true;
  }

  if (sprite_mode) {
    if (current_sprite != last_submitted_sprite) need = true;
    if (abs(blink_amount - last_submitted_blink) > 0.05f) need = true;
  } else {
    if (abs(current_look_x - last_submitted_x) > 1 ||
        abs(current_look_y - last_submitted_y) > 1) {
      need = true;
    }
    if (abs(blink_amount - last_submitted_blink) > 0.05f) {
      need = true;
    }
    if (current_sclera_color != last_submitted_sclera ||
        current_iris_dark_color != last_submitted_iris_dark ||
        current_iris_med_color != last_submitted_iris_med) {
      need = true;
    }
    if (global_squint != last_submitted_squint) {
      need = true;
    }
  }

  if (!need) return;

  eye_frame_t f;
  f.blink = blink_amount;
  f.sprite = sprite_mode;

  if (sprite_mode) {
    f.sprite_index = current_sprite;
  } else {
    f.sprite_index = -1;
    f.look_x = current_look_x;
    f.look_y = current_look_y;
    f.squint_left = global_squint + left_squint;
    f.squint_right = global_squint + right_squint;
    f.sclera = current_sclera_color;
    f.iris_med = current_iris_med_color;
    f.iris_dark = current_iris_dark_color;
    f.falloff = curve_falloff;
    f.minimum = curve_minimum;
    f.strength = closure_strength;
  }

  renderer_request(&f);

  last_submitted_x = current_look_x;
  last_submitted_y = current_look_y;
  last_submitted_blink = blink_amount;
  last_submitted_sclera = current_sclera_color;
  last_submitted_iris_dark = current_iris_dark_color;
  last_submitted_iris_med = current_iris_med_color;
  last_submitted_squint = global_squint;
  last_submitted_sprite = current_sprite;
}
