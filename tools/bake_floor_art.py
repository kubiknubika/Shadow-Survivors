#!/usr/bin/env python3
"""Запекает четыре напольных предмета в src/floor_art.h.

Зачем свой инструмент. Картинки для пола рисует генератор (design/floor/*_src.png),
но в игру они идут не как есть: источник — 1024 px с чёрным фоном и мягкими
тенями, а спрайт в кадре — 64 px с прозрачным фоном и ограниченной палитрой.
Пайплайн здесь важен целиком, поэтому он записан кодом, а не руками:

    1) фон вырезается не по «цвету пикселя», а заливкой от края кадра: у
       каменных предметов есть тёмные части, которые по яркости не отличить от
       фона, зато они не связаны с краем;
    2) обрезаем по содержимому и дополняем до квадрата — иначе предмет уезжает
       из центра и кольца/подсветка в игре перестают с ним совпадать;
    3) уменьшаем в 64×64 фильтром «площадь» (BOX), а не бикубикой: у пиксель-арта
       не должно быть мыла на краях;
    4) палитра квантуется до 48 цветов, прозрачность — по маске фона.

    python3 tools/bake_floor_art.py            # собрать src/floor_art.h
    python3 tools/bake_floor_art.py --check    # только проверить, что не устарел

Инструменту нужен Pillow (как и остальным tools/*.py, которые трогают картинки).
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "floor_art.h")
PREVIEW = os.path.join(ROOT, "design", "floor_preview.png")
SIZE = 96
COLORS = 48
# Допуск заливки фона: у генератора фон почти чёрный, но с лёгким градиентом.
BG_TOLERANCE = 26

# Имя в игре → файл источника. Порядок фиксирован: он же порядок в массиве.
SPRITES = [
    ("FLOOR_PLATE", "plate_src.png"),
    ("FLOOR_CRACKS", "telegraph_src.png"),
    ("FLOOR_SPIKES", "spikes_src.png"),
    ("FLOOR_SIGIL", "font_src.png"),
]


def background_mask(im):
    """1 = предмет, 0 = фон. Заливка от края по близости к цвету соседа."""
    w, h = im.size
    px = im.load()
    bg = bytearray(w * h)
    stack = []
    for x in range(w):
        stack.append((x, 0)); stack.append((x, h - 1))
    for y in range(h):
        stack.append((0, y)); stack.append((w - 1, y))
    while stack:
        x, y = stack.pop()
        i = y * w + x
        if bg[i]:
            continue
        r, g, b = px[x, y]
        # Фон — тёмный и почти нейтральный. Проверка двойная: и «темно», и
        # «не цветное»; иначе вместе с фоном съедало бы тёмно-синие шипы.
        if (r + g + b) // 3 > BG_TOLERANCE or max(r, g, b) - min(r, g, b) > 18:
            continue
        bg[i] = 1
        if x > 0: stack.append((x - 1, y))
        if x + 1 < w: stack.append((x + 1, y))
        if y > 0: stack.append((x, y - 1))
        if y + 1 < h: stack.append((x, y + 1))
    # Всё, куда заливка не дошла, — предмет.
    mask = bytearray(0 if v else 1 for v in bg)
    return mask, w, h


def process(path):
    im = Image.open(path).convert("RGB")
    im.thumbnail((320, 320), Image.LANCZOS)
    mask, w, h = background_mask(im)

    # Границы предмета.
    xs = [i % w for i, v in enumerate(mask) if v]
    ys = [i // w for i, v in enumerate(mask) if v]
    if not xs:
        raise SystemExit(f"{path}: предмет не найден — фон залил всё")
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    side = max(x1 - x0, y1 - y0)
    pad = max(2, side // 24)
    side += pad * 2
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2

    rgba = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    src = im.convert("RGBA")
    src.putalpha(Image.frombytes("L", (w, h), bytes(255 if v else 0 for v in mask)))
    rgba.paste(src.crop((cx - side // 2, cy - side // 2, cx + side // 2, cy + side // 2)),
               (0, 0))

    small = rgba.resize((SIZE, SIZE), Image.BOX)
    # Палитра считается по непрозрачным пикселям: фон в неё попадать не должен.
    sp = small.load()
    opaque = [sp[x, y][:3] for y in range(SIZE) for x in range(SIZE) if sp[x, y][3] > 40]
    if not opaque:
        raise SystemExit(f"{path}: после уменьшения не осталось непрозрачных пикселей")
    pal_src = Image.new("RGB", (len(opaque), 1))
    pal_src.putdata(opaque)
    pal = pal_src.quantize(colors=COLORS, method=Image.MEDIANCUT)
    paletted = small.convert("RGB").quantize(palette=pal, dither=Image.NONE).convert("RGB")
    paletted.putalpha(small.getchannel("A"))
    # Тёмная обводка по краю: без неё предмет сливается с полом тёмного места —
    # на «Осыпи» и в «Крипте» плита просто пропадала. Обводка — это те же
    # пиксели, но на 30 % темнее, поэтому палитра не растёт.
    op = paletted.load()
    edge = []
    for y in range(SIZE):
        for x in range(SIZE):
            if op[x, y][3] < 200:
                continue
            border = False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or ny < 0 or nx >= SIZE or ny >= SIZE or op[nx, ny][3] < 60:
                    border = True
                    break
            if border:
                edge.append((x, y))
    for x, y in edge:
        r, g, b, a = op[x, y]
        op[x, y] = (int(r * 0.62), int(g * 0.62), int(b * 0.62), a)
    return paletted


def to_cpp(name, spr):
    w, h = spr.size
    ip = spr.load()
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = ip[x, y]
            if a < 24:
                row.append("0x00000000u")
            else:
                row.append(f"0x{a:02X}{r:02X}{g:02X}{b:02X}u")
        rows.append(",".join(row))
    body = ",\n    ".join(rows)
    return (f"static const uint32_t {name}_PIXELS[{w * h}] = {{\n    {body},\n}};\n"
            f"BakedSprite {name} = {{{w}, {h}, {name}_PIXELS}};\n")


def main():
    check = "--check" in sys.argv
    parts = [
        "#pragma once\n"
        "// ── Напольные предметы: запечённые спрайты ────────────────────────────────\n"
        "//\n"
        "// Сгенерировано tools/bake_floor_art.py из design/floor/*_src.png.\n"
        "// Руками не править: правка потеряется при следующей сборке атласа.\n"
        "//\n"
        "// 96×96, палитра 48 цветов, фон вырезан заливкой от края кадра. Порядок\n"
        "// массивов совпадает с перечислением FloorDeviceKind, поэтому в игре\n"
        "// спрайт берётся по индексу, а не по имени.\n"
        "#include <stdint.h>\n"
        '#include "sprite_atlas.h"\n\n'
    ]
    previews = []
    for name, file in SPRITES:
        spr = process(os.path.join(ROOT, "design", "floor", file))
        previews.append(spr)
        parts.append(to_cpp(name, spr))
    text = "".join(parts)

    if check:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        print("floor_art.h в порядке" if old == text else "floor_art.h устарел: пересоберите")
        return 0 if old == text else 1

    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)

    sheet = Image.new("RGBA", (SIZE * 4 + 30, SIZE + 20), (13, 10, 22, 255))
    for i, spr in enumerate(previews):
        sheet.alpha_composite(spr, (6 + i * (SIZE + 6), 10))
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(PREVIEW)
    print(f"floor_art.h: {len(text)} байт, 4 спрайта {SIZE}×{SIZE}; лист {PREVIEW}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
