#include "sprites.h"
#include "display.h"
#include "generated_sprites.h"

#include <math.h>

void decompress_rle(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size) {
  uint32_t buffer_pos = 0;
  uint16_t data_pos = 0;
  const uint32_t max_pixels = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  
  while (buffer_pos < max_pixels && data_pos + 2 < size) {
    uint8_t count = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_low = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_high = pgm_read_byte_near(compressed_data + data_pos++);
    uint16_t color = (color_high << 8) | color_low;
    
    uint32_t pixels_to_write = min((uint32_t)count, max_pixels - buffer_pos);
    for (uint32_t i = 0; i < pixels_to_write; i++) {
      buffer[buffer_pos++] = color;
    }
    if (count > pixels_to_write) {
      break;
    }
  }
}

void decompress_rle_upscale(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size) {
  const uint16_t src_w = 120;
  const uint16_t src_h = 120;
  uint16_t src_pos = 0;
  uint16_t data_pos = 0;
  const uint32_t max_src = src_w * src_h;

  while (src_pos < max_src && data_pos + 2 < size) {
    uint8_t count = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_low = pgm_read_byte_near(compressed_data + data_pos++);
    uint8_t color_high = pgm_read_byte_near(compressed_data + data_pos++);
    uint16_t color = (color_high << 8) | color_low;

    uint32_t pixels_to_write = min((uint32_t)count, max_src - src_pos);
    for (uint32_t i = 0; i < pixels_to_write; i++) {
      uint16_t sx = src_pos % src_w;
      uint16_t sy = src_pos / src_w;
      uint16_t dx = sx * 2;
      uint16_t dy = sy * 2;
      buffer[dy * DISPLAY_WIDTH + dx] = color;
      buffer[dy * DISPLAY_WIDTH + dx + 1] = color;
      buffer[(dy + 1) * DISPLAY_WIDTH + dx] = color;
      buffer[(dy + 1) * DISPLAY_WIDTH + dx + 1] = color;
      src_pos++;
    }
    if (count > pixels_to_write) {
      break;
    }
  }
}

void decompress_palette(const uint8_t* compressed_data, const uint8_t* palette_data, 
                       uint16_t* buffer, uint8_t bits_per_pixel, uint16_t size) {
  uint16_t buffer_pos = 0;
  uint16_t data_pos = 0;
  uint8_t pixels_per_byte = 8 / bits_per_pixel;
  uint8_t pixel_mask = (1 << bits_per_pixel) - 1;
  
  while (buffer_pos < DISPLAY_WIDTH * DISPLAY_HEIGHT && data_pos < size) {
    uint8_t packed_pixels = pgm_read_byte_near(compressed_data + data_pos++);
    
    for (uint8_t i = 0; i < pixels_per_byte && buffer_pos < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
      uint8_t pixel_index = (packed_pixels >> (i * bits_per_pixel)) & pixel_mask;
      uint16_t color = pgm_read_word_near(palette_data + pixel_index * 2);
      buffer[buffer_pos++] = color;
    }
  }
}

void decompress_sprite(const SpriteFrame* sprite, uint16_t* buffer) {
  if (!sprite) return;
  
  const SpriteHeader* header = &sprite->header;
  
  clear_frame_buffer(buffer, COLOR_BLACK);
  
  switch (header->compression_type) {
    case SPRITE_UNCOMPRESSED:
      for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        buffer[i] = pgm_read_word_near(sprite->compressed_data + i * 2);
      }
      break;
      
    case SPRITE_RLE_COMPRESSED:
      if (sprite->upscale) {
        decompress_rle_upscale(sprite->compressed_data, buffer, header->compressed_size);
      } else {
        decompress_rle(sprite->compressed_data, buffer, header->compressed_size);
      }
      break;
      
    case SPRITE_PALETTE_4BIT:
      decompress_palette(sprite->compressed_data, sprite->palette_data, 
                        buffer, 4, header->compressed_size);
      break;
      
    case SPRITE_PALETTE_8BIT:
      decompress_palette(sprite->compressed_data, sprite->palette_data, 
                        buffer, 8, header->compressed_size);
      break;
  }
}

void apply_sprite_blink_overlay(uint16_t* buffer, float blink_amount) {
  if (blink_amount <= 0.0) return;
  
  uint16_t center_x = DISPLAY_WIDTH / 2;
  uint16_t blink_width = (uint16_t)(blink_amount * center_x);
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < blink_width; x++) {
      buffer[y * DISPLAY_WIDTH + x] = COLOR_BLACK;
      buffer[y * DISPLAY_WIDTH + (DISPLAY_WIDTH - 1 - x)] = COLOR_BLACK;
    }
  }
}

void render_sprite_frame(uint8_t frame_index, uint16_t* left_buffer, uint16_t* right_buffer) {
  if (frame_index >= sprite_count) return;
  
  const SpriteFrame* sprite = &sprite_frames[frame_index];
  
  decompress_sprite(sprite, left_buffer);
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
      uint16_t src_index = y * DISPLAY_WIDTH + x;
      uint16_t dst_x = y;
      uint16_t dst_y = DISPLAY_WIDTH - 1 - x;
      uint16_t dst_index = dst_y * DISPLAY_WIDTH + dst_x;
      right_buffer[dst_index] = left_buffer[src_index];
    }
  }
  
  for (uint16_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    left_buffer[i] = right_buffer[i];
  }
  
  for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
    for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
      uint16_t src_index = y * DISPLAY_WIDTH + x;
      uint16_t dst_x = DISPLAY_WIDTH - 1 - x;
      uint16_t dst_y = DISPLAY_HEIGHT - 1 - y;
      uint16_t dst_index = dst_y * DISPLAY_WIDTH + dst_x;
      right_buffer[dst_index] = left_buffer[src_index];
    }
  }
}

void load_sprite_data() {
  sprite_count = 0;
  
  uint8_t n = NUM_SPRITES < MAX_SPRITE_FRAMES ? NUM_SPRITES : MAX_SPRITE_FRAMES;
  for (uint8_t i = 0; i < n; i++) {
    sprite_frames[i].header.compression_type = SPRITE_RLE_COMPRESSED;
    sprite_frames[i].header.bits_per_pixel = 16;
    sprite_frames[i].header.compressed_size = sprite_sizes[i];
    sprite_frames[i].header.palette_size = 0;
    sprite_frames[i].header.frame_id = i;
    sprite_frames[i].palette_data = nullptr;
    sprite_frames[i].compressed_data = sprite_data[i];
    sprite_frames[i].upscale = sprite_upscale[i];
    sprite_count++;
  }
  
  Serial.printf("Loaded %d sprites successfully\n", sprite_count);
}

static const uint8_t font_space[FONT_H] = {0};

static const uint8_t font_A[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11111111, 0b11000011, 0b11000011, 0b11000011,
};
static const uint8_t font_B[FONT_H] = {
  0b11111100, 0b11000110, 0b11000011, 0b11111110,
  0b11000011, 0b11000011, 0b11000110, 0b11111100,
};
static const uint8_t font_C[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000000,
  0b11000000, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_D[FONT_H] = {
  0b11111100, 0b11000110, 0b11000011, 0b11000011,
  0b11000011, 0b11000011, 0b11000110, 0b11111100,
};
static const uint8_t font_E[FONT_H] = {
  0b11111111, 0b11000000, 0b11000000, 0b11111100,
  0b11000000, 0b11000000, 0b11000000, 0b11111111,
};
static const uint8_t font_F[FONT_H] = {
  0b11111111, 0b11000000, 0b11000000, 0b11111100,
  0b11000000, 0b11000000, 0b11000000, 0b11000000,
};
static const uint8_t font_G[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000000,
  0b11001111, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_H[FONT_H] = {
  0b11000011, 0b11000011, 0b11000011, 0b11111111,
  0b11000011, 0b11000011, 0b11000011, 0b11000011,
};
static const uint8_t font_I[FONT_H] = {
  0b01111110, 0b00011000, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b01111110,
};
static const uint8_t font_J[FONT_H] = {
  0b00011111, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b11001100, 0b01111100, 0b00111000,
};
static const uint8_t font_K[FONT_H] = {
  0b11000011, 0b11000110, 0b11001100, 0b11111000,
  0b11001100, 0b11000110, 0b11000011, 0b11000011,
};
static const uint8_t font_L[FONT_H] = {
  0b11000000, 0b11000000, 0b11000000, 0b11000000,
  0b11000000, 0b11000000, 0b11000000, 0b11111111,
};
static const uint8_t font_M[FONT_H] = {
  0b11000011, 0b11100111, 0b11010101, 0b11010101,
  0b11001011, 0b11001011, 0b11000011, 0b11000011,
};
static const uint8_t font_N[FONT_H] = {
  0b10000001, 0b11000001, 0b10100001, 0b10010001,
  0b10001001, 0b10000101, 0b10000011, 0b10000001,
};
static const uint8_t font_O[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11000011, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_P[FONT_H] = {
  0b11111100, 0b11000110, 0b11000011, 0b11000110,
  0b11111100, 0b11000000, 0b11000000, 0b11000000,
};
static const uint8_t font_Q[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11010011, 0b11001110, 0b01111100, 0b00111011,
};
static const uint8_t font_R[FONT_H] = {
  0b11111100, 0b11000110, 0b11000011, 0b11000110,
  0b11111100, 0b11001100, 0b11000110, 0b11000011,
};
static const uint8_t font_S[FONT_H] = {
  0b00111110, 0b01100011, 0b11000000, 0b01111100,
  0b00000110, 0b00000011, 0b11000110, 0b01111100,
};
static const uint8_t font_T[FONT_H] = {
  0b11111111, 0b11111111, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b00011000,
};
static const uint8_t font_U[FONT_H] = {
  0b11000011, 0b11000011, 0b11000011, 0b11000011,
  0b11000011, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_V[FONT_H] = {
  0b11000011, 0b11000011, 0b11000011, 0b11000011,
  0b01100110, 0b01100110, 0b00111100, 0b00011000,
};
static const uint8_t font_W[FONT_H] = {
  0b11000011, 0b11000011, 0b11000011, 0b11010101,
  0b11010101, 0b11100111, 0b11000011, 0b10000001,
};
static const uint8_t font_X[FONT_H] = {
  0b11000011, 0b11000011, 0b01100110, 0b00111100,
  0b00111100, 0b01100110, 0b11000011, 0b11000011,
};
static const uint8_t font_Y[FONT_H] = {
  0b11000011, 0b11000011, 0b01100110, 0b00111100,
  0b00011000, 0b00011000, 0b00011000, 0b00011000,
};
static const uint8_t font_Z[FONT_H] = {
  0b11111111, 0b11111111, 0b00000110, 0b00001100,
  0b00011000, 0b00110000, 0b01100000, 0b11111111,
};

static const uint8_t font_0[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b11000011, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_1[FONT_H] = {
  0b00011000, 0b00111000, 0b00011000, 0b00011000,
  0b00011000, 0b00011000, 0b00011000, 0b00111100,
};
static const uint8_t font_2[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b00000110,
  0b00001100, 0b00110000, 0b01100000, 0b11111111,
};
static const uint8_t font_3[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b00000110,
  0b00011100, 0b00000110, 0b11000011, 0b01111110,
};
static const uint8_t font_4[FONT_H] = {
  0b00001100, 0b00011100, 0b00101100, 0b01001100,
  0b11001100, 0b11111111, 0b00001100, 0b00001100,
};
static const uint8_t font_5[FONT_H] = {
  0b11111111, 0b11000000, 0b11000000, 0b11111100,
  0b00000110, 0b00000011, 0b11000011, 0b01111110,
};
static const uint8_t font_6[FONT_H] = {
  0b00011110, 0b00110000, 0b01100000, 0b11111100,
  0b11000110, 0b11000011, 0b11000011, 0b01111110,
};
static const uint8_t font_7[FONT_H] = {
  0b11111111, 0b11111111, 0b00000110, 0b00001100,
  0b00011000, 0b00110000, 0b01100000, 0b01100000,
};
static const uint8_t font_8[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b01111110,
  0b01111110, 0b11000011, 0b01111110, 0b00111100,
};
static const uint8_t font_9[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b11000011,
  0b01111110, 0b00000110, 0b00001100, 0b01111000,
};

static const uint8_t font_excl[FONT_H] = {
  0b00011000, 0b00011000, 0b00011000, 0b00011000,
  0b00011000, 0b00000000, 0b00011000, 0b00000000,
};
static const uint8_t font_pct[FONT_H] = {
  0b00000000, 0b01100010, 0b01100100, 0b00001000,
  0b00010000, 0b00100110, 0b01000110, 0b00000000,
};
static const uint8_t font_dot[FONT_H] = {
  0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00011000, 0b00011000,
};
static const uint8_t font_colon[FONT_H] = {
  0b00000000, 0b00011000, 0b00011000, 0b00000000,
  0b00000000, 0b00011000, 0b00011000, 0b00000000,
};
static const uint8_t font_dash[FONT_H] = {
  0b00000000, 0b00000000, 0b00000000, 0b11111111,
  0b11111111, 0b00000000, 0b00000000, 0b00000000,
};
static const uint8_t font_plus[FONT_H] = {
  0b00000000, 0b00011000, 0b00011000, 0b11111111,
  0b11111111, 0b00011000, 0b00011000, 0b00000000,
};
static const uint8_t font_slash[FONT_H] = {
  0b00000011, 0b00000110, 0b00001100, 0b00011000,
  0b00110000, 0b01100000, 0b11000000, 0b10000000,
};
static const uint8_t font_at[FONT_H] = {
  0b00111100, 0b01000010, 0b10100101, 0b10101011,
  0b10111101, 0b10000001, 0b01111110, 0b00000000,
};
static const uint8_t font_ques[FONT_H] = {
  0b00111100, 0b01111110, 0b11000011, 0b00000110,
  0b00011000, 0b00011000, 0b00000000, 0b00011000,
};

const uint8_t* get_font_bitmap(char c) {
  if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
  switch (c) {
    case ' ': return font_space;
    case 'A': return font_A;
    case 'B': return font_B;
    case 'C': return font_C;
    case 'D': return font_D;
    case 'E': return font_E;
    case 'F': return font_F;
    case 'G': return font_G;
    case 'H': return font_H;
    case 'I': return font_I;
    case 'J': return font_J;
    case 'K': return font_K;
    case 'L': return font_L;
    case 'M': return font_M;
    case 'N': return font_N;
    case 'O': return font_O;
    case 'P': return font_P;
    case 'Q': return font_Q;
    case 'R': return font_R;
    case 'S': return font_S;
    case 'T': return font_T;
    case 'U': return font_U;
    case 'V': return font_V;
    case 'W': return font_W;
    case 'X': return font_X;
    case 'Y': return font_Y;
    case 'Z': return font_Z;
    case '0': return font_0;
    case '1': return font_1;
    case '2': return font_2;
    case '3': return font_3;
    case '4': return font_4;
    case '5': return font_5;
    case '6': return font_6;
    case '7': return font_7;
    case '8': return font_8;
    case '9': return font_9;
    case '!': return font_excl;
    case '%': return font_pct;
    case '.': return font_dot;
    case ':': return font_colon;
    case '-': return font_dash;
    case '+': return font_plus;
    case '/': return font_slash;
    case '@': return font_at;
    case '?': return font_ques;
    default:  return font_space;
  }
}

void draw_char_scaled_rotated(uint16_t *buffer, int x, int y,
                              const uint8_t *bitmap, int scale, bool rotate_ccw) {
  for (int row = 0; row < FONT_H; row++) {
    uint8_t bits = bitmap[row];
    for (int col = 0; col < FONT_W; col++) {
      if (!(bits & (0x80 >> col))) continue;
      int bx, by;
      if (rotate_ccw) {
        bx = row;
        by = FONT_W - 1 - col;
      } else {
        bx = col;
        by = row;
      }
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          int px = x + bx * scale + sx;
          int py = y + by * scale + sy;
          if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
            buffer[py * DISPLAY_WIDTH + px] = display_text_color;
          }
        }
      }
    }
  }
}

void draw_text_vertical_ccw(uint16_t *buffer, const char *text, int len, int x, int y_top, int scale) {
  int char_h = FONT_H * scale;
  if (len < 1) len = 1;
  for (int i = 0; i < len; i++) {
    int y = y_top + (len - 1 - i) * char_h;
    draw_char_scaled_rotated(buffer, x, y, get_font_bitmap(text[i]), scale, true);
  }
}

void draw_text_display(const char *text) {
  display_bus_lock();

  clear_frame_buffer(left_eye_buffer, display_text_bg);
  clear_frame_buffer(right_eye_buffer, display_text_bg);

  int len = strlen(text);
  if (len > 0) {
    /* The panel is a 240x240 circle, so columns near the left/right edge of
       a full-square layout get clipped off the visible round surface.
       Lay columns out left->right and give each one a height equal to the
       circle's vertical span at that column's x position, so every glyph
       stays on-panel. Radius is inset by one glyph half-width so glyph
       corners at the block edges never leave the visible circle. */
    const int scale = 2;
    const int char_w = FONT_W * scale;
    const int char_h = FONT_H * scale;
    const int spacing = char_w + 2;
    const int radius = DISPLAY_HEIGHT / 2 - char_w / 2 - 4;
    const int cx = DISPLAY_WIDTH / 2;

    enum { MAX_TEXT_COLS = 16 };
    int col_x[MAX_TEXT_COLS];
    int col_cap[MAX_TEXT_COLS];
    int num_cols = 0;

    /* pass 1: place columns inside the widest inscribed rectangle so every
       column is fully on-panel, sizing each by the circle span at its x */
    int band_half = radius * 7071 / 10000;   /* radius / sqrt(2) */
    int x = cx - band_half;
    int remaining = len;
    while (remaining > 0 && num_cols < MAX_TEXT_COLS) {
      int dx = x + char_w / 2 - cx;
      int half = (int)sqrt((float)(radius * radius - dx * dx));
      int capacity = half * 2 / char_h;
      if (capacity < 1) capacity = 1;

      col_x[num_cols] = x;
      col_cap[num_cols] = capacity;
      remaining -= capacity;
      num_cols++;
      x += spacing;
    }

    /* pass 2: center the finished block horizontally */
    int block_w = col_x[num_cols - 1] + char_w - col_x[0];
    int shift = (DISPLAY_WIDTH - block_w) / 2 - col_x[0];

    int pos = 0;
    for (int col = 0; col < num_cols; col++) {
      int col_len = len - pos;
      if (col_len > col_cap[col]) col_len = col_cap[col];

      int col_x_final = col_x[col] + shift;
      int y_total = col_len * char_h;
      int y_top = (DISPLAY_HEIGHT - y_total) / 2;

      draw_text_vertical_ccw(left_eye_buffer, text + pos, col_len, col_x_final, y_top, scale);
      pos += col_len;
    }
  }

  uint32_t npix = DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (uint32_t i = 0; i < npix / 2; i++) {
    uint16_t t = left_eye_buffer[i];
    left_eye_buffer[i] = left_eye_buffer[npix - 1 - i];
    left_eye_buffer[npix - 1 - i] = t;
  }

  send_frame_buffer(LEFT_EYE_CS, left_eye_buffer);
  send_frame_buffer(RIGHT_EYE_CS, left_eye_buffer);

  display_bus_unlock();
}
