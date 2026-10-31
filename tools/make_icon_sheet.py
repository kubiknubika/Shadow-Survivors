#!/usr/bin/env python3
"""Лист всех иконок одного стиля: 41 предмет в сетке с номерами.

Лист собирается не из PNG, а из того же запечённого атласа, что идёт в exe
(src/skill_icon_color_atlas.h): значит на листе видно игру, а не отдельный
эскиз. Раньше лист на 40 предметов собирался вручную, и с 41-м предметом его
пришлось бы собирать заново руками — теперь это одна команда.

    python3 tools/make_icon_sheet.py                # design/icon_sheet_41.png
    python3 tools/make_icon_sheet.py --out x.png    # другой файл
"""
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATLAS = os.path.join(ROOT, "src", "skill_icon_color_atlas.h")
OUT = os.path.join(ROOT, "design", "icon_sheet_41.png")

CELL = 160          # сторона иконки в листе
LABEL = 20          # полоса под номером
GAP = 10
MARGIN = 24
PER_ROW = 8
BG = (11, 8, 18)
PANEL = (24, 18, 40)
INK = (216, 208, 236)
DIM = (150, 140, 172)
TITLE = (255, 210, 120)


def parse_atlas():
    text = open(ATLAS, encoding="utf-8").read()
    icons = {}
    for match in re.finditer(r"static const uint32_t SKILL_ICON_COLOR_(\d+)_PALETTE\[128\] = \{(.*?)\};", text, re.S):
        index = int(match.group(1))
        colors = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{8})u", match.group(2))]
        icons.setdefault(index, {})["palette"] = colors
    for match in re.finditer(r"static const uint8_t SKILL_ICON_COLOR_(\d+)_PIXELS\[4096\] = \{(.*?)\};", text, re.S):
        index = int(match.group(1))
        pixels = [int(v) for v in re.findall(r"\d+", match.group(2))]
        icons.setdefault(index, {})["pixels"] = pixels
    return [icons[i] for i in sorted(icons)]


def icon_image(entry):
    palette = entry["palette"]
    pixels = entry["pixels"]
    image = Image.new("RGB", (64, 64))
    out = []
    for index in pixels:
        color = palette[index] if index < len(palette) else 0
        if color >> 24 == 0:          # индекс 0 — прозрачный
            out.append(PANEL)
        else:
            out.append(((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF))
    image.putdata(out)
    return image


def main(argv):
    out = OUT
    if "--out" in argv:
        out = os.path.join(ROOT, argv[argv.index("--out") + 1])
    icons = parse_atlas()
    rows = (len(icons) + PER_ROW - 1) // PER_ROW
    step = CELL + LABEL + GAP
    width = MARGIN * 2 + PER_ROW * step - GAP
    height = MARGIN * 2 + 92 + rows * step - GAP
    sheet = Image.new("RGB", (width, height), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((MARGIN + 2, MARGIN), f"SHADOW SURVIVORS  -  {len(icons)} ITEM ICONS  -  STYLE \"C\" (engraved)",
              fill=TITLE)
    draw.text((MARGIN + 2, MARGIN + 26), "design/ai_icons  -  baked into src/skill_icon_color_atlas.h",
              fill=DIM)
    top = MARGIN + 92
    for i, entry in enumerate(icons):
        x = MARGIN + (i % PER_ROW) * step
        y = top + (i // PER_ROW) * step
        sheet.paste(icon_image(entry).resize((CELL, CELL), Image.LANCZOS), (x, y))
        draw.rectangle([x, y, x + CELL - 1, y + CELL - 1], outline=(70, 56, 104))
        draw.text((x + 3, y + CELL + 4), f"{i:02d}", fill=INK)
    sheet.save(out)
    print(f"{out}  {width}x{height}  предметов {len(icons)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
