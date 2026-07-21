#pragma once

#include "globals.h"

void decompress_rle(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size);
void decompress_rle_upscale(const uint8_t* compressed_data, uint16_t* buffer, uint16_t size);
void decompress_palette(const uint8_t* compressed_data, const uint8_t* palette_data,
                        uint16_t* buffer, uint8_t bits_per_pixel, uint16_t size);
void decompress_sprite(const SpriteFrame* sprite, uint16_t* buffer);
void apply_sprite_blink_overlay(uint16_t* buffer, float blink_amount);
void render_sprite_frame(uint8_t frame_index, uint16_t* left_buffer, uint16_t* right_buffer);
void load_sprite_data();

const uint8_t* get_font_bitmap(char c);
void draw_char_scaled_rotated(uint16_t *buffer, int x, int y,
                              const uint8_t *bitmap, int scale, bool rotate_ccw);
void draw_text_vertical_ccw(uint16_t *buffer, const char *text, int x, int y_top, int scale);
