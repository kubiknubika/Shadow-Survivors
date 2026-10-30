// ── Стенд виртуального управления ────────────────────────────────────────────
//
// Собирает кадр боя и кладёт поверх него ту же схему касаний, что использует
// Android-версия (src/touch_input.h). Нужен, чтобы проверять управление без
// телефона: подписи, размеры кнопок, что стик не закрывает героя и телеграфы.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/touch_preview.cpp -o build/touch
//     build/touch 1600x720 phone.bmp     # экран телефона 20:9
//     build/touch 1280x720 desktop.bmp
//     build/touch 1280x720 sheet.bmp 1   # лист А5 с подписями
//
// Второй номер (третий аргумент) включает режим листа: два кадра рядом, как в
// design/touch_controls.png.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "renderer.h"
#include "game.h"
#include "touch_input.h"

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
    if (!f) { printf("не смог открыть %s\n", path); return; }
    fwrite(header, 1, 54, f);
    for (int y = h - 1; y >= 0; --y) fwrite(px + (size_t)y * w, 4, (size_t)w, f);
    fclose(f);
}

static bool parseSize(const char* s, int& w, int& h) {
    const char* x = strchr(s, 'x');
    if (!x) return false;
    w = atoi(s);
    h = atoi(x + 1);
    return w >= 240 && h >= 240;
}

// Один кадр боя с нажатым стиком и пальцем на цели.
static void renderScene(int w, int h, int graphics, std::vector<uint32_t>& out) {
    SCREEN_W = w; SCREEN_H = h;
    Renderer r;
    r.resize(w, h);
    Game g;
    g.simulationMode = true;
    srand(4242);
    g.init();
    g.gameMode = MODE_ENDLESS;
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.options.graphicsLevel = graphics;
    g.runTime = 260.0f;
    for (int i = 0; i < 7; ++i) g.itemLevels[i] = 4;
    for (int i = 0; i < 80; ++i) g.spawnMonster(i % 9, i % 11 == 0, false);
    for (int f = 0; f < 180; ++f) {
        g.update(1.0f / 60.0f);
        g.state = STATE_PLAYING;
    }

    TouchLayout layout;
    layout.layout(w, h);
    TouchPointer pointers[TOUCH_MAX_POINTERS];
    // Стик слева, палец прицела справа, большой палец ещё и на рывке — так
    // видно, что три касания одновременно работают.
    touchBegin(pointers, g, layout, 1, (float)w * 0.18f, (float)h * 0.78f);
    touchMove(pointers, g, 1, (float)w * 0.18f + 42.0f, (float)h * 0.78f - 26.0f);
    touchBegin(pointers, g, layout, 2, (float)w * 0.62f, (float)h * 0.55f);
    touchBegin(pointers, g, layout, 3, layout.dash.x + layout.dash.w * 0.5f, layout.dash.y + layout.dash.h * 0.5f);
    g.mouseX = (int)((float)w * 0.62f);
    g.mouseY = (int)((float)h * 0.55f);
    touchUpdate(g, pointers);

    r.clear(0xFF08060F);
    g.render(r);
    drawTouchOverlay(r, g, pointers, layout);
    out.assign(r.pixels, r.pixels + (size_t)w * h);
}

int main(int argc, char** argv) {
    int w = 1600, h = 720;
    if (argc > 1) parseSize(argv[1], w, h);
    const char* path = argc > 2 ? argv[2] : "/tmp/touch.bmp";
    const bool sheet = argc > 3 && atoi(argv[3]) != 0;

    if (!sheet) {
        std::vector<uint32_t> frame;
        renderScene(w, h, 1, frame);
        writeBmp(path, frame.data(), w, h);
        printf("%s  %dx%d\n", path, w, h);
        return 0;
    }

    // Лист: сверху телефон 20:9 (1600×720), снизу 16:9 (1280×720). Оба кадра
    // приведены к одной ширине, иначе 20:9 и 16:9 не сравнить глазами.
    const int phoneW = 1600, phoneH = 720, deskW = 1280, deskH = 720;
    std::vector<uint32_t> phone, desk;
    renderScene(phoneW, phoneH, 1, phone);
    renderScene(deskW, deskH, 1, desk);

    const int outW = 1000 + 24;
    const int scalePhone = 960;                     // 1600 -> 960 (0.6)
    const int phoneOutH = phoneH * scalePhone / phoneW;
    const int scaleDesk = 960;                      // 1280 -> 960 (0.75)
    const int deskOutH = deskH * scaleDesk / deskW;
    const int labelH = 26, pad = 12;
    const int outH = pad + labelH + phoneOutH + pad + labelH + deskOutH + pad;
    SCREEN_W = outW; SCREEN_H = outH;
    Renderer canvas;
    canvas.resize(outW, outH);
    for (int i = 0; i < outW * outH; ++i) canvas.pixels[i] = 0xFF0B0912;

    auto place = [&](const std::vector<uint32_t>& src, int sw, int sh, int outCellW, int outCellH, int y0) {
        const int x0 = (outW - outCellW) / 2;
        for (int y = 0; y < outCellH; ++y) {
            const int sy = y * sh / outCellH;
            uint32_t* dst = canvas.pixels + (size_t)(y0 + y) * outW + x0;
            for (int x = 0; x < outCellW; ++x) dst[x] = src[(size_t)sy * sw + x * sw / outCellW];
        }
        canvas.drawRect(x0 - 1, y0 - 1, outCellW + 2, outCellH + 2, 0xFF3A3350, 1);
        return x0;
    };

    int y = pad;
    canvas.drawText(FONT_SMALL, 12, y + 6,
                    "ТЕЛЕФОН, 20:9 — кадр 1600×720: стик слева, прицел справа, кнопки у больших пальцев", 0xFFE8E0F8);
    y += labelH;
    place(phone, phoneW, phoneH, scalePhone, phoneOutH, y);
    y += phoneOutH + pad;
    canvas.drawText(FONT_SMALL, 12, y + 6,
                    "16:9 — кадр 1280×720: та же схема, кнопки прижаты к краям независимо от формата", 0xFFE8E0F8);
    y += labelH;
    place(desk, deskW, deskH, scaleDesk, deskOutH, y);

    writeBmp(path, canvas.pixels, outW, outH);
    printf("%s  %dx%d\n", path, outW, outH);
    return 0;
}
