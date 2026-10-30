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


// ── Режим стратегии: картинка из стенда build_probe ───────────────────────────
//
//     g++ -O2 -std=c++17 -Isrc tools/build_probe.cpp -o build/build_probe
//     build/build_probe 60 300 /tmp/build.csv
//     g++ -O2 -std=c++17 -Isrc tools/weapon_chart.cpp -o build/weapon_chart
//     build/weapon_chart /tmp/weapon_after.csv design/weapons_strategy.ppm \
//         --strategy /tmp/build.csv
struct StratRow {
    std::string kind, name;
    double survived = 0.0;
    int kills = 0;
    double damage = 0.0;
    int peak = 0;
};

static std::vector<std::string> splitComma(const char* line) {
    std::vector<std::string> out;
    const char* p = line;
    while (*p) {
        const char* c = strchr(p, ',');
        if (!c) {
            std::string tail(p);
            while (!tail.empty() && (tail.back() == '\n' || tail.back() == '\r')) tail.pop_back();
            out.push_back(tail);
            break;
        }
        out.emplace_back(p, (size_t)(c - p));
        p = c + 1;
    }
    return out;
}

static bool readStrategyCsv(const char* path, std::vector<StratRow>* rows) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("не открыть %s\n", path); return false; }
    char line[512];
    bool header = true;
    while (fgets(line, sizeof(line), f)) {
        if (header) { header = false; continue; }
        const std::vector<std::string> f7 = splitComma(line);
        if (f7.size() < 6) continue;
        StratRow row;
        row.kind = f7[0];
        row.name = f7[1];
        row.survived = atof(f7[2].c_str());
        row.kills = atoi(f7[3].c_str());
        row.damage = atof(f7[4].c_str());
        row.peak = (int)atof(f7[6].c_str());
        rows->push_back(row);
    }
    fclose(f);
    return !rows->empty();
}

// Имя для картинки: в шрифте нет стрелки, поэтому «->».
static std::string wallName(std::string s) {
    const std::string arrow = "→";
    for (size_t at = s.find(arrow); at != std::string::npos; at = s.find(arrow, at)) s.replace(at, arrow.size(), "->");
    return s;
}

// «стартовая сборка (0:1) -> БЬЁРН» на картинке нужно как «БЬЁРН».
static std::string afterArrow(const std::string& s) {
    const std::string arrow = "-> ";
    const size_t at = s.find(arrow);
    return at == std::string::npos ? s : s.substr(at + arrow.size());
}

static const StratRow* findRow(const std::vector<StratRow>& rows, const char* kind, const char* prefix) {
    for (const StratRow& row : rows)
        if (row.kind == kind && row.name.rfind(prefix, 0) == 0) return &row;
    return nullptr;
}

static void drawBars(Renderer& r, int x, int y, int labelW, int barW, const char* title,
                     const std::vector<const StratRow*>& items, bool secondsMode, uint32_t color) {
    r.drawText(FONT_MED, x, y, title, 0xFFE8DCFF);
    double maxV = 1.0;
    for (const StratRow* item : items) maxV = std::max(maxV, secondsMode ? item->survived : (double)item->kills);
    int rowY = y + 34;
    for (const StratRow* item : items) {
        const double v = secondsMode ? item->survived : (double)item->kills;
        const int w = (int)(barW * (v / maxV));
        r.drawText(FONT_SMALL, x, rowY + 2, wallName(item->name).c_str(), 0xFFCFC4E4, false);
        r.fillRect(x + labelW, rowY, barW, 18, 0xFF221C33);
        r.fillRect(x + labelW, rowY, std::max(2, w), 18, color);
        char buf[32];
        if (secondsMode) snprintf(buf, sizeof(buf), "%.0f с", v);
        else snprintf(buf, sizeof(buf), "%d", (int)v);
        r.drawText(FONT_SMALL, x + labelW + barW + 10, rowY + 2, buf, 0xFFF4EEFF, false);
        rowY += 26;
    }
}

static void drawStrategy(Renderer& r, const std::vector<StratRow>& rows) {
    r.drawText(FONT_LARGE, 40, 22, "СТРАТЕГИЯ: ЧТО ДАЮТ ОДНИ И ТЕ ЖЕ 17 РАНГОВ ВЛОЖЕНИЙ", 0xFFFFE6A8);
    r.drawText(FONT_SMALL, 40, 58,
               "стенд tools/build_probe.cpp: 12 сборок с равным бюджетом, фиксированное зерно, поток 8 мобов/с, здоровье как на 10-й минуте",
               0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 40, 78,
               "слева герой бессмертен (меряется темп), справа смертен (меряется, сколько он живёт); справа 300 с — предел замера, а не «ровно столько и умер»",
               0xFF9C93B4, false);

    std::vector<const StratRow*> pressure, real;
    for (const StratRow& row : rows) {
        if (row.kind == "pressure") pressure.push_back(&row);
        else if (row.kind == "real") real.push_back(&row);
    }
    drawBars(r, 40, 112, 215, 330, "1. НАПОР: убито за 60 с потока", pressure, false, 0xFF6FD3C8);
    drawBars(r, 810, 112, 215, 330, "2. ЗАБЕГ: сколько секунд прожил", real, true, 0xFFD08A6F);

    // Наблюдения считаются из того же CSV, что и полосы: подпись не разъедется
    // с картинкой после следующей правки баланса.
    const StratRow* deep = findRow(rows, "real", "Клинки: симбиоз");
    const StratRow* wide = findRow(rows, "real", "Клинки: ширина");
    const StratRow* axe = findRow(rows, "real", "Топор: симбиоз");
    const StratRow* blade = findRow(rows, "pressure", "Клинки: симбиоз");
    const StratRow* wideP = findRow(rows, "pressure", "Клинки: ширина");

    double startMin = 1.0e9, startMax = 0.0;
    std::string startMinName, startMaxName;
    for (const StratRow& row : rows) {
        if (row.kind != "hero" || row.name.find("(0:1)") == std::string::npos) continue;
        if (row.survived < startMin) { startMin = row.survived; startMinName = row.name; }
        if (row.survived > startMax) { startMax = row.survived; startMaxName = row.name; }
    }
    const StratRow* standStart = findRow(rows, "style", "старт: стоит");
    const StratRow* moveStart = findRow(rows, "style", "старт: обходит");
    const StratRow* standStrong = findRow(rows, "style", "сильная: стоит");
    const StratRow* moveStrong = findRow(rows, "style", "сильная: обходит");

    r.drawText(FONT_MED, 40, 476, "ЧТО ИЗ ЭТОГО СЛЕДУЕТ", 0xFFE8DCFF);
    char buf[256];
    int y = 510;
    if (deep && wide) {
        snprintf(buf, sizeof(buf), "• Ширина проигрывает глубине: два оружия по 6 — %.0f с и %d убийств, одно оружие 9-го ранга — %.0f с и %d.",
                 wide->survived, wide->kills, deep->survived, deep->kills);
        r.drawText(FONT_SMALL, 46, y, buf, 0xFFBFB4D8, false); y += 22;
    }
    if (blade && wideP) {
        snprintf(buf, sizeof(buf), "  В напоре у смешанной сборки %d убийств против %d: очередь врагов растёт, а не рассасывается.",
                 wideP->kills, blade->kills);
        r.drawText(FONT_SMALL, 46, y, buf, 0xFF9C93B4, false); y += 22;
    }
    if (startMin > 0.0 && startMax > 0.0) {
        snprintf(buf, sizeof(buf), "• Герой решает на старте: жизнь от %.1f с (%s) до %.1f с (%s), разброс x%.1f.",
                 startMin, afterArrow(wallName(startMinName)).c_str(),
                 startMax, afterArrow(wallName(startMaxName)).c_str(),
                 startMax / std::max(0.1, startMin));
        r.drawText(FONT_SMALL, 46, y, buf, 0xFFBFB4D8, false); y += 22;
        r.drawText(FONT_SMALL, 46, y, "  На сильной сборке все пятеро доживают до 300 с: к пятому-девятому рангу оружия статы героя тонут в билде.",
                   0xFF9C93B4, false); y += 22;
    }
    if (standStart && moveStart) {
        snprintf(buf, sizeof(buf), "• Стиль: на старте обход по кругу продлевает жизнь (%.1f -> %.1f с).", standStart->survived, moveStart->survived);
        r.drawText(FONT_SMALL, 46, y, buf, 0xFFBFB4D8, false); y += 22;
    }
    if (standStrong && moveStrong) {
        snprintf(buf, sizeof(buf), "  На сильной сборке оба стиля доживают до %.0f с: движение перестаёт быть обязательным.", std::min(standStrong->survived, moveStrong->survived));
        r.drawText(FONT_SMALL, 46, y, buf, 0xFF9C93B4, false); y += 22;
    }
    if (axe && deep) {
        snprintf(buf, sizeof(buf), "• Разница между лучшей и худшей сборкой: %.0f с против %.0f с — в %.1f раза.",
                 deep->survived, axe->survived, deep->survived / std::max(1.0, axe->survived));
        r.drawText(FONT_SMALL, 46, y, buf, 0xFFBFB4D8, false); y += 22;
    }
    r.drawText(FONT_SMALL, 46, y, "• Решения: забег на 5 минут даёт 16 прокачек; симбиоз (оружие 9 + резонанс IV) забирает 13 из них — 81 %.",
               0xFFBFB4D8, false);
    y += 40;

    // Цена симбиоза: какой артефакт обязан быть IV ранга и что он даёт по урону.
    // Числа — из таблицы пассивок того же стенда (weapon_probe, блок пассивок).
    r.drawText(FONT_MED, 40, y, "ЦЕНА СИМБИОЗА: КАКОЙ АРТЕФАКТ ОБЯЗАН БЫТЬ IV РАНГА И ЧТО ОН ДАЁТ", 0xFFE8DCFF);
    static const char* price[7] = {
        "Клинки  ->  Книга мудрости: 0 % урона, только опыт",
        "Шар     ->  Сапоги ветра: 0 % урона, только скорость",
        "Вихрь   ->  Магнитный амулет: 0 % урона, только сбор",
        "Молния  ->  Сердце титана: 0 % урона, только HP",
        "Аура    ->  Перчатка берсерка: +129 % урона",
        "Топор   ->  Часы вечности: +68 % урона",
        "Лёд     ->  Проклятая монета: 0 % урона, золото и злее элиты",
    };
    for (int i = 0; i < 7; ++i) {
        const int col = i % 3;
        const int rowI = i / 3;
        const uint32_t color = (i >= 4) ? 0xFF9FE8B8 : 0xFFBFB4D8;
        r.drawText(FONT_SMALL, 46 + col * 500, y + 34 + rowI * 24, price[i], color, false);
    }
    r.drawText(FONT_SMALL, 46, y + 34 + 3 * 24 + 10, "У пяти из семи оружий симбиоз требует вложить четыре ранга в артефакт без урона; у ауры и топора тот же ранг идёт в урон.", 0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 46, y + 34 + 3 * 24 + 30, "На карточке оружия 8-го ранга теперь написано, какой именно артефакт нужен и сколько у него уже рангов.", 0xFF9C93B4, false);

    r.drawText(FONT_SMALL, 40, 866,
               "цвет полосы — порядок строки: слева темп, справа живучесть; подробности — design/weapons_strategy.md",
               0xFF8E84A8, false);
}


// ── Режим матрицы: 7 оружий × 3 способа добрать третий слот ──────────────────
//     build/build_probe 60 300 /tmp/build.csv
//     build/weapon_chart /tmp/weapon_after.csv design/build_diversity.ppm --matrix /tmp/build.csv
static void drawMatrix(Renderer& r, const std::vector<StratRow>& rows) {
    r.drawText(FONT_LARGE, 40, 22, "РАЗНООБРАЗИЕ СБОРОК: 7 ОРУЖИЙ × 3 ТРЕТЬИХ СЛОТА", 0xFFFFE6A8);
    r.drawText(FONT_SMALL, 40, 58,
               "у всех 21 сборки один бюджет: оружие 9-го ранга + его резонанс IV + один артефакт; 300 с штатного спавна, среднее по трём зёрнам",
               0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 40, 78,
               "в клетке — сколько секунд сборка прожила; 300 с — предел замера (значит «пережила»), меньше — умерла; под числом убийства",
               0xFF9C93B4, false);

    static const char* fillers[3] = {"урон (берсерк)", "оборона (клятва)", "частота (часы)"};
    int colX[4] = {420, 760, 1100, 1440};
    r.drawText(FONT_SMALL, 44, 118, "оружие", 0xFF8E84A8, false);
    for (int c = 0; c < 3; ++c) r.drawText(FONT_SMALL, colX[c], 118, fillers[c], 0xFF8E84A8, false);

    int rowY = 146;
    for (int w = 0; w < 7; ++w) {
        // Строки лежат в CSV по три подряд: ищем по префиксу имени.
        const StratRow* cells[3] = {nullptr, nullptr, nullptr};
        static const char* shortName[7] = {"Клинки", "Шар", "Вихрь", "Молния", "Аура", "Топор", "Лёд"};
        for (const StratRow& row : rows) {
            if (row.kind != "matrix") continue;
            for (int c = 0; c < 3; ++c) {
                char prefix[64];
                snprintf(prefix, sizeof(prefix), "%s + %s", shortName[w], fillers[c]);
                if (row.name == prefix) cells[c] = &row;
            }
        }
        r.drawText(FONT_MED, 44, rowY + 12, shortName[w], 0xFFE8DCFF, false);
        for (int c = 0; c < 3; ++c) {
            const int x = colX[c], y = rowY, wdt = 300, h = 46;
            if (!cells[c]) { r.fillRect(x, y, wdt, h, 0xFF1A1626); continue; }
            const double t = std::min(1.0, cells[c]->survived / 300.0);
            const uint32_t fill = mixColor(0xFF7A2E3A, 0xFF3FA97B, (float)t);
            r.fillRect(x, y, wdt, h, 0xFF120F1C);
            r.fillRect(x, y, (int)(wdt * t), h, fill);
            char buf[48];
            snprintf(buf, sizeof(buf), "%.0f с", cells[c]->survived);
            r.drawText(FONT_SMALL, x + 10, y + 6, buf, 0xFFF4EEFF, false);
            snprintf(buf, sizeof(buf), "%d убийств", cells[c]->kills);
            r.drawText(FONT_SMALL, x + 10, y + 26, buf, 0xFFD8D0EA, false);
        }
        rowY += 54;
    }

    // Итоги считаются из данных, а не переписаны руками.
    int survivedAll = 0, total = 0;
    int bestKills = 0, worstKills = 1 << 30;
    std::string bestName, worstName;
    for (const StratRow& row : rows) {
        if (row.kind != "matrix") continue;
        ++total;
        if (row.survived >= 299.0) ++survivedAll;
        if (row.kills > bestKills) { bestKills = row.kills; bestName = row.name; }
        if (row.kills < worstKills) { worstKills = row.kills; worstName = row.name; }
    }
    char line[256];
    r.drawText(FONT_MED, 40, rowY + 20, "ЧТО ВИДНО", 0xFFE8DCFF);
    snprintf(line, sizeof(line), "• Пережили замер во всех трёх зёрнах %d сборок из %d: третий слот решает не всё, а только для слабых оружий.", survivedAll, total);
    r.drawText(FONT_SMALL, 46, rowY + 54, line, 0xFFBFB4D8, false);
    snprintf(line, sizeof(line), "• Разброс темпа: от %d убийств (%s) до %d (%s) — в %.1f раза.", worstKills, worstName.c_str(), bestKills, bestName.c_str(), (double)bestKills / std::max(1, worstKills));
    r.drawText(FONT_SMALL, 46, rowY + 76, line, 0xFFBFB4D8, false);
    r.drawText(FONT_SMALL, 46, rowY + 98, "• У клинков, молнии, ауры и льда все три слота дают 300 с — там третий слот не меняет исход, только счёт.", 0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 46, rowY + 120, "• У шара и топора без обороны забег кончается на 164–206-й секунде: это не «слабый урон», а нехватка живучести.", 0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 46, rowY + 142, "  Разбор — design/build_diversity.md.", 0xFF9C93B4, false);

    // Артефакты по живой волне: сюда попадают добивание, ожог и откол,
    // которых не видно на бессмертных мишенях.
    std::vector<const StratRow*> artifacts;
    for (const StratRow& row : rows) if (row.kind == "artifact") artifacts.push_back(&row);
    if (!artifacts.empty()) {
        std::sort(artifacts.begin(), artifacts.end(), [](const StratRow* a, const StratRow* b) {
            return a->survived < b->survived;
        });
        r.drawText(FONT_MED, 40, rowY + 190, "АРТЕФАКТЫ ПО ЖИВОЙ ВОЛНЕ: клинки 4-го ранга против 40 мобов", 0xFFE8DCFF);
        const int shown = std::min<int>(8, (int)artifacts.size());
        for (int i = 0; i < shown; ++i) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s — %.2f с, %+.0f%%",
                     artifacts[i]->name.c_str(), artifacts[i]->survived, artifacts[i]->damage);
            const uint32_t color = artifacts[i]->damage >= 8.0 ? 0xFF9FE8B8 : 0xFFBFB4D8;
            r.drawText(FONT_SMALL, 46 + (i / 4) * 760, rowY + 222 + (i % 4) * 22, buf, color, false);
        }
    }
}


// ── Режим каталога: 21 сборка в трёх окнах забега ───────────────────────────
//     build/build_probe 60 300 /tmp/build.csv
//     build/weapon_chart /tmp/weapon_after.csv design/builds_catalog.ppm --catalog /tmp/build.csv
// Полоса — убийства к концу окна. В CSV две строки на сборку: catalog — ранги
// стоят с нулевой секунды (потолок), catalogramp — те же ранги, выданные по
// одной прокачке (честная сборка). Тёмная часть полосы — потолок, светлая —
// честная сборка; разница между ними и есть цена позднего прихода карт.
static void drawCatalog(Renderer& r, const std::vector<StratRow>& rows) {
    r.drawText(FONT_LARGE, 40, 20, "КАТАЛОГ СБОРОК: ЧТО РЕАЛЬНО СОБИРАЕТСЯ ЗА 5, 10 И 15 МИНУТ", 0xFFFFE6A8);
    r.drawText(FONT_SMALL, 40, 56,
               "стенд tools/build_probe.cpp: 21 сборка из существующих 40 предметов, три зерна, герой бессмертен (меряется состав, а не выживание)",
               0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 40, 76,
               "цифры справа от полосы — убийства к концу окна и пик живых врагов; тёмный хвост полосы — потолок, если бы все ранги пришли на нулевой секунде",
               0xFF9C93B4, false);

    std::vector<const StratRow*> honest, ceiling;
    for (const StratRow& row : rows) {
        if (row.kind == "catalogramp") honest.push_back(&row);
        else if (row.kind == "catalog") ceiling.push_back(&row);
    }
    if (honest.empty() || honest.size() != ceiling.size()) {
        r.drawText(FONT_MED, 40, 130, "в CSV нет строк catalog/catalogramp — пересоберите стенд", 0xFFE07A7A, false);
        return;
    }

    struct EpochDef { const char* title; const char* budget; int first; int count; };
    const EpochDef epochs[3] = {
        {"0–5 МИНУТ", "15–17 прокачек, открыт тир I", 0, 7},
        {"5–10 МИНУТ", "25–28 прокачек, открылись тиры II–III", 7, 8},
        {"10–15 МИНУТ", "33–37 прокачек, открылись тиры IV–V", 15, 6},
    };
    const int colW = 512, labelW = 196, barW = 190, rowH = 30;

    for (int e = 0; e < 3; ++e) {
        const EpochDef& def = epochs[e];
        const int x = 40 + e * colW;
        double maxKills = 1.0;
        for (int i = def.first; i < def.first + def.count && i < (int)ceiling.size(); ++i)
            maxKills = std::max(maxKills, (double)ceiling[i]->kills);
        r.drawText(FONT_MED, x, 110, def.title, 0xFFFFD79A);
        r.drawText(FONT_SMALL, x + 4, 132, def.budget, 0xFF8E84A8, false);
        int y = 158;
        for (int i = def.first; i < def.first + def.count && i < (int)honest.size(); ++i) {
            const StratRow* h = honest[i];
            const StratRow* c = ceiling[i];
            r.drawText(FONT_SMALL, x, y + 2, wallName(h->name).c_str(), 0xFFD8D0EA, false);
            const int full = (int)(barW * std::min(1.0, (double)c->kills / maxKills));
            const int fill = (int)(barW * std::min(1.0, (double)h->kills / maxKills));
            r.fillRect(x + labelW, y, barW, 17, 0xFF1B1728);
            r.fillRect(x + labelW, y, std::max(2, full), 17, 0xFF3A586E);
            r.fillRect(x + labelW, y, std::max(2, fill), 17, 0xFF57C9A0);
            char buf[48];
            snprintf(buf, sizeof(buf), "%d", h->kills);
            r.drawText(FONT_SMALL, x + labelW + barW + 8, y + 1, buf, 0xFFF4EEFF, false);
            snprintf(buf, sizeof(buf), "%d", h->peak);
            r.drawText(FONT_SMALL, x + labelW + barW + 76, y + 1, buf, 0xFF8E84A8, false);
            y += rowH;
        }
    }

    // Итоги считаются из данных, а не переписаны руками.
    double sumGap = 0.0, maxGap = 0.0;
    std::string maxGapName;
    int peakLo = 1 << 30, peakHi = 0;
    for (size_t i = 0; i < honest.size(); ++i) {
        const double gap = ceiling[i]->kills > 0
            ? 100.0 * (ceiling[i]->kills - honest[i]->kills) / (double)ceiling[i]->kills : 0.0;
        sumGap += gap;
        if (gap > maxGap) { maxGap = gap; maxGapName = wallName(honest[i]->name); }
        peakLo = std::min(peakLo, honest[i]->peak);
        peakHi = std::max(peakHi, honest[i]->peak);
    }
    char line[320];
    r.drawText(FONT_MED, 40, 434, "ЧТО ВИДНО", 0xFFE8DCFF);
    int y = 468;
    snprintf(line, sizeof(line), "• Поздний приход карт стоит %.1f %% убийств в среднем; хуже всех «%s» — %.0f %%. Сборка не рушится от того, что карты пришли позже.",
             sumGap / (double)honest.size(), maxGapName.c_str(), maxGap);
    r.drawText(FONT_SMALL, 46, y, line, 0xFFBFB4D8, false); y += 24;
    static const char* epochNames[3] = {"5 мин", "10 мин", "15 мин"};
    snprintf(line, sizeof(line), "• Внутри окна разброс убийств: %s — ", epochNames[0]);
    {
        std::string spread;
        for (int e = 0; e < 3; ++e) {
            const EpochDef& def = epochs[e];
            int lo = 1 << 30, hi = 0;
            for (int i = def.first; i < def.first + def.count && i < (int)honest.size(); ++i) {
                lo = std::min(lo, honest[i]->kills);
                hi = std::max(hi, honest[i]->kills);
            }
            char part[64];
            snprintf(part, sizeof(part), "%s%s %.0f %%", e ? ", " : "", epochNames[e],
                     100.0 * (hi - lo) / std::max(1, lo));
            spread += part;
        }
        snprintf(line, sizeof(line), "• Внутри окна разброс убийств: %s. До 5-й минуты билд почти неразличим, после — решает.", spread.c_str());
    }
    r.drawText(FONT_SMALL, 46, y, line, 0xFFBFB4D8, false); y += 24;
    snprintf(line, sizeof(line), "• Затор в толпе гуляет от %d до %d живых: сборки с заморозкой и отталкиванием держат поле, чистый урон копит.", peakLo, peakHi);
    r.drawText(FONT_SMALL, 46, y, line, 0xFFBFB4D8, false); y += 24;
    r.drawText(FONT_SMALL, 46, y, "• Планы на 30 и 39 рангов не влезают: за окно игра выдаёт 15–17, 25–28 и 33–37 прокачек — последние карты приходят после закрытия окна.", 0xFFBFB4D8, false); y += 24;
    r.drawText(FONT_SMALL, 46, y, "• Опыт — тоже ресурс билда: самая сильная сборка получает 17.5 / 30.5 / 39.0 прокачек там, где средняя — 17 / 26 / 37.", 0xFFBFB4D8, false); y += 36;

    r.drawText(FONT_MED, 40, y, "ЧЕГО В ИГРЕ НЕТ (подробно — design/builds_catalog.md)", 0xFFE8DCFF);
    y += 30;
    static const char* gaps[4] = {
        "• Билда на щит: щит дают только Покров и Чаша, «щит за убийство» отсутствует.",
        "• Золото в забеге ничего не покупает: монета — 4 ранга в ноль боевой ценности.",
        "• Добивание не архетип: Палач капут на 3 рангах и не работает по стражам.",
        "• Нет отражения, дальнего боя, призыва и складывающегося урона по времени.",
    };
    for (int i = 0; i < 4; ++i) {
        r.drawText(FONT_SMALL, 46, y, gaps[i], 0xFF9C93B4, false);
        y += 20;
    }

    // Пустое место внизу — под правила, из которых эти сборки и складываются.
    const int ry = 724;
    r.drawText(FONT_MED, 40, ry, "ПЯТЬ ПРАВИЛ, ИЗ КОТОРЫХ СКЛАДЫВАЕТСЯ ЛЮБОЙ БИЛД", 0xFFE8DCFF);
    static const char* rules[5] = {
        "1. Бюджет окна: 15–17 / 25–28 / 33–37 прокачек. План на 39 рангов в 15 минут не влезает.",
        "2. Опыт — ресурс: быстрее убиваешь — больше карт. Слабый старт отстаёт навсегда.",
        "3. Девятый ранг оружия существует только вместе с резонансной реликвией IV.",
        "4. Поздний предмет в начале плана сжигает прокачки: окно выбора не ждёт открытия.",
        "5. Слоты: 5 + стражи + уровень/10. К 15-й минуте это 8–9 предметов, а не «всё открытое».",
    };
    for (int i = 0; i < 5; ++i) {
        r.drawText(FONT_SMALL, 46, ry + 30 + i * 20, rules[i], 0xFF9C93B4, false);
    }

    r.drawText(FONT_SMALL, 40, 866,
               "полный разбор каждой сборки с составом и числами — design/builds_catalog.md; стенд — tools/build_probe.cpp, блок 8",
               0xFF8E84A8, false);
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: weapon_chart <in.csv> <out.ppm> [--variety <clear.csv>]\n"); return 1; }
    std::vector<Row> rows;
    if (!readCsv(argv[1], &rows)) return 1;

    SCREEN_W = 1600; SCREEN_H = 900;
    Renderer r;
    r.resize(SCREEN_W, SCREEN_H);
    r.clear(0xFF0E0C16);

    if (argc >= 5 && !strcmp(argv[3], "--matrix")) {
        std::vector<StratRow> strat;
        if (!readStrategyCsv(argv[4], &strat)) return 1;
        drawMatrix(r, strat);
        FILE* mf = fopen(argv[2], "wb");
        if (!mf) { printf("не записать %s\n", argv[2]); return 1; }
        fprintf(mf, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            const uint32_t c = r.pixels[i];
            unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)(c & 0xFF)};
            fwrite(rgb, 1, 3, mf);
        }
        fclose(mf);
        printf("%s\n", argv[2]);
        return 0;
    }

    if (argc >= 5 && !strcmp(argv[3], "--strategy")) {
        std::vector<StratRow> strat;
        if (!readStrategyCsv(argv[4], &strat)) return 1;
        drawStrategy(r, strat);
        FILE* sf = fopen(argv[2], "wb");
        if (!sf) { printf("не записать %s\n", argv[2]); return 1; }
        fprintf(sf, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            const uint32_t c = r.pixels[i];
            unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)(c & 0xFF)};
            fwrite(rgb, 1, 3, sf);
        }
        fclose(sf);
        printf("%s\n", argv[2]);
        return 0;
    }

    if (argc >= 5 && !strcmp(argv[3], "--catalog")) {
        std::vector<StratRow> strat;
        if (!readStrategyCsv(argv[4], &strat)) return 1;
        drawCatalog(r, strat);
        FILE* cf = fopen(argv[2], "wb");
        if (!cf) { printf("не записать %s\n", argv[2]); return 1; }
        fprintf(cf, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            const uint32_t c = r.pixels[i];
            unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)(c & 0xFF)};
            fwrite(rgb, 1, 3, cf);
        }
        fclose(cf);
        printf("%s\n", argv[2]);
        return 0;
    }

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
