// ── Картинка к отчёту по аудиту оружия ───────────────────────────────────────
//
// Рисует две таблицы-теплокарты из CSV, который выдаёт стенд:
//     g++ -O2 -std=c++17 -Isrc tools/weapon_probe.cpp -o build/weapon_probe
//     build/weapon_probe 20 -1 --csv /tmp/weapon_after.csv
//     g++ -O2 -std=c++17 -Isrc tools/weapon_chart.cpp -o build/weapon_chart
//     build/weapon_chart /tmp/weapon_after.csv design/weapons_audit.ppm
//
// Режим разнообразия (вторая картинка, из стенда variety_probe):
//     build/weapon_chart /tmp/weapon_after.csv design/weapons_variety.ppm \
//         --variety /tmp/variety.csv
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
    double far220 = 0.0, close90 = 0.0, near20 = 0.0, moving = 0.0, crowd = 0.0;
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
        double a = 0, b = 0, n = 0, c = 0, d = 0, e = 0, g = 0, h = 0;
        if (sscanf(line, "%127[^,],%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", name, &level,
                   &a, &b, &n, &c, &d, &e, &g, &h) < 5) continue;
        row.name = name;
        row.level = level;
        row.far220 = a; row.close90 = b; row.near20 = n; row.moving = c; row.crowd = d;
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

// ── Режим разнообразия: время зачистки волны + роли ─────────────────────────
struct ClearRow {
    std::string name;
    int level = 0;
    double seconds = 0.0;
};

static bool readClearCsv(const char* path, std::vector<ClearRow>* rows) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("не открыть %s\n", path); return false; }
    char line[512];
    bool header = true;
    while (fgets(line, sizeof(line), f)) {
        if (header) { header = false; continue; }
        char name[128] = {0};
        int level = 0, cleared = 0, kills = 0, maxAlive = 0;
        double seconds = 0, avgAlive = 0;
        if (sscanf(line, "%127[^,],%d,%d,%lf,%d,%lf,%d", name, &level, &cleared,
                   &seconds, &kills, &avgAlive, &maxAlive) < 4) continue;
        rows->push_back({name, level, seconds});
    }
    fclose(f);
    return !rows->empty();
}

static void drawVariety(Renderer& r, const std::vector<Row>& weapons, const std::vector<ClearRow>& clears) {
    r.drawText(FONT_LARGE, 40, 24, "РАЗНООБРАЗИЕ: ЧЕМ ОРУЖИЕ ОТЛИЧАЕТСЯ, А ЧЕМ ДУБЛИРУЕТСЯ", 0xFFFFE6A8);
    r.drawText(FONT_SMALL, 40, 62,
               "слева — зачистка волны из 40 живых мобов (здоровье как на 5-й минуте), герой стоит, оружие одно",
               0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 40, 82,
               "справа — из пяти сценариев стенда оружия: кто лучший, чьи профили похожи и кто перекрыт полностью",
               0xFF9C93B4, false);

    // ── Слева: полосы времени зачистки ──────────────────────────────────────
    const int x0 = 40, barX = 300, barW = 470, rowH = 44, maxSeconds = 20.0;
    r.drawText(FONT_MED, x0, 122, "ЗАЧИСТКА ВОЛНЫ, СЕКУНД (меньше — лучше)", 0xFFE8DCFF);
    std::vector<std::string> names;
    for (const ClearRow& c : clears)
        if (std::find(names.begin(), names.end(), c.name) == names.end()) names.push_back(c.name);
    int y = 164;
    for (const std::string& name : names) {
        double v4 = 0, v8 = 0;
        for (const ClearRow& c : clears) {
            if (c.name != name) continue;
            if (c.level == 4) v4 = c.seconds;
            if (c.level == 8) v8 = c.seconds;
        }
        r.drawText(FONT_SMALL, x0 + 6, y + 12, name.c_str(), 0xFFBFB4D8);
        const int w4 = (int)(barW * std::min(1.0, v4 / maxSeconds));
        const int w8 = (int)(barW * std::min(1.0, v8 / maxSeconds));
        r.fillRect(barX, y, std::max(3, w4), 15, 0xFF2F5F8B);
        r.fillRect(barX, y + 18, std::max(3, w8), 15, 0xFFD8C24A);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", v4);
        r.drawText(FONT_SMALL, barX + w4 + 8, y, buf, 0xFF9FB4D8, false);
        snprintf(buf, sizeof(buf), "%.1f", v8);
        r.drawText(FONT_SMALL, barX + w8 + 8, y + 18, buf, 0xFFFFE6A8, false);
        y += rowH;
    }
    r.drawText(FONT_SMALL, x0 + 6, y + 6, "верхняя полоса — ранг 4, нижняя — ранг 8", 0xFF8E84A8, false);
    r.drawText(FONT_SMALL, x0 + 6, y + 26,
               "ранг 8: разброс 3.2–5.5 с — арену освобождает любое оружие.", 0xFF8E84A8, false);
    r.drawText(FONT_SMALL, x0 + 6, y + 46,
               "ранг 4: 6.3–19.3 с — вот здесь выбор уже что-то значит.", 0xFF8E84A8, false);

    // ── Слева снизу: пул выбора ─────────────────────────────────────────────
    r.drawText(FONT_MED, x0, y + 86, "ПУЛ ВЫБОРА", 0xFFE8DCFF);
    static const char* pool[5] = {
        "30 с:   16 из 40 (оружие 7, артефакты 9)",
        "3 мин:  17 из 40 (оружие 7, артефакты 10)",
        "5 мин:  19 из 40 (оружие 7, артефакты 12)",
        "10 мин: 37 из 40 (оружие 7, артефакты 30)",
        "15 мин: 40 из 40 (оружие 7, артефакты 33)",
    };
    for (int i = 0; i < 5; ++i)
        r.drawText(FONT_SMALL, x0 + 6, y + 122 + i * 22, pool[i], 0xFFBFB4D8, false);

    // ── Справа: роли и перекрытия из таблицы оружия ─────────────────────────
    const int rx = 880;
    std::vector<Row> maxed;
    for (const Row& row : weapons) if (row.level == 8 && row.name.rfind("tier", 0) != 0) maxed.push_back(row);

    r.drawText(FONT_MED, rx, 122, "ЛУЧШИЙ В СЦЕНАРИИ (ранг 8)", 0xFFE8DCFF);
    static const char* scen[5] = {"220 px", "вплотную 90", "в упор 20", "идущая", "толпа 24"};
    int yy = 160;
    for (int s = 0; s < 5; ++s) {
        int best = -1; double bestVal = 0.0;
        for (size_t i = 0; i < maxed.size(); ++i) {
            const double v = s == 0 ? maxed[i].far220 : (s == 1 ? maxed[i].close90
                            : (s == 2 ? maxed[i].near20 : (s == 3 ? maxed[i].moving : maxed[i].crowd)));
            if (v > bestVal) { bestVal = v; best = (int)i; }
        }
        if (best < 0) continue;
        char buf[128];
        snprintf(buf, sizeof(buf), "%s: %s — %.0f", scen[s], maxed[best].name.c_str(), bestVal);
        r.drawText(FONT_SMALL, rx + 6, yy, buf, 0xFFBFB4D8, false);
        yy += 24;
    }

    r.drawText(FONT_MED, rx, yy + 22, "ПЕРЕКРЫТИЕ ПРОФИЛЕЙ", 0xFFE8DCFF);
    int y2 = yy + 58;
    for (size_t i = 0; i < maxed.size(); ++i)
        for (size_t j = i + 1; j < maxed.size(); ++j) {
            const double va[5] = {maxed[i].far220, maxed[i].close90, maxed[i].near20, maxed[i].moving, maxed[i].crowd};
            const double vb[5] = {maxed[j].far220, maxed[j].close90, maxed[j].near20, maxed[j].moving, maxed[j].crowd};
            double lo = 0, hi = 0;
            for (int k = 0; k < 5; ++k) { lo += std::min(va[k], vb[k]); hi += std::max(va[k], vb[k]); }
            const double ov = hi > 0 ? lo / hi : 0.0;
            if (ov < 0.70) continue;
            char buf[160];
            snprintf(buf, sizeof(buf), "%.2f  %s ~ %s", ov, maxed[i].name.c_str(), maxed[j].name.c_str());
            r.drawText(FONT_SMALL, rx + 6, y2, buf, 0xFFBFB4D8, false);
            y2 += 22;
        }
    if (y2 == yy + 58)
        r.drawText(FONT_SMALL, rx + 6, y2, "ни одна пара не перекрывается на 0.70 — профили разные", 0xFFBFB4D8, false);

    r.drawText(FONT_MED, rx, y2 + 26, "ПЕРЕКРЫТО ПОЛНОСТЬЮ (хуже во всех пяти)", 0xFFE8DCFF);
    int y3 = y2 + 62;
    int dominatedCount = 0;
    for (const Row& b : maxed) {
        const Row* best = nullptr;
        for (const Row& a : maxed) {
            if (&a == &b) continue;
            if (a.far220 >= b.far220 && a.close90 >= b.close90 && a.near20 >= b.near20 &&
                a.moving >= b.moving && a.crowd >= b.crowd &&
                (a.far220 > b.far220 || a.close90 > b.close90 || a.near20 > b.near20 ||
                 a.moving > b.moving || a.crowd > b.crowd)) {
                if (!best) best = &a;
            }
        }
        if (!best) continue;
        char buf[160];
        snprintf(buf, sizeof(buf), "%s  ->  %s", b.name.c_str(), best->name.c_str());
        r.drawText(FONT_SMALL, rx + 6, y3, buf, 0xFFD89090, false);
        y3 += 22;
        ++dominatedCount;
    }
    if (dominatedCount == 0)
        r.drawText(FONT_SMALL, rx + 6, y3, "нет: у каждого есть сценарий, где оно лучше всех", 0xFFBFB4D8, false);
    r.drawText(FONT_SMALL, rx + 6, y3 + 12,
               "но в живом бою решает не это: см. время зачистки слева", 0xFF8E84A8, false);
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: weapon_chart <in.csv> <out.ppm> [--variety <clear.csv>]\n"); return 1; }
    std::vector<Row> rows;
    if (!readCsv(argv[1], &rows)) return 1;

    SCREEN_W = 1600; SCREEN_H = 900;
    Renderer r;
    r.resize(SCREEN_W, SCREEN_H);
    r.clear(0xFF0E0C16);

    if (argc >= 5 && !strcmp(argv[3], "--variety")) {
        std::vector<ClearRow> clears;
        if (!readClearCsv(argv[4], &clears)) return 1;
        drawVariety(r, rows, clears);
        FILE* vf = fopen(argv[2], "wb");
        if (!vf) { printf("не записать %s\n", argv[2]); return 1; }
        fprintf(vf, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            const uint32_t c = r.pixels[i];
            unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)(c & 0xFF)};
            fwrite(rgb, 1, 3, vf);
        }
        fclose(vf);
        printf("%s\n", argv[2]);
        return 0;
    }

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
    // Цифры в этом блоке считаются из того же CSV, что и таблицы: подпись не
    // может разъехаться с картинкой после следующей правки баланса.
    {
        double bestSingle = 0.0, bestCrowd = 0.0, worstCrowd = 1e9;
        std::string singleName, crowdName, worstName;
        for (const Row& row : base) {
            if (row.level != 8) continue;
            const double single = std::max(row.far220, std::max(row.close90, row.near20));
            if (single > bestSingle) { bestSingle = single; singleName = row.name; }
            if (row.crowd > bestCrowd) { bestCrowd = row.crowd; crowdName = row.name; }
            if (row.crowd < worstCrowd) { worstCrowd = row.crowd; worstName = row.name; }
        }
        char buf[160];
        snprintf(buf, sizeof(buf), "• Лучшее по одной цели: %s — %.0f.", singleName.c_str(), bestSingle);
        r.drawText(FONT_SMALL, rx + 6, 616, buf, 0xFFBFB4D8, false);
        snprintf(buf, sizeof(buf), "• Лучшее по толпе: %s — %.0f.", crowdName.c_str(), bestCrowd);
        r.drawText(FONT_SMALL, rx + 6, 636, buf, 0xFFBFB4D8, false);
        snprintf(buf, sizeof(buf), "• Худшее по толпе: %s — %.0f (в %.1f раза меньше).",
                 worstName.c_str(), worstCrowd, bestCrowd / std::max(1.0, worstCrowd));
        r.drawText(FONT_SMALL, rx + 6, 656, buf, 0xFFBFB4D8, false);
        r.drawText(FONT_SMALL, rx + 6, 676,
                   "• Разрыв между лучшим и худшим в толпе — и есть цена выбора.",
                   0xFFBFB4D8, false);
        r.drawText(FONT_SMALL, rx + 6, 696,
                   "• Кто кого перекрывает и у кого своя роль —",
                   0xFFBFB4D8, false);
        r.drawText(FONT_SMALL, rx + 6, 716,
                   "  в design/weapons_variety.md и на weapons_variety.png.",
                   0xFFBFB4D8, false);
    }

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
