#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
// SIMD-ветки ниже нужны для трёх самых горячих операций кадра (свечение,
// полупрозрачная заливка, добавление света). На x86-64 SSE2 есть всегда; на
// другой платформе или при сборке с -DRENDERER_NO_SIMD остаётся тот же
// скалярный код, что и раньше, поэтому картинка не зависит от флагов.
#if defined(__SSE2__) && !defined(RENDERER_NO_SIMD)
#define RENDERER_SIMD 1
#include <emmintrin.h>
#else
#define RENDERER_SIMD 0
#endif
#include "font_atlas.h"
#include "sprite_atlas.h"
#include "skill_icon_atlas.h"
#include "skill_icon_color_atlas.h"
#include "forge_icon_atlas.h"

static int SCREEN_W = 1280;
static int SCREEN_H = 720;

// Логический размер кадра: во что игра рисует и что потом растягивается на окно.
// Высота (в альбоме) и ширина (в портрете) держатся на 720 px, поэтому плотность
// мира не зависит от монитора, а видимая площадь — зависит от соотношения сторон.
// Это же число, а не размер окна, определяет, где кончается экран: «за краем»
// значит «за пределами этих пикселей». Одна функция на игру и на стенды, иначе
// тест меряет не то, что видит игрок.
inline void logicalScreenSize(int clientW, int clientH, int& outW, int& outH) {
    if (clientW < 240 || clientH < 240) { outW = SCREEN_W; outH = SCREEN_H; return; }
    const double aspect = (double)clientW / (double)clientH;
    if (aspect >= 1.0) {
        const int h = 720;
        outH = h;
        outW = std::max(640, ((int)(h * aspect + 8.0) / 16) * 16);
    } else {
        const int w = 720;
        outW = w;
        outH = std::max(720, ((int)(w / aspect + 8.0) / 16) * 16);
    }
}

inline uint32_t makeRGB(int r, int g, int b) {
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

inline uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    return makeRGB(r1 + (int)((r2 - r1) * t), g1 + (int)((g2 - g1) * t), b1 + (int)((b2 - b1) * t));
}

struct ItemTone { uint32_t bg, edge, glow; };
// Тон сорока предметов: подложка, рамка, свечение. Смысл важнее красоты:
// игрок должен узнавать предмет по цвету и не путать соседние.
//
// Сорок предметов — сорок разных цветов, а не десять по кругу. Цвет задаётся
// семейством (смысл: огонь — оранжевый, кровь — красный, лёд — бирюза, ветер —
// зелёный, руны — фиолет, магнит — пурпур, пергамент — янтарь, бездна — тёмный
// фиолет) и весом предмета (тёмный вариант для тяжёлых печатей, светлый для
// лёгких). Таблица в tools/generate_icon_color_atlas.py — источник тех же цветов
// для атласа иконок, поэтому рамка слота и сама иконка всегда одного тона.
static const ItemTone ITEM_TONE[40] = {
    {0xFF0A1329, 0xFF7EA4FC, 0xFFC8D7F9},  // 00 blue/bright
    {0xFF29140A, 0xFFF66C28, 0xFFF2B292},  // 01 orange/pure
    {0xFF180A29, 0xFFB97EFC, 0xFFDFC8F9},  // 02 violet/bright
    {0xFF23290A, 0xFFCDF628, 0xFFDFF292},  // 03 lime/pure
    {0xFF290A0E, 0xFFF62843, 0xFFF2929F},  // 04 red/pure
    {0xFF28150B, 0xFFC64D10, 0xFFE48758},  // 05 orange/deep
    {0xFF0A2429, 0xFF28D4F6, 0xFF92E2F2},  // 06 cyan/pure
    {0xFF251E0E, 0xFF8B6818, 0xFFCDA23C},  // 07 amber/dark
    {0xFF0A2910, 0xFF28F651, 0xFF92F2A5},  // 08 green/pure
    {0xFF290A23, 0xFFF628CD, 0xFFF292DF},  // 09 magenta/pure
    {0xFF250E20, 0xFF8B1874, 0xFFCD3CB0},  // 10 magenta/dark
    {0xFF29140A, 0xFFFCA87E, 0xFFF9D8C8},  // 11 orange/bright
    {0xFF29200A, 0xFFFCD67E, 0xFFF9EAC8},  // 12 amber/bright
    {0xFF22280B, 0xFFA2C610, 0xFFC8E458},  // 13 lime/deep
    {0xFF281F0B, 0xFFC69010, 0xFFE4BA58},  // 14 amber/deep
    {0xFF190E25, 0xFF4E188B, 0xFF803CCD},  // 15 violet/dark
    {0xFF0E2125, 0xFF18788B, 0xFF3CB5CD},  // 16 cyan/dark
    {0xFF190B28, 0xFF6510C6, 0xFF9958E4},  // 17 violet/deep
    {0xFF0A2429, 0xFF7EE7FC, 0xFFC8F1F9},  // 18 cyan/bright
    {0xFF290A0E, 0xFFFC7E8F, 0xFFF9C8CE},  // 19 red/bright
    {0xFF0E2513, 0xFF188B2F, 0xFF3CCD59},  // 20 green/dark
    {0xFF0A2923, 0xFF28F6CD, 0xFF92F2DF},  // 21 teal/pure
    {0xFF0B2822, 0xFF10C6A2, 0xFF58E4C8},  // 22 teal/deep
    {0xFF290A23, 0xFFFC7EE2, 0xFFF9C8EF},  // 23 magenta/bright
    {0xFF0A1329, 0xFF2866F6, 0xFF92AFF2},  // 24 blue/pure
    {0xFF0A2910, 0xFF7EFC97, 0xFFC8F9D2},  // 25 green/bright
    {0xFF0E1525, 0xFF183B8B, 0xFF3C67CD},  // 26 blue/dark
    {0xFF20250E, 0xFF748B18, 0xFFB0CD3C},  // 27 lime/dark
    {0xFF0B2328, 0xFF10A8C6, 0xFF58CDE4},  // 28 cyan/deep
    {0xFF23290A, 0xFFE2FC7E, 0xFFEFF9C8},  // 29 lime/bright
    {0xFF0B2811, 0xFF10C634, 0xFF58E474},  // 30 green/deep
    {0xFF280B0F, 0xFFC61028, 0xFFE4586B},  // 31 red/deep
    {0xFF29200A, 0xFFF6B828, 0xFFF2D592},  // 32 amber/pure
    {0xFF0A2923, 0xFF7EFCE2, 0xFFC8F9EF},  // 33 teal/bright
    {0xFF250E11, 0xFF8B1828, 0xFFCD3C4F},  // 34 red/dark
    {0xFF0E2520, 0xFF188B74, 0xFF3CCDB0},  // 35 teal/dark
    {0xFF0B1428, 0xFF1047C6, 0xFF5882E4},  // 36 blue/deep
    {0xFF280B22, 0xFFC610A2, 0xFFE458C8},  // 37 magenta/deep
    {0xFF25160E, 0xFF8B3F18, 0xFFCD6C3C},  // 38 orange/dark
    {0xFF180A29, 0xFF8828F6, 0xFFBF92F2},  // 39 violet/pure
};

struct Renderer {
    uint32_t* pixels;

    int width = 0;
    int height = 0;
    std::vector<uint8_t> vignetteAlpha;   // запечённая карта затемнения
    int vignetteW = -1, vignetteH = -1;
    // Снимок области кадра до отрисовки фигуры и маска её пикселей: по ним
    // считается объём (свет сверху-слева, тень с обратной стороны, AO у земли).
    std::vector<uint32_t> figureScratch;
    std::vector<uint16_t> figureColor;   // упакованный цвет детали (5 бит на канал)
    std::vector<uint8_t> figureMask;
    // 0 = compact vector icons, 1 = AI-rendered art for the two approved skill variants.
    int skillIconStyle = 0; // 0 = vector, 1 = AI art, 2 = «цвет» (один предмет — один цвет)

    Renderer() : pixels(nullptr) {
        resize(SCREEN_W, SCREEN_H);
    }
    ~Renderer() {
        delete[] pixels;
    }

    void resize(int newWidth, int newHeight) {
        newWidth = std::max(1, newWidth);
        newHeight = std::max(1, newHeight);
        if (pixels && width == newWidth && height == newHeight) return;
        delete[] pixels;
        width = newWidth;
        height = newHeight;
        pixels = new uint32_t[(size_t)width * (size_t)height];
        for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) pixels[i] = 0xFF0E0B16;
        vignetteW = -1;   // виньетка пересчитается под новый размер
    }

    void clear(uint32_t col) {
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) pixels[i] = col;
    }

    inline void putPixel(int x, int y, uint32_t col) {
        if ((unsigned)x < (unsigned)SCREEN_W && (unsigned)y < (unsigned)SCREEN_H) {
            pixels[y * SCREEN_W + x] = col;
        }
    }

    // Обрезка текста по прямоугольнику. Нужна там, где текст длиннее панели:
    // патчноуты, словарь. Действует только на вывод глифов, поэтому панели и
    // рамки рисуются как обычно.
    int clipX0 = 0, clipY0 = 0, clipX1 = SCREEN_W, clipY1 = SCREEN_H;
    bool clipActive = false;
    void setClip(int x, int y, int w, int h) {
        clipActive = true;
        clipX0 = std::max(0, x); clipY0 = std::max(0, y);
        clipX1 = std::min(SCREEN_W, x + w); clipY1 = std::min(SCREEN_H, y + h);
    }
    void clearClip() { clipActive = false; clipX0 = 0; clipY0 = 0; clipX1 = SCREEN_W; clipY1 = SCREEN_H; }

    inline void blendPixel(int x, int y, uint32_t col, int alpha) {
        if ((unsigned)x >= (unsigned)SCREEN_W || (unsigned)y >= (unsigned)SCREEN_H) return;
        // Отсечение на уровне пикселя: линии и круги идут через blendPixel, и
        // без этой проверки скроллящаяся карта рисовала маршрут поверх кнопок.
        if (clipActive && ((unsigned)(x - clipX0) >= (unsigned)(clipX1 - clipX0) ||
                           (unsigned)(y - clipY0) >= (unsigned)(clipY1 - clipY0))) return;
        if (alpha <= 0) return;
        if (alpha >= 255) {
            pixels[y * SCREEN_W + x] = col;
            return;
        }
        uint32_t dst = pixels[y * SCREEN_W + x];
        int sr = (col >> 16) & 0xFF, sg = (col >> 8) & 0xFF, sb = col & 0xFF;
        int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
        int inv = 255 - alpha;
        pixels[y * SCREEN_W + x] = 0xFF000000u
            | (((sr * alpha + dr * inv) >> 8) << 16)
            | (((sg * alpha + dg * inv) >> 8) << 8)
            | ((sb * alpha + db * inv) >> 8);
    }

    inline void addPixel(int x, int y, uint32_t col, int intensity) {
        if ((unsigned)x >= (unsigned)SCREEN_W || (unsigned)y >= (unsigned)SCREEN_H || intensity <= 0) return;
        if (clipActive && ((unsigned)(x - clipX0) >= (unsigned)(clipX1 - clipX0) ||
                           (unsigned)(y - clipY0) >= (unsigned)(clipY1 - clipY0))) return;
        uint32_t dst = pixels[y * SCREEN_W + x];
        int r = ((dst >> 16) & 0xFF) + ((((col >> 16) & 0xFF) * intensity) >> 8);
        int g = ((dst >> 8) & 0xFF)  + ((((col >> 8) & 0xFF) * intensity) >> 8);
        int b = (dst & 0xFF)         + (((col & 0xFF) * intensity) >> 8);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        pixels[y * SCREEN_W + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }

    inline void blendSpan(int y, int x0, int x1, uint32_t col, int alpha) {
        if ((unsigned)y >= (unsigned)SCREEN_H || alpha <= 0 || x0 > x1) return;
        x0 = std::max(0, x0); x1 = std::min(SCREEN_W - 1, x1);
        // Область отсечения уважают и заливки: раньше setClip действовал только
        // на текст и контуры, а круги, линии и скруглённые панели лезли за
        // границу списка — прокручиваемая карта кампании рисовала дела поверх
        // кнопок внизу экрана.
        if (clipActive) {
            if (y < clipY0 || y >= clipY1) return;
            x0 = std::max(x0, clipX0); x1 = std::min(x1, clipX1 - 1);
        }
        if (x0 > x1) return;
        uint32_t* row = pixels + y * SCREEN_W;
        if (alpha >= 255) {
            std::fill(row + x0, row + x1 + 1, col);
            return;
        }
        const int sr = (col >> 16) & 0xFF, sg = (col >> 8) & 0xFF, sb = col & 0xFF;
        const int inv = 255 - alpha;
        int x = x0;
#if RENDERER_SIMD
        // Полупрозрачная заливка — самая частая операция кадра (панели, тени,
        // вода, ореолы). Формула ((канал*alpha + фон*(255-alpha)) >> 8) точно
        // ложится на 16-битные дорожки: каждое произведение ≤ 65025, сумма
        // ≤ 65025, то есть переполнения нет, а насыщение не нужно вовсе.
        {
            const __m128i zero = _mm_setzero_si128();
            const __m128i alphaVec = _mm_set1_epi16((short)alpha);
            const __m128i invVec = _mm_set1_epi16((short)inv);
            const __m128i alphaMask = _mm_set1_epi32((int)0xFF000000u);
            // Цвет, уже умноженный на alpha: на строку он один и тот же.
            const __m128i srcBytes = _mm_set1_epi32((int)((uint32_t)(sr << 16) | (uint32_t)(sg << 8) | (uint32_t)sb));
            const __m128i preLo = _mm_mullo_epi16(_mm_unpacklo_epi8(srcBytes, zero), alphaVec);
            const __m128i preHi = _mm_mullo_epi16(_mm_unpackhi_epi8(srcBytes, zero), alphaVec);
            for (; x + 3 <= x1; x += 4) {
                const __m128i dst = _mm_loadu_si128((const __m128i*)(row + x));
                const __m128i lo = _mm_srli_epi16(
                    _mm_add_epi16(preLo, _mm_mullo_epi16(_mm_unpacklo_epi8(dst, zero), invVec)), 8);
                const __m128i hi = _mm_srli_epi16(
                    _mm_add_epi16(preHi, _mm_mullo_epi16(_mm_unpackhi_epi8(dst, zero), invVec)), 8);
                _mm_storeu_si128((__m128i*)(row + x),
                                 _mm_or_si128(_mm_packus_epi16(lo, hi), alphaMask));
            }
        }
#endif
        for (; x <= x1; ++x) {
            const uint32_t dst = row[x];
            const int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
            row[x] = 0xFF000000u | (((sr * alpha + dr * inv) >> 8) << 16)
                   | (((sg * alpha + dg * inv) >> 8) << 8) | ((sb * alpha + db * inv) >> 8);
        }
    }

    void fillRect(int x, int y, int w, int h, uint32_t col, int alpha = 255) {
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        if (clipActive) {
            x0 = std::max(x0, clipX0); x1 = std::min(x1, clipX1);
            y0 = std::max(y0, clipY0); y1 = std::min(y1, clipY1);
        }
        if (x0 >= x1 || y0 >= y1 || alpha <= 0) return;
        // Полуразмерная заливка идёт через blendSpan: там те же вычисления, но
        // без проверки границ и клипа на каждый пиксель. Раньше blendPixel
        // вызывался построчно-постолбцово и тратил на это больше, чем на саму
        // арифметику цвета.
        if (alpha >= 255) {
            for (int py = y0; py < y1; ++py) {
                uint32_t* row = pixels + py * SCREEN_W;
                std::fill(row + x0, row + x1, col);
            }
        } else {
            for (int py = y0; py < y1; ++py) blendSpan(py, x0, x1 - 1, col, alpha);
        }
    }

    void fillGradientV(int x, int y, int w, int h, uint32_t topCol, uint32_t botCol, int alpha = 255) {
        if (h <= 0 || w <= 0) return;
        int x0 = std::max(0, x), x1 = std::min(SCREEN_W, x + w);
        int y0 = std::max(0, y), y1 = std::min(SCREEN_H, y + h);
        if (clipActive) {
            x0 = std::max(x0, clipX0); x1 = std::min(x1, clipX1);
            y0 = std::max(y0, clipY0); y1 = std::min(y1, clipY1);
        }
        for (int py = y0; py < y1; ++py) {
            float t = (float)(py - y) / (float)h;
            uint32_t c = lerpColor(topCol, botCol, t);
            if (alpha >= 255) {
                uint32_t* row = pixels + py * SCREEN_W;
                std::fill(row + x0, row + x1, c);
            } else {
                blendSpan(py, x0, x1 - 1, c, alpha);
            }
        }
    }

    void drawRect(int x, int y, int w, int h, uint32_t col, int thickness = 1, int alpha = 255) {
        fillRect(x, y, w, thickness, col, alpha);
        fillRect(x, y + h - thickness, w, thickness, col, alpha);
        fillRect(x, y + thickness, thickness, h - 2 * thickness, col, alpha);
        fillRect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, col, alpha);
    }

    void fillRoundedRect(int x, int y, int w, int h, int rad, uint32_t col, int alpha = 255) {
        if (rad <= 0) { fillRect(x, y, w, h, col, alpha); return; }
        if (rad * 2 > w) rad = w / 2;
        if (rad * 2 > h) rad = h / 2;
        const int x0 = std::max(0, x), y0 = std::max(0, y);
        const int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        if (x0 >= x1 || y0 >= y1 || alpha <= 0) return;
        const int r2 = rad * rad;
        // Скругление — это отступ по краям строки, а не отдельная проверка на
        // каждый пиксель: раньше вся панель (включая ровную середину) шла
        // пиксель за пикселем через blendPixel с проверкой границ и клипа.
        for (int py = y0; py < y1; ++py) {
            int dy = 0;
            if (py < y + rad) dy = (y + rad) - py;
            else if (py >= y + h - rad) dy = py - (y + h - rad - 1);
            int inset = 0;
            if (dy > 0) {
                const int rem = r2 - dy * dy;
                if (rem < 0) continue;
                inset = rad - (int)sqrtf((float)rem);
                if (inset < 0) inset = 0;
            }
            blendSpan(py, x + inset, x + w - 1 - inset, col, alpha);
        }
    }

    void drawRoundedRect(int x, int y, int w, int h, int rad, uint32_t col, int thickness = 2, int alpha = 255) {
        if (rad <= 0) { drawRect(x, y, w, h, col, thickness, alpha); return; }
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        int rOuter2 = rad * rad;
        int innerRad = std::max(0, rad - thickness);
        int rInner2 = innerRad * innerRad;
        for (int py = y0; py < y1; ++py) {
            int dy = 0;
            if (py < y + rad) dy = (y + rad) - py;
            else if (py >= y + h - rad) dy = py - (y + h - rad - 1);
            for (int px = x0; px < x1; ++px) {
                int dx = 0;
                if (px < x + rad) dx = (x + rad) - px;
                else if (px >= x + w - rad) dx = px - (x + w - rad - 1);
                if (dx > 0 && dy > 0) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 > rOuter2 || d2 < rInner2) continue;
                    blendPixel(px, py, col, alpha);
                } else {
                    if (px < x + thickness || px >= x + w - thickness ||
                        py < y + thickness || py >= y + h - thickness) {
                        blendPixel(px, py, col, alpha);
                    }
                }
            }
        }
    }

    void fillCircle(int cx, int cy, int r, uint32_t col, int alpha = 255) {
        if (r <= 0 || alpha <= 0) return;
        const int r2 = r * r;
        const int y0 = std::max(0, cy - r), y1 = std::min(SCREEN_H - 1, cy + r);
        for (int py = y0; py <= y1; ++py) {
            const int dy = py - cy;
            const int rem = r2 - dy * dy;
            if (rem < 0) continue;
            const int extent = (int)sqrtf((float)rem);
            blendSpan(py, cx - extent, cx + extent, col, alpha);
        }
    }

    // Ядро свечения считается один раз на радиус и живёт в таблице: в кадре
    // это десятки тысяч обращений, поэтому читается оно по сырому указателю.
    static const std::vector<uint32_t>* getGlowKernel(int radius) {
        if (radius <= 0 || radius > 256) return nullptr;
        static std::vector<std::vector<uint32_t>> kernels(513);
        std::vector<uint32_t>& kernel = kernels[radius];
        if (kernel.empty()) {
            const int r2 = radius * radius;
            kernel.resize((size_t)r2);
            for (int d2 = 0; d2 < r2; ++d2) {
                const int falloff = 256 - (d2 * 256) / r2;
                kernel[d2] = (uint32_t)(falloff * falloff);
            }
        }
        return &kernel;
    }

    void addGlow(int cx, int cy, int r, uint32_t col, int maxIntensity = 180) {
        if (r <= 0 || maxIntensity <= 0) return;
        // Дешёвый отсев: свечение целиком за кадром. Частицы и снаряды улетают
        // за край постоянно, а до этой проверки для каждой из них считался
        // диапазон строк, корень и границы.
        if (cx + r < 0 || cy + r < 0 || cx - r >= SCREEN_W || cy - r >= SCREEN_H) return;
        const int r2 = r * r;
        const int y0 = std::max(0, cy - r), y1 = std::min(SCREEN_H - 1, cy + r);
        const int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
        const std::vector<uint32_t>* kernel = getGlowKernel(r);
        const uint32_t* k = kernel ? kernel->data() : nullptr;
        // Таблица «яркость → прибавка» на этот вызов: в кадре десятки свечений,
        // и три умножения на каждый пиксель дороже, чем 256 входов таблицы.
        // Значения те же, что и в прежней формуле ((канал * intensity) >> 8).
        // Прибавка упакована в те же байты, что и пиксель (r<<16 | g<<8 | b):
        // тогда к четырём пикселям сразу можно прибавить вектор одной командой.
        uint32_t addPacked[256];
        for (int i = 0; i < 256; ++i) {
            addPacked[i] = ((uint32_t)((cr * i) >> 8) << 16)
                         | ((uint32_t)((cg * i) >> 8) << 8)
                         | (uint32_t)((cb * i) >> 8);
        }
        for (int py = y0; py <= y1; ++py) {
            const int dy = py - cy, dy2 = dy * dy;
            const int rem = r2 - 1 - dy2; // the glow is defined for d² < r²
            if (rem < 0) continue;
            const int extent = (int)sqrtf((float)rem);
            const int dx0 = std::max(-extent, -cx);
            const int dx1 = std::min(extent, SCREEN_W - 1 - cx);
            if (dx0 > dx1) continue;
            uint32_t* dstRow = pixels + py * SCREEN_W + cx;
            if (k) {
                int dx = dx0;
#if RENDERER_SIMD
                // Прибавление света — это сложение байтов с насыщением, ровно
                // то, что делает paddusb: четыре пикселя за команду, без
                // разбора на каналы и без трёх сравнений с 255 на каждый.
                // Альфа принудительно в 255 — так же, как в скалярной ветке.
                const __m128i alphaMask = _mm_set1_epi32((int)0xFF000000u);
                for (; dx + 3 <= dx1; dx += 4) {
                    const uint32_t a0 = addPacked[(maxIntensity * (int)k[(dx + 0) * (dx + 0) + dy2]) >> 16];
                    const uint32_t a1 = addPacked[(maxIntensity * (int)k[(dx + 1) * (dx + 1) + dy2]) >> 16];
                    const uint32_t a2 = addPacked[(maxIntensity * (int)k[(dx + 2) * (dx + 2) + dy2]) >> 16];
                    const uint32_t a3 = addPacked[(maxIntensity * (int)k[(dx + 3) * (dx + 3) + dy2]) >> 16];
                    const __m128i diff = _mm_set_epi32((int)a3, (int)a2, (int)a1, (int)a0);
                    __m128i* quad = (__m128i*)(dstRow + dx);
                    const __m128i sum = _mm_adds_epu8(_mm_loadu_si128(quad), diff);
                    _mm_storeu_si128(quad, _mm_or_si128(sum, alphaMask));
                }
#endif
                for (; dx <= dx1; ++dx) {
                    // maxIntensity ≤ 255, ядро ≤ 65536 — произведение влезает
                    // в 32 бита, 64-битное умножение здесь было лишним.
                    const int intensity = (maxIntensity * (int)k[dx * dx + dy2]) >> 16;
                    if (intensity <= 0) continue;
                    const uint32_t d = addPacked[intensity];
                    uint32_t& dst = dstRow[dx];
                    int rr = ((dst >> 16) & 0xFF) + (int)((d >> 16) & 0xFF);
                    int gg = ((dst >> 8) & 0xFF) + (int)((d >> 8) & 0xFF);
                    int bb = (dst & 0xFF) + (int)(d & 0xFF);
                    if (rr > 255) rr = 255;
                    if (gg > 255) gg = 255;
                    if (bb > 255) bb = 255;
                    dst = 0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
                }
            } else {
                for (int dx = dx0; dx <= dx1; ++dx) {
                    const int d2 = dx * dx + dy2;
                    const int falloff = 256 - (d2 * 256) / r2;
                    const int intensity = (maxIntensity * falloff * falloff) >> 16;
                    if (intensity <= 0) continue;
                    const uint32_t d = addPacked[intensity];
                    uint32_t& dst = dstRow[dx];
                    int rr = ((dst >> 16) & 0xFF) + (int)((d >> 16) & 0xFF);
                    int gg = ((dst >> 8) & 0xFF) + (int)((d >> 8) & 0xFF);
                    int bb = (dst & 0xFF) + (int)(d & 0xFF);
                    if (rr > 255) rr = 255;
                    if (gg > 255) gg = 255;
                    if (bb > 255) bb = 255;
                    dst = 0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
                }
            }
        }
    }

    void drawCircleRing(int cx, int cy, int r, int thickness, uint32_t col, int alpha = 255, bool additive = false) {
        if (r <= 0 || alpha <= 0) return;
        const int rOut = r + thickness / 2;
        const int rIn = std::max(0, r - (thickness - thickness / 2));
        const int rOut2 = rOut * rOut, rIn2 = rIn * rIn;
        const int y0 = std::max(0, cy - rOut), y1 = std::min(SCREEN_H - 1, cy + rOut);
        for (int py = y0; py <= y1; ++py) {
            const int dy = py - cy, dy2 = dy * dy;
            const int outerRem = rOut2 - dy2;
            if (outerRem < 0) continue;
            const int outerX = (int)sqrtf((float)outerRem);
            int innerX = 0;
            if (dy2 < rIn2) {
                const int innerRem = rIn2 - dy2;
                innerX = (int)sqrtf((float)innerRem);
                if (innerX * innerX < innerRem) ++innerX; // ceil: keep only pixels at/ outside the inner radius
            }
            if (innerX <= 0) {
                if (additive) addSpan(py, cx - outerX, cx + outerX, col, alpha);
                else blendSpan(py, cx - outerX, cx + outerX, col, alpha);
            } else {
                if (additive) {
                    addSpan(py, cx - outerX, cx - innerX, col, alpha);
                    addSpan(py, cx + innerX, cx + outerX, col, alpha);
                } else {
                    blendSpan(py, cx - outerX, cx - innerX, col, alpha);
                    blendSpan(py, cx + innerX, cx + outerX, col, alpha);
                }
            }
        }
    }

    inline void addSpan(int y, int x0, int x1, uint32_t col, int intensity) {
        if ((unsigned)y >= (unsigned)SCREEN_H || intensity <= 0 || x0 > x1) return;
        x0 = std::max(0, x0); x1 = std::min(SCREEN_W - 1, x1);
        if (x0 > x1) return;
        const int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
        const uint32_t addPacked = ((uint32_t)((cr * intensity) >> 8) << 16)
                                 | ((uint32_t)((cg * intensity) >> 8) << 8)
                                 | (uint32_t)((cb * intensity) >> 8);
        uint32_t* row = pixels + y * SCREEN_W;
        int x = x0;
#if RENDERER_SIMD
        // Свет прибавляется к каналу с отсечкой на 255 — это paddusb, четыре
        // пикселя за команду. Раньше на каждый пиксель было три сравнения.
        {
            const __m128i diff = _mm_set1_epi32((int)addPacked);
            const __m128i alphaMask = _mm_set1_epi32((int)0xFF000000u);
            for (; x + 3 <= x1; x += 4) {
                __m128i* quad = (__m128i*)(row + x);
                _mm_storeu_si128(quad, _mm_or_si128(_mm_adds_epu8(_mm_loadu_si128(quad), diff), alphaMask));
            }
        }
#endif
        for (; x <= x1; ++x) {
            uint32_t& dst = row[x];
            int rr = ((dst >> 16) & 0xFF) + ((cr * intensity) >> 8);
            int gg = ((dst >> 8) & 0xFF) + ((cg * intensity) >> 8);
            int bb = (dst & 0xFF) + ((cb * intensity) >> 8);
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            dst = 0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
        }
    }

    void drawLine(int x0, int y0, int x1, int y1, uint32_t col, int thickness = 1, int alpha = 255, bool additive = false) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        int rad = thickness / 2;
        while (true) {
            if (rad <= 0) {
                if (additive) addPixel(x0, y0, col, alpha);
                else blendPixel(x0, y0, col, alpha);
            } else {
                for (int oy = -rad; oy <= rad; ++oy) {
                    for (int ox = -rad; ox <= rad; ++ox) {
                        if (ox * ox + oy * oy <= rad * rad + 1) {
                            if (additive) addPixel(x0 + ox, y0 + oy, col, alpha);
                            else blendPixel(x0 + ox, y0 + oy, col, alpha);
                        }
                    }
                }
            }
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Atmospheric Dark Corner Vignette.
    // The gradient depends only on the screen size, but the first version recomputed a
    // division per pixel every frame (1,8 мс на 720p и 3,8 мс на 1080p, замерено).
    // The darkness map is baked once per resolution and then applied as a plain blend.
    void buildVignetteMap() {
        vignetteW = SCREEN_W;
        vignetteH = SCREEN_H;
        vignetteAlpha.assign((size_t)SCREEN_W * SCREEN_H, 0);
        const int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        const int halfW2 = std::max(1, cx * cx), halfH2 = std::max(1, cy * cy);
        for (int y = 0; y < SCREEN_H; ++y) {
            const int dy = y - cy;
            const int dy2 = (dy * dy * 256) / halfH2;
            const bool middleBand = dy2 < 110 && y > 90 && y < SCREEN_H - 90;
            const int limit = middleBand ? std::min(180, SCREEN_W) : SCREEN_W;
            uint8_t* row = vignetteAlpha.data() + (size_t)y * SCREEN_W;
            for (int x = 0; x < limit; ++x) {
                const int dx = x - cx;
                const int d2 = dy2 + (dx * dx * 256) / halfW2;
                if (d2 <= 160) continue;
                const int dark = std::min(middleBand ? 140 : 150, d2 - 160);
                row[x] = (uint8_t)dark;
                if (middleBand) row[SCREEN_W - 1 - x] = (uint8_t)dark;   // обе стороны сразу
            }
        }
    }

    void applyVignette() {
        if (vignetteW != SCREEN_W || vignetteH != SCREEN_H || vignetteAlpha.empty())
            buildVignetteMap();
        // Blend with a precomputed multiply table: the per-pixel division by 255 was the
        // remaining cost after baking the map (3,8 мс на 1080p).
        static uint8_t mulLut[256 * 256];
        static bool lutReady = false;
        if (!lutReady) {
            for (int inv = 0; inv < 256; ++inv)
                for (int v = 0; v < 256; ++v)
                    mulLut[(inv << 8) | v] = (uint8_t)((v * inv) / 255);
            lutReady = true;
        }
        const int tr = 0x04, tg = 0x02, tb = 0x08;
        for (int y = 0; y < SCREEN_H; ++y) {
            const uint8_t* row = vignetteAlpha.data() + (size_t)y * SCREEN_W;
            uint32_t* dst = pixels + (size_t)y * SCREEN_W;
            for (int x = 0; x < SCREEN_W; ++x) {
                const int a = row[x];
                if (a == 0) continue;
                const int inv = 255 - a;
                const uint8_t* lut = mulLut + (inv << 8);
                const uint32_t c = dst[x];
                const int rr = lut[(c >> 16) & 0xFF] + (tr * a) / 255;
                const int gg = lut[(c >> 8) & 0xFF] + (tg * a) / 255;
                const int bb = lut[c & 0xFF] + (tb * a) / 255;
                dst[x] = 0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
            }
        }
    }

    // Blit pre-baked shaded RGBA pixel-art sprite
    void drawBakedSprite(const BakedSprite* spr, int cx, int cy, bool flipX = false, bool hitFlash = false, bool frozen = false, int alphaScale = 255) {
        if (!spr) return;
        int w = spr->width, h = spr->height;
        int startX = cx - w / 2;
        int startY = cy - h / 2;
        for (int sy = 0; sy < h; ++sy) {
            int py = startY + sy;
            if ((unsigned)py >= (unsigned)SCREEN_H) continue;
            const uint32_t* row = spr->pixels + sy * w;
            for (int sx = 0; sx < w; ++sx) {
                int px = startX + (flipX ? (w - 1 - sx) : sx);
                if ((unsigned)px >= (unsigned)SCREEN_W) continue;
                uint32_t c = row[sx];
                int a = ((c >> 24) & 0xFF);
                if (a <= 10) continue;
                a = (a * alphaScale) >> 8;
                if (hitFlash) {
                    c = lerpColor(c, 0xFFFFE8B0, 0.72f);
                } else if (frozen) {
                    c = lerpColor(c, 0xFF50E8FF, 0.55f);
                }
                blendPixel(px, py, c, a);
            }
        }
    }

    // Nearest-neighbor full-screen artwork blit for embedded cutscene paintings.
    // Baked pixel art carries transparent padding that is not symmetric: the visible
    // figure can sit a few pixels left/above the canvas centre. Actors are drawn from
    // their logical centre, so every ring, HP bar and marker must use the figure centre
    // instead — otherwise the "diameter" marks look shifted to one side of the mob.
    void bakedSpriteVisualOffset(const BakedSprite* spr, int& dx, int& dy) const {
        dx = dy = 0;
        if (!spr || spr->width <= 0 || spr->height <= 0) return;
        struct Entry { const BakedSprite* spr; int dx, dy; };
        static Entry cache[256];
        static int used = 0;
        for (int i = 0; i < used; ++i) {
            if (cache[i].spr == spr) { dx = cache[i].dx; dy = cache[i].dy; return; }
        }
        int x0 = spr->width, y0 = spr->height, x1 = -1, y1 = -1;
        for (int y = 0; y < spr->height; ++y) {
            for (int x = 0; x < spr->width; ++x) {
                if (((spr->pixels[y * spr->width + x] >> 24) & 0xFF) <= 10) continue;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
        if (x1 >= 0 && y1 >= 0) {
            // Round to whole pixels so the sprite grid stays crisp.
            dx = (int)std::lround((x0 + x1) * 0.5 - (spr->width - 1) * 0.5);
            dy = (int)std::lround((y0 + y1) * 0.5 - (spr->height - 1) * 0.5);
        }
        if (used < 256) cache[used++] = Entry{spr, dx, dy};
    }

    // Draw a baked sprite so its visible pixels are centred on (cx, cy).
    void drawBakedSpriteCentred(const BakedSprite* spr, int cx, int cy, bool flipX = false, bool hitFlash = false,
                                bool frozen = false, int alphaScale = 255) {
        int dx = 0, dy = 0;
        bakedSpriteVisualOffset(spr, dx, dy);
        drawBakedSprite(spr, cx - dx, cy - dy, flipX, hitFlash, frozen, alphaScale);
    }

    // Nearest-neighbour stretch with the full sprite treatment (flip, flash, freeze).
    // Two crisp frames plus squash-and-stretch read as real animation; two frames alone
    // read as a slideshow.
    void drawBakedSpriteStretched(const BakedSprite* spr, int cx, int cy, float scaleX, float scaleY,
                                  bool flipX = false, bool hitFlash = false, bool frozen = false, int alphaScale = 255) {
        if (!spr) return;
        const int w = std::max(1, (int)(spr->width * scaleX));
        const int h = std::max(1, (int)(spr->height * scaleY));
        const int x = cx - w / 2, y = cy - h / 2;
        const int x0 = std::max(0, x), y0 = std::max(0, y);
        const int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        for (int py = y0; py < y1; ++py) {
            const int sy = std::min(spr->height - 1, ((py - y) * spr->height) / h);
            const uint32_t* row = spr->pixels + sy * spr->width;
            for (int px = x0; px < x1; ++px) {
                const int dx = px - x;
                const int sx = std::min(spr->width - 1, (flipX ? (w - 1 - dx) : dx) * spr->width / w);
                uint32_t c = row[sx];
                int a = (c >> 24) & 0xFF;
                if (a <= 10) continue;
                a = (a * alphaScale) >> 8;
                if (hitFlash) c = lerpColor(c, 0xFFFFE8B0, 0.72f);
                else if (frozen) c = lerpColor(c, 0xFF50E8FF, 0.55f);
                blendPixel(px, py, c, a);
            }
        }
    }

    void drawBakedSpriteScaled(const BakedSprite* spr, int x, int y, int w, int h, int alphaScale = 255) {
        if (!spr || w <= 0 || h <= 0) return;
        const int x0 = std::max(0, x), y0 = std::max(0, y);
        const int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        for (int py = y0; py < y1; ++py) {
            const int sy = std::min(spr->height - 1, ((py - y) * spr->height) / h);
            const uint32_t* row = spr->pixels + sy * spr->width;
            for (int px = x0; px < x1; ++px) {
                const int sx = std::min(spr->width - 1, ((px - x) * spr->width) / w);
                const uint32_t c = row[sx];
                const int a = (((c >> 24) & 0xFF) * alphaScale) >> 8;
                if (a >= 255) pixels[py * SCREEN_W + px] = c;
                else if (a > 0) blendPixel(px, py, c, a);
            }
        }
    }

    // UTF-8 & Text Rendering
    static uint32_t nextUTF8(const char*& s) {
        unsigned char c = (unsigned char)*s;
        if (!c) return 0;
        s++;
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0) {
            unsigned char c2 = (unsigned char)*s; if (c2) s++;
            return ((c & 0x1F) << 6) | (c2 & 0x3F);
        }
        if ((c & 0xF0) == 0xE0) {
            unsigned char c2 = (unsigned char)*s; if (c2) s++;
            unsigned char c3 = (unsigned char)*s; if (c3) s++;
            return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        if ((c & 0xF8) == 0xF0) {
            unsigned char c2 = (unsigned char)*s; if (c2) s++;
            unsigned char c3 = (unsigned char)*s; if (c3) s++;
            unsigned char c4 = (unsigned char)*s; if (c4) s++;
            return ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
        }
        return '?';
    }

    static const BakedGlyph* findGlyph(const BakedFont& font, uint32_t cp) {
        int lo = 0, hi = font.glyphCount - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            uint32_t mcp = font.glyphs[mid].codepoint;
            if (mcp == cp) return &font.glyphs[mid];
            if (mcp < cp) lo = mid + 1;
            else hi = mid - 1;
        }
        if (cp != '?') return findGlyph(font, '?');
        return nullptr;
    }

    int textWidth(const BakedFont& font, const char* text) const {
        if (!text) return 0;
        const char* p = text;
        int w = 0;
        while (*p) {
            uint32_t cp = nextUTF8(p);
            const BakedGlyph* g = findGlyph(font, cp);
            if (g) w += g->advance;
        }
        return w;
    }

    void drawTextRaw(const BakedFont& font, int x, int y, const char* text, uint32_t col, int alpha = 255) {
        if (!text || alpha <= 0) return;
        const char* p = text;
        int curX = x;
        while (*p) {
            uint32_t cp = nextUTF8(p);
            const BakedGlyph* g = findGlyph(font, cp);
            if (!g) continue;
            if (g->width > 0 && g->height > 0) {
                int gx = curX + g->bearingX;
                int gy = y + g->bearingY;
                const uint8_t* bmp = font.bitmapData + g->dataOffset;
                for (int row = 0; row < g->height; ++row) {
                    int py = gy + row;
                    if ((unsigned)py >= (unsigned)SCREEN_H) continue;
                    if (clipActive && (py < clipY0 || py >= clipY1)) continue;
                    const uint8_t* rptr = bmp + row * g->width;
                    for (int colIdx = 0; colIdx < g->width; ++colIdx) {
                        uint8_t cov = rptr[colIdx];
                        if (cov <= 8) continue;
                        const int px = gx + colIdx;
                        if (clipActive && (px < clipX0 || px >= clipX1)) continue;
                        blendPixel(px, py, col, (cov * alpha) >> 8);
                    }
                }
            }
            curX += g->advance;
        }
    }

    void drawText(const BakedFont& font, int x, int y, const char* text, uint32_t col, bool shadow = true, int alpha = 255) {
        if (shadow) drawTextRaw(font, x + 2, y + 2, text, 0xFF040208, (alpha * 225) >> 8);
        drawTextRaw(font, x, y, text, col, alpha);
    }

    void drawTextCentered(const BakedFont& font, int cx, int y, const char* text, uint32_t col, bool shadow = true, int alpha = 255) {
        int w = textWidth(font, text);
        drawText(font, cx - w / 2, y, text, col, shadow, alpha);
    }

    void drawTextRight(const BakedFont& font, int rx, int y, const char* text, uint32_t col, bool shadow = true, int alpha = 255) {
        int w = textWidth(font, text);
        drawText(font, rx - w, y, text, col, shadow, alpha);
    }

    int drawTextWrapped(const BakedFont& font, int x, int y, int maxWidth, const char* text, uint32_t col, bool centered = false, int alpha = 255) {
        if (!text) return y;
        std::string str(text), word, line;
        int curY = y;
        auto flushLine = [&](const std::string& l, bool preserveBlank = false) {
            if (l.empty()) {
                if (preserveBlank) curY += font.lineHeight;
                return;
            }
            if (centered) drawTextCentered(font, x + maxWidth / 2, curY, l.c_str(), col, true, alpha);
            else drawText(font, x, curY, l.c_str(), col, true, alpha);
            curY += font.lineHeight;
        };
        for (size_t i = 0; i <= str.size(); ++i) {
            char c = (i < str.size()) ? str[i] : ' ';
            if (c == ' ' || c == '\n') {
                if (!word.empty()) {
                    std::string cand = line.empty() ? word : (line + " " + word);
                    if (textWidth(font, cand.c_str()) > maxWidth && !line.empty()) {
                        flushLine(line);
                        line = word;
                    } else line = cand;
                    word.clear();
                }
                if (c == '\n') { flushLine(line, true); line.clear(); }
            } else word.push_back(c);
        }
        if (!line.empty()) flushLine(line);
        return curY;
    }

    // Высота текста, свёрстанного с той же шириной, что и drawTextWrapped:
    // по ней считается прокрутка.
    int measureTextWrapped(const BakedFont& font, int maxWidth, const char* text) const {
        if (!text) return 0;
        std::string str(text), word, line;
        int height = 0;
        auto flushLine = [&](const std::string& l, bool preserveBlank = false) {
            if (l.empty()) {
                if (preserveBlank) height += font.lineHeight;
                return;
            }
            height += font.lineHeight;
        };
        for (size_t i = 0; i <= str.size(); ++i) {
            char c = (i < str.size()) ? str[i] : ' ';
            if (c == ' ' || c == '\n') {
                if (!word.empty()) {
                    std::string cand = line.empty() ? word : (line + " " + word);
                    if (textWidth(font, cand.c_str()) > maxWidth && !line.empty()) {
                        flushLine(line);
                        line = word;
                    } else line = cand;
                    word.clear();
                }
                if (c == '\n') { flushLine(line, true); line.clear(); }
            } else word.push_back(c);
        }
        if (!line.empty()) flushLine(line);
        return height;
    }

    void drawShadow(int cx, int cy, int rx, int ry) {
        if (rx <= 0 || ry <= 0) return;
        const int y0 = std::max(0, cy - ry), y1 = std::min(SCREEN_H - 1, cy + ry);
        for (int py = y0; py <= y1; ++py) {
            const float dy = (float)(py - cy) / (float)ry;
            const float dy2 = dy * dy;
            const float remaining = 1.0f - dy2;
            if (remaining < 0.0f) continue;
            int extent = (int)(rx * sqrtf(remaining));
            // Match the former normalized-ellipse pixel test exactly at float boundaries.
            while (extent < rx) {
                const float nextDx = (float)(extent + 1) / (float)rx;
                if (nextDx * nextDx + dy2 > 1.0f) break;
                ++extent;
            }
            while (extent > 0) {
                const float dx = (float)extent / (float)rx;
                if (dx * dx + dy2 <= 1.0f) break;
                --extent;
            }
            blendSpan(py, cx - extent, cx + extent, 0xFF040208, 135);
        }
    }

    // ── Общий свет фигур ──────────────────────────────────────────────────────
    // Одно направление света на весь бестиарий: сверху-слева. Раньше у каждого
    // стража блик стоял там, где его нарисовали: у Моргара сверху, у Малакора
    // справа, у Протоколиста вообще снизу — шесть стражей выглядели как шесть
    // разных игр. Направление задаётся здесь один раз и одинаково для всех.
    static constexpr int FIG_LIGHT_DX = -1;
    static constexpr int FIG_LIGHT_DY = -1;
    // Толщины и силы блика, тени и стыка переехали в FigMaterial: с 2.10.32 они
    // зависят от материала, и одной константы на всех больше нет.
    // Выключатель нужен стендам: свет сравнивается «до и после» на одном кадре,
    // иначе цифры смешивают разные палитры стражей.
    static inline bool figureVolumeEnabled = true;
    static inline float figureVolumeGradient = 1.0f;

    // ── Пружина позы (аудит §8, P2 №7–8) ─────────────────────────────────────
    // Поза стража раньше переключалась мгновенно: пришёл замах — фигура в тот же
    // кадр встала в позу, кончился выброс — в тот же кадр вернулась. В анимации
    // это называется отсутствием follow-through: движение обрывается, а не
    // догоняет. Теперь тело идёт за позой пружиной с небольшим перелётом:
    // замах слегка отстаёт, а после выброса фигура ещё пару кадров проседает.
    //
    // Коэффициент демпфирования меньше критического, поэтому сигнал реально
    // перелетает цель (до ~15 %), а не просто плавно к ней подходит.
    struct BossSpring {
        float pose = 0.0f, poseV = 0.0f;
        float burst = 0.0f, burstV = 0.0f;
        float lastT = -1.0f;
    };
    BossSpring bossSpring[6];
    static inline bool bossSpringEnabled = true;
    // Для стендов: последние сглаженные значения, которые получила фигура.
    static inline float lastBossPose = 0.0f, lastBossBurst = 0.0f;
    static inline void springStep(float& x, float& v, float target, float dt, float omega, float zeta) {
        const float a = omega * omega * (target - x) - 2.0f * zeta * omega * v;
        v += a * dt;
        x += v * dt;
    }

    // ── Материалы (аудит §8, P1 №4–6) ────────────────────────────────────────
    // Раньше проход объёма применял один приём ко всем шести стражам: металл,
    // ткань, камень и стекло получали одинаковый блик и одинаковую тень, и вся
    // линейка выглядела отлитой из одного материала. Теперь у материала своя
    // рампа: металл — узкий резкий блик и короткая жёсткая тень, ткань и плоть —
    // широкий мягкий переход, стекло — яркий край почти без тени, пергамент и
    // кость блика не получают вовсе.
    struct FigMaterial {
        int rimDepth, coreDepth;      // толщина блика и тени, px
        float rimK, coreK;            // сила блика и тени на силуэте
        float internalK;              // блик на стыке деталей (тень стыка — отдельно)
        float rampLight, rampDark;    // тональная рампа: свет и тень по корпусу
    };
    // Тип стража: 0 металл, 1 плоть/плесень, 2 ткань, 3–4 камень, 5 стекло.
    static FigMaterial figMaterial(int bossId) {
        switch (bossId) {
            case 0:  return {2, 3, 0.62f, 0.44f, 0.30f, 0.20f, 0.26f};
            case 1:  return {4, 5, 0.34f, 0.26f, 0.00f, 0.11f, 0.16f};
            case 2:  return {4, 5, 0.30f, 0.24f, 0.00f, 0.10f, 0.16f};
            case 3:  return {3, 4, 0.44f, 0.32f, 0.28f, 0.15f, 0.20f};
            case 4:  return {3, 4, 0.40f, 0.30f, 0.28f, 0.14f, 0.20f};
            default: return {2, 2, 0.70f, 0.10f, 0.24f, 0.26f, 0.06f};
        }
    }
    // Пергамент, страницы книг, кость, маска: блик на них читается как пластик.
    static FigMaterial figMaterialParchment() { return {3, 3, 0.10f, 0.12f, 0.00f, 0.06f, 0.10f}; }

    // Снимок области кадра ДО отрисовки фигуры. Ореол, знак на полу и осколки
    // уже нарисованы, поэтому в маску объёма они не попадут.
    void captureFigureRegion(int x0, int y0, int w, int h) {
        const int W = std::min(SCREEN_W, width), H = std::min(SCREEN_H, height);
        figureScratch.resize((size_t)w * h);
        for (int j = 0; j < h; ++j) {
            const int sy = y0 + j;
            for (int i = 0; i < w; ++i) {
                const int sx = x0 + i;
                figureScratch[(size_t)j * w + i] =
                    (sx >= 0 && sy >= 0 && sx < W && sy < H) ? pixels[(size_t)sy * W + sx] : 0u;
            }
        }
    }

    // Объём фигуры: край со стороны света светлеет, противоположный уходит в
    // тень, низ темнеет (AO от земли). В 3D это rim-свет, core shadow и AO;
    // здесь — один проход по маске, поэтому фигуру не нужно перерисовывать.
    // Границы берутся двух родов: силуэт (фигура против фона) и стык деталей.
    // Стык даёт вполовину более слабый эффект — иначе фигура выглядит не
    // объёмной, а тиснёной, с двойными контурами у каждой детали.
    void applyFigureVolume(int x0, int y0, int w, int h, float strength = 1.0f, float gradient = 0.0f,
                           int bodyCx = 0, int bodyCy = 0, int matId = -1) {
        if (strength <= 0.01f || w <= 0 || h <= 0) return;
        const int W = std::min(SCREEN_W, width), H = std::min(SCREEN_H, height);
        if ((int)figureScratch.size() != w * h) return;
        figureMask.resize((size_t)w * h);
        figureColor.resize((size_t)w * h);
        int bottom = -1, top = h, leftmost = w, rightmost = -1;
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const int sx = x0 + i, sy = y0 + j;
                if (sx < 0 || sy < 0 || sx >= W || sy >= H) { figureMask[(size_t)j * w + i] = 0; continue; }
                const uint32_t pix = pixels[(size_t)sy * W + sx];
                // Плотный ключ цвета: 5 бит на канал. Полный цвет детали не нужен,
                // нужно только «это та же деталь или соседняя», а 32-битная копия
                // области стоила дороже самого прохода.
                figureColor[(size_t)j * w + i] = (uint16_t)((((pix >> 19) & 0x1F) << 10) |
                                                           (((pix >> 11) & 0x1F) << 5) |
                                                           ((pix >> 3) & 0x1F));
                const bool solid = pix != figureScratch[(size_t)j * w + i];
                figureMask[(size_t)j * w + i] = solid ? 1 : 0;
                if (solid) {
                    if (j > bottom) bottom = j;
                    if (j < top) top = j;
                    if (i < leftmost) leftmost = i;
                    if (i > rightmost) rightmost = i;
                }
            }
        }
        if (bottom < 0) return;
        const float k = std::max(0.0f, std::min(1.0f, strength));
        // Центр градиента — центр самой фигуры (cx, cy вызова), а не середина
        // рамки: у Моргара щит уводит рамку влево, и по рамке половина тела
        // попадала в «тень» независимо от позы.
        const float gc = (float)(bodyCx - x0), gr = (float)(bodyCy - y0);
        const float ghw = std::max(1.0f, std::max(std::fabs((float)leftmost - gc), std::fabs((float)rightmost - gc)));
        const float ghh = std::max(1.0f, std::max(std::fabs((float)top - gr), std::fabs((float)bottom - gr)));
        auto solidAt = [&](int i, int j) {
            if (i < 0 || j < 0 || i >= w || j >= h) return false;
            return figureMask[(size_t)j * w + i] != 0;
        };
        // Ищет ближайшую границу в заданном направлении. Возвращает расстояние и
        // говорит, силуэт это или стык деталей. У стыка деталей толщина ограничена
        // (internalMax): контактная тень — это кант в 2 px, а не размытое пятно.
        auto boundaryToward = [&](int i, int j, int dx, int dy, int depth, int internalMax, bool& silhouette) {
            silhouette = false;
            const int c0 = figureColor[(size_t)j * w + i];
            const int r0 = (c0 >> 10) & 0x1F, g0 = (c0 >> 5) & 0x1F, b0 = c0 & 0x1F;
            for (int d = 1; d <= depth; ++d) {
                const int ni = i + dx * d, nj = j + dy * d;
                if (!solidAt(ni, nj)) { silhouette = true; return d; }
                const int c1 = figureColor[(size_t)nj * w + ni];
                const int dr = std::abs(r0 - ((c1 >> 10) & 0x1F));
                const int dg = std::abs(g0 - ((c1 >> 5) & 0x1F));
                const int db = std::abs(b0 - (c1 & 0x1F));
                if (dr + dg + db > 4) return d <= internalMax ? d : 0;
            }
            return 0;
        };
        // Материал зависит только от типа стража (или от «это пергамент»),
        // поэтому таблица считается до цикла, а не на каждый пиксель фигуры.
        const FigMaterial matBase = figMaterial(matId);
        const FigMaterial matParch = figMaterialParchment();
        for (int j = 0; j <= bottom; ++j) {
            const uint8_t* maskRow = &figureMask[(size_t)j * w];
            const uint16_t* colorRow = &figureColor[(size_t)j * w];
            const int sy = y0 + j;
            if (sy < 0 || sy >= H) continue;
            uint32_t* pixRow = pixels + (size_t)sy * W;
            for (int i = 0; i < w; ++i) {
                if (!maskRow[i]) continue;
                const int sx = x0 + i;
                if (sx < 0 || sx >= W) continue;
                uint32_t c = pixRow[sx];
                // Материал по цвету детали: пергамент, страницы и кость узнаются
                // по высокой светлоте при низкой насыщенности, остальное — по типу
                // стража. Один и тот же приём для стали и для ткани — это и была
                // причина, по которой линейка читалась отлитой из одного металла.
                const uint16_t key = colorRow[i];
                const int r5 = (key >> 10) & 0x1F, g5 = (key >> 5) & 0x1F, b5 = key & 0x1F;
                const int lum8 = ((r5 * 77 + g5 * 150 + b5 * 29) >> 8) * 8;
                // Тёплое и светлое — пергамент, страницы, кость, маска. Камень
                // Протоколиста тоже светлый, но холодный (синего больше красного),
                // поэтому одним порогом насыщенности их не разделить.
                const int chroma = std::max(r5, std::max(g5, b5)) - std::min(r5, std::min(g5, b5));
                const bool parchment = (lum8 > 150) && (chroma <= 6) && (r5 > b5 + 2);
                const FigMaterial mat = parchment ? matParch : matBase;
                // Тональная рампа по материалу. Ступенчатую (три тона по осям
                // кадра) пробовал: на крупном теле она даёт прямой диагональный
                // шов через всю фигуру — у Азарота балахон делился на светлую и
                // тёмную половину ровной линией. Тона даёт не порог по координате,
                // а сама форма: блик у края, кольцо собственной тени внутри и
                // контактный кант на стыке деталей. Здесь — только сила рампи,
                // своя для каждого материала.
                if (gradient > 0.001f) {
                    const float nx = (i - gc) / ghw, ny = (j - gr) / ghh;
                    const float dot = (nx * FIG_LIGHT_DX + ny * FIG_LIGHT_DY) * 0.70710678f;
                    if (dot > 0.0f) c = lerpColor(c, 0xFFFFE8C0, dot * mat.rampLight * gradient * k);
                    else c = lerpColor(c, 0xFF0A0714, -dot * mat.rampDark * gradient * k);
                }
                bool rimSil = false, coreSil = false;
                const int rimD = boundaryToward(i, j, FIG_LIGHT_DX, FIG_LIGHT_DY, mat.rimDepth, 2, rimSil);
                const int coreD = rimD ? 0
                    : boundaryToward(i, j, -FIG_LIGHT_DX, -FIG_LIGHT_DY, mat.coreDepth, 2, coreSil);
                if (rimD) {
                    const float edge = 1.0f - (rimD - 1) / (float)mat.rimDepth;
                    const float t = edge * mat.rimK * (rimSil ? 1.0f : mat.internalK) * k;
                    c = lerpColor(c, 0xFFFFF4D6, t);
                } else if (coreD) {
                    const float edge = 1.0f - (coreD - 1) / (float)mat.coreDepth;
                    // Контактная тень между деталями: кант в 2 px, в полтора раза
                    // сильнее обычной тени силуэта. Раньше стык шёл на 45 % от
                    // силуэта и детали сливались в одно пятно (средний контраст
                    // стыка 21 против 34 у плоской заливки).
                    const float boost = coreSil ? 1.0f : 1.5f;
                    const float t = edge * mat.coreK * boost * k;
                    c = lerpColor(c, 0xFF06040C, std::min(0.75f, t));
                } else {
                    // Кольцо собственной тени: полоса в 4–7 px внутри от тёмного
                    // края. Это третий тон рампы, и он идёт по форме, а не по
                    // координате кадра, поэтому читается как объём, а не как
                    // половина, залитая краской.
                    bool ringSil = false;
                    const int ringD = boundaryToward(i, j, -FIG_LIGHT_DX, -FIG_LIGHT_DY,
                                                    mat.coreDepth + 6, 0, ringSil);
                    if (ringD > mat.coreDepth) {
                        const float t = (1.0f - (ringD - mat.coreDepth) / 6.0f) * mat.coreK * 0.55f * k;
                        c = lerpColor(c, 0xFF0A0714, t);
                    }
                }
                // AO у земли: последние 14 px фигуры темнеют — фигура стоит на
                // полу, а не лежит на нём картинкой.
                const int aoBand = 14;
                if (bottom - j < aoBand) {
                    const float t = (1.0f - (bottom - j) / (float)aoBand) * 0.16f * k;
                    c = lerpColor(c, 0xFF07050E, t);
                }
                pixels[(size_t)sy * W + sx] = c;
            }
        }
    }

    // Низ фигуры в пикселях от cy при масштабе 1.12. Замерено стендом по кадрам
    // (design/char_model_audit.md, §6): у Азарота, Протоколиста и Двойника низ
    // на 22–34 px ниже общего cy+50, и они висели над собственной тенью.
    static int bossFootOffset(int id) {
        static const int t[6] = {60, 50, 52, 74, 84, 72};
        return t[std::max(0, std::min(5, id))];
    }
    // Ширина тени по ширине силуэта, а не одна на всех.
    static int bossShadowWidth(int id) {
        static const int t[6] = {34, 30, 32, 46, 44, 46};
        return t[std::max(0, std::min(5, id))];
    }

    void drawHeroSprite(int cx, int cy, int heroClass, float animTime, int facing, bool hurtFlash, bool dashing) {
        if (dashing) addGlow(cx, cy, 42, 0xFF40E8FF, 195);
        int frame = ((int)(animTime * 6.0f)) & 1;
        int fx = 0, fy = 0;
        bakedSpriteVisualOffset(HERO_SPRITES[heroClass % 5][frame], fx, fy);
        drawShadow(cx, cy + 20 - fy, 15, 6);
        drawBakedSpriteCentred(HERO_SPRITES[heroClass % 5][frame], cx, cy, facing < 0, hurtFlash, false, 255);
    }

    // A soft coloured wash over a baked sprite's opaque pixels: cheap, and it survives any
    // palette because it only tints what is already drawn.
    void drawBakedSpriteWash(const BakedSprite* spr, int cx, int cy, bool flipX, uint32_t tint, int alpha) {
        if (!spr || alpha <= 0) return;
        int dx = 0, dy = 0;
        bakedSpriteVisualOffset(spr, dx, dy);
        const int w = spr->width, h = spr->height;
        const int startX = cx - dx - w / 2;
        const int startY = cy - dy - h / 2;
        for (int sy = 0; sy < h; ++sy) {
            const int py = startY + sy;
            if ((unsigned)py >= (unsigned)SCREEN_H) continue;
            const uint32_t* row = spr->pixels + sy * w;
            for (int sx = 0; sx < w; ++sx) {
                const int px = startX + (flipX ? (w - 1 - sx) : sx);
                if ((unsigned)px >= (unsigned)SCREEN_W) continue;
                const int a = (((row[sx] >> 24) & 0xFF) * alpha) >> 8;
                if (a > 0) blendPixel(px, py, tint, a);
            }
        }
    }

    // Every family carries itself differently: imps jitter, skeletons march, slimes hop,
    // wraiths hover, brutes plant their feet. The silhouette alone is not the identity —
    // the way it moves is, and that is what stops a swarm reading as one repeated blob.
    struct MonsterMotion {
        float bobAmp;      // vertical breathing in px
        float bobFreq;     // cycles per second
        float swayAmp;     // horizontal sway in px
        float swayFreq;
        float hoverY;      // constant lift (flyers)
        float fps;         // animation frames per second
        int shadowW, shadowH;
        float squash;      // squash-and-stretch strength (0 = rigid)
    };
    static const MonsterMotion& monsterMotion(int type) {
        static const MonsterMotion profiles[12] = {
            {2.0f, 9.0f, 1.2f, 3.2f, 0.0f, 7.5f, 11, 4, 0.05f}, // 0 — imp: nervous, twitchy
            {1.0f, 5.0f, 0.0f, 1.0f, 0.0f, 5.0f, 12, 5, 0.00f},  // 1 — skeleton: steady march
            {3.2f, 6.5f, 0.0f, 1.0f, 0.0f, 6.5f, 13, 5, 0.14f},  // 2 — slime: hop and squash
            {1.4f, 2.4f, 1.6f, 1.1f, -2.0f, 4.2f, 11, 4, 0.04f}, // 3 — cultist: glides, robes sway
            {1.0f, 3.0f, 0.6f, 1.0f, 0.0f, 4.0f, 16, 6, 0.05f},  // 4 — brute: heavy, planted
            {2.2f, 2.0f, 2.0f, 0.9f, -7.0f, 3.6f, 12, 4, 0.07f}, // 5 — wraith: floats above the floor
            {1.2f, 4.0f, 0.8f, 1.4f, 0.0f, 4.6f, 13, 5, 0.03f},  // 6 — warden
            {0.8f, 3.4f, 0.0f, 1.0f, 0.0f, 3.8f, 15, 6, 0.02f},  // 7 — knight: barely bobs
            {1.3f, 5.2f, 1.0f, 2.2f, 0.0f, 5.4f, 12, 5, 0.04f},  // 8 — archer: light on its feet
            {2.0f, 2.8f, 2.4f, 1.2f, 0.0f, 3.4f, 18, 7, 0.09f},  // 9 — ogre: heavy stomp
            {1.6f, 4.2f, 0.0f, 1.0f, 0.0f, 4.4f, 15, 6, 0.00f},  // 10 — matryoshka: rigid, doll-like
            {0.9f, 16.0f, 0.0f, 1.0f, 0.0f, 14.0f, 9, 3, 0.06f}, // 11 — rat: scurries
        };
        return profiles[std::max(0, std::min(11, type))];
    }

    // The rat is rasterised in its own local space and then rotated onto the screen, so a
    // swarm reads as animals running in a direction instead of a row of stickers. It is
    // deliberately small: a rat the size of a dog stops being a rat.
    void drawRatSprite(int cx, int cy, float angle, float animTime, bool hitFlash, bool frozen) {
        const uint32_t fur      = frozen ? 0xFF7FE4FF : (hitFlash ? 0xFFFFF0C0 : 0xFF8B7A6A);
        const uint32_t furDark  = frozen ? 0xFF4FB8D8 : (hitFlash ? 0xFFFFD890 : 0xFF5A4E46);
        const uint32_t snoutCol = frozen ? 0xFFBFF2FF : 0xFFBFB0A0;
        const uint32_t eyeCol   = frozen ? 0xFFFFFFFF : 0xFFFF5A4A;
        const float sway = sinf(animTime * 15.0f);

        const int LW = 40, LH = 24;
        uint32_t local[LW * LH] = {};
        auto plot = [&](int x, int y, uint32_t c) {
            if (x < 0 || y < 0 || x >= LW || y >= LH) return;
            const int idx = y * LW + x;
            const int a = (int)((c >> 24) & 0xFF);
            if (a >= 250) { local[idx] = c; return; }
            if (a <= 0) return;
            const uint32_t d = local[idx];
            const int da = (int)((d >> 24) & 0xFF);
            if (da >= 250) return;
            local[idx] = c;
        };
        auto ellipse = [&](float ecx, float ecy, float rx, float ry, uint32_t c) {
            for (int y = (int)(ecy - ry) - 1; y <= (int)(ecy + ry) + 1; ++y)
                for (int x = (int)(ecx - rx) - 1; x <= (int)(ecx + rx) + 1; ++x) {
                    const float nx = (x - ecx) / std::max(0.6f, rx);
                    const float ny = (y - ecy) / std::max(0.6f, ry);
                    if (nx * nx + ny * ny <= 1.02f) plot(x, y, c);
                }
        };
        auto leg = [&](int x0, int y0, int x1, int y1) {
            const int steps = std::max(2, std::abs(x1 - x0) + std::abs(y1 - y0));
            for (int i = 0; i <= steps; ++i) {
                const float t = (float)i / steps;
                plot((int)std::lround(x0 + (x1 - x0) * t), (int)std::lround(y0 + (y1 - y0) * t), furDark);
            }
        };

        const int cyLocal = LH / 2;
        const int cxLocal = LW / 2 + 2; // the head sits right of centre: the rat faces +X
        // tail: thin, whipping behind the body
        for (int i = 0; i <= 12; ++i) {
            const float t = i / 12.0f;
            const int tx = (int)(cxLocal - 8 - t * 11);
            const int ty = (int)(cyLocal - t * 3 + powf(t, 2.0f) * 6 + sway * t * 2.2f);
            plot(tx, ty, furDark);
            if (i > 8) plot(tx, ty - 1, furDark);
        }
        // legs, alternating in two pairs
        leg(cxLocal - 5, cyLocal + 3, cxLocal - 6 - (int)(sway * 2), cyLocal + 7);
        leg(cxLocal - 1, cyLocal + 3, cxLocal - 2 + (int)(sway * 2), cyLocal + 7);
        leg(cxLocal + 4, cyLocal + 3, cxLocal + 3 + (int)(sway * 2), cyLocal + 7);
        leg(cxLocal + 7, cyLocal + 3, cxLocal + 8 - (int)(sway * 2), cyLocal + 7);
        // body, head, snout
        ellipse(cxLocal - 3, cyLocal, 8.4f, 4.6f, fur);
        ellipse(cxLocal - 3, cyLocal + 2, 8.0f, 2.6f, furDark);
        ellipse(cxLocal + 6, cyLocal - 0.5f, 4.6f, 3.8f, fur);
        ellipse(cxLocal + 10, cyLocal + 1.0f, 3.4f, 2.2f, snoutCol);
        // ears
        ellipse(cxLocal + 5, cyLocal - 4.5f, 2.1f, 2.1f, furDark);
        ellipse(cxLocal + 8, cyLocal - 4.0f, 2.1f, 2.1f, fur);
        // eye
        ellipse(cxLocal + 8, cyLocal - 0.5f, 1.4f, 1.4f, eyeCol);
        // A one-pixel dark edge keeps the silhouette readable at swarm size.
        const uint32_t outline = frozen ? 0xFF1B4A5A : 0xFF241C18;
        uint32_t ring[LW * LH];
        memcpy(ring, local, sizeof(ring));
        for (int y = 1; y < LH - 1; ++y)
            for (int x = 1; x < LW - 1; ++x) {
                if ((local[y * LW + x] >> 24) != 0) continue;
                if ((local[y * LW + x - 1] >> 24) == 0 && (local[y * LW + x + 1] >> 24) == 0 &&
                    (local[(y - 1) * LW + x] >> 24) == 0 && (local[(y + 1) * LW + x] >> 24) == 0) continue;
                ring[y * LW + x] = outline;
            }
        memcpy(local, ring, sizeof(local));

        // Rotate onto the screen. Scale keeps the rat small enough to read as a swarm.
        const float scale = 0.80f;
        const float c = cosf(angle), s = sinf(angle);
        const int reach = (int)(LW * scale * 0.5f) + 3;
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                // inverse rotation
                const float lx = (dx * c + dy * s) / scale;
                const float ly = (-dx * s + dy * c) / scale;
                const int ix = (int)std::lround(lx) + LW / 2;
                const int iy = (int)std::lround(ly) + LH / 2;
                if (ix < 0 || iy < 0 || ix >= LW || iy >= LH) continue;
                const uint32_t col = local[iy * LW + ix];
                if ((col >> 24) == 0) continue;
                putPixel(cx + dx, cy + dy, col);
            }
        }
    }

    void drawMonsterSprite(int cx, int cy, int type, float animTime, bool hitFlash, bool isElite, bool frozen = false, bool flipX = false, float heading = 0.0f) {
        const MonsterMotion& motion = monsterMotion(type);
        if (type == 11) {
            const int bob = (int)(sinf(animTime * 16.0f) * motion.bobAmp * 0.5f);
            drawShadow(cx, cy + 8, motion.shadowW, motion.shadowH);
            drawRatSprite(cx, cy + bob, heading, animTime, hitFlash, frozen);
            return;
        }
        const int frame = ((int)(animTime * motion.fps + cx * 0.04f)) & 1;
        const BakedSprite* spr = MONSTER_SPRITES[type % 11][frame];
        const int bob = (int)(sinf(animTime * motion.bobFreq * 6.2831853f) * motion.bobAmp);
        const int sway = (int)(sinf(animTime * motion.swayFreq * 6.2831853f) * motion.swayAmp);
        cx += sway;
        cy += bob + (int)motion.hoverY;
        if (isElite) {
            // Elite / extra mobs: a breathing amber halo under the silhouette plus a light gold
            // wash on the body, so they read as "different" at a glance in a full-screen swarm.
            const float pulse = 0.5f + 0.5f * sinf(animTime * 4.2f);
            addGlow(cx, cy, 28 + (int)(6.0f * pulse), 0xFFFFA83A, 96);
            drawCircleRing(cx, cy + 14, 18, 2, 0xFFFFC028, 190, true);
        }
        int fx = 0, fy = 0;
        bakedSpriteVisualOffset(spr, fx, fy);
        drawShadow(cx, cy + 16 - fy, motion.shadowW, motion.shadowH);
        if (motion.squash > 0.001f && !hitFlash) {
            // Squash on the fall, stretch on the rise — volume-preserving, so it never pops.
            const float beat = sinf(animTime * motion.bobFreq * 6.2831853f);
            drawBakedSpriteStretched(spr, cx, cy, 1.0f + motion.squash * beat, 1.0f - motion.squash * beat,
                                     flipX, hitFlash, frozen, 255);
        } else {
            drawBakedSpriteCentred(spr, cx, cy, flipX, hitFlash, frozen, 255);
        }
        if (isElite && !hitFlash) {
            const float pulse = 0.5f + 0.5f * sinf(animTime * 4.2f);
            drawBakedSpriteWash(spr, cx, cy, flipX, 0xFFFFC64A, 78 + (int)(26.0f * pulse));
        }
    }

    // Four guardians, four silhouettes. They are drawn from primitives instead of sharing
    // one baked sprite family, because "same shape, different tint" reads as a placeholder.
    // echo == true renders the trio copy: same shape, spectral and translucent.
    // castPose: 0..1 — насколько замах уже сделан (колба поднята над головой),
    // castBurst: 0..1 — удар/выброс уже случился (колба выброшена вперёд).
    // mirror: страж повёрнут влево — фигура рисуется зеркально, к герою.
    // recoil: 0..1 — отдача от попадания: фигура приседает и сжимается.
    void drawBossFigure(int cx, int cy, int bossType, float animTime, bool hitFlash, bool enraged, bool echo,
                        float s = 1.0f, float castPose = 0.0f, float castBurst = 0.0f,
                        bool mirror = false, float recoil = 0.0f, float fade = 1.0f) {
        const int id = bossType % 6;
        const uint32_t flash = 0xFFFFF0C8;
        const float breath = 0.5f + 0.5f * sinf(animTime * 2.2f);
        const int lift = (int)(sinf(animTime * 1.6f) * 2.0f);
        auto tint = [&](uint32_t c) { return hitFlash ? lerpColor(c, flash, 0.7f) : c; };
        const int alpha = (int)((echo ? 205 : 255) * std::max(0.0f, std::min(1.0f, fade)));

        // Everything below is written in one canonical 1:1 size; the helpers rescale it so
        // the same figure works in the arena and on the campaign dossier card.
        //
        // Зеркало делается здесь, в переводе координат: вся фигура нарисована
        // в одной системе, поэтому достаточно отразить смещение от центра — и
        // страж поворачивается к герою, а не идёт спиной вперёд. Раньше стражи
        // не разворачивались вообще: любой из них смотрел вправо при любом
        // движении по арене.
        // Squash & stretch (P2 №8). Раньше от удара фигура сжималась целиком —
        // одинаково по обеим осям, и попадание читалось как «страж стал меньше».
        // Теперь она приседает: по вертикали ужимается сильнее, чем по
        // горизонтали, поэтому объём сохраняется и удар «вдавливает» стража в пол.
        const float scX = s * (1.0f + recoil * 0.055f);
        const float scY = s * (1.0f - recoil * 0.075f);
        const float sc = scX;  // радиусы, толщины линий и ширина прямоугольников
        const int cyAdj = cy + (int)(recoil * 3.0f);
        // Выпад. Раньше удар стража читался только по одной детали — древку, книге,
        // перу: корпус стоял на месте, будто статуя махнула рукой. Теперь на замахе
        // страж отклоняется назад, а на выбросе подаётся всем телом к герою.
        const float cpLean = std::max(0.0f, std::min(1.2f, castPose));
        const float cbLean = std::max(0.0f, std::min(1.2f, castBurst));
        const int lunge = (int)lrintf((cbLean * 13.0f - cpLean * 6.0f) * sc) * (mirror ? -1 : 1);
        cx += lunge;
        // ── Задержка периферии (P2 №7) ───────────────────────────────────────
        // Раньше корпус и все свисающие детали считались от одного animTime, и
        // фигура двигалась монолитом, как картонная вырезка. В жизни периферия
        // догоняет корпус: кисть отстаёт от плеча, подол — от пояса, кончик пера —
        // от кисти. Здесь это сдвиг фазы: деталь считает синус от (animTime - lag),
        // и чем дальше она от центра фигуры, тем больше lag.
        // Величины подобраны по читаемости, а не по физике: настоящая тряпка
        // отстаёт на 1–3 кадра (17–50 мс), и на 100-пиксельной фигуре это не
        // видно вовсе. 60–160 мс — это 4–10 кадров, уже заметный «хвост».
        const float T_HAND = animTime - 0.060f;   // кисти и оружие в руках
        const float T_CLOAK = animTime - 0.110f;  // полы, ленты, страницы
        const float T_HEM = animTime - 0.160f;    // подол, шлейф, длинные перья
        // ── Походка (P2 №9) ─────────────────────────────────────────────────
        // Корпус проседает в момент шага: ноги разошлись — вес ушёл вниз, ноги
        // сошлись — корпус подбросило. Раньше корпус ходил ровно, и страж
        // «скользил» над полом.
        // Периферия ходит не «своей волной рядом с корпусом», а запаздывающим
        // корпусом: тот же сигнал наклона, но снятый на lag раньше. Разница
        // между корпусом и подолом — это и есть видимый хвост: корпус уже пошёл
        // влево, а подол ещё висит справа.
        auto swayLag = [&](float freq, float amp, float lag) {
            return (int)lrintf(sinf((animTime - lag) * freq) * amp * sc);
        };
        auto footfall = [&](float freq) {
            return (int)lrintf(fabsf(sinf(animTime * freq)) * 3.0f * sc);
        };
        // ── LOD (P2 №10) ─────────────────────────────────────────────────────
        // Каноническая фигура — около 120 px в высоту. Ниже 60 px (s < 0.5)
        // частицы в 1–2 px, заклёпки, звенья цепи и трещины перестают читаться и
        // превращают силуэт в грязь: SS() отдаёт им минимум 1 px, и точка пары
        // размером с глаз перекрывает сам глаз. Поэтому мелочь рисуется только
        // на крупной фигуре, а силуэт и основные плоскости — всегда.
        const bool lodSmall = (s * 120.0f) < 60.0f;
        auto MO = [&](int ax) { return mirror ? (cx - ax) : (ax - cx); };
        auto PX = [&](int ax) { return cx + (int)lrintf(MO(ax) * sc); };
        auto PY = [&](int ay) { return cyAdj + (int)lrintf((ay - cy) * scY); };
        auto SS = [&](int v) { return std::max(1, (int)lrintf(v * sc)); };
        // Для прямоугольников в зеркале берём правый край как левый, иначе
        // деталь съезжает на свою же ширину.
        auto RX = [&](int x, int w) { return cx + (int)lrintf((MO(x) - (mirror ? w : 0)) * sc); };
        auto FR = [&](int x, int y, int w, int h, int rad, uint32_t c, int a = 255) {
            fillRoundedRect(RX(x, w), PY(y), SS(w), SS(h), std::max(1, (int)lrintf(rad * sc)), c, a);
        };
        auto FC = [&](int x, int y, int r, uint32_t c, int a = 255) { fillCircle(PX(x), PY(y), SS(r), c, a); };
        auto FX = [&](int x, int y, int w, int h, uint32_t c, int a = 255) { fillRect(RX(x, w), PY(y), SS(w), SS(h), c, a); };
        auto DL = [&](int x0, int y0, int x1, int y1, uint32_t c, int th = 1, int a = 255, bool add = false) {
            // Порядок аргументов у drawLine: (col, thickness, alpha, additive).
            // Раньше здесь цвет и толщина стояли наоборот: в цвет попадало число
            // толщины (почти чёрный 0x0000000A без альфы), а в толщину — упакованный
            // цвет, который как int отрицателен, поэтому rad <= 0 и линия вырождалась
            // в цепочку одиночных пикселей. Из-за этого все DL-детали фигур (руки,
            // посохи, перья) были невидимы, а силуэт читался обрубком.
            drawLine(PX(x0), PY(y0), PX(x1), PY(y1), c, std::max(1, (int)lrintf(th * sc)), a, add);
        };
        auto CR = [&](int x, int y, int r, int th, uint32_t c, int a = 255, bool add = false) {
            drawCircleRing(PX(x), PY(y), SS(r), std::max(1, (int)lrintf(th * sc)), c, a, add);
        };

        if (id == 0) {
            // МОРГАР — воротный страж: приземистый, широкий, щит-башня и алебарда с ключом.
            //
            // Раньше он только «дышал» на 2 px: копьё, щит и ноги стояли мёртво, и
            // страж читался как картинка. Теперь переступает, качает алебарду, щит
            // ходит вместе с дыханием, а на замах приседает и поднимает древко.
            const uint32_t steel = tint(0xFF7A6A48), steelLit = tint(0xFFB49B62), steelDark = tint(0xFF3E3423);
            const uint32_t ember = tint(enraged ? 0xFFFF6A2A : 0xFFFFAE3C);
            const float cp = std::max(0.0f, std::min(1.2f, castPose));
            const float cb = std::max(0.0f, std::min(1.2f, castBurst));
            // Просадка корпуса на шаге: тяжесть уходит вниз, когда ноги разошлись.
            const int gaitDip = footfall(3.4f);
            // Приседает на замахе и выпрямляется на ударе.
            const int bodyY = (int)(cp * 5.0f) - (int)(cb * 3.0f) + gaitDip;
            const int step = (int)(sinf(animTime * 3.4f) * 2.0f);
            // Вес: корпус переносит тяжесть с ноги на ногу, медленнее шага. Без
            // этого многотонная фигура «скользила» ровно, будто её несут.
            const int weight = (int)(sinf(animTime * 1.7f) * 2.0f);
            // Древко: в покое качается, на замахе уходит вверх и назад.
            const int shaftX = cx + 30 + (int)(sinf(T_HAND * 1.7f) * 2.0f) - (int)(cp * 8.0f) + (int)(cb * 16.0f);
            const int shaftTopY = cy - 62 - (int)(cp * 16.0f) + (int)(cb * 20.0f);
            DL(shaftX, shaftTopY, cx + 22, cy + 46, steelDark, 5, alpha);
            DL(shaftX, shaftTopY, cx + 22, cy + 46, steel, 3, alpha);
            // key-head of the halberd
            FR(shaftX - 10, shaftTopY - 12, 20, 7, 3, steel, alpha);
            FR(shaftX, shaftTopY - 16, 7, 14, 3, steelLit, alpha);
            FR(shaftX - 4, shaftTopY - 2, 6, 6, 2, steel, alpha);
            // legs: переступает, стопы расходятся
            FR(cx - 26 + step, cy + 18, 18, 34, 5, steelDark, alpha);
            FR(cx + 6 - step, cy + 18, 18, 34, 5, steelDark, alpha);
            FR(cx - 28 + step, cy + 46, 22, 8, 3, steel, alpha);
            FR(cx + 4 - step, cy + 46, 22, 8, 3, steel, alpha);
            // torso
            FR(cx - 24 + weight, cy - 24 - lift + bodyY, 48, 50, 12, steel, alpha);
            FR(cx - 18 + weight, cy - 18 - lift + bodyY, 36, 22, 8, steelLit, alpha);
            // chest ember slit: разгорается на замахе, в покое пульсирует
            const float emberPulse = 0.72f + 0.16f * sinf(animTime * 2.9f) + 0.12f * cp;
            FR(cx - 9 + weight, cy - 6 - lift + bodyY, 18, 7, 3, ember, (int)(alpha * emberPulse));
            // pauldrons
            FR(cx - 36 + weight, cy - 30 - lift + bodyY, 18, 20, 8, steelLit, alpha);
            FR(cx + 18 + weight, cy - 30 - lift + bodyY, 18, 20, 8, steelLit, alpha);
            // Голова и гребень. Шею с просветом пробовал дважды: на приземистом
            // торсе она читалась то тонкой палкой, то горлышком бутылки, а рога по
            // краям шлема — висящими в воздухе точками. Для стража-башни сплошная
            // масса — намеренная часть образа (квадрат = надёжность), поэтому голова
            // остаётся на плечах, а верхнюю линию ломает гребень, вросший в шлем.
            // Он же даёт опознание в миниатюре: единственная ступенька на макушке.
            FR(cx - 13 + weight, cy - 47 - lift + bodyY, 26, 24, 8, steel, alpha);
            FR(cx - 9 + weight, cy - 39 - lift + bodyY, 18, 4, 2, ember, alpha);
            FR(cx - 5 + weight, cy - 51 - lift + bodyY, 14, 5, 2, steel, alpha);
            FR(cx + 1 + weight, cy - 55 - lift + bodyY, 9, 6, 2, steelLit, alpha);
            FR(cx + 5 + weight, cy - 58 - lift + bodyY, 4, 5, 1, steelLit, alpha);
            // Искры из щели в груди: поднимаются и гаснут. Раньше страж был
            // единственным, у кого не было ни одной живой частицы.
            for (int i = 0; i < 5 && !lodSmall; ++i) {
                const float t = fmodf(animTime * 0.7f + i * 0.2f, 1.0f);
                const int ex = cx + weight + (int)(sinf(t * 6.0f + i) * 7.0f);
                const int ey = cy - 2 - lift + bodyY - (int)(t * 40.0f);
                FC(ex, ey, 1 + (i % 2), ember, (int)(170 * (1.0f - t)));
            }
            // tower shield: в покое чуть качается, на замахе поднимается к груди
            const int shieldX = cx - 44 + weight + (int)(sinf(T_HAND * 1.5f) * 1.0f) + (int)(cp * 6.0f);
            const int shieldY = cy - 22 - lift + bodyY - (int)(cp * 10.0f) + (int)(sinf(T_HAND * 2.2f) * 1.0f);
            FR(shieldX, shieldY, 30, 58, 8, steelDark, alpha);
            FR(shieldX + 4, shieldY + 4, 22, 50, 6, steel, alpha);
            FR(shieldX + 8, shieldY + 10, 14, 38, 5, steelLit, alpha);
            FR(shieldX + 11, shieldY + 24, 8, 10, 3, ember, alpha);
            // rivets
            for (int i = 0; i < 3 && !lodSmall; ++i) FC(shieldX + 7, shieldY + 10 + i * 16, 2, steelDark, alpha);
        } else if (id == 1) {
            // МАЛАКОР — чумной алхимик. Читается по трём вещам: клюв-респиратор,
            // горб над плечами и колба в вытянутой руке. Сгорбленный силуэт шире
            // книзу, поэтому фигура не выглядит мешком.
            //
            // Живость даёт цикл: переступает с ноги на ногу, капюшон качается,
            // подол хлещет тонкими прядями, в колбе кипит и поднимаются пары,
            // глаза горят неровно. На замах он поднимает колбу над головой,
            // при выбросе — швыряет её вперёд.
            const uint32_t robe = tint(0xFF35502A), robeLit = tint(0xFF6FA04A), robeDark = tint(0xFF141F0F);
            const uint32_t glass = tint(0xFFA8F07C), plague = tint(enraged ? 0xFFC6FF52 : 0xFF7CE04E);
            const float poseCast = std::max(0.0f, std::min(1.2f, castPose));
            const float poseBurst = std::max(0.0f, std::min(1.2f, castBurst));
            const float breathe = 0.5f + 0.5f * sinf(animTime * 2.3f);
            const int pace = (int)(sinf(animTime * 3.3f) * 3.0f);
            const int crouch = (int)(breathe * 2.0f) + (int)(poseCast * 4.0f) - (int)(poseBurst * 3.0f);
            const int torsoY = -lift - crouch + footfall(3.3f);
            const int sway = pace;
            const int lean = (int)(poseCast * 4.0f);
            // Стопы выглядывают из-под подола и переступают: видно, что он идёт,
            // а не скользит.
            const int stepL = (int)(sinf(animTime * 3.3f) * 3.0f);
            FR(cx - 16 + stepL, cy + 34 + torsoY, 12, 12, 4, robeDark, alpha);
            FR(cx + 4 - stepL, cy + 34 + torsoY, 12, 12, 4, robeDark, alpha);
            FR(cx - 18 + stepL, cy + 43 + torsoY, 15, 4, 2, robe, alpha);
            FR(cx + 3 - stepL, cy + 43 + torsoY, 15, 4, 2, robe, alpha);
            // Мантия тремя поясами: узкая грудь, пояс, широкий подол.
            FR(cx - 18 + sway, cy - 24 + torsoY, 36, 28, 10, robe, alpha);
            FR(cx - 23 + sway, cy - 2 + torsoY, 46, 24, 10, robe, alpha);
            FR(cx - 26 + sway, cy + 14 + torsoY, 52, 18, 9, robe, alpha);
            // Рваный край подола — плотные пряди, а не тонкие штрихи: раньше две
            // линии на тёмном фоне читались как царапины.
            const int swayHem = swayLag(3.3f, 3.0f, 0.16f);
            for (int i = -3; i <= 3; ++i) {
                // Волна идёт от пояса к краю: крайние пряди отстают сильнее внутренних.
                const int hemWave = (int)(sinf(T_HEM * 3.6f + i * 0.9f - std::abs(i) * 0.45f) * 2.0f);
                const int strandH = 4 + ((i + 3) % 3) * 3;
                FR(cx + i * 7 + swayHem + hemWave, cy + 30 + torsoY, 6, strandH, 2, robe, alpha);
            }
            FX(cx - 26 + sway, cy + 30 + torsoY, 52, 2, robeLit, 150);
            // Пояс и пара склянок на нём: видно, что перед нами алхимик.
            FR(cx - 21 + sway, cy + 12 + torsoY, 42, 4, 2, robeLit, alpha);
            for (int v = -1; v <= 1; v += 2) {
                FR(cx + v * 12 + sway, cy + 16 + torsoY, 5, 8, 2, robe, alpha);
                FR(cx + v * 12 + sway, cy + 16 + torsoY, 5, 2, 1, glass, alpha);
            }
            // Плечи, горб и капюшон. На замахе капюшон наклоняется вперёд.
            const int headX = cx + lean + sway;
            FR(cx - 19 + sway, cy - 32 + torsoY, 38, 14, 7, robeLit, alpha);   // воротник
            FR(cx - 16 + sway, cy - 38 + torsoY, 34, 14, 8, robe, alpha);      // горб
            FR(headX - 17, cy - 56 + torsoY, 34, 24, 11, robe, alpha);
            FR(headX - 17, cy - 56 + torsoY, 34, 4, 2, robeLit, alpha);
            // Остроконечный верх капюшона: залитый конус из сужающихся полос.
            // Две тонкие линии читались как отдельный треугольник над головой.
            for (int k = 0; k < 10; ++k) {
                const int tipW = std::max(2, 22 - k * 2);
                // Кончик капюшона отстаёт тем сильнее, чем выше ряд: колпак
                // не приклеен к голове, а запаздывает за ней.
                const int tipTrail = (int)lrintf((float)k * 0.45f * sinf(T_HEM * 2.4f));
                const int tipX = headX - tipW / 2 + (k * 9) / 10 + tipTrail;
                FR(tipX, cy - 58 - k + torsoY, tipW, 2, 1, k < 2 ? robeLit : robe, alpha);
            }
            // Тёмное лицо и клюв-респиратор.
            FR(headX - 12, cy - 50 + torsoY, 24, 16, 6, 0xFF0B1206, alpha);
            FR(headX - 8, cy - 40 + torsoY, 16, 9, 3, robeLit, alpha);      // пластина маски
            DL(headX + 4, cy - 37 + torsoY, headX + 24, cy - 33 + torsoY, robeLit, 8, alpha);
            DL(headX + 5, cy - 37 + torsoY, headX + 22, cy - 34 + torsoY, glass, 4, alpha);
            FC(headX + 24, cy - 33 + torsoY, 3, plague, 190);
            // Линзы: обе смотрят вперёд, зрачки горят неровно.
            const int eyeGlow = (int)(170.0f + 60.0f * sinf(animTime * 4.1f)) - (int)(poseCast * 60.0f);
            FR(headX - 10, cy - 48 + torsoY, 8, 4, 1, glass, alpha);
            FR(headX + 2, cy - 48 + torsoY, 7, 4, 1, glass, alpha);
            FR(headX - 9, cy - 47 + torsoY, 4, 2, 1, plague, eyeGlow);
            FR(headX + 4, cy - 47 + torsoY, 4, 2, 1, plague, eyeGlow);
            // ── Колба ────────────────────────────────────────────────────────
            // В покое колба висит ниже и правее лица, чтобы ореол не резал глаза.
            const int idleX = cx + 40 + sway, idleY = cy + 14 - lift;
            // На замахе колба уходит над головой и чуть в сторону: ровно над лицом
            // она закрывала ему глаза.
            const int castX = headX + 19, castY = cy - 78 + (int)(poseCast * 3.0f) - lift;
            const int burstX = cx + 56, burstY = cy - 2 - lift;
            int flaskX = idleX, flaskY = idleY;
            if (poseCast > 0.0f) {
                flaskX = idleX + (int)((castX - idleX) * poseCast);
                flaskY = idleY + (int)((castY - idleY) * poseCast);
            }
            if (poseBurst > 0.0f) {
                flaskX = castX + (int)((burstX - castX) * poseBurst);
                flaskY = castY + (int)((burstY - castY) * poseBurst);
            }
            // Рука от плеча к колбе, всегда живая (чуть дрожит).
            const int shoulderX = cx + 16 + sway, shoulderY = cy - 14 + torsoY;
            const int handJitter = (int)(sinf(T_HAND * 7.0f) * 1.0f);
            DL(shoulderX, shoulderY, flaskX - 5 + handJitter, flaskY - 2, robeLit, 5, alpha);
            DL(shoulderX, shoulderY, flaskX - 5 + handJitter, flaskY - 2, robe, 3, alpha);
            // Ореол колбы: на замахе разгорается, после удара вспыхивает.
            const float halo = std::max(poseCast * 0.8f, poseBurst);
            if (halo > 0.01f) {
                CR(flaskX, flaskY, 13 + (int)(halo * 12.0f), 2, plague, (int)(90 + 120 * halo), true);
                CR(flaskX, flaskY, 19 + (int)(halo * 18.0f), 1, glass, (int)(60 + 80 * halo), true);
            }
            // Колба: стекло, ободок, светящееся содержимое с уровнем жидкости.
            // Стекло приглушённое: раньше оно было почти белым и колба читалась
            // как светящийся шар, а не как сосуд с отравой.
            FC(flaskX, flaskY, 10, robeLit, alpha);
            FC(flaskX, flaskY, 9, 0xFF5F9440, alpha);
            const int liquidTop = flaskY + 8 - (int)(5.0f + 2.0f * breathe);
            FC(flaskX, flaskY + 8, 6, plague, alpha);
            FX(flaskX - 5, liquidTop, 10, 2, plague, (int)(180 + 60 * breathe));
            FR(flaskX - 3, flaskY - 9, 6, 8, 3, 0xFF6FA04A, alpha);
            FR(flaskX - 8, flaskY - 15, 16, 4, 2, robeLit, alpha);
            // Кипение: пузыри идут вверх по колбе и лопаются у поверхности.
            for (int b = 0; b < 4 && !lodSmall; ++b) {
                const float rise = fmodf(T_CLOAK * 0.9f + b * 0.27f, 1.0f);
                const int by = flaskY + 10 - (int)(rise * 16.0f);
                const int bx = flaskX + (int)(sinf(T_CLOAK * 3.0f + b * 2.0f) * 5.0f);
                const int br = rise > 0.85f ? 0 : 1 + (b % 2);
                if (br > 0) FC(bx, by, br, plague, (int)(120 + 100 * (1.0f - rise)));
            }
            // Пары над колбой.
            for (int w = 0; w < 3 && !lodSmall; ++w) {
                const float up = fmodf(animTime * 0.6f + w * 0.33f, 1.0f);
                const int wx = flaskX + (int)(sinf(T_CLOAK * 2.2f + w) * 6.0f);
                const int wy = flaskY - 20 - (int)(up * 20.0f);
                FC(wx, wy, 2 + (w % 2), plague, (int)(80 * (1.0f - up)));
            }
            // Капли: в покое редкие, на замахе чаще, при выбросе — веером.
            const int drops = 3 + (int)(poseCast * 5.0f) + (int)(poseBurst * 6.0f);
            for (int i = 0; i < drops && !lodSmall; ++i) {
                const float phase = fmodf(animTime * (1.1f + poseBurst * 1.6f) + i * 0.19f, 1.0f);
                const int dxSpr = (int)((sinf(i * 2.7f) * 8.0f) * (1.0f + poseBurst * 2.2f));
                const int dySpr = (int)(phase * (26.0f + poseBurst * 22.0f));
                FC(flaskX + 4 + dxSpr, flaskY + 12 + dySpr, 2, plague, (int)(190 * (1.0f - phase * 0.7f)));
            }
        } else if (id == 2) {
            // КАРДИНАЛ — счётчик имён: высокий, в широкой шляпе, с книгой и пером.
            const uint32_t cloth = tint(0xFF3C1A28), clothLit = tint(0xFF6E2E44), clothDark = tint(0xFF1E0C14);
            const uint32_t paper = tint(0xFFE8DCC8), ink = tint(0xFFFF4E74);
            // Замах: он откидывается назад и поднимает перо, на выбросе — бросает
            // руку вперёд. Раньше фигура на замах не реагировала вовсе.
            const float cp = std::max(0.0f, std::min(1.2f, castPose));
            const float cb = std::max(0.0f, std::min(1.2f, castBurst));
            const int lean = -(int)(cp * 7.0f) + (int)(cb * 5.0f);
            // Корпус качается в такт шагу: раньше он не двигался боком вовсе и
            // фигура стояла истуканом, пока ноги переступали.
            const int gaitSway = (int)lrintf(sinf(animTime * 2.6f) * 2.0f * sc);
            const int gaitDip = footfall(2.6f);
            const int bodyY = -(int)(cp * 4.0f) + (int)(cb * 2.0f);
            // long robe, narrow silhouette
            FR(cx - 20 + lean + gaitSway, cy - 16 - lift + bodyY + gaitDip, 40, 62, 14, cloth, alpha);
            FR(cx - 14 + lean + gaitSway, cy - 4 - lift + bodyY + gaitDip, 28, 48, 12, clothDark, alpha);
            const int swayHem = swayLag(2.6f, 2.0f, 0.16f);
            for (int i = -1; i <= 1; ++i)
                DL(cx + i * 8 + lean + swayHem, cy + 8 - lift, cx + i * 8 + lean + swayHem + (int)(sinf(T_HEM * 2.0f + i) * 3), cy + 46, clothLit, 3, alpha);
            // shoulders and collar
            FR(cx - 24 + lean + gaitSway, cy - 22 - lift + bodyY + gaitDip, 48, 16, 8, clothLit, alpha);
            FR(cx - 12 + lean + gaitSway, cy - 20 - lift + bodyY + gaitDip, 24, 10, 5, paper, alpha);
            // porcelain mask: на замахе чуть опускает голову, будто читает строку
            const int maskY = cy - 44 - lift + bodyY + gaitDip + (int)(cp * 3.0f);
            FR(cx - 11 + lean + gaitSway, maskY, 22, 26, 10, paper, alpha);
            FR(cx - 7 + lean + gaitSway, maskY + 8, 5, 3, 1, ink, alpha);
            FR(cx + 2 + lean + gaitSway, maskY + 8, 5, 3, 1, ink, alpha);
            FR(cx - 4 + lean + gaitSway, maskY + 17, 8, 2, 1, ink, alpha);
            // a crack across the mask
            DL(cx + 2 + lean + gaitSway, maskY, cx - 1 + lean + gaitSway, maskY + 14, clothDark, 1, alpha);
            // wide cardinal hat: покачивается, иначе фигура стоит как истукан
            const int hatSway = (int)(sinf(T_HAND * 1.4f) * 2.0f) + lean + gaitSway;
            FR(cx - 30 + hatSway, cy - 52 - lift + bodyY, 60, 6, 3, clothLit, alpha);
            FR(cx - 16 + hatSway, cy - 66 - lift + bodyY - (int)(cp * 2.0f), 32, 16, 6, cloth, alpha);
            FR(cx - 16 + hatSway, cy - 66 - lift + bodyY - (int)(cp * 2.0f), 32, 4, 2, clothLit, alpha);
            // open ledger: на замахе книга поднимается к груди
            const int bookY = cy - 6 - lift - (int)(cp * 20.0f) + (int)(cb * 6.0f);
            FR(cx - 40 + lean, bookY, 22, 16, 3, paper, alpha);
            DL(cx - 29 + lean, bookY + 2, cx - 29 + lean, bookY + 14, clothDark, 1, alpha);
            // строки бегут: раз в 0,9 с заново — книга «пишется»
            const int lineDrift = (int)(fmodf(animTime * 6.0f, 4.0f));
            for (int i = 0; i < 3 && !lodSmall; ++i)
                DL(cx - 37 + lean, bookY + 4 + i * 4 - lineDrift / 2, cx - 33 + lean, bookY + 4 + i * 4 - lineDrift / 2, ink, 1, 170);
            // перо: в покое пишет мелким движением, на замахе вскинуто, на выбросе
            // уходит вперёд — видно момент удара.
            const float write = sinf(T_HAND * 5.5f) * 2.0f;
            const int quillBaseX = cx + 24 + lean, quillBaseY = cy + 6 - lift + (int)(write);
            // Кончик пера отстаёт от кисти ещё на полкадра — перо не палка.
            const int quillTipX = cx + 36 + gaitSway + (int)(-cp * 16.0f + cb * 26.0f) + (int)lrintf(sinf(T_HEM * 5.5f) * 2.0f);
            const int quillTipY = cy - 30 - lift - (int)(cp * 26.0f) + (int)(cb * 22.0f);
            DL(quillBaseX, quillBaseY, quillTipX, quillTipY, paper, 2, alpha);
            DL(quillTipX - 3, quillTipY + 6, quillTipX + 4, quillTipY - 4, paper, 1, alpha);
            FC(quillBaseX + 2, quillBaseY - 2, 2, ink, alpha);
            // seal chain hanging from the belt: подрагивает на ходу
            for (int i = 0; i < 3 && !lodSmall; ++i)
                CR(cx + 12 + gaitSway + (int)(sinf(T_HEM * 2.4f + i * 0.9f) * 1.5f), cy + 16 + i * 8 - lift, 3, 1, ink, 190, true);
        } else if (id == 3) {
            // АЗАРОТ — пепельный архивариус. Раньше это было маленькое парящее ядро с
            // кольцами: в бою оно читалось как подбираемый предмет, а не как страж.
            // Теперь это высокая фигура в балахоне с горящим сердцем в груди, короной
            // из пепельных осколков, руками-углями и вихрем пепла вокруг.
            const uint32_t robe = tint(0xFF3A2B58), robeLit = tint(0xFF6A5495), robeDark = tint(0xFF1B142B);
            const uint32_t ash = tint(0xFF6E6076), ashLit = tint(0xFFB4A6C0);
            const uint32_t core = tint(enraged ? 0xFFFF5A2A : 0xFFFFB45C);
            // На замахе сердце разгорается, руки разводятся шире и фигура
            // приподнимается; на выбросе — резко проседает. Раньше на замах
            // не реагировал только он один из всей линейки.
            const float cp = std::max(0.0f, std::min(1.2f, castPose));
            const float cb = std::max(0.0f, std::min(1.2f, castBurst));
            const float pulse = 0.5f + 0.5f * sinf(animTime * 3.1f * (1.0f + cp * 1.2f));
            const int bob = (int)(sinf(animTime * 1.4f) * 4.0f) - (int)(cp * 5.0f) + (int)(cb * 4.0f);
            const int oy = cy + bob;
            const int sway = (int)(sinf(animTime * 0.9f) * 3.0f);
            // тень под парящей фигурой
            FX(cx - 40, cy + 58, 80, 9, 0xFF08060E, 90);
            // рваный подол балахона — живёт, шевелится
            const int swayHem = swayLag(0.9f, 3.0f, 0.16f);
            for (int i = 0; i < 7; ++i) {
                const int hx = cx - 33 + i * 11 + swayHem + (int)(sinf(T_HEM * 1.3f + i * 1.0f) * 2.0f);
                FX(hx, cy + 40 + (i % 3) * 3, 9, 18, robeDark, alpha);
            }
            // тело-балахон
            FR(cx - 32 + sway, oy - 8, 64, 54, 16, robeDark, alpha);
            FR(cx - 26 + sway, oy - 4, 52, 46, 14, robe, alpha);
            FR(cx - 26 + sway, oy - 4, 8, 46, 4, robeLit, alpha);   // блик ткани
            // плечи-пластины
            // плечи плотно лежат на балахоне: сдвинуты внутрь и ниже, иначе
            // висели в воздухе отдельными камешками
            FR(cx - 48 + sway, oy - 14, 26, 20, 8, ash, alpha);
            FR(cx + 22 + sway, oy - 14, 26, 20, 8, ash, alpha);
            FR(cx - 46 + sway, oy - 12, 22, 8, 4, ashLit, alpha);
            FR(cx + 24 + sway, oy - 12, 22, 8, 4, ashLit, alpha);
            // руки-угли, тянутся вниз и в стороны
            for (int side = -1; side <= 1; side += 2) {
                // Руки разводятся на замахе и вскидываются на выбросе.
                const int spread = (int)(cp * 12.0f) - (int)(cb * 6.0f);
                const int hx = cx + side * (40 + sway + spread) + (int)(sinf(T_HAND * 1.1f + side) * 3.0f);
                const int hy = oy + 26 - (int)(cp * 14.0f) + (int)(cosf(T_HAND * 1.1f + side) * 3.0f);
                DL(cx + side * 26 + sway, oy - 12, hx, hy, robeDark, 6, alpha);
                DL(cx + side * 26 + sway, oy - 12, hx, hy, robe, 4, alpha);
                FC(hx, hy, 6, ash, alpha);
                FC(hx, hy, 3, core, 220);
            }
            // горящее сердце в груди
            FC(cx + sway, oy + 14, 17 + (int)(cp * 4.0f), robeDark, alpha);
            FC(cx + sway, oy + 14, 13 + (int)(cp * 4.0f), core, (int)(150 + 90 * pulse));
            FC(cx + sway, oy + 14, 7 + (int)(cp * 3.0f), 0xFFFFF0C8, 235);
            FX(cx + sway - 20, oy + 30, 40, 3, core, (int)(60 + 60 * pulse));
            // Каждый удар сердца выходит кольцом — силуэт «живёт», а не просто стоит.
            // Кольцо рождается в такт 2,2 с и расходится за 0,7 с.
            {
                const float beat = fmodf(animTime, 2.2f) / 0.7f;
                if (beat < 1.0f) {
                    CR(cx + sway, oy + 14, 18 + (int)(beat * 26.0f), 2, core, (int)(150 * (1.0f - beat)));
                }
            }
            // голова в капюшоне
            FR(cx - 21 + sway, oy - 60, 42, 46, 14, robeDark, alpha);
            FR(cx - 15 + sway, oy - 52, 30, 30, 10, 0xFF0B0812, alpha);
            FX(cx - 10 + sway, oy - 42, 7, 4, core, 240);
            FX(cx + 3 + sway, oy - 42, 7, 4, core, 240);
            // Корона — рваный гребень по краю капюшона: плотные клинья-пластины,
            // а не тонкие лучи с шариком на конце (те читались как усики).
            for (int i = -2; i <= 2; ++i) {
                const int hgt = 7 + (int)(4.0f + 4.0f * sinf(T_CLOAK * 2.0f + i * 1.1f)) - (i % 2 ? 2 : 0);
                const int bx2 = cx + sway + i * 9 - 2;
                FX(bx2, oy - 60 - hgt, 6, hgt + 2, ash, alpha);
                FX(bx2 + 1, oy - 60 - hgt, 4, hgt, ashLit, alpha);
                FX(bx2 + 2, oy - 60 - hgt, 2, 3, core, 220);
            }
            // вихрь пепла вокруг фигуры — эллипс, читается как орбита
            for (int ring = 0; ring < 2 && !lodSmall; ++ring) {
                const int rr = 56 + ring * 14;
                // В ярости пепел идёт быстрее и плотнее: одна и та же фигура, но злее.
                const float spin = animTime * ((0.7f + ring * 0.5f) * (enraged ? 1.6f : 1.0f));
                for (int seg = 0; seg < 24; ++seg) {
                    const float a0 = spin + seg * 6.2831853f / 24.0f;
                    const int sxp = cx + (int)(cosf(a0) * rr);
                    const int syp = oy + 4 + (int)(sinf(a0) * rr * 0.3f);
                    FX(sxp, syp, 2 + (seg % 2), 2, ring == 0 ? ashLit : ash, 170);
                }
            }
            // поднимающиеся угли
            for (int i = 0; i < 9 && !lodSmall; ++i) {
                const float t2 = fmodf(animTime * 0.45f + i * 0.111f, 1.0f);
                const int eY = cy + 46 - (int)(t2 * 118.0f);
                const int eX = cx + (int)(sinf(t2 * 9.0f + i) * (10.0f + t2 * 26.0f));
                FC(eX, eY, 1 + (int)(t2 * 2.0f), core, (int)(200 * (1.0f - t2)));
            }
        } else if (id == 4) {
            // ПРОТОКОЛИСТ — писарь, который пишет строки прямо через арену. Раньше это
            // были две скрижали на ножке и выходил «раскрытый том», а не страж.
            // Теперь это высокая фигура в каменной мантии со скрижалью-лицом и огромным
            // томом перед ней: строки на страницах ползут вверх, как в живой записи.
            const uint32_t stone = tint(0xFF4A5566), stoneLit = tint(0xFF8798AD), stoneDark = tint(0xFF232B36);
            const uint32_t ink = tint(enraged ? 0xFF6AF0FF : 0xFF49C8E8);
            const float cp = std::max(0.0f, std::min(1.2f, castPose));
            const float cb = std::max(0.0f, std::min(1.2f, castBurst));
            const int sway = (int)(sinf(animTime * 0.8f) * 3.0f);
            const int bob = (int)(sinf(animTime * 1.5f) * 3.0f) - (int)(cp * 3.0f);
            const int oy = cy + bob;
            // тень
            FX(cx - 38, cy + 58, 76, 9, 0xFF08060E, 90);
            // мантия
            FR(cx - 34 + sway, oy - 6, 68, 62, 14, stoneDark, alpha);
            FR(cx - 28 + sway, oy - 2, 56, 54, 12, stone, alpha);
            FR(cx - 28 + sway, oy - 2, 7, 54, 3, stoneLit, alpha);
            // наплечник-скрижаль
            FR(cx - 44 + sway, oy - 34, 88, 18, 8, stoneLit, alpha);
            FR(cx - 40 + sway, oy - 31, 80, 7, 4, stone, alpha);
            for (int i = 0; i < 4; ++i) FC(cx - 31 + i * 21 + sway, oy - 27, 2, ink, 200);
            // Шея: капюшон поднят, между ним и наплечником виден просвет.
            // Раньше капюшон лежал на плечах вплотную, и в силуэте не было ни
            // одного промежутка — страж читался плитой (аудит §3: 0 дыр).
            FR(cx - 7 + sway, oy - 42, 14, 12, 4, stoneDark, alpha);
            // капюшон и скрижаль-лицо с прорезью чернил
            FR(cx - 22 + sway, oy - 84, 44, 48, 14, stoneDark, alpha);
            FR(cx - 16 + sway, oy - 76, 32, 32, 8, 0xFF0E1219, alpha);
            for (int ln = 0; ln < 3; ++ln) {
                const int ly = oy - 70 + ln * 9;
                const int lw = 20 - ln * 4;
                const float glow = 0.5f + 0.5f * sinf(animTime * 4.0f - ln * 1.2f);
                FX(cx - 8 + sway, ly, lw, 2, ink, (int)(110 + 140 * glow));
            }
            // Бегущая строка по лицу: раз в 3,4 с по скрижали проходит светлая черта —
            // видно, что страж не стоит, а пишет.
            {
                const float tick = fmodf(animTime, 3.4f) / 3.4f;
                if (tick < 0.35f) {
                    // Развёртка идёт вниз по скрижали, как сканирующая строка: поперёк
                    // лица двигать нельзя — получался крест на всю голову.
                    const int dy = (int)(tick / 0.35f * 28.0f);
                    FX(cx - 15 + sway, oy - 76 + dy, 30, 2, 0xFFDFF6FF, 190);
                }
            }
            // Руки. Ширину силуэта держит наплечник, поэтому кисти не выходят за
            // него: плечо уходит под наплечник, локоть чуть наружу мантии (34),
            // кисть возвращается на верхний угол тома. Между предплечьем и мантией
            // остаётся сквозной просвет — единственная дыра в силуэте стража.
            {
                const int bend = (int)(sinf(T_HAND * 1.1f) * 2.0f);
                const int shY = oy - 12;              // низ наплечника
                for (int side = -1; side <= 1; side += 2) {
                    const int sx = cx + side * 36 + sway;
                    const int ex = cx + side * 54 + sway, ey = oy + 2 + bend;
                    const int hx = cx + side * 34 + sway, hy = oy + 26;
                    // Объём: тело руки камнем, блик — тонкой линией по верхне-левой
                    // стороне. Раньше блик шёл по центру трубы, и рука светилась
                    // целиком, как пластиковая.
                    DL(sx, shY, ex, ey, stoneDark, 10, alpha);
                    DL(sx - 2, shY - 2, ex - 2, ey - 2, stone, 3, alpha);
                    DL(ex, ey, hx, hy, stoneDark, 9, alpha);
                    DL(ex - 2, ey - 2, hx - 2, hy - 2, stone, 3, alpha);
                    FC(hx, hy + 1, 5, stoneDark, alpha);
                    FC(hx - 1, hy, 3, stone, alpha);
                }
                // Перо в правой кисти: короткое, вверх-вправо от кисти, с кончиком,
                // лежащим на странице. Длинное перо через полфигуры пересекало
                // предплечье у локтя и читалось случайной светлой царапиной.
                const float qa = T_HAND * 2.2f;
                const int qt = (int)(sinf(qa) * 2.0f);
                const int hqx = cx + 34 + sway, hqy = oy + 25;
                DL(hqx, hqy, hqx + 11 + qt, hqy - 20, stoneLit, 2, alpha);
                DL(hqx + 11 + qt, hqy - 20, hqx + 6 + qt, hqy - 27, ink, 1, 220);
                FC(hqx + 6 + qt, hqy - 28, 2, ink, 230);
                DL(hqx, hqy, hqx - 9, hqy + 6, ink, 2, 210);   // кончик по странице
            }
            // большой том перед фигурой: страницы дышат, строки ползут вверх
            // Том держим ниже и уже: на арене (s≈1.1) прежний размер закрывал
            // мантию целиком, и страж читался как «скрижаль над книгой».
            // Том на замахе поднимается к груди и раскрывается шире, на выбросе
            // захлопывается и проседает — момент удара видно по книге.
            const int bookY = oy + 42 - (int)(cp * 26.0f) + (int)(cb * 8.0f);
            const int bookSpread = (int)(cp * 5.0f) - (int)(cb * 4.0f);
            const int flutter = (int)(sinf(T_CLOAK * 5.0f) * 2.0f);
            FR(cx - 29, bookY - 15, 58, 32, 6, 0xFF2B2119, alpha);      // переплёт
            FR(cx - 27, bookY - 13, 54, 27, 4, 0xFF4A382A, alpha);
            for (int side = -1; side <= 1; side += 2) {
                const int pw = 23;
                const int px3 = side < 0 ? cx - 25 - bookSpread : cx + 2 + bookSpread;
                const int ph = 23 + (side > 0 ? flutter : -flutter);
                FR(px3, bookY - ph / 2, pw, ph, 3, 0xFFDCD2BC, alpha);
                FR(px3 + 2, bookY - ph / 2 + 2, pw - 4, ph - 4, 2, 0xFFF6EFDD, alpha);
                const int scroll = (int)(animTime * 9.0f) % 6;
                for (int ln = 0; ln < 5; ++ln) {
                    const int ly = bookY - ph / 2 + 4 + ln * 5 - scroll;
                    if (ly < bookY - ph / 2 + 2 || ly > bookY + ph / 2 - 3) continue;
                    FX(px3 + 4, ly, pw - 8, 1, 0xFF3A4652, 220);
                }
            }
            // Переворачивающаяся страница: раз в 2,6 с лист поднимается над правой
            // страницей и падает обратно. Без этого том читался как картинка.
            {
                const float turn = fmodf(animTime, 2.6f) / 2.6f;
                if (turn < 0.42f) {
                    const float t2 = turn / 0.42f;                 // 0…1
                    const float ang = t2 * 3.14159f;                // лист идёт по дуге
                    const int lift = (int)(sinf(ang) * 9.0f);
                    const int sw = (int)(20.0f * cosf(ang));
                    if (sw > 0) {
                        FR(cx + 2, bookY - 12 - lift, sw, 24, 2, 0xFFEFE7D4, alpha);
                        FX(cx + 3, bookY - 10 - lift, std::max(1, sw - 2), 1, 0xFF3A4652, 200);
                    }
                }
            }
            FR(cx - 3, bookY - 16, 6, 34, 3, stoneLit, alpha);
            FC(cx, bookY - 20, 5, ink, 230);
            // чернильные капли падают с тома
            for (int i = 0; i < 3 && !lodSmall; ++i) {
                const float t2 = fmodf(animTime * 0.6f + i * 0.33f, 1.0f);
                FX(cx - 13 + i * 13, bookY + 16 + (int)(t2 * 24.0f), 2, 3, ink, (int)(200 * (1.0f - t2)));
            }
            // перья-перья вокруг: летят по орбите с хвостом
            for (int i = 0; i < 4 && !lodSmall; ++i) {
                const float a = animTime * 1.2f + i * 1.5708f;
                const int qx = cx + (int)(cosf(a) * 58.0f);
                const int qy = oy - 6 + (int)(sinf(a) * 30.0f);
                for (int tr = 1; tr <= 3; ++tr) {
                    const float a2 = a - tr * 0.16f;
                    FC(cx + (int)(cosf(a2) * 58.0f), oy - 6 + (int)(sinf(a2) * 30.0f), 2, ink, 120 - tr * 30);
                }
                DL(qx, qy, qx + (int)(cosf(a + 1.2f) * 16.0f), qy + (int)(sinf(a + 1.2f) * 16.0f), stoneLit, 2, alpha);
                FC(qx, qy, 3, ink, alpha);
            }
        } else {
            // КОПИЯ-ДВОЙНИК — два стеклянных слепка одного человека. Раньше это были
            // тонкие палочки с кружками: силуэт не читался как страж. Теперь у обоих
            // тел есть масса, плечи, трещины и ядро, а двойник отстаёт по фазе и
            // «просвечивает», поэтому сразу видно, кто настоящий.
            const uint32_t glass = tint(0xFF4E7C8A), glassLit = tint(0xFFB4E8F4), glassDark = tint(0xFF16242A);
            const uint32_t core = tint(enraged ? 0xFFFF5A7A : 0xFF8FF0FF);
            const float cp = std::max(0.0f, std::min(1.2f, castPose));
            const float cb = std::max(0.0f, std::min(1.2f, castBurst));
            const float pulse = 0.5f + 0.5f * sinf(animTime * 3.4f);
            const int sway = (int)(sinf(animTime * 1.6f) * 3.0f);
            const int bob = (int)(sinf(animTime * 2.1f) * 3.0f);
            const int oy = cy + bob;
            // Обмен местами виден по фигуре: перед броском слепок отходит дальше,
            // после — оба сходятся. Раньше телепорт был мгновенным и без позы.
            const int separation = (int)(cp * 34.0f) - (int)(cb * 26.0f);
            // тень у настоящего тела
            FX(cx - 34, cy + 56, 68, 9, 0xFF08060E, 80);
            auto glassBody = [&](int bx, int by, int a, bool mirror, float phase) {
                const int legSwing = (int)(sinf(animTime * 2.4f + phase) * 3.0f);
                // ноги
                FR(bx - 15, by + 26, 12, 30 + legSwing, 4, glassDark, a);
                FR(bx + 3, by + 26, 12, 30 - legSwing, 4, glassDark, a);
                FR(bx - 15, by + 24, 12, 14, 4, glass, a);
                FR(bx + 3, by + 24, 12, 14, 4, glass, a);
                // торс — трапеция из двух плит
                FR(bx - 24, by - 20, 48, 50, 14, glassDark, a);
                FR(bx - 19, by - 16, 38, 42, 12, glass, a);
                FR(bx - 19, by - 16, 7, 42, 3, glassLit, a);          // блик стекла
                FR(bx + 12, by - 16, 5, 42, 2, glassLit, (a * 2) / 3);
                // плечи
                FC(bx - 22, by - 18, 8, glass, a);
                FC(bx + 22, by - 18, 8, glass, a);
                // голова: два глаза и пустота между ними
                FR(bx - 7, by - 32, 14, 10, 4, glassLit, a);
                FC(bx, by - 42, 15, glassDark, a);
                FC(bx, by - 42, 12, glassLit, a);
                FC(bx, by - 42, 8, glassDark, (a * 3) / 4);
                FC(bx - 4, by - 43, 3, core, a);
                FC(bx + 4, by - 43, 3, core, a);
                // руки, слегка в стороны. Тело руки — стекло, блик тонкой линией
                // по внешней стороне: раньше вся рука была glassLit, и после
                // починки drawLine они читались белыми палками, ярче головы.
                DL(bx - 20, by - 8, bx - 30, by + 16, glass, 6, a);
                DL(bx + 20, by - 8, bx + 30, by + 16, glass, 6, a);
                DL(bx - 22, by - 8, bx - 32, by + 16, glassLit, 2, a);
                DL(bx + 22, by - 8, bx + 32, by + 16, glassLit, 2, a);
                DL(bx - 30, by + 16, bx - 28, by + 26, glassDark, 5, a);
                DL(bx + 30, by + 16, bx + 28, by + 26, glassDark, 5, a);
                // ядро в груди
                FC(bx, by + 4, 9, glassDark, a);
                FC(bx, by + 4, 6, core, (int)((a * (0.55f + 0.45f * pulse))));
                // трещины
                for (int i = 0; i < 3 && !lodSmall; ++i) {
                    const float ang = (mirror ? -1.0f : 1.0f) * (0.7f + i * 0.9f) + animTime * 0.4f;
                    DL(bx, by + 2, bx + (int)(cosf(ang) * 22.0f), by + (int)(sinf(ang) * 26.0f), core, 1, 130);
                }
            };
            // шлейф: два запоздавших слепка, каждый бледнее предыдущего —
            // движение читается как «стекло», а не как две статичные фигурки
            for (int ghost = 2; ghost >= 1; --ghost) {
                const int lagX = cx + 40 + sway + separation - (int)(sinf(T_CLOAK * 2.6f - ghost * 0.5f) * 5.0f) - ghost * 6;
                glassBody(lagX, oy - 4 + ghost, 36 - ghost * 8, true, 1.1f + ghost * 0.4f);
            }
            // двойник: та же фигура, отстаёт по фазе и просвечивает сквозь
            const int tx = cx + 46 + sway + separation;
            glassBody(tx, oy - 4, 120, true, 1.1f);
            // трос между двумя телами
            for (int i = 0; i < 6 && !lodSmall; ++i)
                FX(cx + 22 + i * 5, oy + 2 + (int)(sinf(T_HEM * 3.0f + i * 0.7f) * 2.0f), 2, 2, core, 170);
            // настоящее тело
            glassBody(cx + sway, oy, alpha, false, 0.0f);
            // осколки на орбите
            for (int i = 0; i < 8; ++i) {
                const float a2 = animTime * 1.15f + i * 0.785f;
                const int sxp = cx + sway + (int)(cosf(a2) * 54.0f);
                const int syp = oy - 2 + (int)(sinf(a2) * 40.0f);
                const int sz = (i % 3 == 0) ? 4 : 3;
                FX(sxp - sz / 2, syp - sz / 2, sz, sz, i % 2 ? core : glassLit, 190);
            }
        }
    }

    void drawBossSprite(int cx, int cy, int bossType, float animTime, bool hitFlash, bool enraged = false,
                        bool reinforced = false, float castPose = 0.0f, float castBurst = 0.0f,
                        bool mirror = false, float recoil = 0.0f, float fade = 1.0f) {
        // Поза догоняется пружиной (см. BossSpring): dt берём из animTime, потому
        // что рендерер не знает про кадровое время игры. Пауза и переход сцены
        // дают скачок dt — тогда пружину не двигаем, иначе она «выстрелит».
        // Пружина живёт здесь, а не в drawBossFigure: сюда приходит только живой
        // страж (эхо-копии, карточка дела и смерть рисуют фигуру напрямую), так
        // что один тип стража не двигает пружину дважды за кадр.
        if (bossSpringEnabled) {
            const int sid = ((bossType % 6) + 6) % 6;
            BossSpring& sp = bossSpring[sid];
            float dt = sp.lastT < 0.0f ? 0.016f : (animTime - sp.lastT);
            if (!(dt > 0.0f) || dt > 0.25f) {
                // Пауза, переход сцены или новый страж с тем же типом: время ушло
                // вперёд скачком или назад. Пружину не интегрируем, а ставим в цель
                // — иначе она «выстрелит» из старого состояния.
                dt = 0.0f;
                sp.pose = castPose;
                sp.burst = castBurst;
                sp.poseV = sp.burstV = 0.0f;
            }
            sp.lastT = animTime;
            if (dt > 0.0f) {
                springStep(sp.pose, sp.poseV, castPose, dt, 15.0f, 0.62f);
                springStep(sp.burst, sp.burstV, castBurst, dt, 19.0f, 0.55f);
            }
            castPose = sp.pose;
            castBurst = sp.burst;
            lastBossPose = sp.pose;
            lastBossBurst = sp.burst;
        }
        int auraRad = 40 + (int)(sinf(animTime * 7.0f) * 5.0f);
        static const uint32_t auraPalette[6] = {0xFFFFB028, 0xFF80FF40, 0xFFFF3068, 0xFFFF6818, 0xFF49C8E8, 0xFF9EDBE8};
        const int bossId = bossType % 6;
        uint32_t auraCol = enraged ? 0xFFFF2040 : auraPalette[bossId];
        if (bossId == 1) {
            // У Малакора знак — пролитая лужа на полу. Кольцо вокруг туловища
            // читалось как обруч на фигуре, поэтому здесь эллипс у ног.
            for (int seg = 0; seg < 34; ++seg) {
                const float a0 = seg * 6.2831853f / 34.0f, a1 = (seg + 1) * 6.2831853f / 34.0f;
                drawLine(cx + (int)(cosf(a0) * auraRad), cy + 52 + (int)(sinf(a0) * auraRad * 0.28f),
                         cx + (int)(cosf(a1) * auraRad), cy + 52 + (int)(sinf(a1) * auraRad * 0.28f),
                         auraCol, 3, 205, false);
            }
        } else {
            drawCircleRing(cx, cy + 24, auraRad, 3, auraCol, 210, true);
        }
        // Every guardian brands the floor with its own sigil: gate bars, a broken vial,
        // ledger rules, an archive wheel, marching protocol lines and mirrored shards.
        static const int tickTable[6] = {4, 5, 6, 8, 3, 6};
        const int ticks = tickTable[bossId];
        static const float spinTable[6] = {0.42f, 0.42f, 0.30f, 0.55f, 0.22f, 0.72f};
        const float spin = animTime * spinTable[bossId];
        for (int i = 0; i < ticks; ++i) {
            const float a = spin + i * (6.2831853f / ticks);
            const float r0 = auraRad * 0.94f, r1 = auraRad * (1.14f + (i % 2 ? 0.06f : 0.0f));
            drawLine(cx + (int)(cosf(a) * r0), cy + 24 + (int)(sinf(a) * r0 * 0.42f),
                     cx + (int)(cosf(a) * r1), cy + 24 + (int)(sinf(a) * r1 * 0.42f),
                     auraCol, i % 2 ? 1 : 2, i % 2 ? 90 : 150, true);
        }
        if (bossId == 0) {
            // Моргар: gate bars rooted in the floor.
            for (int bar = -1; bar <= 1; ++bar)
                drawLine(cx + bar * 13, cy + 40, cx + bar * 13, cy + 8, auraCol, 2, 70);
        } else if (bossId == 1) {
            // Малакор: пролитая лужа у ног и поднимающиеся споры. Раньше здесь было
            // кольцо поперёк живота — оно читалось как обруч на фигуре, а не как
            // разлитая отрава. Лужа лежит на полу, поэтому сплющена по вертикали.
            const int poolR = auraRad + 6;
            // Сначала заливка лужи, потом край: без аддитивного режима цвет остаётся
            // зелёным, а не выгорает в белый от наложения сегментов.
            for (int y = -8; y <= 8; ++y) {
                const float t = (float)y / 8.0f;
                const int halfW = (int)(poolR * sqrtf(std::max(0.05f, 1.0f - t * t)));
                drawLine(cx - halfW, cy + 52 + y / 3, cx + halfW, cy + 52 + y / 3, auraCol, 1, y == 0 ? 70 : 34, false);
            }
            for (int seg = 0; seg < 26; ++seg) {
                const float a0 = seg * 6.2831853f / 26.0f, a1 = (seg + 1) * 6.2831853f / 26.0f;
                drawLine(cx + (int)(cosf(a0) * poolR), cy + 52 + (int)(sinf(a0) * poolR * 0.26f),
                         cx + (int)(cosf(a1) * poolR), cy + 52 + (int)(sinf(a1) * poolR * 0.26f),
                         auraCol, 2, 165, false);
            }
            for (int bubble = 0; bubble < 5; ++bubble) {
                const float rise = fmodf(animTime * 0.8f + bubble * 0.2f, 1.0f);
                const int bx = cx + (int)(sinf(animTime * 1.7f + bubble * 2.1f) * (18.0f + poolR * 0.3f));
                const int by = cy + 50 - (int)(rise * 34.0f);
                fillCircle(bx, by, 1 + (bubble % 2), auraCol, (int)(130 * (1.0f - rise * 0.6f)));
            }
        } else if (bossId == 2) {
            // Кардинал: ruled ledger lines crossing the sigil.
            for (int line = -2; line <= 2; ++line)
                drawLine(cx - 46, cy + 24 + line * 9, cx + 46, cy + 24 + line * 9, auraCol, 1, 55);
        } else if (bossId == 3) {
            // Азарот: a toothed archive wheel with a slow inner counter-rotation.
            drawCircleRing(cx, cy + 24, (int)(auraRad * 0.66f), 1, auraCol, 110,
                           true);
            for (int tooth = 0; tooth < 12; ++tooth) {
                const float a = -spin * 0.7f + tooth * 0.5235988f;
                const int rr = (int)(auraRad * 0.72f);
                fillRect(cx + (int)(cosf(a) * rr), cy + 24 + (int)(sinf(a) * rr * 0.42f), 2, 2, auraCol, 120);
            }
        } else if (bossId == 4) {
            // Протоколист: ruled lines marching across the floor, one direction only.
            for (int line = 0; line < 3; ++line) {
                const int off = (int)(fmodf(animTime * 46.0f + line * 30.0f, 90.0f)) - 45;
                drawLine(cx - 52 + off / 2, cy + 10 + line * 14, cx + 52 + off / 2, cy + 10 + line * 14, auraCol, 1, 110);
            }
        } else {
            // Двойник: two overlapping sigil rings, half a turn out of phase.
            drawCircleRing(cx - 12, cy + 24, (int)(auraRad * 0.7f), 1, auraCol, 95, true);
            drawCircleRing(cx + 12, cy + 24, (int)(auraRad * 0.7f), 1, auraCol, 95, true);
        }
        if (reinforced) {
            // Возвращение: второй, более широкий и медленный контур и корона осколков.
            // Достаточно, чтобы ветерана было видно с первого кадра, но силуэт не
            // перекрывался — игрок должен узнать стража, а не гадать, кто это.
            const int outer = (int)(auraRad * 1.5f) + 8;
            drawCircleRing(cx, cy + 24, outer, 2, 0xFFFF3A5A, 150, true);
            for (int i = 0; i < 8; ++i) {
                const float a = -animTime * 0.9f + i * 0.7853982f;
                const int rr = outer + 6;
                const int px = cx + (int)(cosf(a) * rr);
                const int py = cy + 24 + (int)(sinf(a) * rr * 0.42f);
                drawLine(px - 3, py - 3, px + 3, py + 3, 0xFFFF6A80, 2, 190, true);
                drawLine(px + 3, py - 3, px - 3, py + 3, 0xFFFF6A80, 2, 190, true);
            }
        }
        addGlow(cx, cy, 62, auraCol, enraged ? 190 : 140);
        // Тень стоит под ступнями, а не на фиксированном cy+50: у трёх стражей
        // низ фигуры ниже на 22–34 px, и они висели в воздухе. Второй, узкий
        // эллипс — контактная AO, без неё фигура «лежит» на полу, а не стоит.
        const int footY = cy + bossFootOffset(bossId);
        const int shadowW = bossShadowWidth(bossId);
        drawShadow(cx, footY - 3, shadowW, 12);
        drawShadow(cx, footY - 2, shadowW * 3 / 5, 6);
        // Снимок области до фигуры: по нему считается маска для общего света,
        // чтобы ореол и знак на полу не подсвечивались как часть стража.
        // Область с запасом: замеренный размах стражей при 1.12 — от -125 до +119
        // по x и от -123 до +93 по y в позах замаха и выброса. Меньшая рамка
        // обрезала выброшенную колбу и поднятое древко.
        const int lx = cx - 125, ly = cy - 132, lw = 250, lh = 232;
        captureFigureRegion(lx, ly, lw, lh);
        drawBossFigure(cx, cy, bossType, animTime, hitFlash, enraged, false, 1.12f, castPose, castBurst,
                       mirror, recoil, fade);
        if (figureVolumeEnabled) applyFigureVolume(lx, ly, lw, lh, std::min(1.0f, fade), figureVolumeGradient, cx, cy, bossType);
    }

    // Смерть стража. Раньше босс исчезал между кадрами: попадал последний удар —
    // и на его месте уже пусто. Теперь фигура проседает и сжимается, вспыхивает,
    // рассыпается осколками и уходит в расширяющееся кольцо.
    //   progress: 0 — только что погиб, 1 — пора убирать.
    void drawBossDeath(int cx, int cy, int bossType, float animTime, float progress) {
        static const uint32_t pal[6] = {0xFFFFB028, 0xFF80FF40, 0xFFFF3068,
                                        0xFFFF6818, 0xFF49C8E8, 0xFF9EDBE8};
        const float p = std::max(0.0f, std::min(1.0f, progress));
        const uint32_t col = pal[bossType % 6];
        const int fadeA = (int)(255.0f * (1.0f - p * p));
        addGlow(cx, cy, 56 + (int)(p * 50.0f), col, (int)(190.0f * (1.0f - p)));
        // Кольцо смерти расходится от фигуры.
        drawCircleRing(cx, cy + 24, 26 + (int)(p * 130.0f), p < 0.5f ? 4 : 2, col,
                       (int)(225.0f * (1.0f - p)), true);
        drawCircleRing(cx, cy + 24, 14 + (int)(p * 78.0f), 1, 0xFFFFFFFF,
                       (int)(160.0f * (1.0f - p)), true);
        // Осколки: разлетаются по кругу и провисают вниз, как настоящие обломки.
        for (int i = 0; i < 16; ++i) {
            const float a = i * 6.2831853f / 16.0f + animTime * 0.4f;
            const float dist = 18.0f + p * (46.0f + (i % 4) * 16.0f);
            const int px = cx + (int)(cosf(a) * dist);
            const int py = cy + 10 + (int)(sinf(a) * dist * 0.6f + p * p * 26.0f);
            const int sz = std::max(1, 4 - (int)(p * 3.0f));
            fillRect(px - sz / 2, py - sz / 2, sz, sz + (i % 2), i % 3 ? col : 0xFFFFF0C8,
                     (int)(215.0f * (1.0f - p)));
        }
        // Сама фигура: проседает, сжимается и гаснет. Первую треть она ещё
        // светится вспышкой — удар был последним.
        const float shrink = 1.12f * (1.0f - p * 0.55f);
        const int sink = (int)(p * 20.0f);
        // Область с запасом: замеренный размах стражей при 1.12 — от -125 до +119
        // по x и от -123 до +93 по y в позах замаха и выброса. Меньшая рамка
        // обрезала выброшенную колбу и поднятое древко.
        const int lx = cx - 125, ly = cy - 132, lw = 250, lh = 232;
        captureFigureRegion(lx, ly, lw, lh);
        drawBossFigure(cx, cy + sink, bossType, animTime, p < 0.3f, true, false, shrink,
                       0.0f, p * 0.85f, false, p * 0.7f, 1.0f - p * 0.85f);
        if (figureVolumeEnabled) applyFigureVolume(lx, ly, lw, lh, 1.0f - p * 0.8f, figureVolumeGradient, cx, cy, bossType);
        (void)fadeA;
    }

    // Эхо-двойники из третьей фазы: та же фигура, но призрачная и полупрозрачная.
    void drawBossEcho(int cx, int cy, int bossType, float animTime, bool hitFlash) {
        addGlow(cx, cy, 40, 0xFFFF3058, 80);
        drawBossFigure(cx, cy, bossType, animTime, hitFlash, true, true);
    }

    // ── Тон предмета ───────────────────────────────────────────────────────────
    // Один предмет — один смысловой цвет. Раньше палитра шла по кругу из десяти
    // оттенков (предметы 00, 10, 20, 30 были одного цвета) и половина цветов не
    // имела отношения к предмету. Здесь сорок разных оттенков, и каждый выбран по
    // смыслу: огонь оранжевый, лёд бирюзовый, молния золотая, кровь алая, магнетизм
    // пурпурный, бумага пергаментная, ветер зелёный.
    //   bg   — подложка, edge — рамка, glow — свечение
    static bool isPassiveItem(int id) { return !((id >= 0 && id <= 6) || (id >= 24 && id <= 27)); }

    void drawWeaponIcon(int cx, int cy, int weaponId, int size = 22, bool evolved = false) {
        static const uint32_t glowColors[40] = {
            0xFF40D0FF,0xFFFF6818,0xFF43D9F4,0xFF40B8FF,0xFFFF2850,
            0xFFFFA030,0xFF30E8FF,0xFF40B8FF,0xFF40FF90,0xFFFFD030,
            0xFFFF3058,0xFFFF7820,0xFFC068FF,0xFFFFA818,0xFFFF5028,
            0xFFB040FF,0xFFFF7438,0xFFFF426E,0xFF35E1FF,0xFF62E8A0,
            0xFFFFC56A,0xFF77EAD7,0xFFFF758E,0xFFA895FF,
            0xFF70DEFF,0xFF62D9E8,0xFFBC8AFF,0xFFFF9A42,0xFF63E9E8,0xFFBD83FF,
            0xFFFFD878,0xFFFF9B58,0xFF67E7D4,0xFF68E6D8,0xFFFF9347,0xFF72DFFF,
            0xFF8BEAFF,0xFFFF705E,0xFFFF9A72,0xFFD6A0FF
        };
        const int id = std::max(0, std::min(39, weaponId));
        // Арт-наборы для любого предмета 00–39: ИИ-АРТ или набор «ЦВЕТ», где первые
        // десять предметов различаются цветом и силуэтом, а не общей рамкой.
        const ItemTone& tone = ITEM_TONE[id];
        // Активные и пассивные предметы носят разные подложки: у оружия — угловатая
        // стальная пластина с бликом сверху, у артефакта — круглая каменная шайба.
        // Форма видна на 22 px раньше цвета, поэтому слоты больше не путаются.
        const bool passive = isPassiveItem(id);
        const uint32_t frameBg = evolved ? 0xFF3A1828 : tone.bg;
        const uint32_t frameBorder = evolved ? 0xFFFFD838 : (passive ? tone.edge : 0xFF8FA6C8);
        const uint32_t glow = evolved ? 0xFFFFB820 : tone.glow;
        if (skillIconStyle >= 1) {
            addGlow(cx, cy, size + (evolved ? 9 : 3), glow, evolved ? 145 : 68);
            if (passive) {
                fillCircle(cx, cy, size, frameBg);
                drawCircleRing(cx, cy, size, passive ? 2 : 3, frameBorder, 235, false);
            } else {
                fillRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, frameBg);
                fillRect(cx - size + 3, cy - size + 2, size * 2 - 6, std::max(1, size / 10), 0xFFA9C0E0, 120);
            }
            const BakedIconImage& image = skillIconStyle == 2 ? SKILL_ICONS_COLOR[id] : SKILL_ICONS_AI[id];
            const int side = std::max(1, size * 2 - 8);
            const int left = cx - side / 2, top = cy - side / 2;
            const int corner = passive ? 0 : std::max(2, side / 11);
            for (int y = 0; y < side; ++y) {
                const int sy = y * image.height / side;
                for (int x = 0; x < side; ++x) {
                    if (passive) {
                        // Круглая маска: артефакт не выглядит как оружие.
                        const int dxp = x - side / 2, dyp = y - side / 2;
                        if (dxp * dxp + dyp * dyp > (side / 2) * (side / 2)) continue;
                    } else {
                        int dx = 0, dy = 0;
                        if (x < corner) dx = corner - x;
                        else if (x >= side - corner) dx = x - (side - corner - 1);
                        if (y < corner) dy = corner - y;
                        else if (y >= side - corner) dy = y - (side - corner - 1);
                        if (dx > 0 && dy > 0 && dx * dx + dy * dy > corner * corner) continue;
                    }
                    const int sx = x * image.width / side;
                    const uint8_t index = image.pixels[sy * image.width + sx];
                    putPixel(left + x, top + y, image.palette[index]);
                }
            }
            if (!passive) drawRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, frameBorder, evolved ? 3 : 2);
            else drawCircleRing(cx, cy, size, evolved ? 3 : 2, frameBorder, 245, false);
            return;
        }
        const uint32_t bg = frameBg;
        const uint32_t border = frameBorder;
        const uint32_t accent = tone.edge;
        const int t = std::max(2, size / 8);
        const int thin = std::max(1, size / 13);
        if (passive) { fillCircle(cx, cy, size, bg); drawCircleRing(cx, cy, size, 2, border, 235, false); }
        else {
            fillRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, bg);
            drawRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, border, evolved ? 3 : 2);
            fillRect(cx - size + 3, cy - size + 2, size * 2 - 6, std::max(1, size / 10), 0xFFA9C0E0, 120);
        }
        addGlow(cx, cy, size + (evolved ? 9 : 3), evolved ? 0xFFFFB820 : glow, evolved ? 145 : 65);
        auto L = [&](int x1, int y1, int x2, int y2, uint32_t color, int width = -1, int alpha = 255) {
            drawLine(x1, y1, x2, y2, color, width > 0 ? width : t, alpha, true);
        };
        auto C = [&](int x, int y, int radius, uint32_t color, int alpha = 255) {
            fillCircle(x, y, std::max(1, radius), color, alpha);
        };
        const uint32_t white = 0xFFF4F2FF;
        // Shared faceted-icon painter: each icon gets a solid silhouette, a shaded plane and a crisp edge.
        auto X = [&](int u) { return cx + (u * size) / 12; };
        auto Y = [&](int v) { return cy + (v * size) / 12; };
        auto Q = [&](int x1, int y1, int x2, int y2, uint32_t color, int width = 1) {
            drawLine(X(x1), Y(y1), X(x2), Y(y2), color, std::max(1, width * size / 12), 255, false);
        };
        auto dot = [&](int x, int y, int radius, uint32_t color) {
            fillCircle(X(x), Y(y), std::max(1, radius * size / 12), color);
        };
        auto ring = [&](int x, int y, int radius, int width, uint32_t color) {
            drawCircleRing(X(x), Y(y), std::max(2, radius * size / 12),
                           std::max(1, width * size / 12), color, 245, false);
        };
        auto poly = [&](std::initializer_list<std::pair<int,int>> raw, uint32_t fill, uint32_t edge, int edgeWidth = 1) {
            std::vector<std::pair<int,int>> pts;
            pts.reserve(raw.size());
            for (const auto& q : raw) pts.push_back({X(q.first), Y(q.second)});
            if (pts.size() < 3) return;
            int minY = SCREEN_H - 1, maxY = 0;
            for (const auto& q : pts) { minY = std::min(minY, q.second); maxY = std::max(maxY, q.second); }
            minY = std::max(0, minY); maxY = std::min(SCREEN_H - 1, maxY);
            std::vector<float> cuts;
            cuts.reserve(pts.size());
            for (int y = minY; y <= maxY; ++y) {
                cuts.clear();
                for (size_t k = 0; k < pts.size(); ++k) {
                    const auto& a = pts[k]; const auto& b = pts[(k + 1) % pts.size()];
                    if ((a.second <= y && b.second > y) || (b.second <= y && a.second > y))
                        cuts.push_back(a.first + (float)(y - a.second) * (float)(b.first - a.first) / (float)(b.second - a.second));
                }
                std::sort(cuts.begin(), cuts.end());
                for (size_t k = 0; k + 1 < cuts.size(); k += 2)
                    blendSpan(y, (int)ceilf(cuts[k]), (int)floorf(cuts[k + 1]), fill, 255);
            }
            for (size_t k = 0; k < pts.size(); ++k) {
                const auto& a = pts[k]; const auto& b = pts[(k + 1) % pts.size()];
                drawLine(a.first, a.second, b.first, b.second, edge, std::max(1, edgeWidth), 255, false);
            }
        };
        const uint32_t outline = lerpColor(bg, 0xFF05050A, 0.55f);
        const uint32_t body = lerpColor(accent, bg, 0.28f);
        const uint32_t facet = lerpColor(accent, white, 0.27f);
        const uint32_t shade = lerpColor(accent, bg, 0.56f);
        const uint32_t sparkle = lerpColor(accent, white, 0.72f);
        // Custom silhouettes for 1..39. Existing per-item linework below becomes the engraved/facet detail.
        switch (id) {
        case 1: // Fireball
            poly({{-1,-10},{3,-7},{2,-4},{8,-5},{6,0},{9,3},{4,7},{1,10},{-4,7},{-8,4},{-5,0},{-7,-4},{-2,-3}},body,outline,1);
            poly({{-1,-6},{2,-3},{0,0},{4,-2},{3,2},{0,6},{-3,3},{-2,0}},facet,outline,1); dot(0,1,2,0xFFFFD36B); break;
        case 2: // Rune vortex
            ring(0,0,9,2,accent); ring(0,0,5,1,facet); dot(0,0,3,body);
            for(int k=0;k<3;++k){float a=-1.5707963f+k*2.0943951f;dot((int)(cosf(a)*7),(int)(sinf(a)*7),2,k==1?sparkle:accent);} break;
        case 3: // Chain lightning
            poly({{1,-10},{-6,0},{-2,1},{-5,9},{7,-3},{2,-3},{5,-10}},body,outline,1);
            Q(-2,-3,2,-2,sparkle,1); Q(-1,2,3,1,facet,1); break;
        case 4: // Blood aura
            ring(0,0,8,2,accent); poly({{0,-6},{-4,-1},{-3,3},{0,7},{3,3},{4,-1}},body,outline,1); dot(0,0,2,sparkle); break;
        case 5: // Battle axe
            Q(-5,8,4,-7,0xFF8A552D,3); Q(-5,8,4,-7,0xFFE5B16E,1);
            poly({{1,-8},{5,-10},{9,-8},{10,-3},{7,1},{3,0},{1,-3},{-1,-1}},body,outline,1);
            Q(3,-7,8,-8,facet,1); break;
        case 6: // Ice ring
            ring(0,0,8,3,body); ring(0,0,8,1,facet);
            Q(0,-6,0,6,sparkle,1); Q(-6,0,6,0,sparkle,1); Q(-4,-4,4,4,accent,1); Q(-4,4,4,-4,accent,1); dot(0,0,2,white); break;
        case 7: // Open book
            poly({{-9,-6},{-2,-8},{0,-5},{0,8},{-3,5},{-9,6}},body,outline,1);
            poly({{9,-6},{2,-8},{0,-5},{0,8},{3,5},{9,6}},facet,outline,1);
            Q(0,-5,0,8,sparkle,1); Q(-7,-3,-2,-4,white,1); Q(2,-4,7,-3,white,1); break;
        case 8: // Wind boots
            poly({{-9,-7},{-3,-7},{-2,1},{1,4},{-2,7},{-9,7},{-7,3}},body,outline,1);
            poly({{2,-9},{8,-9},{7,-1},{10,2},{8,6},{1,6},{3,2}},facet,outline,1);
            Q(-9,7,-2,7,sparkle,1); Q(1,6,9,6,sparkle,1); break;
        case 9: // Magnet amulet
            Q(-6,-7,-6,3,body,4); Q(-6,3,-3,7,body,4); Q(-3,7,3,7,body,4); Q(3,7,6,3,body,4); Q(6,3,6,-7,body,4);
            Q(-8,-7,-4,-7,0xFFFF7A70,2); Q(4,-7,8,-7,0xFF86D9FF,2); break;
        case 10: // Titan heart
            poly({{-8,-3},{-7,-7},{-3,-8},{0,-5},{3,-8},{7,-7},{8,-3},{6,2},{0,9},{-6,2}},body,outline,1);
            poly({{-6,-4},{-4,-6},{-1,-4},{0,-1},{-3,1}},facet,outline,1); Q(1,-4,5,-5,sparkle,1); break;
        case 11: // Berserker glove
            poly({{-8,1},{-7,-2},{-4,-2},{-4,-7},{-1,-7},{0,-3},{1,-8},{4,-7},{4,-2},{5,-6},{8,-5},{7,0},{9,3},{6,7},{-5,7},{-8,4}},body,outline,1);
            Q(-6,4,6,4,facet,1); Q(-2,-2,-2,1,sparkle,1); break;
        case 12: // Hourglass
            poly({{-7,-9},{7,-9},{5,-6},{2,-2},{0,0},{-2,-2},{-5,-6}},body,outline,1);
            poly({{-2,2},{0,0},{2,2},{5,6},{7,9},{-7,9},{-5,6}},facet,outline,1);
            Q(-7,-9,7,-9,sparkle,1); Q(-7,9,7,9,sparkle,1); dot(0,2,1,0xFFFFD56D); break;
        case 13: // Cursed coin
            dot(0,0,8,body); ring(0,0,8,1,facet); ring(0,0,6,1,outline);
            poly({{0,-5},{-2,-1},{1,0},{-1,4},{3,1},{0,0},{2,-2}},sparkle,outline,1); break;
        case 14: // Phoenix ash: rising flame-body with symmetrical wings and split tail.
            poly({{0,-10},{3,-6},{2,-3},{5,0},{4,4},{0,10},{-4,4},{-5,0},{-2,-3},{-3,-6}},body,outline,1);
            poly({{-2,-2},{-10,-7},{-7,0},{-3,4},{0,2}},facet,outline,1);
            poly({{2,-2},{10,-7},{7,0},{3,4},{0,2}},facet,outline,1);
            Q(-2,6,0,-5,sparkle,1); Q(-6,0,-2,2,shade,1); Q(6,0,2,2,shade,1); break;
        case 15: // Eye of the Abyss
            poly({{-10,0},{-6,-4},{0,-6},{6,-4},{10,0},{6,4},{0,6},{-6,4}},body,outline,1);
            dot(0,0,4,accent); dot(0,0,2,outline); dot(1,-1,1,white); break;
        case 16: // Ashen Heart seal
            poly({{0,-10},{8,-6},{8,3},{0,10},{-8,3},{-8,-6}},body,outline,1);
            poly({{0,-5},{4,-1},{2,4},{0,6},{-3,3},{-2,-1}},facet,outline,1); Q(0,-7,0,-4,sparkle,1); break;
        case 17: // Blood moon shard
            poly({{-6,-9},{0,-7},{7,-2},{5,5},{0,9},{-6,5},{-8,-1}},body,outline,1);
            poly({{-6,-8},{-1,-6},{1,0},{-4,4}},facet,outline,1); Q(1,-5,5,-2,sparkle,1); break;
        case 18: // Rift lens
            ring(0,0,7,3,body); ring(0,0,5,1,facet); dot(0,0,2,accent);
            Q(4,5,9,10,outline,3); Q(4,5,9,10,sparkle,1); Q(-2,-2,2,2,white,1); break;
        case 19: // Last Dawn oath shield
            poly({{-8,-8},{8,-8},{7,2},{4,7},{0,10},{-4,7},{-7,2}},body,outline,1);
            poly({{0,-6},{2,-2},{6,-1},{3,2},{3,6},{0,4},{-3,6},{-3,2},{-6,-1},{-2,-2}},facet,outline,1);
            Q(-5,-6,5,-6,sparkle,1); break;
        case 20: // Hunter's brand
            ring(0,0,7,1,accent); poly({{0,-8},{2,-2},{8,0},{2,2},{0,8},{-2,2},{-8,0},{-2,-2}},body,outline,1);
            Q(-5,5,5,-5,sparkle,1); dot(0,0,2,white); break;
        case 21: // Return rune
            ring(0,0,7,2,body); Q(-7,1,-7,-4,facet,2); Q(-7,-4,-2,-8,facet,2); Q(-2,-8,4,-7,facet,2);
            poly({{5,-9},{9,-6},{5,-3}},sparkle,outline,1); dot(0,0,2,white); break;
        case 22: // Heart of Last Echo
            ring(0,0,9,1,accent); poly({{-8,-3},{-7,-7},{-3,-8},{0,-5},{3,-8},{7,-7},{8,-3},{6,2},{0,9},{-6,2}},body,outline,1);
            Q(-4,-4,-1,-5,sparkle,1); break;
        case 23: // Veil of Interworlds
            poly({{0,-10},{7,-6},{7,3},{0,9},{-7,3},{-7,-6}},body,outline,1);
            Q(0,-7,0,6,facet,1); Q(-5,-2,0,-5,sparkle,1); Q(0,5,5,1,accent,1); break;
        case 24: // Comet fragment
            poly({{-9,8},{-4,-3},{5,-9},{10,-7},{8,-2},{0,4}},body,outline,1);
            Q(-10,10,-3,4,facet,2); Q(-8,10,-2,7,sparkle,1); dot(5,-5,2,white); break;
        case 25: // Runic turn
            ring(0,0,7,2,body); poly({{0,-9},{3,-4},{1,-2},{-3,-4}},facet,outline,1);
            poly({{9,0},{4,3},{2,1},{4,-3}},sparkle,outline,1); poly({{0,9},{-3,4},{-1,2},{3,4}},body,outline,1);
            poly({{-9,0},{-4,-3},{-2,-1},{-4,3}},facet,outline,1); dot(0,0,2,white); break;
        case 26: // Rift spear
            Q(-8,8,6,-6,outline,4); Q(-8,8,6,-6,facet,2);
            poly({{4,-7},{7,-10},{10,-7},{7,1},{4,0},{2,-3}},body,outline,1); Q(5,-6,8,-8,sparkle,1); break;
        case 27: // Ash star
            poly({{0,-10},{2,-3},{9,-6},{5,0},{10,3},{3,4},{2,10},{-1,5},{-8,8},{-5,1},{-10,-2},{-3,-3}},body,outline,1);
            poly({{0,-5},{2,-1},{5,0},{1,2},{0,6},{-2,2},{-5,1},{-1,-1}},facet,outline,1); dot(0,0,2,white); break;
        case 28: // Evasion mirror: oval face and centered vertical grip, distinct from the Rift Lens.
            poly({{-5,-9},{0,-10},{5,-8},{8,-4},{8,1},{5,5},{0,6},{-5,4},{-8,0},{-8,-5}},body,outline,1);
            poly({{-4,-7},{0,-8},{4,-6},{6,-3},{6,0},{2,3},{-3,2},{-6,-1},{-5,-5}},facet,outline,1);
            Q(0,6,0,10,body,3); Q(-3,10,3,10,sparkle,1); Q(-2,-5,1,-1,white,1); break;
        case 29: // Revelation seal
            ring(0,0,8,1,accent); ring(0,0,5,2,body); poly({{0,-6},{2,-1},{0,2},{-2,-1}},facet,outline,1); dot(0,0,2,white); break;
        case 30: // Overflow chalice
            poly({{-7,-8},{7,-8},{5,-1},{3,1},{3,5},{7,7},{-7,7},{-3,5},{-3,1},{-5,-1}},body,outline,1);
            Q(-5,-5,5,-5,sparkle,1); Q(-3,8,3,8,facet,2); dot(0,-9,2,white); break;
        case 31: // Last strike shard: a long decisive splinter with a single cut face.
            poly({{-9,7},{-6,-2},{3,-10},{7,-9},{5,-1},{9,3},{2,8},{-2,5}},body,outline,1);
            poly({{-6,-2},{3,-10},{7,-9},{1,0}},facet,outline,1); Q(-7,5,1,-1,sparkle,1); break;
        case 32: // Ashen Rhythm key
            ring(-4,-4,4,2,body); ring(-4,-4,2,1,facet); Q(-1,-1,8,8,body,3); Q(-1,-1,8,8,sparkle,1);
            Q(5,5,9,5,facet,2); Q(7,5,7,9,facet,2); dot(7,-7,2,0xFFFFD87A); break;
        case 33: // Crossfire prism
            poly({{-7,6},{0,-8},{7,6}},body,outline,1); poly({{-3,4},{0,-3},{4,4}},facet,outline,1);
            Q(-10,0,-5,0,sparkle,1); Q(6,-1,10,-5,accent,1); Q(6,1,10,5,sparkle,1); dot(0,0,2,white); break;
        case 34: // Ember wick
            Q(0,9,0,1,body,3); Q(-4,9,4,9,facet,2);
            poly({{0,-9},{3,-5},{2,-2},{6,-4},{5,1},{2,4},{-3,2},{-5,-1},{-2,-4}},body,outline,1);
            poly({{0,-4},{2,-1},{0,2},{-2,0}},facet,outline,1); break;
        case 35: // Thaw shard
            poly({{0,-10},{6,-5},{5,0},{8,5},{2,9},{-4,7},{-8,2},{-5,-4}},body,outline,1);
            poly({{-4,-5},{0,-7},{2,-2},{-1,2},{-5,1}},facet,outline,1); Q(0,-2,4,3,sparkle,1); break;
        case 36: // Reversal current: shielded deflection with a returning trajectory, not another circular rune.
            poly({{-7,-7},{0,-10},{7,-7},{8,1},{4,7},{0,10},{-5,6},{-8,1}},body,outline,1);
            Q(-5,-3,-1,-3,facet,2); Q(-1,-3,-2,2,sparkle,2); Q(-2,2,4,2,facet,2); Q(4,2,7,-1,sparkle,2);
            poly({{5,-4},{9,-1},{5,2}},sparkle,outline,1); break;
        case 37: // Execution mark
            poly({{0,-10},{7,-4},{5,3},{0,9},{-6,3},{-7,-4}},body,outline,1);
            Q(-1,-7,4,3,sparkle,2); Q(-4,-4,4,-4,facet,1); Q(-8,5,-4,5,accent,1); break;
        case 38: // Vengeance seal: heart-bearing shield and riposte slash, distinct from the Echo Heart.
            poly({{0,-10},{8,-6},{7,3},{0,10},{-7,3},{-8,-6}},body,outline,1);
            poly({{-5,-2},{-4,-5},{-1,-5},{0,-3},{2,-5},{5,-4},{5,-1},{0,4},{-4,1}},facet,outline,1);
            Q(-7,5,6,-6,sparkle,2); dot(6,-6,1,white); break;
        case 39: // Witness rune
            poly({{0,-10},{7,-4},{5,4},{0,10},{-6,5},{-8,-2}},body,outline,1);
            poly({{0,-5},{3,-1},{1,2},{-2,5},{-3,0}},facet,outline,1);
            ring(0,0,9,1,accent); dot(0,0,1,white); break;
        default: break;
        }
        switch (id) {
        case 0: { // Chosen ice-cyan blades; the grips continue directly from each heel on the same axis.
            const uint32_t bladeFill = 0xFF1E799A, bladeEdge = 0xFF12465F;
            const uint32_t bladeHi = 0xFF59D0E5, bladeGlow = 0xFFA1F8FF;
            // Handles are laid down first; the blade heels overlap their roots for a continuous join.
            poly({{3,4},{5,2},{10,7},{10,10},{7,10},{3,6}},0xFF20546B,bladeEdge,1);
            poly({{-3,4},{-5,2},{-10,7},{-10,10},{-7,10},{-3,6}},0xFF20546B,bladeEdge,1);
            Q(5,5,9,9,bladeHi,1); Q(-5,5,-9,9,bladeHi,1);
            Q(5,7,7,5,bladeGlow,1); Q(-5,7,-7,5,bladeGlow,1);
            Q(7,9,9,7,bladeGlow,1); Q(-7,9,-9,7,bladeGlow,1);
            poly({{8,9},{9,8},{10,9},{9,10}},bladeGlow,bladeEdge,1);
            poly({{-8,9},{-9,8},{-10,9},{-9,10}},bladeGlow,bladeEdge,1);
            poly({{-10,-10},{-7,-9},{5,3},{4,5},{1,4},{-9,-4}},bladeFill,bladeEdge,1);
            poly({{10,-10},{7,-9},{-5,3},{-4,5},{-1,4},{9,-4}},bladeFill,bladeEdge,1);
            poly({{-9,-9},{-7,-8},{2,2},{0,2}},bladeHi,bladeEdge,1);
            poly({{9,-9},{7,-8},{-2,2},{0,2}},bladeHi,bladeEdge,1);
            Q(-8,-8,1,1,bladeGlow,1); Q(8,-8,-1,1,bladeGlow,1);
            // Tiny ferrules meet the blade heels; the cyan center gem is shared by both blades.
            Q(2,3,4,5,bladeGlow,2); Q(-2,3,-4,5,bladeGlow,2);
            poly({{-2,0},{0,-2},{2,0},{0,2}},bladeGlow,bladeEdge,1);
            dot(0,0,1,white);
            break;
        }
        case 1: // Fireball with a bright core and three licking flames.
            drawCircleRing(cx,cy,size/3,thin,0xFFFF923D,230,true);
            L(cx-size/4,cy+size/5,cx-size/5,cy-size/5,0xFFFF6A28,t+1);
            L(cx-size/5,cy-size/5,cx,cy-size/2,0xFFFFD36B,t+1);
            L(cx,cy-size/2,cx+size/7,cy-size/5,0xFFFF7A24,t+1);
            L(cx+size/7,cy-size/5,cx+size/3,cy-size/3,0xFFFFCA54,t+1);
            L(cx+size/3,cy-size/3,cx+size/4,cy+size/5,0xFFFF6A28,t+1);
            L(cx+size/4,cy+size/5,cx,cy+size/2,0xFFFFA43A,t+1);
            L(cx,cy+size/2,cx-size/4,cy+size/5,0xFFFF6A28,t+1);
            C(cx,cy,std::max(3,size/5),0xFFFFB83E);
            C(cx,cy,std::max(1,size/10),0xFFFFF4C2);
            break;
        case 2: { // Three orbiting runes around the vortex core.
            drawCircleRing(cx,cy,size/2,thin,0xFF47DDF4,190,true);
            drawCircleRing(cx,cy,size/4,thin,0xFFB0F8FF,220,true);
            const int r=size/3;
            for(int k=0;k<3;++k){float a=-1.5707963f+k*2.0943951f;int x=cx+(int)(cosf(a)*r),y=cy+(int)(sinf(a)*r);C(x,y,std::max(2,size/8),k==1?0xFF90F4FF:0xFF40BDE0);C(x,y,1,white);}
            C(cx,cy,std::max(2,size/7),0xFF83ECFF);
            break;
        }
        case 3: // Angular lightning bolt with a white-hot center.
            L(cx+size/8,cy-size/2,cx-size/4,cy,0xFF5BD7FF,t+2);
            L(cx-size/4,cy,cx,cy-size/12,white,t);
            L(cx,cy-size/12,cx-size/8,cy+size/2,0xFF42B8FF,t+2);
            L(cx-size/8,cy+size/2,cx+size/5,cy-size/8,0xFF67E7FF,thin);
            C(cx-size/3,cy-size/3,thin,0xFFB9F6FF); C(cx+size/3,cy+size/4,thin,0xFFB9F6FF);
            break;
        case 4: // Blood aura: a drop contained by a hot circular ward.
            drawCircleRing(cx,cy,size/2,thin,0xFFFF3F64,220,true);
            drawCircleRing(cx,cy,size/3,thin,0xFFB92348,210,true);
            L(cx,cy-size/3,cx-size/5,cy+size/12,0xFFFF8090,t+1);
            L(cx-size/5,cy+size/12,cx,cy+size/3,0xFFFF4E68,t+1);
            L(cx,cy+size/3,cx+size/5,cy+size/12,0xFFFF8090,t+1);
            L(cx+size/5,cy+size/12,cx,cy-size/3,0xFFFF4E68,t+1);
            C(cx,cy,std::max(2,size/10),0xFFFFB7C2);
            break;
        case 5: // Broad crescent axe head on a wooden haft, with a bright cutting edge.
            L(cx-size/2,cy+size/2,cx+size/8,cy-size/8,0xFFB87E4B,t+1);
            L(cx-size/4,cy+size/4,cx-size/3,cy+size/2,0xFFFFD18A,t);
            L(cx+size/8,cy-size/8,cx+size/4,cy-size/2,0xFFFFD18A,t+1);
            L(cx+size/4,cy-size/2,cx+size/2,cy-size/2,0xFFE7EDF6,t+1);
            L(cx+size/2,cy-size/2,cx+size/2,cy+size/8,0xFFE7EDF6,t+1);
            L(cx+size/2,cy+size/8,cx+size/4,cy+size/4,0xFF9FC9E6,t+1);
            L(cx+size/4,cy+size/4,cx+size/8,cy-size/8,0xFFFFD18A,t);
            L(cx+size/8,cy-size/8,cx-size/8,cy+size/8,0xFFFFE7AA,t);
            C(cx+size/8,cy-size/8,std::max(2,size/9),0xFFFFE7AA);
            break;
        case 6: // Frost ring: a crystalline snowflake within an expanding circle.
            drawCircleRing(cx,cy,size/2,thin,0xFF5DDCFF,220,true);
            L(cx,cy-size/3,cx,cy+size/3,0xFFD8FBFF,t);
            L(cx-size/3,cy,cx+size/3,cy,0xFF79E6FF,t);
            L(cx-size/4,cy-size/4,cx+size/4,cy+size/4,white,thin);
            L(cx-size/4,cy+size/4,cx+size/4,cy-size/4,0xFF9AF0FF,thin);
            C(cx,cy,std::max(2,size/9),white);
            break;
        case 7: // Open spellbook with gold spine and ruled pages.
            L(cx,cy-size/3,cx-size/2,cy-size/2,0xFFFFD88A,t);
            L(cx-size/2,cy-size/2,cx-size/2,cy+size/3,0xFFB78345,t);
            L(cx-size/2,cy+size/3,cx,cy+size/2,0xFFFFD88A,t);
            L(cx,cy+size/2,cx+size/2,cy+size/3,0xFFFFD88A,t);
            L(cx+size/2,cy+size/3,cx+size/2,cy-size/2,0xFFB78345,t);
            L(cx+size/2,cy-size/2,cx,cy-size/3,0xFFFFD88A,t);
            L(cx,cy-size/3,cx,cy+size/2,0xFFFFF0B2,thin);
            for(int k=0;k<3;++k){int y=cy-size/5+k*size/6;L(cx-size/3,y,cx-size/10,y+size/24,0xFFE7D9B9,thin);L(cx+size/10,y+size/24,cx+size/3,y,0xFFE7D9B9,thin);}
            C(cx,cy-size/10,std::max(1,size/12),0xFFB77CFF);
            break;
        case 8: // Wind boots: two angled ankle boots with distinct cuffs, laces and lifted soles.
            L(cx-size/2,cy+size/3,cx-size/8,cy+size/3,0xFFECFFF2,t+1);
            L(cx+size/10,cy+size/5,cx+size/2,cy+size/5,0xFFECFFF2,t+1);
            L(cx-size/3,cy-size/4,cx-size/3,cy,0xFFB8F0D2,t);
            L(cx-size/3,cy-size/4,cx-size/6,cy-size/4,0xFFB8F0D2,thin);
            L(cx-size/3,cy-size/10,cx-size/6,cy-size/8,white,thin);
            L(cx-size/3,cy,cx-size/6,cy-size/12,white,thin);
            L(cx+size/4,cy-size/3,cx+size/4,cy-size/8,0xFFD9FFE7,t);
            L(cx+size/4,cy-size/3,cx+size/2,cy-size/3,0xFFD9FFE7,thin);
            L(cx+size/4,cy-size/5,cx+size/2,cy-size/4,white,thin);
            L(cx+size/4,cy-size/10,cx+size/2,cy-size/6,white,thin);
            L(cx-size/2,cy-size/2,cx-size/3,cy-size/2,white,thin);
            L(cx-size/2,cy+size/2,cx-size/3,cy+size/2,0xFF60EAA0,thin);
            break;
        case 9: // Horseshoe magnet with opposite poles.
            L(cx-size/3,cy-size/3,cx-size/3,cy+size/8,0xFFFF626B,t+2);
            L(cx-size/3,cy+size/8,cx-size/5,cy+size/3,white,t+2);
            L(cx-size/5,cy+size/3,cx+size/5,cy+size/3,white,t+2);
            L(cx+size/5,cy+size/3,cx+size/3,cy+size/8,white,t+2);
            L(cx+size/3,cy+size/8,cx+size/3,cy-size/3,0xFF65BFFF,t+2);
            L(cx-size/2,cy-size/3,cx-size/3,cy-size/3,0xFFFFD6DF,t+1);
            L(cx+size/3,cy-size/3,cx+size/2,cy-size/3,0xFFD5F0FF,t+1);
            break;
        case 10: // Full red titan heart, framed by two gold armor plates.
            C(cx-size/6,cy-size/7,std::max(3,size/4),0xFFFF6078);
            C(cx+size/6,cy-size/7,std::max(3,size/4),0xFFFF6078);
            L(cx-size/3,cy-size/8,cx,cy+size/2,0xFFFF8294,t+1);
            L(cx+size/3,cy-size/8,cx,cy+size/2,0xFFDF324E,t+1);
            L(cx-size/2,cy-size/3,cx-size/3,cy-size/8,0xFFFFD486,t);
            L(cx-size/3,cy-size/8,cx-size/2,cy+size/3,0xFFFFD486,t);
            L(cx-size/2,cy+size/3,cx-size/3,cy+size/2,0xFFFFD486,t);
            L(cx+size/2,cy-size/3,cx+size/3,cy-size/8,0xFFFFD486,t);
            L(cx+size/3,cy-size/8,cx+size/2,cy+size/3,0xFFFFD486,t);
            L(cx+size/2,cy+size/3,cx+size/3,cy+size/2,0xFFFFD486,t);
            C(cx,cy-size/8,std::max(1,size/12),0xFFFFEDAF);
            break;
        case 11: // Berserker gauntlet with plated knuckles.
            fillRoundedRect(cx-size/3,cy+size/8,size*2/3,size/3,3,0xFFB8793A);
            L(cx-size/3,cy+size/8,cx+size/3,cy+size/8,0xFFFFD780,t);
            for(int k=0;k<4;++k)fillRoundedRect(cx-size/3+k*(size/6),cy-size/3,size/6+2,size/2,3,k%2?0xFFE9AA55:0xFFFFC96A);
            L(cx+size/4,cy,cx+size/2,cy+size/6,0xFFFFD780,t+1);
            C(cx-size/4,cy-size/4,std::max(1,size/12),white);
            break;
        case 12: // Hourglass: sand falls through a narrow waist.
            L(cx-size/3,cy-size/2,cx+size/3,cy-size/2,0xFFE5D0FF,t);
            L(cx-size/3,cy+size/2,cx+size/3,cy+size/2,0xFFE5D0FF,t);
            L(cx-size/3,cy-size/2,cx+size/3,cy+size/2,0xFFC28AFF,t);
            L(cx+size/3,cy-size/2,cx-size/3,cy+size/2,0xFFC28AFF,t);
            L(cx-size/4,cy-size/3,cx+size/4,cy-size/3,0xFFFFE6A0,t);
            L(cx-size/4,cy+size/3,cx+size/4,cy+size/3,0xFFFFE6A0,t);
            C(cx,cy,std::max(1,size/10),0xFFFFCF72);
            L(cx,cy+size/8,cx,cy+size/3,0xFFFFE79B,thin);
            break;
        case 13: // Cursed coin with a split rune.
            drawCircleRing(cx,cy,size/2,t,0xFFFFC34E,245,true);
            drawCircleRing(cx,cy,size/3,thin,0xFFFFE18A,220,true);
            L(cx-size/5,cy-size/4,cx+size/5,cy-size/4,0xFF9B552B,t);
            L(cx+size/5,cy-size/4,cx+size/5,cy+size/4,0xFF9B552B,t);
            L(cx+size/5,cy+size/4,cx-size/5,cy+size/4,0xFF9B552B,t);
            L(cx-size/5,cy+size/4,cx-size/5,cy-size/4,0xFF9B552B,t);
            L(cx,cy-size/3,cx-size/9,cy,0xFFFFF3BD,thin);
            L(cx-size/9,cy,cx+size/8,cy+size/3,0xFFFFF3BD,thin);
            C(cx,cy,std::max(1,size/14),0xFFFFA82D);
            break;
        case 14: // Phoenix ash: lifted wings, bright head and three falling ember-feathers.
            L(cx,cy+size/3,cx,cy-size/3,0xFFFFD889,t+1);
            L(cx-size/8,cy-size/8,cx-size/2,cy-size/3,0xFFFF7840,t+1);
            L(cx-size/2,cy-size/3,cx-size/3,cy+size/8,0xFFFFA53D,t);
            L(cx+size/8,cy-size/8,cx+size/2,cy-size/3,0xFFFFA53D,t+1);
            L(cx+size/2,cy-size/3,cx+size/3,cy+size/8,0xFFFFC34A,t);
            L(cx,cy+size/4,cx-size/4,cy+size/2,0xFFFF6846,t);
            L(cx,cy+size/4,cx+size/4,cy+size/2,0xFFFFC34A,t);
            C(cx,cy-size/3,std::max(2,size/9),0xFFFFF2B0);
            break;
        case 15: // Eye of the Abyss, with a slit pupil inside an iris.
            L(cx-size/2,cy,cx-size/5,cy-size/4,0xFFD3B0FF,t);
            L(cx-size/5,cy-size/4,cx+size/5,cy-size/4,0xFF9D72FF,t);
            L(cx+size/5,cy-size/4,cx+size/2,cy,0xFFD3B0FF,t);
            L(cx+size/2,cy,cx+size/5,cy+size/4,0xFF9D72FF,t);
            L(cx+size/5,cy+size/4,cx-size/5,cy+size/4,0xFFD3B0FF,t);
            L(cx-size/5,cy+size/4,cx-size/2,cy,0xFF9D72FF,t);
            drawCircleRing(cx,cy,size/5,thin,0xFFE0CEFF,255,true);
            C(cx,cy,std::max(2,size/9),0xFFB94FFF); C(cx,cy,std::max(1,size/18),white);
            break;
        case 16: // Ash-heart seal: hexagonal ward around a burning core.
            L(cx,cy-size/2,cx+size/2,cy-size/4,0xFFFFCE74,thin); L(cx+size/2,cy-size/4,cx+size/2,cy+size/4,0xFFFFCE74,thin);
            L(cx+size/2,cy+size/4,cx,cy+size/2,0xFFFFCE74,thin); L(cx,cy+size/2,cx-size/2,cy+size/4,0xFFFFCE74,thin);
            L(cx-size/2,cy+size/4,cx-size/2,cy-size/4,0xFFFFCE74,thin); L(cx-size/2,cy-size/4,cx,cy-size/2,0xFFFFCE74,thin);
            C(cx,cy+2,std::max(3,size/5),0xFFFF743F); L(cx,cy+4,cx,cy-size/3,0xFFFFE79B,thin);
            break;
        case 17: // Blood shard: a clean ruby silhouette with three cut facets.
            L(cx,cy-size/2,cx+size/3,cy-size/5,0xFFFFB2C0,t+1);
            L(cx+size/3,cy-size/5,cx+size/4,cy+size/4,0xFFDE315C,t+1);
            L(cx+size/4,cy+size/4,cx,cy+size/2,0xFFFF6C8D,t+1);
            L(cx,cy+size/2,cx-size/3,cy+size/5,0xFFB9224C,t+1);
            L(cx-size/3,cy+size/5,cx-size/3,cy-size/5,0xFFFF6B8A,t+1);
            L(cx-size/3,cy-size/5,cx,cy-size/2,0xFFFFB8C8,t+1);
            L(cx-size/3,cy-size/5,cx,cy-size/10,white,thin);
            L(cx,cy-size/10,cx+size/4,cy+size/4,0xFFFFD5DE,thin);
            L(cx,cy-size/10,cx-size/5,cy+size/5,0xFFFFD5DE,thin);
            break;
        case 18: // Rift lens with a clear glass aperture and handle.
            drawCircleRing(cx-size/10,cy-size/10,size/3,std::max(2,size/8),0xFF9DEEFF,255,true);
            C(cx-size/10,cy-size/10,std::max(3,size/7),0xFF63CBEA,235); C(cx-size/10,cy-size/10,std::max(1,size/14),white);
            L(cx+size/8,cy+size/8,cx+size/2,cy+size/2,0xFFE2D7FF,std::max(3,size/7));
            break;
        case 19: // Last Dawn oath: broad emerald shield with a gold three-point crown.
            fillRoundedRect(cx-size/3,cy-size/3,size*2/3,size*2/3,4,0xFF286A50);
            L(cx,cy-size/2,cx+size/3,cy-size/3,0xFFB7FFD0,t); L(cx+size/3,cy-size/3,cx+size/3,cy+size/8,0xFF72E5A0,t);
            L(cx+size/3,cy+size/8,cx,cy+size/2,0xFFB7FFD0,t); L(cx,cy+size/2,cx-size/3,cy+size/8,0xFF72E5A0,t);
            L(cx-size/3,cy+size/8,cx-size/3,cy-size/3,0xFFB7FFD0,t); L(cx-size/3,cy-size/3,cx,cy-size/2,0xFF72E5A0,t);
            L(cx-size/4,cy-size/8,cx-size/8,cy-size/3,0xFFFFE98A,t); L(cx-size/8,cy-size/3,cx,cy-size/6,0xFFFFE98A,t);
            L(cx,cy-size/6,cx+size/8,cy-size/3,0xFFFFE98A,t); L(cx+size/8,cy-size/3,cx+size/4,cy-size/8,0xFFFFE98A,t);
            L(cx-size/4,cy-size/8,cx+size/4,cy-size/8,0xFFFFE98A,t);
            C(cx,cy+size/8,std::max(2,size/10),0xFFFFE98A);
            break;
        case 20: // Hunter's brand: reticle pierced by a hunting arrow.
            drawCircleRing(cx,cy,size/3,std::max(2,size/9),0xFFFFDA76,255,true);
            L(cx-size/2,cy+size/2,cx+size/3,cy-size/3,0xFFFFEAA3,t);
            L(cx+size/3,cy-size/3,cx+size/2,cy-size/2,white,thin);
            L(cx+size/3,cy-size/3,cx+size/5,cy-size/10,0xFFFFEAA3,thin);
            L(cx-size/2,cy,cx-size/3,cy,0xFFFFDA76,thin); L(cx+size/3,cy,cx+size/2,cy,0xFFFFDA76,thin);
            break;
        case 21: // Returning rune: loop-arrow circling back to its source.
            drawCircleRing(cx,cy,size/3,std::max(2,size/9),0xFFB8FFF1,255,true);
            L(cx+size/8,cy-size/3,cx+size/2,cy-size/3,0xFFB8FFF1,t);
            L(cx+size/2,cy-size/3,cx+size/3,cy-size/10,0xFFB8FFF1,t);
            L(cx+size/2,cy-size/3,cx+size/2,cy+size/12,0xFFB8FFF1,t);
            C(cx-size/3,cy+size/3,std::max(2,size/9),0xFFFFF2C1);
            break;
        case 22: // Echo heart: oversized lobes, pointed base, and an audible ripple.
            C(cx-size/6,cy-size/8,std::max(3,size/4),0xFFFF8FA6,250);
            C(cx+size/6,cy-size/8,std::max(3,size/4),0xFFFFA4B2,250);
            L(cx-size/3,cy-size/8,cx,cy+size/2,0xFFFFA4B2,t+1);
            L(cx+size/3,cy-size/8,cx,cy+size/2,0xFFFFD9E0,t+1);
            drawCircleRing(cx,cy,size/2,thin,0xFFFF8FA6,170,true);
            drawCircleRing(cx,cy,size/3,thin,0xFFFFB6C5,130,true);
            C(cx,cy,std::max(1,size/12),white);
            break;
        case 23: // Interworld veil: a protective shield with a translucent split.
            L(cx,cy-size/2,cx+size/3,cy-size/3,0xFFD5CAFF,t); L(cx+size/3,cy-size/3,cx+size/3,cy+size/8,0xFFB99BFF,t);
            L(cx+size/3,cy+size/8,cx,cy+size/2,0xFFE6DEFF,t); L(cx,cy+size/2,cx-size/3,cy+size/8,0xFFB99BFF,t);
            L(cx-size/3,cy+size/8,cx-size/3,cy-size/3,0xFFD5CAFF,t); L(cx-size/3,cy-size/3,cx,cy-size/2,0xFFE6DEFF,t);
            L(cx,cy-size/3,cx,cy+size/3,0xFF8E80C0,thin,230); L(cx-size/5,cy,cx+size/5,cy,0xFFE8E0FF,thin,220);
            break;
        case 24: // Comet shard: bright head with three directional tails.
            L(cx-size/2,cy+size/3,cx+size/3,cy-size/3,0xFF6BE8FF,t);
            L(cx-size/3,cy+size/2,cx+size/3,cy-size/3,0xFFB8F7FF,thin);
            L(cx-size/2,cy+size/6,cx+size/3,cy-size/3,0xFF6BE8FF,thin,220);
            C(cx+size/3,cy-size/3,std::max(3,size/7),white);
            break;
        case 25: // Rune Turn: four radial blades around a bright pulse.
            drawCircleRing(cx,cy,size/3,std::max(2,size/9),0xFFB9F5FF,245,true); C(cx,cy,std::max(2,size/8),white,250);
            L(cx,cy-size/2,cx,cy-size/3,0xFF69E8FF,t+1); L(cx+size/3,cy,cx+size/2,cy,0xFF69E8FF,t+1);
            L(cx,cy+size/3,cx,cy+size/2,0xFF69E8FF,t+1); L(cx-size/2,cy,cx-size/3,cy,0xFF69E8FF,t+1);
            C(cx,cy-size/2,std::max(1,size/12),0xFFFFE67A); C(cx+size/2,cy,std::max(1,size/12),0xFFFFE67A);
            break;
        case 26: // Rift spear: faceted spearhead, shaft and counterweight.
            L(cx-size/2,cy+size/2,cx+size/3,cy-size/3,0xFFDCC8FF,t+1);
            L(cx+size/3,cy-size/3,cx+size/2,cy-size/2,white,t);
            L(cx+size/2,cy-size/2,cx+size/5,cy-size/3,0xFFC48AFF,t);
            L(cx+size/5,cy-size/3,cx+size/3,cy-size/3,0xFFE9DFFF,thin);
            L(cx-size/2,cy+size/2,cx-size/5,cy+size/3,0xFF9B68E8,t);
            L(cx-size/3,cy+size/6,cx-size/6,cy+size/3,0xFFC48AFF,thin);
            break;
        case 27: // Four-point ash star with a separate falling meteor trail.
            L(cx,cy-size/2,cx+size/7,cy-size/8,0xFFFFE49B,t+1);
            L(cx+size/7,cy-size/8,cx+size/2,cy,0xFFFFB34F,t+1);
            L(cx+size/2,cy,cx+size/7,cy+size/8,0xFFFFE49B,t+1);
            L(cx+size/7,cy+size/8,cx,cy+size/2,0xFFFFB34F,t+1);
            L(cx,cy+size/2,cx-size/7,cy+size/8,0xFFFFE49B,t+1);
            L(cx-size/7,cy+size/8,cx-size/2,cy,0xFFFFB34F,t+1);
            L(cx-size/2,cy,cx-size/7,cy-size/8,0xFFFFE49B,t+1);
            L(cx-size/7,cy-size/8,cx,cy-size/2,0xFFFFB34F,t+1);
            L(cx-size/2,cy+size/2,cx-size/5,cy+size/5,0xFFFF763C,t);
            L(cx-size/4,cy+size/2,cx,cy+size/4,white,thin);
            C(cx,cy,std::max(2,size/10),0xFFFFF1BF);
            break;
        case 28: // Evasion mirror: oval face, centered stem and a split reflection.
            L(cx,cy+size/5,cx,cy+size/2,0xFFB8FFF1,t+1);
            L(cx-size/8,cy+size/2,cx+size/8,cy+size/2,0xFF8AF5E9,thin);
            L(cx-size/8,cy-size/4,cx+size/8,cy-size/8,white,thin);
            L(cx+size/8,cy-size/8,cx-size/8,cy+size/8,0xFF8AF5E9,thin);
            C(cx,cy-size/10,std::max(1,size/12),white);
            break;
        case 29: // Exposed weak point: a cracked target with three impact cuts, unlike the hunter's arrow.
            drawCircleRing(cx,cy,size/2,thin,0xFFBD83FF,210,true);
            drawCircleRing(cx,cy,size/4,thin,0xFFFFD9FF,240,true);
            L(cx-size/3,cy-size/5,cx-size/8,cy-size/10,0xFFE8C8FF,t);
            L(cx-size/8,cy-size/10,cx-size/4,cy+size/5,0xFFBD83FF,t);
            L(cx+size/3,cy-size/5,cx+size/8,cy-size/10,0xFFE8C8FF,t);
            L(cx+size/8,cy-size/10,cx+size/4,cy+size/5,0xFFBD83FF,t);
            C(cx,cy,std::max(2,size/9),white);
            break;
        case 30: // Overflow chalice: cup, falling healing drop and protective base.
            L(cx-size/3,cy-size/3,cx-size/4,cy+size/8,0xFFFFD878,t);
            L(cx-size/4,cy+size/8,cx+size/4,cy+size/8,0xFFFFE9AD,t);
            L(cx+size/4,cy+size/8,cx+size/3,cy-size/3,0xFFFFD878,t);
            L(cx-size/3,cy-size/3,cx+size/3,cy-size/3,0xFFFFE9AD,t);
            L(cx,cy+size/8,cx,cy+size/3,0xFFFFD878,t);
            L(cx-size/5,cy+size/3,cx+size/5,cy+size/3,0xFFFFE9AD,t);
            L(cx-size/3,cy+size/2,cx,cy+size/3,0xFFFFD878,thin); L(cx,cy+size/3,cx+size/3,cy+size/2,0xFFFFD878,thin);
            C(cx,cy-size/2,std::max(2,size/9),0xFFFFF5C8);
            break;
        case 31: // Last strike splinter: one bright fracture and three detached chips.
            L(cx-size/4,cy+size/3,cx+size/3,cy-size/3,0xFFFF9B58,t+1);
            L(cx+size/3,cy-size/3,cx+size/2,cy-size/2,0xFFFFE2A8,t);
            L(cx-size/4,cy+size/3,cx+size/8,cy+size/4,0xFFFFE2A8,thin);
            L(cx-size/2,cy-size/4,cx-size/3,cy-size/2,0xFFFFA64D,t);
            L(cx+size/4,cy-size/2,cx+size/2,cy-size/3,0xFFFFA64D,t);
            L(cx+size/3,cy+size/4,cx+size/2,cy+size/2,0xFFFFA64D,t);
            C(cx+size/4,cy-size/5,std::max(1,size/12),white);
            break;
        case 32: // Ashen Rhythm Key: key teeth and charged XP sparks, no hourglass silhouette.
            drawCircleRing(cx-size/4,cy-size/4,size/5,std::max(2,size/10),0xFFB6FFF0,245,true);
            C(cx-size/4,cy-size/4,std::max(1,size/13),0xFFFFD98A);
            L(cx-size/8,cy-size/8,cx+size/2,cy+size/2,0xFF67E7D4,t+1);
            L(cx+size/8,cy+size/8,cx+size/3,cy+size/8,0xFFB6FFF0,t);
            L(cx+size/4,cy+size/4,cx+size/4,cy+size/2,0xFFB6FFF0,t);
            C(cx+size/3,cy-size/3,std::max(2,size/10),0xFFFFE59A);
            L(cx+size/4,cy-size/2,cx+size/4,cy-size/3,white,thin); L(cx+size/6,cy-size*2/5,cx+size/3,cy-size*2/5,white,thin);
            break;
        case 33: { // Crossfire prism: rays converge through a triangular prism then split.
            const int lx=cx-size/5, rx=cx+size/5, ty=cy-size/3, by=cy+size/3;
            L(lx,by,cx,ty,0xFFB8FFF1,t); L(cx,ty,rx,by,0xFF68E6D8,t); L(rx,by,lx,by,0xFFB8FFF1,t);
            L(cx-size/2,cy,cx-size/5,cy,0xFFFFD98A,t);
            L(rx,cy-size/12,cx+size/2,cy-size/3,0xFF68E6D8,t);
            L(rx,cy+size/12,cx+size/2,cy+size/3,0xFFB8FFF1,t);
            C(cx,cy,std::max(2,size/10),white);
            break;
        }
        case 34: // Ember wick: lit fuse over a coal.
            L(cx-size/5,cy+size/2,cx+size/5,cy+size/2,0xFF9A5940,t+1);
            L(cx,cy+size/2,cx,cy+size/8,0xFFC47A48,t);
            L(cx,cy+size/8,cx-size/8,cy-size/8,0xFFFFA85A,t);
            L(cx-size/8,cy-size/8,cx+size/10,cy-size/2,0xFFFFD28A,t+1);
            L(cx+size/10,cy-size/2,cx+size/3,cy-size/5,0xFFFF9347,t);
            C(cx+size/3,cy-size/5,std::max(3,size/7),0xFFFFC16A); C(cx+size/3,cy-size/5,std::max(1,size/12),white);
            break;
        case 35: // Thaw shard: one fractured ice crystal and two detached splinters.
            L(cx,cy-size/2,cx-size/5,cy-size/8,0xFFB8F5FF,t+1); L(cx-size/5,cy-size/8,cx-size/8,cy+size/4,0xFF72DFFF,t);
            L(cx-size/8,cy+size/4,cx,cy+size/2,0xFFC7FBFF,t+1); L(cx,cy+size/2,cx+size/5,cy+size/8,0xFF72DFFF,t);
            L(cx+size/5,cy+size/8,cx+size/6,cy-size/4,0xFFB8F5FF,t); L(cx+size/6,cy-size/4,cx,cy-size/2,0xFF72DFFF,t);
            L(cx-size/5,cy-size/8,cx+size/10,cy+size/12,white,thin);
            L(cx-size/2,cy-size/5,cx-size/3,cy-size/3,0xFF8BEAFF,thin); L(cx+size/3,cy+size/5,cx+size/2,cy+size/3,0xFF8BEAFF,thin);
            break;
        case 36: // Reversal current: a captured projectile ricochets back out of a ward.
            L(cx-size/3,cy-size/6,cx-size/8,cy-size/6,0xFFBDF8FF,t);
            L(cx-size/8,cy-size/6,cx-size/12,cy+size/8,0xFF72DFFF,t);
            L(cx-size/12,cy+size/8,cx+size/4,cy+size/8,0xFFBDF8FF,t+1);
            L(cx+size/4,cy+size/8,cx+size/3,cy-size/10,0xFF72DFFF,t);
            L(cx+size/3,cy-size/10,cx+size/2,cy-size/4,0xFFBDF8FF,t);
            C(cx-size/3,cy-size/6,std::max(2,size/10),0xFFFF8B74);
            break;
        case 37: // Execution sigil: blade falls through a low-health notch.
            drawCircleRing(cx,cy,size/2,thin,0xFF9C3D42,190,true);
            L(cx-size/4,cy-size/2,cx+size/5,cy+size/3,0xFFFF705E,t+1);
            L(cx+size/5,cy+size/3,cx+size/3,cy+size/8,0xFFFFC4A8,t);
            L(cx-size/3,cy-size/3,cx+size/5,cy-size/3,0xFFFFC4A8,t);
            L(cx+size/5,cy-size/3,cx+size/3,cy-size/8,0xFFFF705E,t);
            L(cx-size/2,cy+size/3,cx-size/4,cy+size/3,0xFFFFD895,thin);
            C(cx+size/5,cy+size/3,std::max(2,size/10),white);
            break;
        case 38: // Vengeance seal: heart inset within a shield, crossed by the riposte.
            L(cx-size/7,cy-size/8,cx-size/14,cy-size/4,0xFFFF795A,t);
            L(cx-size/14,cy-size/4,cx,cy-size/8,0xFFFFB080,t);
            L(cx,cy-size/8,cx+size/10,cy-size/4,0xFFFF795A,t);
            L(cx+size/10,cy-size/4,cx+size/5,cy-size/8,0xFFFFB080,t);
            L(cx+size/5,cy-size/8,cx,cy+size/5,0xFFFF8B61,t);
            L(cx-size/3,cy+size/3,cx+size/3,cy-size/3,white,t);
            C(cx+size/3,cy-size/3,std::max(1,size/12),0xFFFFE0B0);
            break;
        case 39: // Witness rune: a shattered resonator crystal surrounded by three echo arcs, not another eye.
            L(cx,cy-size/2,cx+size/4,cy-size/5,0xFFE9D8FF,t+1); L(cx+size/4,cy-size/5,cx+size/5,cy+size/4,0xFFB98AFF,t);
            L(cx+size/5,cy+size/4,cx,cy+size/2,0xFFD8B8FF,t+1); L(cx,cy+size/2,cx-size/4,cy+size/5,0xFF9B65E8,t);
            L(cx-size/4,cy+size/5,cx-size/5,cy-size/4,0xFFE9D8FF,t); L(cx-size/5,cy-size/4,cx,cy-size/2,0xFFB98AFF,t);
            L(cx,cy-size/4,cx+size/8,cy,white,thin); L(cx+size/8,cy,cx-size/8,cy+size/5,0xFFE3BEFF,thin);
            for(int radius=size/3;radius<=size/2;radius+=std::max(2,size/8)) drawCircleRing(cx,cy,radius,thin,0xFFD7B7FF,150,true);
            C(cx,cy,std::max(2,size/10),0xFFD8B8FF);
            break;
        }
        if (evolved) {
            drawCircleRing(cx, cy, size - 2, 1, 0xFFFFE78A, 195, true);
            fillCircle(cx + size - 5, cy - size + 5, std::max(2, size / 7), 0xFFFFF0A0);
        }
    }

    // Small particles use varied crisp silhouettes instead of repeating soft circles.
    void drawForgeIcon(int cx, int cy, int forgeId, int size = 22, bool evolved = false) {
        const int id = std::max(0, std::min(11, forgeId));
        const uint32_t glow = evolved ? 0xFFFFB820 : 0xFFB8A0FF;
        const uint32_t frameBg = evolved ? 0xFF3A1828 : 0xFF181228;
        const uint32_t frameBorder = evolved ? 0xFFFFD838 : 0xFF685890;
        fillRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, frameBg);
        addGlow(cx, cy, size + (evolved ? 9 : 3), glow, evolved ? 145 : 65);
        const BakedIconImage& image = FORGE_ICONS[id];
        const int side = std::max(1, size * 2 - 8);
        const int left = cx - side / 2, top = cy - side / 2;
        const int corner = std::max(2, side / 11);
        for (int y = 0; y < side; ++y) {
            const int sy = y * image.height / side;
            for (int x = 0; x < side; ++x) {
                int dx = 0, dy = 0;
                if (x < corner) dx = corner - x;
                else if (x >= side - corner) dx = x - (side - corner - 1);
                if (y < corner) dy = corner - y;
                else if (y >= side - corner) dy = y - (side - corner - 1);
                if (dx > 0 && dy > 0 && dx * dx + dy * dy > corner * corner) continue;
                const int sx = x * image.width / side;
                const uint8_t index = image.pixels[sy * image.width + sx];
                putPixel(left + x, top + y, image.palette[index]);
            }
        }
        drawRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, frameBorder, evolved ? 3 : 2);
    }

    void drawParticle(int cx, int cy, int baseSize, uint32_t color, int alpha, int style,
                      float rotation, float stretch, float scale, bool additive) {
        alpha = std::max(0, std::min(255, alpha));
        const int s = std::max(1, (int)(baseSize * scale + 0.5f));
        const bool add = additive && style != 4;
        const float length = std::max(2.0f, s * std::max(0.55f, std::min(2.0f, stretch)));
        const int dx = (int)(cosf(rotation) * length);
        const int dy = (int)(sinf(rotation) * length);
        const uint32_t highlight = lerpColor(color, 0xFFFFFFFF, style == 2 ? 0.62f : 0.48f);
        switch (style) {
        case 0: { // impact star: one long arm, one shorter cross arm
            drawLine(cx - dx, cy - dy, cx + dx, cy + dy, color, std::max(1, s / 2), alpha, add);
            const int px = (int)(-sinf(rotation) * length * 0.58f);
            const int py = (int)( cosf(rotation) * length * 0.58f);
            drawLine(cx - px, cy - py, cx + px, cy + py, color, 1, alpha * 3 / 4, add);
            fillCircle(cx, cy, 1, highlight, alpha);
            break;
        }
        case 1: { // faceted shard / crystal splinter
            const float a = rotation;
            const int tipX = cx + (int)(cosf(a) * length * 1.25f), tipY = cy + (int)(sinf(a) * length * 1.25f);
            const int backX = cx - (int)(cosf(a) * length * 0.85f), backY = cy - (int)(sinf(a) * length * 0.85f);
            const int wingX = (int)(-sinf(a) * std::max(1, s)), wingY = (int)(cosf(a) * std::max(1, s));
            drawLine(tipX, tipY, cx + wingX, cy + wingY, color, 1, alpha, add);
            drawLine(cx + wingX, cy + wingY, backX, backY, color, 1, alpha, add);
            drawLine(backX, backY, cx - wingX, cy - wingY, color, 1, alpha, add);
            drawLine(cx - wingX, cy - wingY, tipX, tipY, color, 1, alpha, add);
            drawLine(tipX, tipY, cx, cy, highlight, 1, alpha, add);
            break;
        }
        case 2: { // warm ember: tapering tail and hot head
            const int tailX = cx - (int)(cosf(rotation) * length * 1.45f);
            const int tailY = cy - (int)(sinf(rotation) * length * 1.45f);
            const int sideX = (int)(-sinf(rotation) * std::max(1, s));
            const int sideY = (int)( cosf(rotation) * std::max(1, s));
            drawLine(tailX + sideX, tailY + sideY, cx, cy, color, std::max(1, s), alpha * 3 / 4, add);
            drawLine(tailX - sideX, tailY - sideY, cx, cy, lerpColor(color, 0xFFFFD48A, 0.42f), std::max(1, s), alpha, add);
            fillCircle(cx, cy, std::max(1, s / 2), highlight, alpha);
            break;
        }
        case 3: { // arcane mote: diamond with a broken orbit cross
            const int px = (int)(-sinf(rotation) * length), py = (int)(cosf(rotation) * length);
            drawLine(cx - dx, cy - dy, cx + dx, cy + dy, highlight, 1, alpha, add);
            drawLine(cx - px, cy - py, cx + px, cy + py, color, 1, alpha * 3 / 4, add);
            fillCircle(cx, cy, std::max(1, s / 2), color, alpha);
            break;
        }
        case 4: { // drifting ash: soft, growing puffs with a short dim streak
            const int smokeAlpha = alpha * 2 / 5;
            fillCircle(cx - dx / 3, cy - dy / 3, s, color, smokeAlpha);
            fillCircle(cx + dx / 3, cy + dy / 3, std::max(1, s - 1), lerpColor(color, 0xFF171322, 0.36f), smokeAlpha);
            drawLine(cx - dx, cy - dy, cx, cy, color, 1, smokeAlpha, false);
            break;
        }
        default: { // projectile trail: narrow, directional streak
            drawLine(cx - dx, cy - dy, cx + dx / 2, cy + dy / 2, color, std::max(1, s / 2), alpha * 3 / 4, add);
            drawLine(cx, cy, cx + dx / 2, cy + dy / 2, highlight, 1, alpha, add);
            break;
        }
        }
    }

    void drawPickupSprite(int type, int cx, int cy, float animTime) {
        static const int sizes[8] = {18,20,24,19,26,26,38,52};
        const int id = std::max(0, std::min(7, type));
        const int frame = ((int)(animTime * 4.0f)) & 1;
        const int size = sizes[id];
        drawBakedSpriteScaled(PICKUP_SPRITES[id][frame], cx - size / 2, cy - size / 2, size, size, 255);
    }

    void drawResonatorSprite(int cx, int cy, float animTime, bool hitFlash = false) {
        const int frame = ((int)(animTime * 3.0f)) & 1;
        drawBakedSprite(RESONATOR_SPRITES[frame], cx, cy - 4, false, hitFlash, false, 255);
    }

};
