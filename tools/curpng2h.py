#!/usr/bin/env python3
import argparse
import itertools
from PIL import Image
from pathlib import Path


def encode_cursor(image_path: Path):
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    pixels = img.load()

    num_pixels = width * height
    num_bytes = (num_pixels + 7) // 8

    data = bytearray(num_bytes)
    mask = bytearray(num_bytes)

    for y in range(height):
        for x in range(width):
            i = y * width + x
            byte_index = i // 8
            bit_offset = 7 - (i % 8)

            r, g, b, a = pixels[x, y]

            if a >= 128:
                mask[byte_index] |= 1 << bit_offset  # opaque
                lum = int(0.299 * r + 0.587 * g + 0.114 * b)
                if lum < 128:
                    data[byte_index] |= 1 << bit_offset  # black pixel

    return data, mask, width, height


def to_c_array(name, data):
    lines = []
    for rowdata in itertools.batched(data, 12):
        lines.append(", ".join(f"0x{byte:02X}" for byte in rowdata) + ",")
    array_str = "\n    ".join(lines)
    return f"static const unsigned char {name}[] = {{\n    {array_str}\n}};\n"


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("inputs", nargs="+", help="PNG images", type=Path)
    args = parser.parse_args()

    input_files: list[Path] = args.inputs

    for input_file in input_files:
        data, mask, width, height = encode_cursor(input_file)

        input_file_name = input_file.stem
        output_file = input_file.with_name(f"{input_file_name}_bmp.h")

        with output_file.open("w", newline="\n") as f:
            f.write(f"#pragma once\n\n")
            f.write(f"// Generated from {input_file}\n")
            f.write(f"// Dimensions: {width}x{height}\n")
            f.write("// This file is auto-generated, do not edit it.\n\n")
            f.write(f'#include "cursor.h"\n\n')
            f.write(to_c_array(f"{input_file_name}_data", data))
            f.write("\n")
            f.write(to_c_array(f"{input_file_name}_mask", mask))
            f.write("\n")
            f.write(
                f"static const CursorBitmap {input_file_name}_cursor = {'{'} {width}, {height}, {input_file_name}_data, {input_file_name}_mask {'}'};\n"
            )

        print(f"Written {output_file} with cursor data.")


if __name__ == "__main__":
    raise SystemExit(main())
