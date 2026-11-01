// Лист «одна и та же сцена на четырёх ступенях графики» для design/mobile_levels.png.
// Кадр один и тот же: состояние игры не меняется, меняется только уровень
// эффектов, поэтому разницу видно честно — как она и выглядит в игре.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <chrono>
#include <string>
#include "renderer.h"
#include "game.h"

static void writeBmp(const char* path, const uint32_t* px, int w, int h) {
    const int dataSize = w * 4 * h;
    unsigned char header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    const int fileSize = 54 + dataSize;
    header[2] = fileSize & 0xFF; header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF; header[5] = (fileSize >> 24) & 0xFF;
    header[10] = 54; header[14] = 40;
    header[18] = w & 0xFF; header[19] = (w >> 8) & 0xFF;
    header[22] = h & 0xFF; header[23] = (h >> 8) & 0xFF;
    header[26] = 1; header[28] = 32;
    header[34] = dataSize & 0xFF; header[35] = (dataSize >> 8) & 0xFF;
    header[36] = (dataSize >> 16) & 0xFF; header[37] = (dataSize >> 24) & 0xFF;
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(header, 1, 54, f);
    for (int y = h - 1; y >= 0; --y) fwrite(px + (size_t)y * w, 4, (size_t)w, f);
    fclose(f);
}

int main() {
    const int srcW = 1280, srcH = 720;
    SCREEN_W = srcW; SCREEN_H = srcH;
    Renderer r;
    r.resize(srcW, srcH);
    Game g;
    g.simulationMode = true;
    srand(4242);
    g.init();
    g.gameMode = MODE_ENDLESS;
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.runTime = 300.0f;
    for (int i = 0; i < 7; ++i) g.itemLevels[i] = 6;
    g.itemLevels[0] = 9;
    g.itemLevels[7] = 4;
    for (int i = 0; i < 140; ++i) g.spawnMonster(i % 9, i % 13 == 0, false);

    // Одна и та же сцена: 240 кадров игры на высоком уровне.
    g.options.graphicsLevel = 1;
    for (int f = 0; f < 240; ++f) {
        g.update(1.0f / 60.0f);
        g.state = STATE_PLAYING;
        r.clear(0xFF08060F);
        g.render(r);
    }

    static const int levels[4] = {1, 2, 3, 4};
    static const char* const marks[4] = {"A", "B", "C", "D"};
    static const char* const names[4] = {"ВЫСОКОЕ", "СРЕДНЕЕ", "БЫСТРОЕ", "ЭКОНОМ"};
    std::vector<std::vector<uint32_t>> frames(4);
    double msPerFrame[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        g.options.graphicsLevel = levels[i];
        float best = 1e9f;
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = std::chrono::high_resolution_clock::now();
            const int n = 60;
            for (int k = 0; k < n; ++k) { r.clear(0xFF08060F); g.render(r); }
            auto t1 = std::chrono::high_resolution_clock::now();
            const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count() / (float)n;
            if (ms < best) best = ms;
        }
        msPerFrame[i] = best;
        g.render(r);
        frames[i].assign(r.pixels, r.pixels + (size_t)srcW * srcH);
        printf("%s %-9s %.2f мс\n", marks[i], names[i], best);
    }

    // Лист: сверху два полных кадра (высокое и быстрое), снизу два увеличения
    // одной и той же области вблизи стража.
    const int zoomX = 520, zoomY = 300, zoomW = 440, zoomH = 300;
    const int cellW = 640, cellH = 360, pad = 10, header = 28;
    const int outW = 2 * cellW + 3 * pad;
    const int outH = 2 * (cellH + header) + 3 * pad;
    SCREEN_W = outW; SCREEN_H = outH;
    Renderer out;
    out.resize(outW, outH);
    for (int i = 0; i < outW * outH; ++i) out.pixels[i] = 0xFF0B0912;

    auto place = [&](int cell, int levelIndex, bool zoom, const char* title, uint32_t accent) {
        const int cx = pad + (cell % 2) * (cellW + pad);
        const int cy = pad + (cell / 2) * (cellH + header + pad);
        const std::vector<uint32_t>& f = frames[levelIndex];
        for (int y = 0; y < cellH; ++y) {
            uint32_t* dst = out.pixels + (size_t)(cy + header + y) * outW + cx;
            for (int x = 0; x < cellW; ++x) {
                const int sx = zoom ? zoomX + x * zoomW / cellW : x * 2;
                const int sy = zoom ? zoomY + y * zoomH / cellH : y * 2;
                dst[x] = f[(size_t)sy * srcW + sx];
            }
        }
        out.drawText(FONT_SMALL, cx + 2, cy + 7, title, accent);
        out.drawRect(cx - 1, cy + header - 1, cellW + 2, cellH + 2, 0xFF3A3350, 1);
    };

    char title[192];
    snprintf(title, sizeof(title), "A  ВЫСОКОЕ — весь кадр (так рисует 2.10.47)");
    place(0, 0, false, title, 0xFF9BF0B0);
    snprintf(title, sizeof(title), "B  БЫСТРОЕ — тот же кадр, те же существа и метки");
    place(1, 2, false, title, 0xFFFFD878);
    snprintf(title, sizeof(title), "C  ВЫСОКОЕ — увеличение: мелкие ореолы и объём фигуры");
    place(2, 0, true, title, 0xFF9BF0B0);
    snprintf(title, sizeof(title), "D  БЫСТРОЕ — то же место: искры убраны, крупный свет на месте");
    place(3, 2, true, title, 0xFFFFD878);

    writeBmp("/tmp/pb/levels_sheet.bmp", out.pixels, outW, outH);
    printf("лист %dx%d\n", outW, outH);
    return 0;
}
