#!/usr/bin/env python3
"""Вшивает ОДНУ новую иконку в атлас стиля «ИИ-АРТ» (src/skill_icon_atlas.h).

Зачем отдельный инструмент. src/generate_skill_icon_atlas.py собирает весь
атлас заново из design/*_idNN_ai_concept.png, но этих исходников в репозитории
нет: он хранит только готовый 64×64 атлас и рабочие картинки
design/ai_icons/icon_NN_*.png. Пересобрать весь атлас можно только из
сохранённых concept-файлов, а значит любая попытка «просто запустить генератор»
либо падает, либо — что хуже — перерисовывает тридцать девять уже выпущенных
иконок. Инструмент добавляет ровно один предмет и не трогает соседние байты.

    python3 tools/bake_ai_icon.py 40 design/ai_icons/icon_40_ember_trail.png
    python3 tools/bake_ai_icon.py 40 design/ai_icons/icon_40_ember_trail.png --force

Пайплайн тот же, что в генераторе: центр кадра обрезается в квадрат, RGBA
накладывается на цвет подложки кнопки, картинка уменьшается до 64×64 и
квантуется в 128 цветов. Размерность массива SKILL_ICONS_AI растёт на единицу.
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "skill_icon_atlas.h")
SIZE = 64
COLORS = 128
# Цвет подложки кнопки: прозрачные пиксели исходника должны сливаться с ней,
# иначе вокруг иконки появится светлая рамка.
FRAME_BG = (0x18, 0x12, 0x28)


def fmt_array(values, per_line, formatter):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(formatter(v) for v in values[i:i + per_line]) + ",")
    return "\n".join(lines)


def trim_to_subject(image):
    """Обрезает пустой фон по границам предмета и добивает до квадрата.

    Картинка, нарисованная «в общий квадрат», часто занимает треть кадра: на
    64x64 от неё остаётся тонкая полоска. Обрезка по фону возвращает предмету
    место в слоте, не перерисовывая сам рисунок.
    """
    import statistics
    rgb = image.convert("RGB")
    w, h = rgb.size
    border = []
    for x in range(0, w, max(1, w // 64)):
        border.append(rgb.getpixel((x, 0)))
        border.append(rgb.getpixel((x, h - 1)))
    for y in range(0, h, max(1, h // 64)):
        border.append(rgb.getpixel((0, y)))
        border.append(rgb.getpixel((w - 1, y)))
    bg = tuple(int(statistics.median(c[i] for c in border)) for i in range(3))
    px = rgb.load()
    minX, minY, maxX, maxY = w, h, -1, -1
    for y in range(0, h, max(1, h // 256)):
        for x in range(0, w, max(1, w // 256)):
            r, g, b = px[x, y]
            if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) > 40:
                if x < minX: minX = x
                if x > maxX: maxX = x
                if y < minY: minY = y
                if y > maxY: maxY = y
    if maxX <= minX or maxY <= minY:
        return image
    pad = int(max(maxX - minX, maxY - minY) * 0.08) + 2
    cx, cy = (minX + maxX) // 2, (minY + maxY) // 2
    half = max(maxX - minX, maxY - minY) // 2 + pad
    box = (max(0, cx - half), max(0, cy - half), min(w, cx + half), min(h, cy + half))
    cropped = image.crop(box)
    side = max(cropped.size)
    square = Image.new("RGBA", (side, side), bg + (255,))
    square.paste(cropped, ((side - cropped.width) // 2, (side - cropped.height) // 2))
    return square


def bake(path, fit=False):
    image = Image.open(path).convert("RGBA")
    if fit:
        image = trim_to_subject(image)
    side = min(image.size)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    image = image.crop((left, top, left + side, top + side)).resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    flat = []
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = image.getpixel((x, y))
            t = a / 255.0
            flat.append((int(round(r * t + FRAME_BG[0] * (1 - t))),
                         int(round(g * t + FRAME_BG[1] * (1 - t))),
                         int(round(b * t + FRAME_BG[2] * (1 - t)))))
    rgb = Image.new("RGB", (SIZE, SIZE))
    rgb.putdata(flat)
    quantized = rgb.quantize(colors=COLORS, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.FLOYDSTEINBERG)
    indices = list(quantized.getdata())
    raw = quantized.getpalette()[:COLORS * 3]
    palette = []
    for i in range(COLORS):
        r, g, b = raw[i * 3:i * 3 + 3]
        palette.append(0xFF000000 | (r << 16) | (g << 8) | b)
    return palette, indices


def main(argv):
    # --force перезаписывает уже вшитую иконку. Без него инструмент только
    # добавляет новую: выпущенные иконки нельзя затирать случайным запуском.
    force = "--force" in argv
    # --fit обрезает пустой фон: рисунок, снятый «в общий квадрат», иначе
    # остаётся в слоте тонкой полоской.
    fit = "--fit" in argv
    args = [a for a in argv if a not in ("--force", "--fit")]
    if len(args) != 3:
        print(__doc__)
        return 2
    item, png = int(args[1]), args[2]
    if not os.path.exists(png):
        print(f"нет файла: {png}")
        return 2
    text = open(OUT, encoding="utf-8").read()
    name = f"SKILL_ICON_{item:02d}_AI"
    already = f"static const uint32_t {name}_PALETTE" in text
    if already and not force:
        print(f"{name} уже вшита — инструмент не перезаписывает выпущенные иконки (--force, если так и надо)")
        return 1
    if already:
        palette, indices = bake(png, fit)
        block = [
            f"static const uint32_t {name}_PALETTE[{COLORS}] = {{",
            fmt_array(palette, 8, lambda v: f"0x{v:08X}u"),
            "};",
            f"static const uint8_t {name}_PIXELS[{SIZE * SIZE}] = {{",
            fmt_array(indices, 24, lambda v: str(int(v))),
            "};",
        ]
        start = text.index(f"static const uint32_t {name}_PALETTE")
        end = text.index("};", text.index(f"static const uint8_t {name}_PIXELS")) + 2
        out = text[:start] + "\n".join(block) + text[end:]
        open(OUT, "w", encoding="utf-8").write(out)
        print(f"перезаписана {name} из {os.path.relpath(png, ROOT)}; соседние иконки не тронуты")
        return 0

    marker = "static const BakedIconImage SKILL_ICONS_AI["
    start = text.index(marker)
    count = int(text[start + len(marker):text.index("]", start)])
    body_start = text.index("= {", start) + len("= {")
    body_end = text.index("};", body_start)
    palette, indices = bake(png, fit)
    block = [
        f"static const uint32_t {name}_PALETTE[{COLORS}] = {{",
        fmt_array(palette, 8, lambda v: f"0x{v:08X}u"),
        "};",
        f"static const uint8_t {name}_PIXELS[{SIZE * SIZE}] = {{",
        fmt_array(indices, 24, lambda v: str(int(v))),
        "};",
    ]
    entry = f"    {{{SIZE},{SIZE},{name}_PALETTE,{name}_PIXELS}},"
    body = text[body_start:body_end].rstrip()
    new_body = body + "\n" + entry + "\n"
    head = text[:start].rstrip("\n") + "\n"
    aggregate_head = f"static const BakedIconImage SKILL_ICONS_AI[{count + 1}] = {{"
    tail = text[body_end:]
    out = head + "\n" + "\n".join(block) + "\n" + aggregate_head + new_body + tail
    open(OUT, "w", encoding="utf-8").write(out)
    print(f"вшита {name} из {os.path.relpath(png, ROOT)}; предметов в атласе: {count + 1}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
