// ── Лист трёх стилей иконок для одного предмета ───────────────────────────────
//
// В игре три стиля иконок (настройки → «ИКОНКИ»: ВЕКТОР, ИИ-АРТ, ЦВЕТ), и они
// устроены по-разному: ВЕКТОР рисуется примитивами прямо в renderer.h, а два
// других — запечённые картинки 64x64 (ИИ-АРТ из design/ai_icons, ЦВЕТ из того же
// рисунка с подложкой под смысловой цвет, а для отдельных предметов — со своим
// одноцветным рисунком). Поэтому «сделать иконку предмета» значит сделать три
// иконки, и проверять их надо вместе, а не по одной.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/style_preview.cpp -o build/styles
//     build/styles build/styles.bmp 40              # по умолчанию предмет 40
//     build/styles build/styles.bmp 5
//
// Код возврата 1, если в каком-то стиле внутри слота нет рисунка (только
// подложка). Это не теория: у 41-го предмета векторный стиль был именно таким —
// в switch (id) не было case 40, и в слоте стоял пустой кружок.
//
// Рисует ту же функцию, что и игра (drawWeaponIcon), поэтому лист не может
// разойтись с игрой. Размеры: 66 px (карточка арсенала), 16 px и 11 px (слот и
// строка сборки) — иконка обязана читаться на всех трёх.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "renderer.h"
#include "game.h"
static void saveBmp(const char* path, int w, int h, const uint32_t* px) {
    const int rowSize = ((w * 3 + 3) / 4) * 4;
    const int dataSize = rowSize * h;
    unsigned char header[54] = {0};
    header[0]='B'; header[1]='M';
    int fileSize = 54 + dataSize; memcpy(header+2, &fileSize, 4);
    int offset = 54; memcpy(header+10, &offset, 4);
    int hs = 40; memcpy(header+14, &hs, 4);
    memcpy(header+18, &w, 4);
    int nh = -h; memcpy(header+22, &nh, 4);
    short one = 1; memcpy(header+26, &one, 2);
    short bpp = 24; memcpy(header+28, &bpp, 2);
    memcpy(header+34, &dataSize, 4);
    FILE* f = fopen(path, "wb"); if (!f) return;
    fwrite(header, 1, 54, f);
    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) { uint32_t c = px[y*w+x];
            row[x*3+0]=c&0xFF; row[x*3+1]=(c>>8)&0xFF; row[x*3+2]=(c>>16)&0xFF; }
        fwrite(row.data(), 1, rowSize, f);
    }
    fclose(f);
    printf("%s %dx%d\n", path, w, h);
}
int main(int argc, char** argv) {
    const int item = argc > 2 ? atoi(argv[2]) : 40;
    const int cell = 260;
    SCREEN_W = cell * 3; SCREEN_H = cell + 40;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    r.clear(0xFF14101C);
    const char* names[3] = {"ВЕКТОР", "ИИ-АРТ", "ЦВЕТ"};
    for (int style = 0; style < 3; ++style) {
        r.skillIconStyle = style;
        const int cx = cell * style + cell / 2;
        r.drawWeaponIcon(cx, 108, item, 66, false);
        r.drawWeaponIcon(cx, 205, item, 16, false);
        r.drawWeaponIcon(cx, 240, item, 11, false);
        r.drawTextCentered(FONT_SMALL, cx, 24, names[style], 0xFFFFE6C4);
        r.drawTextCentered(FONT_SMALL, cx, 258, style == 0 ? "рисуется кодом" : "запечена 64x64", 0xFF9C93B8);
    }
    // В BMP, а не в PNG: у стендов нет PIL, а картинку в design/ кладёт
    // отдельный конвертер — так же, как screen_shot и make_icon_sheet.
    saveBmp(argc > 1 ? argv[1] : "build/styles.bmp", SCREEN_W, SCREEN_H, r.pixels);

    // Проверка «иконка не пустая»: в каждом стиле считаем яркие пиксели ВНУТРИ
    // круга подложки (рамка не в счёт). Так ловится ровно тот дефект, из-за
    // которого у 41-го предмета был пустой кружок в векторном стиле.
    int bad = 0;
    for (int style = 0; style < 3; ++style) {
        const int cx = cell * style + cell / 2, cy = 108;
        int lit = 0;
        for (int y = cy - 50; y <= cy + 50; ++y) {
            for (int x = cx - 50; x <= cx + 50; ++x) {
                const int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > 50 * 50) continue;
                const uint32_t c = r.pixels[y * SCREEN_W + x];
                const int lum = (((c >> 16) & 0xFF) * 3 + ((c >> 8) & 0xFF) * 6 + (c & 0xFF)) / 10;
                if (lum > 90) ++lit;
            }
        }
        const char* names[3] = {"ВЕКТОР", "ИИ-АРТ", "ЦВЕТ"};
        printf("%-8s рисунок: %4d ярких пикселей внутри слота — %s\n",
               names[style], lit, lit > 500 ? "ok" : "ПУСТО");
        if (lit <= 500) ++bad;
    }
    return bad ? 1 : 0;
}
