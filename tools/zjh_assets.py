#!/usr/bin/env python3
"""Convert the 扎金花 source art and AI sounds into LVGL/PCM C arrays.

Requires ffmpeg on PATH (or --ffmpeg). Generated files are committed, so a
firmware build does not need ffmpeg; this script keeps the conversion
reproducible:

  python3 tools/zjh_assets.py

Sources (assets/images, generated with WeaveForge):
  table-bg.png / title-bg.png   1024x1536 backgrounds
  card-back.png                 1024x1536 card back
  avatar-*.png                  1024x1024 opponent portraits
  icon-sheet.png                296x440 sheet: 4 suits + chip + coin
  chip.png / coin.png           1024x1024 alpha cutouts
  logo.png                      424x152 alpha title lettering
Outputs:
  main/generated/zjh_images.c and zjh_images.h       RGB565(A8) image descriptors
  main/generated/zjh_pcm.c and zjh_audio_assets.h   16 kHz mono PCM cues
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGES_DIR = ROOT / "assets/images"
SOUNDS_DIR = ROOT / "assets/sounds"
OUT_DIR = ROOT / "main/generated"

SAMPLE_RATE = 16000
PEAK = 0.90

# 136 px icon cells inside the 296x440 sheet (columns x=8/152, rows y=8/152/296).
ICON_Y = (8, 152, 296)
ICON_X = (8, 152)

IMAGES = [
    # symbol, source, width, height, alpha, crop(x,y,w,h), key_white
    ("zjh_bg_img", "table-bg.png", 240, 320, False, None, False),
    ("zjh_title_bg_img", "title-bg.png", 240, 320, False, None, False),
    ("zjh_logo_img", "logo.png", 200, 72, True, None, False),
    ("zjh_card_back_img", "card-back.png", 56, 80, True, None, False),
    ("zjh_avatar_bao_img", "avatar-bao.png", 64, 64, True, None, False),
    ("zjh_avatar_chen_img", "avatar-chen.png", 64, 64, True, None, False),
    ("zjh_avatar_tao_img", "avatar-tao.png", 64, 64, True, None, False),
    ("zjh_suit_spade_img", "icon-sheet.png", 48, 48, True,
     (ICON_X[0], ICON_Y[0], 136, 136), False),
    ("zjh_suit_heart_img", "icon-sheet.png", 48, 48, True,
     (ICON_X[1], ICON_Y[0], 136, 136), False),
    ("zjh_suit_diamond_img", "icon-sheet.png", 48, 48, True,
     (ICON_X[0], ICON_Y[1], 136, 136), False),
    ("zjh_suit_club_img", "icon-sheet.png", 48, 48, True,
     (ICON_X[1], ICON_Y[1], 136, 136), False),
    ("zjh_chip_img", "chip.png", 40, 40, True, None, False),
    ("zjh_coin_img", "coin.png", 32, 32, True, None, False),
]

SOUNDS = [
    ("zjh_deal_pcm", "deal.mp3", 0.90),
    ("zjh_bet_pcm", "bet.mp3", 0.90),
    ("zjh_peek_pcm", "peek.mp3", 0.90),
    ("zjh_win_pcm", "win.mp3", 0.85),
    ("zjh_lose_pcm", "lose.mp3", 0.85),
    ("zjh_click_pcm", "click.mp3", 0.75),
]

FFMPEG = "ffmpeg"


def run(cmd: list[str]) -> bytes:
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE).stdout


def decode(filters: list[str], source: Path, pix_fmt: str) -> bytes:
    chain = ",".join(filters) if filters else "null"
    return run([FFMPEG, "-v", "error", "-i", str(source),
                "-vf", chain, "-f", "rawvideo", "-pix_fmt", pix_fmt, "-"])


def image_descriptor(name: str, source: Path, width: int, height: int,
                     alpha: bool, crop, key_white: bool) -> tuple[str, str]:
    filters: list[str] = []
    if crop:
        x, y, w, h = crop
        filters.append(f"crop={w}:{h}:{x}:{y}")
    if key_white:
        # Drop the sheet's flat white matte; opaque white highlights inside a
        # suit stay because the key only removes pixels near pure white that
        # are connected to the outside through the same tolerance band.
        filters.append("colorkey=0xFFFFFF:0.10:0.04")
    filters.append(f"scale={width}:{height}:flags=lanczos")

    if alpha:
        raw = decode(filters, source, "rgba")
        expected = width * height * 4
        if len(raw) != expected:
            raise SystemExit(f"{source}: expected {expected} bytes, got {len(raw)}")
        rgb = bytearray(width * height * 2)
        plane = bytearray(width * height)
        transparent = 0
        for i in range(width * height):
            r, g, b, a = raw[i * 4:i * 4 + 4]
            value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            rgb[i * 2] = value & 0xFF
            rgb[i * 2 + 1] = (value >> 8) & 0xFF
            plane[i] = a
            if a < 16:
                transparent += 1
        data = rgb + plane
        cf = "LV_COLOR_FORMAT_RGB565A8"
        stride = width * 2
        map_stride = width * 3
        extra = f"    // {100 * transparent // (width * height)}% transparent\n"
    else:
        raw = decode(filters, source, "rgb565le")
        expected = width * height * 2
        if len(raw) != expected:
            raise SystemExit(f"{source}: expected {expected} bytes, got {len(raw)}")
        data = bytearray(raw)
        cf = "LV_COLOR_FORMAT_RGB565"
        stride = width * 2
        map_stride = width * 2
        extra = ""

    lines = [
        f"static const uint8_t {name}_map[] = {{",
    ]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines += [
        "};",
        "",
        f"_Static_assert(sizeof({name}_map) == {width} * {height} * {map_stride // width},",
        '               "image map size mismatch");',
        "",
        f"const lv_image_dsc_t {name} = {{",
        "    .header = {",
        "        .magic = LV_IMAGE_HEADER_MAGIC,",
        f"        .cf = {cf},",
        "        .flags = 0,",
        f"        .w = {width},",
        f"        .h = {height},",
        f"        .stride = {stride},",
        "    },",
        f"    .data_size = sizeof({name}_map),",
        f"    .data = {name}_map,",
        "};",
        "",
    ]
    return "\n".join(lines), extra


def normalize(samples: list[int], peak: float) -> list[int]:
    top = max(abs(s) for s in samples) or 1
    scale = (32767.0 * peak) / top
    return [int(max(-32768, min(32767, s * scale))) for s in samples]


def load_pcm(source: Path) -> list[int]:
    raw = run([FFMPEG, "-v", "error", "-i", str(source), "-ac", "1",
               "-ar", str(SAMPLE_RATE), "-f", "s16le", "-acodec", "pcm_s16le", "-"])
    samples = [int.from_bytes(raw[i:i + 2], "little", signed=True)
               for i in range(0, len(raw) - 1, 2)]
    if not samples:
        raise SystemExit(f"{source}: decoded no audio")
    samples = normalize(samples, PEAK)
    fade_in = int(0.004 * SAMPLE_RATE)
    fade_out = int(0.045 * SAMPLE_RATE)
    for i in range(min(fade_in, len(samples))):
        samples[i] = int(samples[i] * i / fade_in)
    for i in range(min(fade_out, len(samples))):
        samples[-1 - i] = int(samples[-1 - i] * i / fade_out)
    return samples


def pcm_array(name: str, samples: list[int]) -> str:
    lines = [f"const int16_t {name}[] = {{"]
    for i in range(0, len(samples), 12):
        lines.append("    " + ", ".join(str(s) for s in samples[i:i + 12]) + ",")
    lines += ["};", f"const size_t {name}_samples = sizeof({name}) / sizeof({name}[0]);", ""]
    return "\n".join(lines)


def main() -> int:
    global FFMPEG
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--no-sounds", action="store_true")
    args = parser.parse_args()
    FFMPEG = args.ffmpeg

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    image_parts = [
        "// Generated by tools/zjh_assets.py from assets/images. Do not edit by hand.",
        '#include "lvgl.h"',
        '#include "zjh_images.h"',
        "",
    ]
    header = [
        "// Generated by tools/zjh_assets.py. Do not edit by hand.",
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
        "LV_IMAGE_DECLARE(zjh_bg_img);",
        "LV_IMAGE_DECLARE(zjh_title_bg_img);",
        "LV_IMAGE_DECLARE(zjh_logo_img);",
        "LV_IMAGE_DECLARE(zjh_card_back_img);",
        "LV_IMAGE_DECLARE(zjh_avatar_bao_img);",
        "LV_IMAGE_DECLARE(zjh_avatar_chen_img);",
        "LV_IMAGE_DECLARE(zjh_avatar_tao_img);",
        "LV_IMAGE_DECLARE(zjh_suit_spade_img);",
        "LV_IMAGE_DECLARE(zjh_suit_heart_img);",
        "LV_IMAGE_DECLARE(zjh_suit_diamond_img);",
        "LV_IMAGE_DECLARE(zjh_suit_club_img);",
        "LV_IMAGE_DECLARE(zjh_chip_img);",
        "LV_IMAGE_DECLARE(zjh_coin_img);",
        "",
        "// Suit order matches zjh_card_t.suit: spade, heart, diamond, club.",
        "extern const lv_image_dsc_t *const zjh_suit_images[4];",
        "",
    ]

    for symbol, filename, width, height, alpha, crop, key_white in IMAGES:
        code, note = image_descriptor(symbol, IMAGES_DIR / filename, width, height,
                                      alpha, crop, key_white)
        image_parts.append(note + code)
        print(f"{filename}: {width}x{height}{' A8' if alpha else ''}{note.strip()}")

    image_parts += [
        "const lv_image_dsc_t *const zjh_suit_images[4] = {",
        "    &zjh_suit_spade_img,",
        "    &zjh_suit_heart_img,",
        "    &zjh_suit_diamond_img,",
        "    &zjh_suit_club_img,",
        "};",
        "",
    ]
    (OUT_DIR / "zjh_images.c").write_text("\n".join(image_parts), encoding="utf-8")
    (OUT_DIR / "zjh_images.h").write_text("\n".join(header), encoding="utf-8")
    print(f"wrote {(OUT_DIR / 'zjh_images.c').relative_to(ROOT)}")

    if args.no_sounds:
        return 0

    pcm_parts = [
        "// Generated by tools/zjh_assets.py from assets/sounds. 16 kHz, 16-bit mono.",
        "// Do not edit by hand.",
        '#include "zjh_audio_assets.h"',
        "",
    ]
    pcm_header = [
        "// Generated by tools/zjh_assets.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
    ]
    for name, filename, _peak in SOUNDS:
        samples = load_pcm(SOUNDS_DIR / filename)
        pcm_parts.append(pcm_array(name, samples))
        pcm_header.append(f"extern const int16_t {name}[];")
        pcm_header.append(f"extern const size_t {name}_samples;")
        print(f"{filename}: {len(samples) / SAMPLE_RATE:.2f}s")

    (OUT_DIR / "zjh_pcm.c").write_text("\n".join(pcm_parts), encoding="utf-8")
    (OUT_DIR / "zjh_audio_assets.h").write_text("\n".join(pcm_header) + "\n",
                                                encoding="utf-8")
    print(f"wrote {(OUT_DIR / 'zjh_pcm.c').relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
