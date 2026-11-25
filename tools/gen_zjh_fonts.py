#!/usr/bin/env python3
"""Build the Chinese LVGL fonts used by the 扎金花 UI.

The firmware ships generated C fonts; this script regenerates them from the
fonts bundled under assets/fonts and the characters actually used under main/.
Regeneration requires Node.js for `npx lv_font_conv`.
The generated fonts are stored in main/generated.

  python3 tools/gen_zjh_fonts.py

The 16/20 px body fonts use Noto Sans SC; the 28 px display font uses the
playful ZCOOL KuaiLe. tools/check_glyphs.py still guards the result.
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "assets/fonts"
OUT_DIR = ROOT / "main/generated"
BODY_FONT = FONT_DIR / "NotoSansSC.ttf"
DISPLAY_FONT = FONT_DIR / "ZCOOLKuaiLe-Regular.ttf"
EXTRA = "0123456789+-—%·，。：、（）！？…/“”‘’《》"


def used_characters() -> str:
    chars: set[str] = set(EXTRA)
    for path in sorted((ROOT / "main").rglob("*")):
        if path.suffix not in {".c", ".h"}:
            continue
        if OUT_DIR in path.parents:
            continue
        for ch in path.read_text(encoding="utf-8", errors="ignore"):
            code = ord(ch)
            if 0x20 <= code < 0x7F or code >= 0x80:
                chars.add(ch)
    return "".join(sorted(chars))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lv-font-conv", default="npx -y lv_font_conv")
    args = parser.parse_args()

    symbols = used_characters()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    prefix = args.lv_font_conv.split()

    jobs = (
        (16, BODY_FONT),
        (20, BODY_FONT),
        (28, DISPLAY_FONT),
    )
    for size, font in jobs:
        out = OUT_DIR / f"zjh_font_{size}.c"
        cmd = prefix + [
            "--size", str(size),
            "--bpp", "4",
            "--format", "lvgl",
            "--no-compress",
            "--font", str(font.relative_to(ROOT)),
            "--range", "0x20-0x7E",
            "--symbols", symbols,
            "--lv-font-name", f"zjh_font_{size}",
            "--lv-include", "lvgl.h",
            "-o", str(out.relative_to(ROOT)),
        ]
        subprocess.run(cmd, check=True, cwd=ROOT)
        print(f"wrote {out.relative_to(ROOT)} ({len(symbols)} symbols)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
