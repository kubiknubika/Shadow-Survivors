#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
#include "font_atlas.h"
#include "sprite_atlas.h"
#include "skill_icon_atlas.h"
#include "forge_icon_atlas.h"

static int SCREEN_W = 1280;
static int SCREEN_H = 720;

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

struct Renderer {
    uint32_t* pixels;

    int width = 0;
    int height = 0;
    // 0 = compact vector icons, 1 = AI-rendered art for the two approved skill variants.
    int skillIconStyle = 0;

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
    }

    void clear(uint32_t col) {
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) pixels[i] = col;
    }

    inline void putPixel(int x, int y, uint32_t col) {
        if ((unsigned)x < (unsigned)SCREEN_W && (unsigned)y < (unsigned)SCREEN_H) {
            pixels[y * SCREEN_W + x] = col;
        }
    }

    inline void blendPixel(int x, int y, uint32_t col, int alpha) {
        if ((unsigned)x >= (unsigned)SCREEN_W || (unsigned)y >= (unsigned)SCREEN_H) return;
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
        if (x0 > x1) return;
        uint32_t* row = pixels + y * SCREEN_W;
        if (alpha >= 255) {
            std::fill(row + x0, row + x1 + 1, col);
            return;
        }
        const int sr = (col >> 16) & 0xFF, sg = (col >> 8) & 0xFF, sb = col & 0xFF;
        const int inv = 255 - alpha;
        for (int x = x0; x <= x1; ++x) {
            const uint32_t dst = row[x];
            const int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
            row[x] = 0xFF000000u | (((sr * alpha + dr * inv) >> 8) << 16)
                   | (((sg * alpha + dg * inv) >> 8) << 8) | ((sb * alpha + db * inv) >> 8);
        }
    }

    void fillRect(int x, int y, int w, int h, uint32_t col, int alpha = 255) {
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        if (x0 >= x1 || y0 >= y1) return;
        if (alpha >= 255) {
            for (int py = y0; py < y1; ++py) {
                uint32_t* row = pixels + py * SCREEN_W;
                for (int px = x0; px < x1; ++px) row[px] = col;
            }
        } else if (alpha > 0) {
            for (int py = y0; py < y1; ++py)
                for (int px = x0; px < x1; ++px) blendPixel(px, py, col, alpha);
        }
    }

    void fillGradientV(int x, int y, int w, int h, uint32_t topCol, uint32_t botCol, int alpha = 255) {
        if (h <= 0 || w <= 0) return;
        int x0 = std::max(0, x), x1 = std::min(SCREEN_W, x + w);
        int y0 = std::max(0, y), y1 = std::min(SCREEN_H, y + h);
        for (int py = y0; py < y1; ++py) {
            float t = (float)(py - y) / (float)h;
            uint32_t c = lerpColor(topCol, botCol, t);
            if (alpha >= 255) {
                uint32_t* row = pixels + py * SCREEN_W;
                for (int px = x0; px < x1; ++px) row[px] = c;
            } else {
                for (int px = x0; px < x1; ++px) blendPixel(px, py, c, alpha);
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
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        int r2 = rad * rad;
        for (int py = y0; py < y1; ++py) {
            int dy = 0;
            if (py < y + rad) dy = (y + rad) - py;
            else if (py >= y + h - rad) dy = py - (y + h - rad - 1);
            for (int px = x0; px < x1; ++px) {
                int dx = 0;
                if (px < x + rad) dx = (x + rad) - px;
                else if (px >= x + w - rad) dx = px - (x + w - rad - 1);
                if (dx > 0 && dy > 0 && dx * dx + dy * dy > r2) continue;
                blendPixel(px, py, col, alpha);
            }
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
        const int r2 = r * r;
        const int y0 = std::max(0, cy - r), y1 = std::min(SCREEN_H - 1, cy + r);
        const int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
        const std::vector<uint32_t>* kernel = getGlowKernel(r);
        for (int py = y0; py <= y1; ++py) {
            const int dy = py - cy, dy2 = dy * dy;
            const int rem = r2 - 1 - dy2; // the glow is defined for d² < r²
            if (rem < 0) continue;
            const int extent = (int)sqrtf((float)rem);
            const int dx0 = std::max(-extent, -cx);
            const int dx1 = std::min(extent, SCREEN_W - 1 - cx);
            if (dx0 > dx1) continue;
            uint32_t* row = pixels + py * SCREEN_W;
            for (int dx = dx0; dx <= dx1; ++dx) {
                const int d2 = dx * dx + dy2;
                const int intensity = kernel
                    ? (int)((maxIntensity * (uint64_t)(*kernel)[d2]) >> 16)
                    : (int)((maxIntensity * (256 - (d2 * 256) / r2) * (256 - (d2 * 256) / r2)) >> 16);
                if (intensity <= 0) continue;
                uint32_t& dst = row[cx + dx];
                int rr = ((dst >> 16) & 0xFF) + ((cr * intensity) >> 8);
                int gg = ((dst >> 8) & 0xFF) + ((cg * intensity) >> 8);
                int bb = (dst & 0xFF) + ((cb * intensity) >> 8);
                if (rr > 255) rr = 255;
                if (gg > 255) gg = 255;
                if (bb > 255) bb = 255;
                dst = 0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
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
        uint32_t* row = pixels + y * SCREEN_W;
        for (int x = x0; x <= x1; ++x) {
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

    // Atmospheric Dark Corner Vignette
    void applyVignette() {
        for (int y = 0; y < SCREEN_H; ++y) {
            int dy = y - SCREEN_H / 2;
            int dy2 = (dy * dy * 256) / ((SCREEN_H / 2) * (SCREEN_H / 2));
            if (dy2 < 110 && y > 90 && y < SCREEN_H - 90) {
                // Only darken left/right edges on middle scanlines for speed
                for (int x = 0; x < 180; ++x) {
                    int dx = x - SCREEN_W / 2;
                    int d2 = dy2 + (dx * dx * 256) / ((SCREEN_W / 2) * (SCREEN_W / 2));
                    if (d2 > 160) {
                        int dark = std::min(140, (d2 - 160));
                        blendPixel(x, y, 0xFF040208, dark);
                        blendPixel(SCREEN_W - 1 - x, y, 0xFF040208, dark);
                    }
                }
            } else {
                for (int x = 0; x < SCREEN_W; ++x) {
                    int dx = x - SCREEN_W / 2;
                    int d2 = dy2 + (dx * dx * 256) / ((SCREEN_W / 2) * (SCREEN_W / 2));
                    if (d2 > 160) {
                        int dark = std::min(150, (d2 - 160));
                        blendPixel(x, y, 0xFF040208, dark);
                    }
                }
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
                    const uint8_t* rptr = bmp + row * g->width;
                    for (int colIdx = 0; colIdx < g->width; ++colIdx) {
                        uint8_t cov = rptr[colIdx];
                        if (cov > 8) blendPixel(gx + colIdx, py, col, (cov * alpha) >> 8);
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

    void drawHeroSprite(int cx, int cy, int heroClass, float animTime, int facing, bool hurtFlash, bool dashing) {
        if (dashing) addGlow(cx, cy, 42, 0xFF40E8FF, 195);
        int frame = ((int)(animTime * 6.0f)) & 1;
        int fx = 0, fy = 0;
        bakedSpriteVisualOffset(HERO_SPRITES[heroClass % 5][frame], fx, fy);
        drawShadow(cx, cy + 20 - fy, 15, 6);
        drawBakedSpriteCentred(HERO_SPRITES[heroClass % 5][frame], cx, cy, facing < 0, hurtFlash, false, 255);
    }

    void drawMonsterSprite(int cx, int cy, int type, float animTime, bool hitFlash, bool isElite, bool frozen = false, bool flipX = false) {
        if (isElite) {
            drawCircleRing(cx, cy + 14, 18, 2, 0xFFFFC028, 200, true);
            addGlow(cx, cy, 24, 0xFFFF9020, 95);
        }
        int frame = ((int)(animTime * 6.5f + cx * 0.04f)) & 1;
        int fx = 0, fy = 0;
        bakedSpriteVisualOffset(MONSTER_SPRITES[type % 11][frame], fx, fy);
        drawShadow(cx, cy + 16 - fy, 12, 5);
        drawBakedSpriteCentred(MONSTER_SPRITES[type % 11][frame], cx, cy, flipX, hitFlash, frozen, 255);
    }

    void drawBossSprite(int cx, int cy, int bossType, float animTime, bool hitFlash, bool enraged = false) {
        int auraRad = 40 + (int)(sinf(animTime * 7.0f) * 5.0f);
        uint32_t auraCol = enraged ? 0xFFFF2040 : ((bossType == 0) ? 0xFFFFB028 : (bossType == 1 ? 0xFF80FF40 : (bossType == 2 ? 0xFFFF3068 : 0xFFFF6818)));
        drawCircleRing(cx, cy + 24, auraRad, 3, auraCol, 210, true);
        addGlow(cx, cy, 62, auraCol, enraged ? 190 : 140);
        int frame = ((int)(animTime * 4.5f)) & 1;
        int fx = 0, fy = 0;
        bakedSpriteVisualOffset(BOSS_SPRITES[bossType % 4][frame], fx, fy);
        drawShadow(cx, cy + 36 - fy, 32, 12);
        drawBakedSpriteCentred(BOSS_SPRITES[bossType % 4][frame], cx, cy, false, hitFlash, false, 255);
    }

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
        // AI-арт для любого оружия 00–39, если игрок выбрал ИИ-АРТ в настройках.
        if (skillIconStyle == 1) {
            const uint32_t frameBg = evolved ? 0xFF3A1828 : 0xFF181228;
            const uint32_t frameBorder = evolved ? 0xFFFFD838 : 0xFF685890;
            const uint32_t glow = evolved ? 0xFFFFB820 : glowColors[id];
            fillRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, frameBg);
            addGlow(cx, cy, size + (evolved ? 9 : 3), glow, evolved ? 145 : 65);
            const BakedIconImage& image = SKILL_ICONS_AI[id];
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
            return;
        }
        const uint32_t bg = evolved ? 0xFF3A1828 : 0xFF181228;
        const uint32_t border = evolved ? 0xFFFFD838 : 0xFF685890;
        const uint32_t accent = glowColors[id];
        const int t = std::max(2, size / 8);
        const int thin = std::max(1, size / 13);
        fillRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, bg);
        drawRoundedRect(cx - size, cy - size, size * 2, size * 2, 7, border, evolved ? 3 : 2);
        addGlow(cx, cy, size + (evolved ? 9 : 3), evolved ? 0xFFFFB820 : accent, evolved ? 145 : 65);
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
