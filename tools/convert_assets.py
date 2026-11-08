#!/usr/bin/env python3
"""
convert_assets.py : Builds a game's pictures and sounds into C, for the PicoCalc.

A microcontroller has no file system the game can LoadTexture() from, and not
nearly enough RAM for a big sprite sheet. So this script:

  1. finds every "resources/....png" and "resources/....wav" in the game's
     .c files (LoadTexture("resources/hero.png"), LoadSound(...) ...)
  2. shrinks each picture by the game's scale on the PicoCalc (none, for a
     game written at 320 x 320; see find_world_size() below), and stores it
     with 4 bits per pixel: 15 colours plus "transparent"
  3. turns each sound into 8-bit samples at SHIM_PCM_RATE
  4. writes it all to <game>/shim_assets.c, with a table the shim's
     LoadTexture() and LoadSound() look the names up in

The game's code doesn't change: LoadTexture("resources/hero.png") finds the
shrunk copy. The paths are relative to the game's folder, as on the desktop
(where you run the game from its folder).

Run it whenever the art or sounds change:

    python3 tools/convert_assets.py game

Needs Pillow:  pip install pillow
"""

import math
import os
import re
import struct
import sys
import wave

try:
    from PIL import Image
except ImportError:
    sys.exit("convert_assets.py needs Pillow: pip install pillow")

PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
GAME = PROJECT  # The game's folder: set in main()


# ---------------------------------------------------------------------------
# Settings from the board's board_config.h
# ---------------------------------------------------------------------------

def read_board_config():
    path = os.path.join(PROJECT, "picocalc", "board_config.h")
    config = {}
    for name, value in re.findall(r"^\s*#define\s+(SHIM_\w+)\s+(\S+)", open(path).read(), re.M):
        config[name] = value
    return config


def config_int(config, name, default):
    return int(config.get(name, default))


# ---------------------------------------------------------------------------
# Which files does the game load?
# ---------------------------------------------------------------------------

def find_game_assets():
    """Every "resources/....png" or ".wav" string in the game's .c files that exists."""
    textures, sounds = set(), set()
    src = GAME
    for name in sorted(os.listdir(src)):
        if not name.endswith(".c"):
            continue
        text = open(os.path.join(src, name), encoding="utf-8", errors="replace").read()
        for path in re.findall(r'"(resources/[^"]+\.(?:png|wav))"', text):
            if os.path.exists(os.path.join(GAME, path)):
                (textures if path.endswith(".png") else sounds).add(path)
            else:
                print(f"  warning: {name} asks for {path}, which doesn't exist")
    return sorted(textures), sorted(sounds)


def pixels_of(image):
    """All the pixels, as a list (get_flattened_data in new Pillow, getdata in old)."""
    if hasattr(image, "get_flattened_data"):
        return list(image.get_flattened_data())
    return list(image.getdata())


def c_name(path):
    return re.sub(r"\W", "_", os.path.splitext(os.path.basename(path))[0])


def c_bytes(data, per_line=24):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("    " + ", ".join(str(b) for b in data[i:i + per_line]) + ",")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Sprite sheets: shrink, then 15 colours + transparent, 4 bits per pixel
# ---------------------------------------------------------------------------

def find_world_size():
    """The game's InitWindow() size: #define SCREEN_WIDTH / SCREEN_HEIGHT, or
    numbers written straight into InitWindow(...). None if neither is there."""
    text = ""
    for name in sorted(os.listdir(GAME)):
        if name.endswith((".c", ".h")):
            text += open(os.path.join(GAME, name), encoding="utf-8", errors="replace").read()
    w = re.search(r"#define\s+SCREEN_WIDTH\s+(\d+)", text)
    h = re.search(r"#define\s+SCREEN_HEIGHT\s+(\d+)", text)
    if w and h:
        return int(w.group(1)), int(h.group(1))
    m = re.search(r"InitWindow\s*\(\s*(\d+)\s*,\s*(\d+)", text)
    if m:
        return int(m.group(1)), int(m.group(2))
    return None


def convert_texture(path, scale):
    image = Image.open(os.path.join(GAME, path)).convert("RGBA")
    world_w, world_h = image.size

    # Shrunk by the game's scale, rounded so a 64 x 64 sprite cell stays a
    # whole number of pixels: every frame's rectangle then lands exactly on
    # the shrunk grid
    cell = max(1, round(64 * scale))
    data_w = max(1, round(world_w * cell / 64))
    data_h = max(1, round(world_h * cell / 64))
    small = image if (data_w, data_h) == (world_w, world_h) else image.resize((data_w, data_h), Image.Resampling.BOX)

    pixels = pixels_of(small)
    opaque = [p[:3] for p in pixels if p[3] >= 128]

    # The 15 colours that best cover the opaque pixels
    if opaque:
        strip = Image.new("RGB", (len(opaque), 1))
        strip.putdata(opaque)
        quantised = strip.quantize(colors=15, method=Image.Quantize.MEDIANCUT)
        flat = quantised.getpalette()[:15 * 3]
        colours = [tuple(flat[i:i + 3]) for i in range(0, len(flat), 3)]
    else:
        colours = []
    colours += [(0, 0, 0)] * (15 - len(colours))

    # Index 0 is transparent, 1..15 the colours
    palette_image = Image.new("P", (1, 1))
    palette_image.putpalette([c for colour in colours for c in colour] + [0] * (256 - 15) * 3)
    rgb = small.convert("RGB").quantize(palette=palette_image, dither=Image.Dither.NONE)
    indices = pixels_of(rgb)

    row_bytes = (data_w + 1) // 2
    packed = bytearray(row_bytes * data_h)
    for y in range(data_h):
        for x in range(data_w):
            i = y * data_w + x
            index = 0 if pixels[i][3] < 128 else min(indices[i], 14) + 1
            if x % 2 == 0:
                packed[y * row_bytes + x // 2] |= index << 4
            else:
                packed[y * row_bytes + x // 2] |= index

    palette = [0, 0, 0, 0]  # Transparent
    for colour in colours:
        palette += [colour[0], colour[1], colour[2], 255]

    name = c_name(path)
    code = [
        f"// {path}: {world_w} x {world_h}, stored {data_w} x {data_h} ({cell} px per 64 px cell), 4 bits per pixel",
        f"static const uint8_t {name}_palette[16 * 4] = {{\n{c_bytes(palette, 16)}\n}};",
        f"static const uint8_t {name}_pixels[{len(packed)}] = {{\n{c_bytes(packed)}\n}};",
        f"static const ShimTexture {name}_texture = {{SHIM_TEX_PAL4, {name}_pixels, {name}_palette, "
        f"{data_w}, {data_h}, {world_w}, {world_h}, true}};",
    ]
    print(f"  {path}: {world_w} x {world_h} -> {data_w} x {data_h}, {len(packed) // 1024} KB")
    return name + "_texture", "\n\n".join(code), len(packed)


# ---------------------------------------------------------------------------
# Sounds
# ---------------------------------------------------------------------------

def read_wav(path):
    """Mono samples from -1 to 1, and the sample rate."""
    with wave.open(os.path.join(GAME, path), "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)

    if width == 1:
        values = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        values = [v / 32768.0 for v in struct.unpack(f"<{len(raw) // 2}h", raw)]
    else:
        sys.exit(f"{path}: only 8 and 16 bit .wav files are supported")

    # Mix the channels down to one
    mono = [sum(values[i:i + channels]) / channels for i in range(0, len(values), channels)]
    return mono, rate


def trim_silence(samples, threshold=0.01):
    end = len(samples)
    while end > 0 and abs(samples[end - 1]) < threshold:
        end -= 1
    return samples[:end]


def convert_sound_pcm(path, rate):
    samples, source_rate = read_wav(path)
    samples = trim_silence(samples)

    # Resample (linear) to the board's rate
    count = max(1, int(len(samples) * rate / source_rate))
    out = bytearray(count)
    for i in range(count):
        position = i * source_rate / rate
        j = int(position)
        frac = position - j
        a = samples[min(j, len(samples) - 1)]
        b = samples[min(j + 1, len(samples) - 1)]
        value = a + (b - a) * frac
        out[i] = max(0, min(255, int(128 + value * 127)))

    name = c_name(path) + "_pcm"
    print(f"  {path}: {len(samples) / source_rate:.2f} s -> {count} samples at {rate} Hz, {count // 1024} KB")
    return name, f"static const uint8_t {name}[{count}] = {{\n{c_bytes(out)}\n}};", count


# ---------------------------------------------------------------------------
# Write <game>/shim_assets.c
# ---------------------------------------------------------------------------

def picture_scale(screen):
    """How much the shim will shrink the game's world on this screen (as InitWindow does)."""
    world = find_world_size()
    if world is None:
        print(f"  warning: couldn't find the game's size (#define SCREEN_WIDTH / SCREEN_HEIGHT, "
              f"or InitWindow(w, h, ...)): assuming {screen[0]} x {screen[1]}")
        world = screen
    return min(1.0, screen[0] / world[0], screen[1] / world[1])


def main():
    global GAME
    if len(sys.argv) != 2:
        sys.exit("Usage: python3 tools/convert_assets.py <game folder>   (e.g. game)")
    GAME = os.path.abspath(sys.argv[1])
    if not os.path.isdir(GAME):
        sys.exit(f"No folder {sys.argv[1]}")
    config = read_board_config()

    rate = config_int(config, "SHIM_PCM_RATE", 11025)
    screen = (config_int(config, "SHIM_FB_WIDTH", 320), config_int(config, "SHIM_FB_HEIGHT", 320))
    scale = picture_scale(screen)

    textures, sounds = find_game_assets()
    print(f"{sys.argv[1]}: {len(textures)} pictures (scale {scale:.3f}), {len(sounds)} sounds (at {rate} Hz)")

    parts, table, total = [], [], 0

    for path in textures:
        name, code, size = convert_texture(path, scale)
        parts.append(code)
        table.append(f'    {{"{path}", SHIM_ASSET_TEXTURE, &{name}}},')
        total += size

    for path in sounds:
        name, code, count = convert_sound_pcm(path, rate)
        parts.append(code)
        total += count
        sound = c_name(path) + "_sound"
        parts.append(f"static const ShimSound {sound} = {{{name}, {count}, {rate}, NULL, 0}};")
        table.append(f'    {{"{path}", SHIM_ASSET_SOUND, &{sound}}},')

    if not table:
        table.append("    {0}, // C needs at least one entry; the count below says there are none")
        count_line = "const int shimAssetCount = 0;\n"
    else:
        count_line = "const int shimAssetCount = sizeof(shimAssets) / sizeof(shimAssets[0]);\n"

    out_path = os.path.join(GAME, "shim_assets.c")
    with open(out_path, "w") as f:
        f.write("// Generated by tools/convert_assets.py from resources/: don't edit by hand,\n")
        f.write(f"// run it again instead:  python3 tools/convert_assets.py {os.path.relpath(GAME, PROJECT)}\n\n")
        f.write("#include <stddef.h>\n#include <stdint.h>\n\n#include \"shim_types.h\"\n\n")
        if parts:
            f.write("\n\n".join(parts) + "\n\n")
        f.write("const ShimAsset shimAssets[] = {\n" + "\n".join(table) + "\n};\n\n")
        f.write(count_line)

    print(f"Wrote {os.path.relpath(out_path, PROJECT)}: {total // 1024} KB of art and sound")
    if total > 1500 * 1024:
        print("  warning: that won't fit a Pico 1's 2 MB of flash with the program. Smaller pictures, or shorter sounds?")


if __name__ == "__main__":
    main()
