// ── Стенд разнообразия: отличаются ли варианты друг от друга ─────────────────
//
// Прошлый стенд (tools/weapon_probe.cpp) отвечал на вопрос «сколько урона».
// Этот отвечает на другой: если урон разный, значит ли это, что выбор разный?
//
// Три проверки:
//
//   1. Зачистка волны. 40 живых мобов с настоящим здоровьем (как на 5-й минуте)
//      идут на героя; оружие одно. Считается время до полной зачистки. На
//      бессмертных мишенях «площадное» и «одиночное» оружие меряется одинаково,
//      а на живой волне видно, кто реально освобождает арену.
//   2. Роли и дубли. Из таблицы tools/weapon_probe.cpp берутся четыре сценария
//      (220 px, вплотную, идущая цель, толпа), считается охват и сходство
//      профилей: два оружия с одинаковым профилем — не выбор, а переименование.
//      Отдельно ищется строгое доминирование: оружие, которое хуже другого во
//      всех четырёх сценариях.
//   3. Пул выбора. Сколько предметов вообще может выпасть в начале забега, на
//      3-й, 5-й, 10-й и 15-й минуте: если к 5-й минуте пул уже 40 из 40, рост
//      силы идёт числами, а не новыми вариантами.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/variety_probe.cpp -o build/variety_probe
//     build/variety_probe 90 /tmp/weapon_after.csv /tmp/variety.csv
//       argv[1] — лимит времени на волну, с;
//       argv[2] — CSV из weapon_probe (необязательно, без него нет блока ролей);
//       argv[3] — куда записать CSV зачистки (необязательно).
//
// Код возврата 1, если оружие не зачистило волну за лимит или если нашлось
// оружие, полностью перекрытое другим.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "renderer.h"
#include "game.h"

static const int WAVE = 40;          // мобов в волне
static const float WAVE_INNER = 260.0f;
static const float WAVE_OUTER = 460.0f;

struct ClearResult {
    bool cleared = false;
    double seconds = 0.0;
    int kills = 0;
    double avgAlive = 0.0;
    int maxAlive = 0;
    int left = 0;          // сколько мобов осталось на арене
};

// Та же чистая арена, что и в стенде оружия: ни волн, ни отрядов, ни стражей.
static void resetArena(Game& g) {
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.bossHazards.clear();
    g.particles.clear();
    g.lightningArcs.clear();
    g.frostWaves.clear();
    g.balance.spawnMultiplier = 0.0f;
    g.squadTimer = 1.0e9f;
    g.ratTimer = 1.0e9f;
    g.resonatorSpawnTimer = 1.0e9f;
    g.portalSpawnTimer = 1.0e9f;
    g.nextBossIndex = 999;
    g.eventCooldown = 1.0e9f;
    g.huntActive = false;
    g.pendingResonatorBurst = 0;
    g.pendingMatryoshkaSplits.clear();
    // Убийства дают опыт, опыт — уровни, а уровень — «ПРОРЫВ» с лишней ордой.
    // В замере волны это лишние мобы: выключаем порог прорыва.
    g.nextBreakthroughLevel = 999999;
    g.breakthroughActive = false;
    g.breakthroughTimer = 0.0f;
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.playerX = 0.0f;
    g.playerY = 0.0f;
    g.playerHp = g.playerMaxHp;
    g.playerHurtTimer = 0.0f;
    g.kills = 0;
    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
}

// Одна волна против одного оружия. Мобы идут сами, оружие стреляет само —
// ровно так, как это выглядит в игре, только окна и звука нет.
static ClearResult measureClear(Game& g, int itemId, int level, bool evolved, int passiveId, float limit) {
    ClearResult res;
    const float dt = 1.0f / 60.0f;
    resetArena(g);
    // Здоровье мобов — как на пятой минуте забега (масштаб времени 1.96).
    g.runTime = 300.0f;
    g.itemLevels[itemId] = level;
    if (itemId >= 0 && itemId < 7) {
        g.evolvedWeapons[itemId] = evolved;
        g.weaponTimers[itemId] = 0.0f;
    }
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;
    if (passiveId >= 0) g.itemLevels[passiveId] = g.getItemMaxLevel(passiveId);

    const int killsBefore = g.kills;
    for (int i = 0; i < WAVE; ++i) {
        const int type = i % 12;
        const bool elite = (i % 8 == 7);
        const float t = WAVE > 1 ? (float)i / (float)(WAVE - 1) : 0.0f;
        const float r = WAVE_INNER + (WAVE_OUTER - WAVE_INNER) * t;
        const float a = i * 2.39996323f;
        g.spawnMonster(type, elite, false);
        if (g.monsters.empty()) break;
        Monster& m = g.monsters.back();
        m.x = cosf(a) * r;
        m.y = sinf(a) * r;
        m.vx = m.vy = 0.0f;
    }

    const int frames = (int)(limit / dt);
    long long aliveSum = 0;
    for (int f = 0; f < frames; ++f) {
        g.state = STATE_PLAYING;
        g.update(dt);
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
        aliveSum += (long long)g.monsters.size();
        res.maxAlive = std::max(res.maxAlive, (int)g.monsters.size());
        res.seconds += dt;
        if (g.monsters.empty()) { res.cleared = true; break; }
    }
    res.kills = g.kills - killsBefore;
    res.left = (int)g.monsters.size();
    res.avgAlive = res.seconds > 0.0 ? (double)aliveSum / (res.seconds / dt) : 0.0;
    return res;
}

// ── Чтение таблицы стенда оружия ─────────────────────────────────────────────
struct WeaponRow {
    std::string name;
    int level = 0;
    double far220 = 0, close90 = 0, near20 = 0, moving = 0, crowd = 0;
};

static std::vector<WeaponRow> readWeaponCsv(const char* path) {
    std::vector<WeaponRow> rows;
    FILE* f = fopen(path, "r");
    if (!f) return rows;
    char line[512];
    bool header = true;
    while (fgets(line, sizeof(line), f)) {
        if (header) { header = false; continue; }
        WeaponRow row;
        char name[128] = {0};
        int level = 0;
        double a = 0, b = 0, n = 0, c = 0, d = 0, e = 0, g = 0, h = 0;
        if (sscanf(line, "%127[^,],%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", name, &level,
                   &a, &b, &n, &c, &d, &e, &g, &h) < 5) continue;
        row.name = name; row.level = level;
        row.far220 = a; row.close90 = b; row.near20 = n; row.moving = c; row.crowd = d;
        rows.push_back(row);
    }
    fclose(f);
    return rows;
}

// Насколько оружие «площадное»: во сколько раз толпа даёт больше, чем одна цель.
static double singleTarget(const WeaponRow& r) {
    // «Одна цель» — лучший из одиночных сценариев: у контактного оружия
    // дистанция 220 px нулевая не потому, что оно слабое, а потому что не достаёт.
    return std::max(std::max(r.far220, r.close90), r.near20);
}

static double coverage(const WeaponRow& r) {
    const double single = singleTarget(r);
    return single > 1.0 ? r.crowd / single : (r.crowd > 0.0 ? 99.0 : 0.0);
}

static const char* archetype(const WeaponRow& r) {
    const double cov = coverage(r);
    if (cov >= 2.5) return "площадное";
    if (cov <= 1.15) return "по одной цели";
    if (r.close90 >= r.far220 * 1.3) return "ближнее";
    return "смешанное";
}

// Перекрытие профилей: сумма минимумов к сумме максимумов. В отличие от
// косинуса учитывает масштаб, поэтому «аура в 20 раз сильнее льда» не
// превращается в «профили совпадают на 1.00».
static double overlap(const WeaponRow& a, const WeaponRow& b) {
    const double va[5] = {a.far220, a.close90, a.near20, a.moving, a.crowd};
    const double vb[5] = {b.far220, b.close90, b.near20, b.moving, b.crowd};
    double lo = 0.0, hi = 0.0;
    for (int i = 0; i < 5; ++i) { lo += std::min(va[i], vb[i]); hi += std::max(va[i], vb[i]); }
    return hi > 0.0 ? lo / hi : 0.0;
}

int main(int argc, char** argv) {
    const float limit = argc > 1 ? (float)atof(argv[1]) : 90.0f;
    const char* weaponCsv = argc > 2 && strcmp(argv[2], "-") ? argv[2] : nullptr;
    const char* outCsv = argc > 3 ? argv[3] : nullptr;
    FILE* csv = outCsv ? fopen(outCsv, "w") : nullptr;
    if (csv) fprintf(csv, "weapon,level,cleared,seconds,kills,avgAlive,maxAlive\n");

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.simulationMode = true;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;

    static const char* weaponName[7] = {
        "Серебряные клинки", "Огненный шар", "Рунический вихрь",
        "Цепная молния", "Кровавая аура", "Боевой топор", "Ледяное кольцо"
    };

    int failures = 0;
    printf("Стенд разнообразия: волна из %d живых мобов (здоровье как на 5-й минуте),\n", WAVE);
    printf("герой стоит в центре, оружие одно, лимит %.0f с.\n\n", limit);
    printf("%-30s %-12s %5s %9s %9s %9s %8s\n", "оружие", "роль", "ранг", "зачистка", "с/моб", "живых", "осталось");
    printf("%-30s %-12s %5s %9s %9s %9s %8s\n", "", "", "", "(с)", "", "в сред.", "мобов");

    struct ClearRow { const char* name; int level; ClearResult res; };
    std::vector<ClearRow> clears;
    for (int w = 0; w < 7; ++w) {
        for (int rankIdx = 0; rankIdx < 2; ++rankIdx) {
            const int level = rankIdx == 0 ? 4 : 8;
            // У роли свой лимит: оружие «по стражам» чистит толпу медленнее АОЕ —
            // это его цена за урон по стражу, а не поломка. Требовать от него
            // скорости АОЕ значило бы требовать, чтобы роли не различались.
            const bool bossRole = WEAPON_ROLES[w] == WEAPON_ROLE_BOSS;
            const float roleLimit = bossRole ? limit * 1.5f : limit;
            // Крит — бросок rand, и на одной волне разница между зёрнами доходит до
            // пары мобов. Три зерна и медиана убирают «повезло/не повезло».
            static const unsigned seeds[3] = {20260201u, 20260202u, 20260203u};
            ClearResult best;
            int clearedRuns = 0;
            double sumSeconds = 0.0;
            int sumKills = 0;
            for (unsigned sd : seeds) {
                srand((int)sd);
                ClearResult run = measureClear(g, w, level, false, -1, roleLimit);
                if (run.cleared) ++clearedRuns;
                if (run.cleared && (best.seconds <= 0.0 || run.seconds < best.seconds)) best = run;
                sumSeconds += run.seconds;
                sumKills += run.kills;
                if (sd == seeds[0]) best.left = run.left;
            }
            ClearResult res = best;
            res.cleared = clearedRuns >= 2;
            res.seconds = sumSeconds / 3.0;
            res.kills = sumKills / 3;
            printf("%-30s %-12s %5d %9.1f %9.2f %9.1f %8d\n", weaponName[w], weaponRoleLabel(w), level,
                   res.seconds, res.kills > 0 ? res.seconds / res.kills : 0.0,
                   res.avgAlive, res.left);
            if (!res.cleared) {
                printf("    волна не зачищена за %.0f с (роль «%s»): зачищено %d прогонов из 3, осталось %d мобов\n",
                       roleLimit, weaponRoleLabel(w), clearedRuns, res.left);
                ++failures;
            }
            if (csv) fprintf(csv, "%s,%d,%d,%.1f,%d,%.1f,%d\n", weaponName[w], level,
                             res.cleared ? 1 : 0, res.seconds, res.kills, res.avgAlive, res.maxAlive);
            clears.push_back({weaponName[w], level, res});
        }
    }

    // ── Роли, дубли и доминирование ─────────────────────────────────────────
    if (weaponCsv) {
        std::vector<WeaponRow> rows = readWeaponCsv(weaponCsv);
        std::vector<WeaponRow> maxed;
        for (const WeaponRow& row : rows) if (row.level == 8 && row.name.rfind("tier", 0) != 0) maxed.push_back(row);
        if (maxed.size() >= 2) {
            printf("\nпрофиль оружия 8-го ранга (доля урона по сценариям) и роль:\n");
            for (const WeaponRow& row : maxed) {
                const double sum = row.far220 + row.close90 + row.moving + row.crowd;
                printf("  %-22s охват x%.1f  %s\n", row.name.c_str(), coverage(row), archetype(row));
                (void)sum;
            }

            printf("\nперекрытие профилей (1.00 — один и тот же вариант):\n");
            for (size_t i = 0; i < maxed.size(); ++i)
                for (size_t j = i + 1; j < maxed.size(); ++j) {
                    const double c = overlap(maxed[i], maxed[j]);
                    if (c >= 0.70)
                        printf("  %-22s ≈ %-22s %.2f\n", maxed[i].name.c_str(), maxed[j].name.c_str(), c);
                }

            // Кто лучший в каждом сценарии: если во всех четырёх побеждает одно
            // и то же оружие, «выбора из семи» на деле нет.
            printf("\nлучший в каждом сценарии:\n");
            static const char* scen[5] = {"220 px", "вплотную", "в упор", "идущая", "толпа"};
            int wins[7] = {0, 0, 0, 0, 0, 0, 0};
            for (int s = 0; s < 5; ++s) {
                int best = -1;
                double bestVal = 0.0;
                for (size_t i = 0; i < maxed.size(); ++i) {
                    const double v = s == 0 ? maxed[i].far220 : (s == 1 ? maxed[i].close90
                                    : (s == 2 ? maxed[i].near20 : (s == 3 ? maxed[i].moving : maxed[i].crowd)));
                    if (v > bestVal) { bestVal = v; best = (int)i; }
                }
                if (best >= 0) {
                    printf("  %-10s %-22s %.0f\n", scen[s], maxed[best].name.c_str(), bestVal);
                    (void)wins;
                }
            }

            printf("\nстрогое доминирование (A лучше B во всех четырёх сценариях):\n");
            int dominated = 0;
            for (const WeaponRow& b : maxed) {
                const WeaponRow* best = nullptr;
                for (const WeaponRow& a : maxed) {
                    if (&a == &b) continue;
                    if (a.far220 >= b.far220 && a.close90 >= b.close90 && a.near20 >= b.near20 &&
                        a.moving >= b.moving && a.crowd >= b.crowd &&
                        (a.far220 > b.far220 || a.close90 > b.close90 || a.near20 > b.near20 ||
                         a.moving > b.moving || a.crowd > b.crowd)) {
                        if (!best || coverage(a) < coverage(*best)) best = &a;
                    }
                }
                if (best) {
                    printf("  %-22s перекрыто: %s\n", b.name.c_str(), best->name.c_str());
                    ++dominated;
                }
            }
            if (dominated == 0) printf("  нет: у каждого оружия есть сценарий, где оно лучшeе\n");
        }
    }

    // ── Пул выбора во времени ───────────────────────────────────────────────
    printf("\nсколько предметов может выпасть (без учёта уже взятых):\n");
    static const float marks[5] = {30.0f, 180.0f, 300.0f, 600.0f, 900.0f};
    for (float mark : marks) {
        g.runTime = mark;
        int unlocked = 0, weapons = 0, artifacts = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            if (g.getItemMaxLevel(i) <= 0 || !g.isItemUnlocked(i)) continue;
            ++unlocked;
            if (i < 7) ++weapons; else ++artifacts;
        }
        printf("  %4.0f с: %2d из %d (оружие %d, артефакты %d)\n",
               mark, unlocked, ITEM_COUNT, weapons, artifacts);
    }

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе замеры выполнены\n");
    return 0;
}
