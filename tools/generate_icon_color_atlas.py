#!/usr/bin/env python3
"""Третий стиль иконок предметов: «ЦВЕТ».

Первые десять предметов (семь активных оружий и три артефакта) получают по одному
ярко выраженному цвету и по одному силуэту, который ни с чем не спутать даже в
слоте 22×22. Остальные тридцать — рунные печати: та же цветовая логика, но узор
строится по номеру предмета.

    python3 tools/generate_icon_color_atlas.py      # пишет src/skill_icon_color_atlas.h

Атлас встраивается в exe: индексная палитра 64×64, без PNG во время работы.
"""
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "skill_icon_color_atlas.h")
SIZE = 64

# Индекс 0 — прозрачный, дальше — палитра одного предмета.
#                       0        1 (фон тёмн.) 2 (фон)   3 (главный) 4 (свет)  5 (контур) 6 (акцент) 7 (белый)
HUES = {
    "azure":   (0xFF12304F, 0xFF1D4C7A, 0xFF3D8BFF, 0xFF8FC4FF, 0xFF0A1524, 0xFFBFE4FF, 0xFFFFFFFF),
    "orange":  (0xFF3A1C08, 0xFF5C2C0C, 0xFFFF8420, 0xFFFFC070, 0xFF241004, 0xFFFFE0A0, 0xFFFFFFFF),
    "violet":  (0xFF261640, 0xFF3A2263, 0xFFB266FF, 0xFFDDB6FF, 0xFF150B26, 0xFFE9D2FF, 0xFFFFFFFF),
    "gold":    (0xFF3A2E08, 0xFF58460E, 0xFFFFD23F, 0xFFFFEE9C, 0xFF231B04, 0xFFFFF6C8, 0xFFFFFFFF),
    "crimson": (0xFF3D0A14, 0xFF5E1120, 0xFFFF3355, 0xFFFF8C9C, 0xFF25040B, 0xFFFFC0C8, 0xFFFFFFFF),
    "steel":   (0xFF242A33, 0xFF37404D, 0xFFC2CEDA, 0xFFEDF4FA, 0xFF12161C, 0xFFAFBECC, 0xFFFF8A6A),
    "aqua":    (0xFF0A3238, 0xFF124C55, 0xFF7CFFE6, 0xFFCDFFF5, 0xFF06191C, 0xFF6FE8FF, 0xFFFFFFFF),
    "ivory":   (0xFF3A3020, 0xFF57472C, 0xFFEFE3C0, 0xFFFFF8E6, 0xFF20180C, 0xFFD9A45C, 0xFFFFFFFF),
    "green":   (0xFF103A1C, 0xFF175628, 0xFF4FE07A, 0xFFA8FFC4, 0xFF081F0F, 0xFFB9FF7A, 0xFFFFFFFF),
    "magenta": (0xFF3A0C31, 0xFF571249, 0xFFFF5FD2, 0xFFFFB2EC, 0xFF24061F, 0xFFFFD0F2, 0xFFFFFFFF),
}
ORDER = ["azure", "orange", "violet", "gold", "crimson", "steel", "aqua", "ivory", "green", "magenta"]

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


def frame(canvas, key):
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


def build_icon(item):
    key = ORDER[item % 10]
    canvas = Canvas()
    frame(canvas, key)
    if item < 10:
        GLYPHS[item](canvas)
    elif item < 20:
        GLYPHS_2[item - 10](canvas)
    else:
        glyph_sigil(canvas, item)
    return canvas


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
        canvas = build_icon(item)
        key = ORDER[item % 10]
        pal = [0x00000000] + list(HUES[key])
        pal = pal + [0] * (128 - len(pal))
        used = sorted({px for row in canvas.px for px in row})
        name = f"SKILL_ICON_COLOR_{item:02d}"
        lines.append(f"static const uint32_t {name}_PALETTE[128] = {{")
        for i in range(0, 128, 8):
            lines.append("    " + ",".join(f"0x{c:08X}u" for c in pal[i:i + 8]) + ",")
        lines.append("};")
        lines.append("")
        lines.append(f"static const uint8_t {name}_PIXELS[4096] = {{")
        flat = [px for row in canvas.px for px in row]
        for i in range(0, 4096, 32):
            lines.append("    " + ",".join(str(v) for v in flat[i:i + 32]) + ",")
        lines.append("};")
        lines.append("")
        tables.append(name)
        print(f"  {item:02d}  {key:8s}  цветов: {len(used)}")
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
