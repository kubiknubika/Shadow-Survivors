// ── Картинка к отчёту по аудиту оружия ───────────────────────────────────────
//
// Рисует две таблицы-теплокарты из CSV, который выдаёт стенд:
//     g++ -O2 -std=c++17 -Isrc tools/weapon_probe.cpp -o build/weapon_probe
//     build/weapon_probe 20 -1 --csv /tmp/weapon_after.csv
//     g++ -O2 -std=c++17 -Isrc tools/weapon_chart.cpp -o build/weapon_chart
//     build/weapon_chart /tmp/weapon_after.csv design/weapons_audit.ppm
//
// Левый блок — урон в секунду по толпе из 24 мишеней, правый — по одной мишени
// вплотную (90 px). Цвет клетки — тот же логарифм урона в обоих блоках, поэтому
// видно не только «кто больше», но и разрыв между оружием.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#include "renderer.h"

struct Row {
    std::string name;
    int level = 0;
    double far220 = 0.0, close90 = 0.0, moving = 0.0, crowd = 0.0;
};

static bool readCsv(const char* path, std::vector<Row>* rows) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("не открыть %s\n", path); return false; }
    char line[512];
    bool header = true;
    while (fgets(line, sizeof(line), f)) {
        if (header) { header = false; continue; }
        Row row;
        char name[128] = {0};
        int level = 0;
        double a = 0, b = 0, c = 0, d = 0, e = 0, g = 0, h = 0;
        if (sscanf(line, "%127[^,],%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf", name, &level,
                   &a, &b, &c, &d, &e, &g, &h) < 4) continue;
        row.name = name;
        row.level = level;
        row.far220 = a; row.close90 = b; row.moving = c; row.crowd = d;
        rows->push_back(row);
    }
    fclose(f);
    return !rows->empty();
}

static uint32_t mixColor(uint32_t a, uint32_t b, float t) {
    auto ch = [](uint32_t c, int shift) { return (float)((c >> shift) & 0xFF); };
    const float r = ch(a, 16) + (ch(b, 16) - ch(a, 16)) * t;
    const float g = ch(a, 8) + (ch(b, 8) - ch(a, 8)) * t;
    const float bl = ch(a, 0) + (ch(b, 0) - ch(a, 0)) * t;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

// Логарифмическая шкала: разрыв между 20 и 30000 урона в секунду иначе
// превращается в «одна клетка яркая, остальные чёрные».
static uint32_t heat(double dps) {
    const double lo = std::log10(20.0), hi = std::log10(32000.0);
    const double v = std::log10(std::max(1.0, dps));
    float t = (float)std::max(0.0, std::min(1.0, (v - lo) / (hi - lo)));
    static const uint32_t stops[5] = {0xFF1A1F33, 0xFF1F5F8B, 0xFF2F9E6B, 0xFFD8C24A, 0xFFE0562E};
    const int seg = std::min(3, (int)(t * 4.0f));
    return mixColor(stops[seg], stops[seg + 1], t * 4.0f - seg);
}

static const char* levelName(int level) {
    switch (level) {
        case 1: return "ранг 1";
        case 4: return "ранг 4";
        case 8: return "ранг 8";
        case 9: return "эволюция";
        default: return "ранг ?";
    }
}

// Возвращает нижнюю границу нарисованной таблицы: панели не должны наезжать.
static int drawTable(Renderer& r, int x, int y, int labelW, int cellW, int cellH,
                     const char* title, const std::vector<Row>& rows, bool crowd) {
    r.drawText(FONT_MED, x, y, title, 0xFFE8DCFF);
    int cy = y + 30;
    std::vector<std::string> names;
    for (const Row& row : rows) {
        if (row.name.rfind("tier", 0) == 0) continue;
        if (std::find(names.begin(), names.end(), row.name) == names.end()) names.push_back(row.name);
    }
    for (const std::string& name : names) {
        double v[4] = {0, 0, 0, 0};
        for (const Row& other : rows) {
            if (other.name != name) continue;
            const int slot = other.level == 1 ? 0 : (other.level == 4 ? 1 : (other.level == 8 ? 2 : 3));
            v[slot] = crowd ? other.crowd : other.close90;
        }
        r.drawText(FONT_SMALL, x + 6, cy + cellH / 2 - 7, name.c_str(), 0xFFBFB4D8);
        for (int i = 0; i < 4; ++i) {
            const int cx = x + labelW + i * cellW;
            r.fillRect(cx + 1, cy + 1, cellW - 3, cellH - 3, heat(v[i]));
            char buf[24];
            snprintf(buf, sizeof(buf), "%.0f", v[i]);
            const float t = (float)std::max(0.0, std::min(1.0,
                (std::log10(std::max(1.0, v[i])) - std::log10(20.0)) / (std::log10(32000.0) - std::log10(20.0))));
            r.drawTextCentered(FONT_SMALL, cx + cellW / 2, cy + cellH / 2 - 7, buf,
                               t > 0.72f ? 0xFF14101C : 0xFFF4EEFF, false);
        }
        cy += cellH;
    }
    static const char* headers[4] = {"ранг 1", "ранг 4", "ранг 8", "эволюция"};
    for (int i = 0; i < 4; ++i)
        r.drawText(FONT_SMALL, x + labelW + i * cellW + 8, cy + 4, headers[i], 0xFF8E84A8, false);
    return cy + 24;
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: weapon_chart <in.csv> <out.ppm>\n"); return 1; }
    std::vector<Row> rows;
    if (!readCsv(argv[1], &rows)) return 1;

    SCREEN_W = 1600; SCREEN_H = 900;
    Renderer r;
    r.resize(SCREEN_W, SCREEN_H);
    r.clear(0xFF0E0C16);

    r.drawText(FONT_LARGE, 40, 24, "АУДИТ ОРУЖИЯ: УРОН В СЕКУНДУ ПО БЕССМЕРТНЫМ МИШЕНЯМ", 0xFFFFE6A8);
    r.drawText(FONT_SMALL, 40, 62,
               "стенд tools/weapon_probe.cpp, 20 с на замер, фиксированное зерно, без артефактов и рангов героя",
               0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 40, 82,
               "цифра — потолок урона по бессмертной мишени, а не число ударов: в игре моб на 5-й минуте имеет около 50 HP",
               0xFF9C93B4, false);

    std::vector<Row> base;
    for (const Row& row : rows) if (row.name.rfind("tier", 0) != 0) base.push_back(row);

    const int labelW = 210, cellW = 132, cellH = 42;
    const int afterCrowd = drawTable(r, 40, 118, labelW, cellW, cellH,
                                     "ТОЛПА: 24 МИШЕНИ НА КОЛЬЦЕ 120–330 px", base, true);
    drawTable(r, 40, afterCrowd + 12, labelW, cellW, cellH,
              "ОДНА МИШЕНЬ ВПЛОТНУЮ, 90 px", base, false);

    // Правый столбец: оружие тиров и короткие выводы.
    const int rx = 880;
    r.drawText(FONT_MED, rx, 118, "ОРУЖИЕ ТИРОВ (уровень 8)", 0xFFE8DCFF);
    r.drawText(FONT_SMALL, rx + 6, 152, "оружие", 0xFF8E84A8, false);
    r.drawTextCentered(FONT_SMALL, rx + 378, 152, "толпа", 0xFF8E84A8, false);
    r.drawTextCentered(FONT_SMALL, rx + 488, 152, "1 цель", 0xFF8E84A8, false);
    static const char* tierNames[4] = {"Кристальный залп", "Рунический импульс", "Разломное копьё", "Пепельная звезда"};
    int ty = 176;
    for (const Row& row : rows) {
        if (row.name.rfind("tier", 0) != 0) continue;
        const int idx = atoi(row.name.c_str() + 4) - 24;
        r.drawText(FONT_SMALL, rx + 6, ty, tierNames[std::max(0, std::min(3, idx))], 0xFFBFB4D8);
        r.fillRect(rx + 330, ty - 2, 96, 18, heat(row.crowd));
        r.fillRect(rx + 440, ty - 2, 96, 18, heat(row.close90));
        char buf[24];
        snprintf(buf, sizeof(buf), "%.0f", row.crowd);
        r.drawTextCentered(FONT_SMALL, rx + 378, ty, buf, 0xFFF4EEFF, false);
        snprintf(buf, sizeof(buf), "%.0f", row.close90);
        r.drawTextCentered(FONT_SMALL, rx + 488, ty, buf, 0xFFF4EEFF, false);
        ty += 26;
    }

    r.drawText(FONT_MED, rx, 320, "ЧТО ИСПРАВЛЕНО ПО ИТОГАМ АУДИТА", 0xFFE8DCFF);
    static const char* notes[10] = {
        "1. Вихрь рос орбитой быстрее сфер: на 8-м ранге",
        "   не бил никого ближе 124 px (вплотную — 0 урона).",
        "2. Огненный шар летел веером шире цели: на 220 px",
        "   из трёх шаров попадал один (588 вместо 1765 DPS).",
        "3. Топор летел только вверх и падал в 110–200 px:",
        "   теперь бросок идёт в сторону ближайшей цели.",
        "4. Клинки и кристаллы летели по прямой мимо цели:",
        "   добавлено наведение (606 → 3160 DPS на 220 px).",
        "5. Молния била одну цель всеми разрядами:",
        "   цепь идёт по разным целям (1 → 7 целей за удар).",
    };
    for (int i = 0; i < 10; ++i)
        r.drawText(FONT_SMALL, rx + 6, 356 + i * 20, notes[i], 0xFFBFB4D8, false);

    r.drawText(FONT_MED, rx, 580, "ГДЕ ОСТАЛСЯ ПЕРЕВЕС", 0xFFE8DCFF);
    static const char* balance[8] = {
        "• Клинки — лучший выбор во всех сценариях:",
        "  3178 вплотную и 8644 в толпе на 8-м ранге.",
        "• Аура и топор — только толпа: 9472 и 8557.",
        "• Молния — одиночная: 3244 в толпе при 3231 на цель.",
        "• Лёд — самый слабый охват: 5183 в толпе (в 1,8 раза",
        "  меньше ауры) при 24 целях за активацию.",
        "• Вихрь после правки — ближний: 7976 в толпе,",
        "  но 0 на дистанции 220 px, и это честно по карточке.",
    };
    for (int i = 0; i < 8; ++i)
        r.drawText(FONT_SMALL, rx + 6, 616 + i * 20, balance[i], 0xFFBFB4D8, false);

    r.drawText(FONT_SMALL, 40, 866,
               "цвет клетки — логарифм урона (20 → 32000): так видно разрыв между оружием, а не только порядок",
               0xFF8E84A8, false);

    FILE* f = fopen(argv[2], "wb");
    if (!f) { printf("не записать %s\n", argv[2]); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        const uint32_t c = r.pixels[i];
        unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)(c & 0xFF)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", argv[2]);
    return 0;
}
