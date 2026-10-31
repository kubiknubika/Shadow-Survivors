// ── Лист всех праздничных символов и персонажей ───────────────────────────────
//
// У восьми праздников по четыре символа и по два персонажа — сорок восемь
// рисунков. Задача 27 требует реализовать КАЖДЫЙ из них, поэтому нужен стенд,
// который показывает сразу все, а не по одному: пропуск одного символа глазами
// в игре не находится (окно праздника длится две недели раз в году).
//
//     g++ -O2 -std=c++17 -Isrc tools/holiday_preview.cpp -o build/holidays
//     build/holidays build/holidays.bmp
//
// Стенд сам себя проверяет и возвращает 1, если:
//   • в клетке нет ярких пикселей (символ нарисован «в никуда»);
//   • два разных рисунка совпали пиксель в пиксель (скопированная заготовка);
//   • символ не читается на 26 px (меньше 18 ярких пикселей) — в игре он
//     встречается именно таким, в слоте инвентаря и в строке календаря;
//   • у симметричного рисунка половины не совпали (перекос): ось — центр
//     клетки, сравнение попиксельное, допуска нет. Ровно ради этой проверки
//     примитивы в holiday_art.h зеркалят сами себя;
//   • односторонний рисунок уехал от центра больше чем на 7 px — тогда он
//     висит в клетке и в бою не по центру.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "renderer.h"
#include "holidays.h"
#include "holiday_art.h"

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

// Яркие пиксели в квадрате — «рисунок вообще есть».
static int litPixels(const Renderer& r, int x0, int y0, int size) {
    int lit = 0;
    for (int y = y0; y < y0 + size; ++y)
        for (int x = x0; x < x0 + size; ++x) {
            const uint32_t c = r.pixels[y * SCREEN_W + x];
            const int rr = (c >> 16) & 0xFF, gg = (c >> 8) & 0xFF, bb = c & 0xFF;
            if (rr + gg + bb > 210) ++lit;
        }
    return lit;
}

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/holidays.bmp";
    // Клетка 150x150: сверху крупный рисунок (86 px), под ним подпись и мелкий
    // вариант (26 px). Поля по краям обязательны: в первой версии клетка
    // начиналась с x=24 при полуразмере 46, и левый столбец срезало краем листа —
    // ровно тот дефект, который стенд и должен ловить, но сначала поймал его глаз.
    // Клетка шире рисунка: некоторые фигуры выходят за ±1 единицу (тыква,
    // дракон), и в клетке 150 px сосед залезал в проверяемый квадрат 86 px,
    // из-за чего «перекос» находился там, где его нет.
    // Слева — колонка с названием праздника: раньше имя рисовалось поверх
    // верхней части первой клетки, и проверка симметрии «находила перекос»
    // там, где его не было (буквы не отражаются).
    const int cellW = 200, cellH = 152, head = 52, marginX = 20, gutterW = 170;
    SCREEN_W = marginX * 2 + gutterW + cellW * 6;
    SCREEN_H = head + cellH * HOLIDAY_COUNT + 8;
    Renderer r;
    r.resize(SCREEN_W, SCREEN_H);
    r.clear(0xFF14101C);
    r.drawText(FONT_MED, marginX, 12, "ПРАЗДНИКИ: 4 СИМВОЛА + 2 ПЕРСОНАЖА", 0xFFFFE6C4);
    r.drawText(FONT_SMALL, marginX, 36,
               "сверху 86 px, снизу 26 px — так символ виден в календаре и в бою",
               0xFF9C93B8);

    std::vector<const char*> names;
    for (int i = 0; i < HOLIDAY_COUNT; ++i) names.push_back(HOLIDAY_DEFS[i].name);

    // Куда попала каждая клетка: нужно самопроверке и не зависит от вёрстки.
    struct Cell { int litX, litY, smallX, smallY; const char* holiday; int holidayId; int index; };
    std::vector<Cell> cells;
    for (int h = 0; h < HOLIDAY_COUNT; ++h) {
        const int y = head + h * cellH;
        r.fillRect(0, y, SCREEN_W, cellH - 2, 0xFF1B1526, 255);
        r.fillRect(0, y, 6, cellH - 2, HOLIDAY_DEFS[h].c1, 255);
        r.drawText(FONT_SMALL, marginX, y + cellH / 2 - 7, names[h], HOLIDAY_DEFS[h].glow);
        for (int i = 0; i < 6; ++i) {
            const int cx = marginX + gutterW + cellW / 2 + i * cellW;
            const int bigY = y + 54, smallY = y + 130;
            const bool isChar = i >= 4;
            const int idx = isChar ? i - 4 : i;
            HolidayArt big(r, cx, bigY, 86, 0.7f);
            HolidayArt small(r, cx, smallY, 26, 0.7f);
            if (isChar) { big.character((HolidayId)h, idx, 0.0f); small.character((HolidayId)h, idx, 0.0f); }
            else { big.symbol((HolidayId)h, idx); small.symbol((HolidayId)h, idx); }
            const char* label = isChar ? (idx == 0 ? "персонаж 1" : "персонаж 2")
                                       : (idx == 0 ? "символ 1" : idx == 1 ? "символ 2"
                                                   : idx == 2 ? "символ 3" : "символ 4");
            r.drawTextCentered(FONT_SMALL, cx, y + 104, label,
                               isChar ? 0xFFC8A8E8 : 0xFF8A82A8);
            cells.push_back({cx - 43, bigY - 43, cx - 13, smallY - 13, names[h], h, i});
        }
    }
    saveBmp(out, SCREEN_W, SCREEN_H, r.pixels);

    // ── Самопроверка ──────────────────────────────────────────────────────────
    int bad = 0;
    std::vector<std::vector<uint32_t>> seen;
    for (size_t i = 0; i < cells.size(); ++i) {
        const Cell& c = cells[i];
        const int lit = litPixels(r, c.litX, c.litY, 86);
        std::vector<uint32_t> sig;
        for (int y = c.litY; y < c.litY + 86; ++y)
            for (int x = c.litX; x < c.litX + 86; ++x) sig.push_back(r.pixels[y * SCREEN_W + x]);
        bool dup = false;
        for (const auto& other : seen) if (other == sig) { dup = true; break; }
        seen.push_back(sig);
        const int smallLit = litPixels(r, c.smallX, c.smallY, 26);
        // Края листа: обрезанный рисунок — это дефект вёрстки, а не приём.
        const bool clipped = c.litX < 2 || c.litY < 2 ||
                             c.litX + 86 > SCREEN_W - 2 || c.litY + 86 > SCREEN_H - 2;
        // ── Геометрия: симметрия и центровка ─────────────────────────────────
        const int axis = c.litX + 43;             // центр клетки по оси рисунка
        const bool isChar = c.index >= 4;
        const int idx = isChar ? c.index - 4 : c.index;
        const bool symmetric = holidayArtMirrored(isChar, (HolidayId)c.holidayId, idx);
        int asym = 0, ink = 0;
        long sumX = 0;
        for (int y = c.litY; y < c.litY + 86; ++y) {
            for (int x = c.litX; x < c.litX + 86; ++x) {
                const uint32_t a = r.pixels[y * SCREEN_W + x];
                const uint32_t b = r.pixels[y * SCREEN_W + (2 * axis - x)];
                const int sa = ((a >> 16) & 0xFF) + ((a >> 8) & 0xFF) + (a & 0xFF);
                const int sb = ((b >> 16) & 0xFF) + ((b >> 8) & 0xFF) + (b & 0xFF);
                if (sa > 210) { ++ink; sumX += x; }
                if (a != b && (sa > 210 || sb > 210)) ++asym;
            }
        }
        const double dx = ink ? (double)sumX / ink - axis : 0.0;
        const bool crooked = symmetric && asym != 0;
        const bool offCenter = !symmetric && fabs(dx) > 7.0;
        if (lit == 0) { printf("ПУСТО: %s символ %d\n", c.holiday, c.index + 1); ++bad; }
        else if (dup) { printf("ДУБЛЬ: %s символ %d\n", c.holiday, c.index + 1); ++bad; }
        else if (clipped) { printf("ОБРЕЗАН: %s символ %d\n", c.holiday, c.index + 1); ++bad; }
        else if (smallLit < 18) { printf("МЕЛКО: %s символ %d — %d px на 26\n", c.holiday, c.index + 1, smallLit); ++bad; }
        else if (crooked) { printf("ПЕРЕКОС: %s символ %d — %d px не совпали с отражением\n",
                                   c.holiday, c.index + 1, asym); ++bad; }
        else if (offCenter) { printf("СМЕЩЁН: %s символ %d — на %.1f px от оси\n",
                                     c.holiday, c.index + 1, dx); ++bad; }
    }
    printf("%s: 48 рисунков, пустых/дублей/мелких — %d\n", bad ? "ПРОВАЛ" : "ОК", bad);
    return bad ? 1 : 0;
}
