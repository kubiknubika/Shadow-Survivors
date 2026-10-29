#!/usr/bin/env python3
"""Третий стиль иконок предметов: «ЦВЕТ».

Первые десять предметов (семь активных оружий и три артефакта) получают по одному
ярко выраженному цвету и по одному силуэту, который ни с чем не спутать даже в
слоте 22×22. Остальные тридцать — рунные печати: та же цветовая логика, но узор
строится по номеру предмета.

    python3 tools/generate_icon_color_atlas.py      # пишет src/skill_icon_color_atlas.h

Атлас встраивается в exe: индексная палитра 64×64, без PNG во время работы.
"""
import colorsys
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "skill_icon_color_atlas.h")
SIZE = 64

# Индекс 0 — прозрачный, дальше — палитра одного предмета.
#                       0        1 (фон тёмн.) 2 (фон)   3 (главный) 4 (свет)  5 (контур) 6 (акцент) 7 (белый)
# Предмет -> (семейство, вариант). Семейство задаёт смысл, вариант — вес предмета.
# Те же сорок цветов стоят в ITEM_TONE в src/renderer.h, поэтому рамка слота и
# иконка никогда не расходятся: один предмет — один цвет, без повторов по кругу.
ASSIGN = [
    ("blue", "bright"), ("orange", "pure"), ("violet", "bright"), ("lime", "pure"), ("red", "pure"),
    ("orange", "deep"), ("cyan", "pure"), ("amber", "dark"), ("green", "pure"), ("magenta", "pure"),
    ("red", "pure"),        # 10 сердце титана — багровое
    ("orange", "bright"), ("amber", "bright"), ("lime", "deep"), ("amber", "deep"),
    ("violet", "dark"), ("cyan", "dark"), ("violet", "deep"), ("cyan", "bright"), ("red", "bright"),
    ("green", "dark"), ("teal", "pure"), ("teal", "deep"), ("magenta", "bright"), ("blue", "pure"),
    ("green", "bright"), ("blue", "dark"), ("lime", "dark"), ("cyan", "deep"), ("lime", "bright"),
    # 30–39: нарисованы ИИ, цвет выбран вручную и разнесён по кругу,
    # чтобы соседние предметы не читались одним оттенком.
    ("green", "deep"),     # 30 чаша переполнения — нефрит
    ("blue", "pure"),      # 31 осколок последнего удара — сталь
    ("violet", "deep"),    # 32 ключ пепельного ритма
    ("lime", "pure"),      # 33 призма перекрёстного огня — янтарь
    ("red", "bright"),     # 34 угольный фитиль — тлеющий багровый уголь
    ("cyan", "bright"),     # 35 осколок оттепели — бледная бирюза
    ("magenta", "deep"),   # 36 печать обратного тока
    ("red", "bright"),     # 37 знак палача — багровый
    ("amber", "pure"),     # 38 печать возмездия — золото
    ("blue", "deep"),      # 39 руна свидетеля — индиго
]
FAMILY_HUE = {"red": 352, "orange": 20, "amber": 42, "lime": 72, "green": 132,
              "teal": 168, "cyan": 190, "blue": 222, "violet": 268, "magenta": 312}
VARIANT = {"bright": (0.95, 0.74), "pure": (0.92, 0.56), "deep": (0.85, 0.42), "dark": (0.70, 0.32)}

# Активные оружия: 0–6 и 24–27. У них квадратная стальная пластина,
# у артефактов — круглая каменная шайба. Форма читается раньше цвета.
WEAPON_ITEMS = set(range(0, 7)) | set(range(24, 28))


def _hsl(hue, sat, light):
    r, g, b = colorsys.hls_to_rgb((hue % 360) / 360.0, light, sat)
    return (int(round(r * 255)) << 16) | (int(round(g * 255)) << 8) | int(round(b * 255))


def _palette(item, hue=None, sat=None, light=None):
    """Палитра одного предмета: подложка, фон, главный, свет, контур, акцент, белый.

    Для нарисованных иконок hue/sat/light берутся из фактических цветов картинки:
    иначе серая иконка на цветной подложке читается как чужой предмет, а блёклая
    теряется. Раньше подложка всегда была из ASSIGN и врала про цвет.
    """
    family, variant = ASSIGN[item]
    if hue is None:
        hue = FAMILY_HUE[family]
    if sat is None or light is None:
        s0, l0 = VARIANT[variant]
        sat = s0 if sat is None else sat
        light = l0 if light is None else light
    return (
        0xFF000000 | _hsl(hue, min(0.60, sat * 0.65), 0.10),
        0xFF000000 | _hsl(hue, min(0.60, sat * 0.72), 0.19),
        0xFF000000 | _hsl(hue, sat, light),
        0xFF000000 | _hsl(hue, min(0.95, sat * 0.85), min(0.88, light + 0.20)),
        0xFF000000 | _hsl(hue, 0.75, 0.05),
        0xFF000000 | _hsl(hue, min(0.90, sat * 0.60), min(0.94, light + 0.30)),
        0xFFFFFFFF,
    )


PALETTES = [_palette(item) for item in range(40)]


# индексы палитры
BG, BG2, MAIN, LIGHT, OUTLINE, ACCENT, WHITE = 1, 2, 3, 4, 5, 6, 7


class Canvas:
    def __init__(self):
        self.px = [[0] * SIZE for _ in range(SIZE)]

    def put(self, x, y, c):
        if 0 <= x < SIZE and 0 <= y < SIZE:
            self.px[y][x] = c

    def disc(self, cx, cy, r, c):
        r2 = r * r
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                dx, dy = x - cx, y - cy
                if dx * dx + dy * dy <= r2:
                    self.put(x, y, c)

    def ring(self, cx, cy, r, width, c):
        ro, ri = r + width / 2.0, max(0.0, r - width / 2.0)
        for y in range(int(cy - ro) - 1, int(cy + ro) + 2):
            for x in range(int(cx - ro) - 1, int(cx + ro) + 2):
                d = math.hypot(x - cx, y - cy)
                if ri <= d <= ro:
                    self.put(x, y, c)

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                self.put(x, y, c)

    def rrect(self, x0, y0, x1, y1, radius, c):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                dx = min(x - x0, x1 - x)
                dy = min(y - y0, y1 - y)
                if dx < radius and dy < radius:
                    if (radius - dx) ** 2 + (radius - dy) ** 2 > radius * radius:
                        continue
                self.put(x, y, c)

    def line(self, x0, y0, x1, y1, width, c):
        steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(steps + 1):
            t = i / steps
            self.disc(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, width / 2.0, c)

    def poly(self, points, c):
        ys = [p[1] for p in points]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            xs = []
            n = len(points)
            for i in range(n):
                x0, y0 = points[i]
                x1, y1 = points[(i + 1) % n]
                if y0 == y1:
                    continue
                if min(y0, y1) <= y <= max(y0, y1):
                    xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                for x in range(int(math.ceil(xs[i])), int(math.floor(xs[i + 1])) + 1):
                    self.put(x, y, c)

    def outlined_poly(self, points, fill, outline, width=2.0):
        # контур: та же фигура, но чуть больше и тёмная — читается на любом фоне
        cx = sum(p[0] for p in points) / len(points)
        cy = sum(p[1] for p in points) / len(points)
        grown = []
        for x, y in points:
            dx, dy = x - cx, y - cy
            d = max(0.001, math.hypot(dx, dy))
            grown.append((x + dx / d * width, y + dy / d * width))
        self.poly(grown, outline)
        self.poly(points, fill)


def mask_fill(c, mask, fill, outline=2, light=None):
    """Заливка булевой маски с тёмным контуром: фигура читается на любом фоне."""
    h = len(mask); w = len(mask[0])
    grown = [[False] * w for _ in range(h)]
    rad = int(outline)
    for y in range(h):
        for x in range(w):
            if not mask[y][x]:
                continue
            for dy in range(-rad, rad + 1):
                for dx in range(-rad, rad + 1):
                    if dx * dx + dy * dy <= rad * rad:
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < w and 0 <= ny < h:
                            grown[ny][nx] = True
    for y in range(h):
        for x in range(w):
            if grown[y][x] and not mask[y][x]:
                c.put(x, y, OUTLINE)
    for y in range(h):
        for x in range(w):
            if mask[y][x]:
                c.put(x, y, fill)
    if light:
        for y in range(h):
            for x in range(w):
                if mask[y][x] and (x - 32) + (y - 32) < -9:
                    c.put(x, y, light)


def new_mask():
    return [[False] * SIZE for _ in range(SIZE)]


def base(canvas, key):
    """Фон: тёмная плитка в оттенке предмета плюс внутренняя рамка того же цвета."""
    canvas.rrect(2, 2, 61, 61, 12, BG)
    canvas.rrect(6, 6, 57, 57, 9, BG2)
    canvas.rrect(6, 6, 57, 57, 9, 0)
    canvas.rrect(7, 7, 56, 56, 8, BG2)
    # мягкий блик сверху
    for y in range(7, 24):
        for x in range(7, 56):
            if canvas.px[y][x] == BG2 and (y - 7) < (24 - y) and (x + y) % 3 == 0:
                canvas.px[y][x] = BG
    canvas.rrect(2, 2, 61, 61, 12, 0)
    canvas.ring(32, 32, 27, 2.0, MAIN) if False else None


def frame(canvas, passive):
    """Подложка предмета.

    Оружие лежит на угловатой стальной пластине с бликом сверху, артефакт — на
    круглой каменной шайбе. Раньше подложка была одна на всех: в слоте 22×22
    артефакт не отличался от оружия до того, как игрок успевал разглядеть цвет.
    """
    if passive:
        canvas.disc(32, 32, 30, OUTLINE)
        canvas.disc(32, 32, 28, BG)
        canvas.disc(32, 32, 23, BG2)
        # мягкая тень внизу справа: шайба читается как объёмная
        for y in range(20, 56):
            for x in range(12, 56):
                if canvas.px[y][x] == BG2 and (x - 32) + (y - 32) > 16:
                    canvas.px[y][x] = BG
        # блик сверху слева
        for y in range(10, 30):
            for x in range(10, 40):
                if canvas.px[y][x] == BG2 and (x - 32) + (y - 32) < -14 and (x + y) % 2 == 0:
                    canvas.px[y][x] = BG
        canvas.ring(32, 32, 25.0, 1.6, MAIN)
        canvas.ring(32, 32, 29.0, 1.2, OUTLINE)
        return
    canvas.rrect(2, 2, 61, 61, 12, 0)
    canvas.rrect(3, 3, 60, 60, 11, BG)
    canvas.rrect(6, 6, 57, 57, 9, BG2)
    # верхний блик и нижняя тень — плитка читается как объёмная, а не плоская заливка
    for x in range(8, 56):
        canvas.put(x, 7, BG)
        canvas.put(x, 8, BG)
        canvas.put(x, 56, OUTLINE)
    for y in range(9, 55):
        canvas.put(7, y, BG)
        canvas.put(56, y, OUTLINE)
    canvas.ring(32, 32, 25.5, 1.6, MAIN)


# ── глифы первых десяти предметов ─────────────────────────────────────────────

def glyph_blades(c):
    for sign in (-1, 1):
        hx, hy = 32 + sign * 7, 52                      # рукоять
        tx, ty = 32 + sign * 19, 9                      # остриё
        dx, dy = tx - hx, ty - hy
        length = math.hypot(dx, dy)
        ux, uy = dx / length, dy / length
        px, py = -uy, ux
        # клинок
        poly = [(hx + ux * 9 + px * 3.4, hy + uy * 9 + py * 3.4),
                (tx + px * 1.2, ty + py * 1.2),
                (tx - px * 1.2, ty - py * 1.2),
                (hx + ux * 9 - px * 3.4, hy + uy * 9 - py * 3.4)]
        c.outlined_poly(poly, MAIN, OUTLINE, 1.6)
        c.line(hx + ux * 12, hy + uy * 12, tx - ux * 4, ty - uy * 4, 1.6, LIGHT)
        # гарда
        gx, gy = hx + ux * 8, hy + uy * 8
        c.line(gx + px * 6, gy + py * 6, gx - px * 6, gy - py * 6, 4.6, OUTLINE)
        c.line(gx + px * 5, gy + py * 5, gx - px * 5, gy - py * 5, 2.4, ACCENT)
        # рукоять и навершие
        c.line(gx - ux * 2, gy - uy * 2, hx - ux * 3, hy - uy * 3, 4.2, OUTLINE)
        c.line(gx - ux * 2, gy - uy * 2, hx - ux * 3, hy - uy * 3, 2.2, ACCENT)
        c.disc(hx - ux * 4, hy - uy * 4, 3.0, OUTLINE)
        c.disc(hx - ux * 4, hy - uy * 4, 1.8, LIGHT)

def glyph_fire(c):
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            t = (y - 10) / 44.0
            if t < 0 or t > 1:
                continue
            half = 4 + 16 * math.sin(min(1.0, t * 1.15) * math.pi * 0.86) ** 0.9
            # живой, «качающийся» край пламени
            half += 2.2 * math.sin(t * 7.0)
            if abs(x - 32) <= half:
                mask[y][x] = True
    # язык пламени сверху
    for y in range(6, 16):
        half = max(0, 4 - abs(y - 11) * 0.7)
        for x in range(int(32 - half), int(32 + half) + 1):
            mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    # ядро
    core = new_mask()
    for y in range(24, 50):
        for x in range(SIZE):
            t = (y - 24) / 26.0
            half = 9 * math.sin(min(1.0, t) * math.pi)
            if abs(x - 32) <= half:
                core[y][x] = True
    mask_fill(c, core, ACCENT, 0)
    hot = new_mask()
    for y in range(32, 46):
        for x in range(SIZE):
            t = (y - 32) / 14.0
            if abs(x - 32) <= 4 * math.sin(min(1.0, t) * math.pi):
                hot[y][x] = True
    mask_fill(c, hot, WHITE, 0)

def glyph_vortex(c):
    for k in range(3):
        base_a = k * 2.0944
        for step in range(26):
            t = step / 25.0
            ang = base_a + t * 2.7
            r = 5 + t * 22
            x, y = 32 + math.cos(ang) * r, 32 + math.sin(ang) * r
            w = 4.2 - t * 2.6
            c.disc(x, y, w / 2 + 0.8, OUTLINE)
        for step in range(26):
            t = step / 25.0
            ang = base_a + t * 2.7
            r = 5 + t * 22
            x, y = 32 + math.cos(ang) * r, 32 + math.sin(ang) * r
            w = 4.2 - t * 2.6
            c.disc(x, y, max(0.6, w / 2), MAIN if step < 18 else LIGHT)
    c.disc(32, 32, 3.2, LIGHT)


def glyph_bolt(c):
    c.outlined_poly([(38, 6), (20, 34), (30, 34), (26, 58), (45, 28), (34, 28)], MAIN, OUTLINE, 2.2)
    c.outlined_poly([(35, 14), (26, 30), (31, 30), (28, 45)], WHITE, MAIN, 0.8)


def glyph_blood(c):
    c.ring(32, 32, 22, 4.0, OUTLINE)
    c.ring(32, 32, 22, 2.2, MAIN)
    c.ring(32, 32, 13, 2.4, MAIN)
    c.disc(32, 32, 6.5, OUTLINE)
    c.disc(32, 32, 4.8, LIGHT)
    for ang in (0.6, 2.2, 3.9, 5.4):
        x, y = 32 + math.cos(ang) * 27, 32 + math.sin(ang) * 27
        c.disc(x, y, 3.4, OUTLINE)
        c.disc(x, y, 2.4, MAIN)


def glyph_axe(c):
    # топорище
    c.line(24, 56, 34, 14, 6.0, OUTLINE)
    c.line(24, 56, 34, 14, 3.0, ACCENT)
    # лезвие
    blade = [(33, 8), (52, 15), (54, 27), (46, 36), (34, 31)]
    c.outlined_poly(blade, MAIN, OUTLINE, 1.8)
    c.outlined_poly([(37, 12), (48, 18), (49, 26), (43, 31), (37, 28)], LIGHT, MAIN, 0.8)
    # обух-шип слева
    spike = [(33, 9), (23, 15), (33, 22)]
    c.outlined_poly(spike, ACCENT, OUTLINE, 1.4)

def glyph_frost(c):
    c.ring(32, 32, 24, 3.0, OUTLINE)
    c.ring(32, 32, 24, 1.6, MAIN)
    for k in range(6):
        ang = k * math.pi / 3
        x1, y1 = 32 + math.cos(ang) * 8, 32 + math.sin(ang) * 8
        x2, y2 = 32 + math.cos(ang) * 20, 32 + math.sin(ang) * 20
        c.line(x1, y1, x2, y2, 3.4, OUTLINE)
        c.line(x1, y1, x2, y2, 2.0, LIGHT)
        # боковые отростки
        for side in (-1, 1):
            bx = 32 + math.cos(ang) * 14 + math.cos(ang + side * 1.1) * 5
            by = 32 + math.sin(ang) * 14 + math.sin(ang + side * 1.1) * 5
            c.line(32 + math.cos(ang) * 14, 32 + math.sin(ang) * 14, bx, by, 2.0, LIGHT)
    c.disc(32, 32, 5.0, WHITE)


def glyph_book(c):
    c.outlined_poly([(10, 20), (32, 14), (32, 50), (10, 56)], MAIN, OUTLINE, 1.6)
    c.outlined_poly([(54, 20), (32, 14), (32, 50), (54, 56)], MAIN, OUTLINE, 1.6)
    c.outlined_poly([(13, 24), (30, 19), (30, 46), (13, 51)], LIGHT, MAIN, 0.8)
    c.outlined_poly([(51, 24), (34, 19), (34, 46), (51, 51)], LIGHT, MAIN, 0.8)
    for y in (27, 32, 37):
        c.line(16, y + 2, 27, y, 1.4, ACCENT)
        c.line(37, y, 48, y + 2, 1.4, ACCENT)
    c.line(32, 14, 32, 50, 2.6, OUTLINE)


def glyph_boots(c):
    mask = new_mask()
    for y in range(12, 52):
        for x in range(SIZE):
            # голенище
            leg = 20 <= x <= 36 and 12 <= y <= 38
            # стопа с носком
            foot = 18 <= x <= 50 and 38 <= y <= 46 + (x - 18) * 0.18
            if leg or foot:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    # подошва
    c.rect(17, 48, 51, 52, OUTLINE)
    c.rect(18, 47, 50, 49, ACCENT)
    # шнуровка
    for k, y in enumerate((17, 23, 29, 35)):
        c.line(23, y, 33, y, 1.6, ACCENT)
    # следы скорости
    for k, y in enumerate((20, 27, 34)):
        c.line(5, y, 15 - k, y, 2.4, ACCENT)
        c.line(7, y + 3, 15 - k, y + 3, 1.6, MAIN)

def glyph_magnet(c):
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - 32, y - 36
            r = math.hypot(dx, dy)
            if dy <= 0:
                if 8 <= abs(dx) <= 18 and y >= 12:
                    mask[y][x] = True
            elif 8 <= r <= 18:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    # полюса
    for sign in (-1, 1):
        c.rect(32 + sign * 18 - (0 if sign > 0 else 3) - (2 if sign < 0 else 0), 10, 32 + sign * 18 + 3, 20, 0)
    for sign in (-1, 1):
        c.rect(32 + sign * 13 - 5, 10, 32 + sign * 13 + 5, 20, OUTLINE)
        c.rect(32 + sign * 13 - 4, 11, 32 + sign * 13 + 4, 19, ACCENT)
    # линии притяжения — только сверху, чтобы не спорить с рамкой
    for k in range(3):
        c.line(32, 8 + k * 2, 32, 12 + k * 2, 1.4, WHITE)

GLYPHS = [glyph_blades, glyph_fire, glyph_vortex, glyph_bolt, glyph_blood,
          glyph_axe, glyph_frost, glyph_book, glyph_boots, glyph_magnet]


# ── вторая десятка: артефакты и оружие Тира II–V ──────────────────────────────

def glyph_heart(c):
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            # сердце: две дуги сверху, клин снизу
            dx, dy = x - 32, y - 30
            big = ((x - 23) ** 2 + (y - 26) ** 2 <= 121) or ((x - 41) ** 2 + (y - 26) ** 2 <= 121)
            tri = abs(x - 32) <= (52 - y) * 0.78 and y >= 26
            if (big or tri) and 8 <= y <= 54:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    # прожилки-кристаллы
    c.line(32, 20, 32, 44, 2.4, OUTLINE)
    c.line(32, 30, 24, 22, 2.0, OUTLINE)
    c.line(32, 30, 40, 22, 2.0, OUTLINE)
    c.disc(32, 34, 3.4, ACCENT)


def glyph_gauntlet(c):
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            body = 18 <= x <= 46 and 22 <= y <= 50
            knuckles = 20 <= x <= 44 and 14 <= y <= 24
            thumb = 14 <= x <= 20 and 30 <= y <= 40
            if body or knuckles or thumb:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    for i in range(4):
        const = 21 + i * 7
        c.line(const, 12, const, 24, 2.0, OUTLINE)
        c.disc(const, 22, 2.4, ACCENT)
    c.rect(18, 46, 46, 52, OUTLINE)
    c.rect(19, 46, 45, 50, ACCENT)
    for i in range(3):
        c.line(24 + i * 8, 30, 24 + i * 8, 44, 1.4, OUTLINE)


def glyph_watch(c):
    c.ring(32, 36, 20, 4.0, OUTLINE)
    c.ring(32, 36, 20, 2.4, MAIN)
    c.disc(32, 36, 17, BG)
    for k in range(12):
        ang = k * math.pi / 6
        r0 = 10 if k % 3 else 8
        c.line(32 + math.cos(ang) * r0, 36 + math.sin(ang) * r0,
               32 + math.cos(ang) * 15, 36 + math.sin(ang) * 15, 1.6 if k % 3 else 2.4,
               LIGHT if k % 3 else WHITE)
    c.line(32, 36, 32, 24, 3.0, ACCENT)
    c.line(32, 36, 41, 40, 2.4, ACCENT)
    c.disc(32, 36, 2.6, WHITE)
    # цепочка и заводная головка
    c.ring(32, 36, 23, 2.0, MAIN)
    for k in range(3):
        c.disc(32, 12 - k * 4, 2.6, OUTLINE)
        c.disc(32, 12 - k * 4, 1.6, ACCENT)


def glyph_coin(c):
    c.disc(32, 33, 22, OUTLINE)
    c.disc(32, 33, 19, MAIN)
    c.ring(32, 33, 15, 2.0, LIGHT)
    # череп
    c.disc(32, 29, 8, LIGHT)
    c.rect(25, 35, 39, 41, LIGHT)
    c.disc(29, 29, 2.6, OUTLINE)
    c.disc(35, 29, 2.6, OUTLINE)
    for i in range(3):
        c.line(28 + i * 4, 36, 28 + i * 4, 41, 1.4, OUTLINE)
    # косой крест под монетой
    c.line(10, 52, 22, 44, 2.2, ACCENT)
    c.line(54, 52, 42, 44, 2.2, ACCENT)


def glyph_feather(c):
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            t = (y - 8) / 44.0
            if t < 0 or t > 1:
                continue
            centre = 32 + 8 * math.sin(t * 2.6)
            half = 9 * math.sin(min(1.0, t) * math.pi) ** 0.75
            if abs(x - centre) <= half:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    # стержень и бородки
    for y in range(10, 54):
        t = (y - 8) / 44.0
        centre = 32 + 8 * math.sin(t * 2.6)
        c.put(int(centre), y, OUTLINE)
        if y % 6 == 0:
            c.line(int(centre) - 6, y - 4, int(centre), y, 1.2, OUTLINE)
            c.line(int(centre) + 6, y - 4, int(centre), y, 1.2, OUTLINE)
    c.disc(32, 8, 3.0, ACCENT)


def glyph_abyss_eye(c):
    c.outlined_poly([(8, 32), (32, 14), (56, 32), (32, 50)], MAIN, OUTLINE, 1.6)
    c.outlined_poly([(14, 32), (32, 20), (50, 32), (32, 44)], BG, MAIN, 1.0)
    c.disc(32, 32, 9.0, OUTLINE)
    c.disc(32, 32, 7.5, ACCENT)
    c.disc(32, 32, 3.4, OUTLINE)
    c.disc(32, 30, 2.0, WHITE)
    # лучи
    for k in range(6):
        ang = -0.5 + k * 0.2
        c.line(32 + math.cos(ang) * 22, 32 + math.sin(ang) * 22,
               32 + math.cos(ang) * 30, 32 + math.sin(ang) * 30, 1.4, ACCENT)


def glyph_seal_heart(c):
    c.disc(32, 32, 22, OUTLINE)
    c.disc(32, 32, 19, BG2)
    c.ring(32, 32, 15, 2.0, MAIN)
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            big = ((x - 27) ** 2 + (y - 28) ** 2 <= 49) or ((x - 37) ** 2 + (y - 28) ** 2 <= 49)
            tri = abs(x - 32) <= (45 - y) * 0.8 and y >= 28
            if (big or tri) and 18 <= y <= 45:
                mask[y][x] = True
    mask_fill(c, mask, ACCENT, 1)
    for k in range(8):
        ang = k * math.pi / 4
        c.line(32 + math.cos(ang) * 16, 32 + math.sin(ang) * 16,
               32 + math.cos(ang) * 22, 32 + math.sin(ang) * 22, 1.6, MAIN)


def glyph_moon_shard(c):
    # полумесяц, из которого выломан осколок
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            outer = (x - 34) ** 2 + (y - 32) ** 2 <= 400
            inner = (x - 24) ** 2 + (y - 30) ** 2 <= 330
            if outer and not inner:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    shard = [(46, 20), (56, 34), (44, 46)]
    c.outlined_poly(shard, ACCENT, OUTLINE, 1.6)
    c.line(50, 26, 46, 40, 1.6, OUTLINE)
    c.disc(22, 46, 2.2, ACCENT)


def glyph_lens(c):
    c.ring(32, 30, 18, 4.0, OUTLINE)
    c.ring(32, 30, 18, 2.2, MAIN)
    c.disc(32, 30, 15, BG)
    c.ring(32, 30, 10, 1.6, ACCENT)
    # трещина по линзе
    c.line(24, 20, 34, 30, 2.0, LIGHT)
    c.line(34, 30, 28, 40, 2.0, LIGHT)
    c.line(34, 30, 42, 34, 1.6, LIGHT)
    # рукоять
    c.line(42, 44, 52, 56, 6.0, OUTLINE)
    c.line(42, 44, 52, 56, 3.2, ACCENT)


def glyph_dawn(c):
    # последний рассвет: половина солнца над линией горизонта
    mask = new_mask()
    for y in range(SIZE):
        for x in range(SIZE):
            if (x - 32) ** 2 + (y - 38) ** 2 <= 240 and y <= 38:
                mask[y][x] = True
    mask_fill(c, mask, MAIN, 2, LIGHT)
    c.rect(12, 38, 52, 45, OUTLINE)
    c.rect(13, 38, 51, 42, ACCENT)
    for k in range(9):
        ang = math.pi + k * math.pi / 8
        c.line(32 + math.cos(ang) * 17, 38 + math.sin(ang) * 17,
               32 + math.cos(ang) * 25, 38 + math.sin(ang) * 25, 2.4, OUTLINE)
        c.line(32 + math.cos(ang) * 17, 38 + math.sin(ang) * 17,
               32 + math.cos(ang) * 24, 38 + math.sin(ang) * 24, 1.2, ACCENT)
    c.line(20, 49, 44, 49, 2.0, LIGHT)
    c.line(24, 54, 40, 54, 1.6, LIGHT)

GLYPHS_2 = [glyph_heart, glyph_gauntlet, glyph_watch, glyph_coin, glyph_feather,
            glyph_abyss_eye, glyph_seal_heart, glyph_moon_shard, glyph_lens, glyph_dawn]


def glyph_sigil(c, item):
    """Печать для предметов 10…39: кольцо, зубцы и узор, зависящий от номера."""
    c.ring(32, 32, 22, 3.0, OUTLINE)
    c.ring(32, 32, 22, 1.6, MAIN)
    teeth = 4 + item % 5
    for k in range(teeth):
        ang = k * 2 * math.pi / teeth
        c.line(32 + math.cos(ang) * 22, 32 + math.sin(ang) * 22,
               32 + math.cos(ang) * 28, 32 + math.sin(ang) * 28, 2.6, OUTLINE)
        c.line(32 + math.cos(ang) * 22, 32 + math.sin(ang) * 22,
               32 + math.cos(ang) * 27, 32 + math.sin(ang) * 27, 1.6, MAIN)
    kind = (item // 5) % 4
    if kind == 0:      # ромб
        c.outlined_poly([(32, 14), (48, 32), (32, 50), (16, 32)], MAIN, OUTLINE, 1.4)
        c.outlined_poly([(32, 24), (40, 32), (32, 40), (24, 32)], LIGHT, MAIN, 0.6)
    elif kind == 1:    # звезда
        pts = []
        for k in range(10):
            ang = -math.pi / 2 + k * math.pi / 5
            r = 18 if k % 2 == 0 else 8
            pts.append((32 + math.cos(ang) * r, 32 + math.sin(ang) * r))
        c.outlined_poly(pts, MAIN, OUTLINE, 1.2)
    elif kind == 2:    # глаз
        c.outlined_poly([(12, 32), (32, 18), (52, 32), (32, 46)], LIGHT, OUTLINE, 1.2)
        c.disc(32, 32, 7.0, OUTLINE)
        c.disc(32, 32, 5.0, MAIN)
        c.disc(32, 32, 2.2, WHITE)
    else:              # чаша
        c.outlined_poly([(16, 20), (48, 20), (44, 44), (20, 44)], MAIN, OUTLINE, 1.2)
        c.rect(28, 44, 36, 50, OUTLINE)
        c.rect(22, 50, 42, 54, MAIN)
        c.disc(32, 30, 5.0, LIGHT)
    pips = item % 4
    for k in range(pips):
        c.disc(12 + k * 5, 56, 1.8, ACCENT)


ICON_DIR = os.path.join(ROOT, "design", "ai_icons")


def ai_art_path(item, key):
    """ИИ-арт предмета, если он нарисован: design/ai_icons/icon_XX_<ключ>.png."""
    for name in sorted(os.listdir(ICON_DIR)) if os.path.isdir(ICON_DIR) else []:
        if name.startswith(f"icon_{item:02d}_") and name.endswith(".png"):
            return os.path.join(ICON_DIR, name)
    return None


def bake_ai_art(path):
    """PNG -> индексная картинка 64x64 (палитра + индексы) для встраивания в exe."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    side = min(image.size)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    image = image.crop((left, top, left + side, top + side)).resize((SIZE, SIZE), Image.LANCZOS)
    quantized = image.quantize(colors=127, method=Image.MEDIANCUT)
    palette = quantized.getpalette()[: 127 * 3]
    pixels = list(quantized.get_flattened_data() if hasattr(quantized, "get_flattened_data") else quantized.getdata())
    colors = []
    for i in range(0, len(palette), 3):
        colors.append(0xFF000000 | (palette[i] << 16) | (palette[i + 1] << 8) | palette[i + 2])
    return [0x00000000] + colors, [int(p) + 1 for p in pixels]


def build_icon(item):
    path = ai_art_path(item, item)
    if path:
        return bake_ai_art(path)
    canvas = Canvas()
    frame(canvas, item not in WEAPON_ITEMS)
    if item < 10:
        GLYPHS[item](canvas)
    elif item < 20:
        GLYPHS_2[item - 10](canvas)
    else:
        glyph_sigil(canvas, item)
    return canvas


def art_palette(path, item):
    """Палитра подложки по самой картинке: медианный цвет светлых пикселей.

    Так подложка любого нарисованного предмета совпадает с его настоящим цветом —
    и серые иконки (перо, луна, звезда, рунный разворот) больше не стоят на
    цветной плитке чужого оттенка.
    """
    from PIL import Image
    image = Image.open(path).convert("RGB").resize((48, 48))
    pix = [c for c in image.get_flattened_data() if sum(c) > 120]
    if not pix:
        return None
    pix.sort(key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
    r, g, b = pix[len(pix) // 2]
    h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
    return h * 360, max(0.18, min(0.92, s)), max(0.22, min(0.80, l))


def emit():
    lines = ["#pragma once", "#include <stdint.h>", "",
             "// Generated by tools/generate_icon_color_atlas.py — стиль иконок «ЦВЕТ».",
             "// Первые десять предметов: один предмет — один цвет и один силуэт.",
             "// Индексная палитра 64x64, без PNG во время работы.",
             "#ifndef SKILL_ICON_COLOR_STRUCT", "#define SKILL_ICON_COLOR_STRUCT",
             "struct BakedIconImage; // объявлена в skill_icon_atlas.h",
             "#endif", ""]
    tables = []
    for item in range(40):
        built = build_icon(item)
        if isinstance(built, tuple):
            pal, flat = built
            pal = list(pal) + [0] * (128 - len(pal))
            source = "ии-арт"
        else:
            canvas = built
            art = art_palette(ai_art_path(item, item), item) if ai_art_path(item, item) else None
            pal = [0x00000000] + list(_palette(item, *art) if art else PALETTES[item])
            pal = pal + [0] * (128 - len(pal))
            flat = [px for row in canvas.px for px in row]
            source = "код"
        used = sorted(set(flat))
        name = f"SKILL_ICON_COLOR_{item:02d}"
        lines.append(f"static const uint32_t {name}_PALETTE[128] = {{")
        for i in range(0, 128, 8):
            lines.append("    " + ",".join(f"0x{c:08X}u" for c in pal[i:i + 8]) + ",")
        lines.append("};")
        lines.append("")
        lines.append(f"static const uint8_t {name}_PIXELS[4096] = {{")
        for i in range(0, 4096, 32):
            lines.append("    " + ",".join(str(v) for v in flat[i:i + 32]) + ",")
        lines.append("};")
        lines.append("")
        tables.append(name)
        print(f"  {item:02d}  {ASSIGN[item][0]:8s}/{ASSIGN[item][1]:6s} {source:6s} цветов: {len(used)}")
    lines.append("static const BakedIconImage SKILL_ICONS_COLOR[40] = {")
    for name in tables:
        lines.append(f"    {{64,64,{name}_PALETTE,{name}_PIXELS}},")
    lines.append("};")
    lines.append("")
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print(f"записано: {OUT}")


if __name__ == "__main__":
    emit()
