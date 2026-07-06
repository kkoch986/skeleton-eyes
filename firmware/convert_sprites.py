#!/usr/bin/env python3
"""
Sprite converter for Skeleton Eye Display.

Converts all images in sprites/ to RLE-compressed sprite data indexed by
alphabetical sort order. Use the index via I2C command 0x18 (SET_SPRITE).

Usage:
  python3 convert_sprites.py            # full resolution (240×240)
  python3 convert_sprites.py --half     # half resolution (120×120, ~4x less flash)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Install with: pip install Pillow")
    sys.exit(1)

SUPPORTED_EXTS = {'.png', '.jpg', '.jpeg', '.bmp', '.tiff'}


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rle_compress(pixels):
    compressed = []
    if not pixels:
        return compressed
    color = pixels[0]
    count = 1
    for p in pixels[1:]:
        if p == color and count < 255:
            count += 1
        else:
            compressed += [count, color & 0xFF, (color >> 8) & 0xFF]
            color = p
            count = 1
    compressed += [count, color & 0xFF, (color >> 8) & 0xFF]
    return compressed


def load_image(path, size=(240, 240)):
    img = Image.open(path).convert('RGB')
    try:
        img = img.resize(size, Image.Resampling.LANCZOS)
    except AttributeError:
        img = img.resize(size, Image.LANCZOS)
    pixels = [rgb565(*img.getpixel((x, y))) for y in range(size[1]) for x in range(size[0])]
    return pixels


def generate_header(frames):
    upscales = [up for _, _, up in frames]

    lines = [
        "/*",
        " * Generated Eye Sprite Data",
        " * Converted by convert_sprites.py",
        " */",
        "",
        "#pragma once",
        "#include <Arduino.h>",
        "",
    ]

    for name, comp, _ in frames:
        lines.append(f"const uint8_t PROGMEM sprite_{name}_rle[] = {{")
        for i in range(0, len(comp), 12):
            row = ", ".join(f"0x{b:02X}" for b in comp[i:i+12])
            lines.append(f"    {row}," if i + 12 < len(comp) else f"    {row}")
        lines.append("};")
        lines.append(f"const uint16_t sprite_{name}_size = {len(comp)};")
        lines.append("")

    lines.append("const uint8_t* const sprite_data[] = {")
    for name, _, _ in frames:
        lines.append(f"    sprite_{name}_rle,")
    lines.append("};")
    lines.append("")

    lines.append("const uint16_t sprite_sizes[] = {")
    for name, _, _ in frames:
        lines.append(f"    sprite_{name}_size,")
    lines.append("};")
    lines.append("")

    lines.append("const bool sprite_upscale[] = {")
    for up in upscales:
        lines.append(f"    {'true' if up else 'false'},")
    lines.append("};")
    lines.append("")

    lines.append(f"const uint8_t NUM_SPRITES = {len(frames)};")
    lines.append("")

    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--half', action='store_true', help='Resample to 120×120 and upscale at runtime')
    args = parser.parse_args()

    size = 120 if args.half else 240

    sprites_dir = Path("sprites")
    if not sprites_dir.is_dir():
        print(f"ERROR: {sprites_dir}/ not found")
        return 1

    image_files = sorted(
        [p for p in sprites_dir.iterdir() if p.suffix.lower() in SUPPORTED_EXTS],
        key=lambda p: p.stem.lower(),
    )

    if not image_files:
        print(f"ERROR: No images found in {sprites_dir}/")
        return 1

    suffix = "half" if args.half else "full"
    print(f"Converting {len(image_files)} sprites ({size}×{size}, {suffix} resolution)")
    print()

    frames = []
    total_orig = 0
    total_comp = 0

    for path in image_files:
        name = path.stem
        print(f"  {path.name} ...", end=" ")
        pixels = load_image(path, (size, size))
        comp = rle_compress(pixels)
        orig = len(pixels) * 2
        ratio = orig / len(comp) if comp else 1
        print(f"{orig} -> {len(comp)} bytes ({ratio:.1f}x)")
        frames.append((name, comp, args.half))
        total_orig += orig
        total_comp += len(comp)

    header = generate_header(frames)

    out = Path("src/generated_sprites.h")
    with open(out, "w") as f:
        f.write(header)

    budget = 530000
    pct = 100.0 * total_comp / budget
    print()
    print(f"Wrote {len(frames)} sprites to {out}")
    print(f"Total: {total_orig} -> {total_comp} bytes ({total_orig/total_comp:.1f}x)")
    print(f"Flash: {pct:.0f}% of ~{budget//1000}KB sprite budget used")

    if total_comp > budget:
        print(f"WARNING: Exceeded {budget//1000}KB budget by {total_comp - budget} bytes.")
        print(f"Remove some images from sprites/ or use --half.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
