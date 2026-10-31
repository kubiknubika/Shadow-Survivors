// ── Стенд синергии: два скилла вместе сильнее, чем по отдельности? ────────────
//
// Стенд оружия (tools/weapon_probe.cpp) мерит одно оружие за раз и на этом
// останавливается. Но игрок собирает не «оружие», а набор: два-три оружия и
// артефакты между ними. Когда такие наборы не проверяются, случается одно из
// двух: либо пара даёт меньше, чем лучшая половина (снаряды бьют одну и ту же
// цель, отбрасывание уносит врагов из ауры — «антисинергия»), либо одна пара
// перекрывает всё остальное, и выбор сборки исчезает.
//
// Что мерится здесь:
//
//   1. Одиночки. Семь оружий 8-го ранга по отдельности — база для сравнения:
//      толпа (24 мишени) и страж (одна мишень 30 px вплотную).
//   2. Пары. Все 21 пара оружий. Для каждой считается
//          отношение = ДПС пары / (ДПС первого + ДПС второго).
//      1.00 — «сложения нет»: два оружия бьют так же, как били бы по отдельности;
//      больше 1 — реальная синергия (одно готовит цель другому);
//      меньше 1 — потеря (снаряды дублируют друг друга, отбрасывание мешает).
//   3. Артефакты-связки. Те артефакты, которые по описанию требуют второго
//      скилла: Призма Перекрёстного Огня (разные атаки по одной цели),
//      Осколок Оттепели (нужна заморозка), Угольный Фитиль (нужен крит),
//      Печать Разоблачения (нужны частые удары), Кровавый Осколок Луны
//      (крит и суперкрит). Считается прирост к той же сборке без артефакта.
//   4. Резонанс. Эволюция: оружие 9-го ранга с реликвией 4-го ранга против того
//      же оружия 8-го ранга с той же реликвией — окупается ли путь «оружие +
//      реликвия» по сравнению с просто ещё одним ранговым апгрейдом.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -pthread -Isrc tools/synergy_probe.cpp -o build/synergy_probe
//     build/synergy_probe                 # 10 с модельного времени на замер
//     build/synergy_probe 16              # дольше и точнее
//     build/synergy_probe 10 pairs.csv    # плюс CSV для картинки
//
// Код возврата 1, если пара бьёт слабее своей лучшей половины больше чем на
// 10 %, если пара даёт больше тройной суммы частей (must-pick), если связанный
// артефакт не даёт обещанного прироста, если эволюция не окупается или если
// сборка не нанесла урона вовсе.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

static const float INF_HP = 1.0e6f;
static const int CROWD = 24;
static constexpr float BOSS_RADIUS = 30.0f;
// Порог «сложения» пары. Ниже 0.90 — это уже не выбор, а ловушка: пара слабее,
// чем её же лучшая половина, то есть второй слот сгорел.
static constexpr double PAIR_MIN_RATIO = 0.90;
static constexpr double PAIR_MAX_RATIO = 3.00;

struct Dummy { float x, y; };

struct Result {
    double damage = 0.0;
    double seconds = 0.0;
    int casts = 0;
    int crossfire = 0;
    int ember = 0;
    int shatter = 0;
    double dps() const { return seconds > 0.0 ? damage / seconds : 0.0; }
};

// Одна сборка: список активных оружий (id, ранг, эволюция) и артефактов (id, ранг).
struct Build {
    std::vector<int> weapons;
    std::vector<int> artifacts;   // id -> ранг лежит в artifactLevels
};

static std::vector<Dummy> layoutTargets(int count, float inner, float outer) {
    std::vector<Dummy> out;
    for (int i = 0; i < count; ++i) {
        const float t = count > 1 ? (float)i / (float)(count - 1) : 0.5f;
        const float r = inner + (outer - inner) * t;
        const float a = i * 2.39996323f;
        out.push_back({cosf(a) * r, sinf(a) * r});
    }
    return out;
}

// Чистая арена: ни волн, ни отрядов, ни стражей по таймеру, ни резонаторов,
// ни порталов, ни предметов на полу. Всё, что могло бы добавить урон не от
// сборки, выключено.
static void resetArena(Game& g) {
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.bossHazards.clear();
    g.particles.clear();
    g.lightningArcs.clear();
    g.frostWaves.clear();
    g.shockwaves.clear();
    g.balance.spawnMultiplier = 0.0f;
    g.squadTimer = 1.0e9f;
    g.ratTimer = 1.0e9f;
    g.resonatorSpawnTimer = 1.0e9f;
    g.portalSpawnTimer = 1.0e9f;
    g.nextBossIndex = 999;
    g.eventCooldown = 1.0e9f;
    g.huntActive = false;
    g.clearFloorDevices();
    g.floorDeviceSpawnTimer = 1.0e9f;
    g.pendingResonatorBurst = 0;
    g.pendingMatryoshkaSplits.clear();
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.playerX = 0.0f;
    g.playerY = 0.0f;
    g.playerHp = g.playerMaxHp;
    g.playerHurtTimer = 0.0f;
    g.runTime = 0.0f;
}

// Один замер сборки. weaponsAndRanks — пары (id оружия, ранг), artifacts — (id, ранг).
static Result measureBuild(Game& g, const std::vector<int>& weaponsAndRanks,
                           const std::vector<int>& artifacts, int targetCount,
                           float inner, float outer, float seconds, bool bossTarget,
                           int tierWeapon = -1, int tierRank = 8) {
    Result res;
    const float dt = 1.0f / 60.0f;

    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
    resetArena(g);

    for (size_t i = 0; i + 1 < weaponsAndRanks.size(); i += 2) {
        const int id = weaponsAndRanks[i], rank = weaponsAndRanks[i + 1];
        if (id < 0 || id >= 7) continue;
        g.itemLevels[id] = rank;
        g.weaponTimers[id] = 0.0f;          // первый удар сразу, а не через таймер
    }
    if (tierWeapon >= 24 && tierWeapon <= 27) {
        g.itemLevels[tierWeapon] = tierRank;
        g.tierWeaponTimers[tierWeapon - 24] = 0.0f;
    }
    for (size_t i = 0; i + 1 < artifacts.size(); i += 2) {
        const int id = artifacts[i], rank = artifacts[i + 1];
        if (id < 0 || id >= ITEM_COUNT) continue;
        g.itemLevels[id] = rank;
    }
    // Эволюция в игре включается сама на 9-м ранге при реликвии 4-го ранга
    // (update, проверка !evolvedWeapons[w]). Стенд ставит тот же ранг, поэтому
    // эволюция должна включиться там же — иначе он мерил бы не то, что игрок.
    g.evolvedWeapons[0] = g.itemLevels[0] >= 9 && g.itemLevels[7] >= 4;
    g.evolvedWeapons[1] = g.itemLevels[1] >= 9 && g.itemLevels[8] >= 4;
    g.evolvedWeapons[2] = g.itemLevels[2] >= 9 && g.itemLevels[9] >= 4;
    g.evolvedWeapons[3] = g.itemLevels[3] >= 9 && g.itemLevels[10] >= 4;
    g.evolvedWeapons[4] = g.itemLevels[4] >= 9 && g.itemLevels[11] >= 4;
    g.evolvedWeapons[5] = g.itemLevels[5] >= 9 && g.itemLevels[12] >= 4;
    g.evolvedWeapons[6] = g.itemLevels[6] >= 9 && g.itemLevels[13] >= 4;

    std::vector<Dummy> spots = layoutTargets(targetCount, inner, outer);
    std::vector<int> dummyIds;
    std::vector<int> dummySlot;   // мишень -> её место в раскладке
    for (size_t i = 0; i < spots.size(); ++i) {
        g.spawnMonster(0, false, false);
        if (g.monsters.empty()) break;
        Monster& m = g.monsters.back();
        m.x = spots[i].x; m.y = spots[i].y;
        m.vx = m.vy = 0.0f;
        m.hp = m.maxHp = INF_HP;
        m.radius = bossTarget ? BOSS_RADIUS : 18.0f;
        m.isElite = false;
        if (bossTarget) {
            m.isBoss = true;
            m.bossType = 0;
            m.bossPhase = 1;
            m.abilityTimer = 1.0e9f;
            m.specialTimer = 1.0e9f;
            m.bossSummonWindup = 0.0f;
            m.phaseInvulnTimer = 0.0f;
            m.shieldHits = 0;
            m.isShieldBearer = false;
        }
        dummyIds.push_back(m.uniqueId);
        dummySlot.push_back((int)i);
    }

    g.crossfireProcCount = 0;
    g.emberProcCount = 0;
    g.frostShatterProcCount = 0;
    const int frames = (int)(seconds / dt);
    for (int f = 0; f < frames; ++f) {
        for (Monster& m : g.monsters) {
            auto it = std::find(dummyIds.begin(), dummyIds.end(), m.uniqueId);
            if (it == dummyIds.end()) continue;
            // Мишени стоят там, куда их поставили. Без этого толпа сама сбегается
            // к герою, оружие по площади бьёт всех 24 сразу, и ДПС втрое-впятеро
            // выше, чем в стенде оружия: сравнивать такое с остальным аудитом
            // нельзя, а «пара сильнее суммы» получалась бы просто из кучности.
            const Dummy& d = spots[dummySlot[(size_t)(it - dummyIds.begin())]];
            m.x = d.x;
            m.y = d.y;
            m.vx = m.vy = 0.0f;
            m.lastMoveX = m.lastMoveY = 0.0f;
            m.hp = INF_HP;
            m.maxHp = INF_HP;
        }
        g.state = STATE_PLAYING;
        g.update(dt);
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;

        for (const Monster& m : g.monsters) {
            if (std::find(dummyIds.begin(), dummyIds.end(), m.uniqueId) == dummyIds.end()) continue;
            const double d = (double)INF_HP - (double)m.hp;
            if (d > 0.0) res.damage += d;
        }
        res.seconds += dt;
    }
    res.crossfire = g.crossfireProcCount;
    res.ember = g.emberProcCount;
    res.shatter = g.frostShatterProcCount;
    return res;
}

// Средний ДПС по трём зёрнам: крит это бросок rand, и на одном зерне разница
// между сборками в 5 % — это везение, а не синергия.
static double avgDps(Game& g, const std::vector<int>& weaponsAndRanks,
                     const std::vector<int>& artifacts, int targetCount,
                     float inner, float outer, float seconds, bool bossTarget,
                     double* procsPerSecond = nullptr, int tierWeapon = -1, int tierRank = 8) {
    static const unsigned seeds[3] = {20260201u, 20260202u, 20260203u};
    double sum = 0.0;
    int procs = 0;
    for (unsigned sd : seeds) {
        srand((int)sd);
        const Result run = measureBuild(g, weaponsAndRanks, artifacts, targetCount, inner, outer, seconds, bossTarget,
                                        tierWeapon, tierRank);
        sum += run.dps();
        procs += run.crossfire + run.ember + run.shatter;
    }
    if (procsPerSecond) *procsPerSecond = procs / (3.0 * std::max(0.001f, seconds));
    return sum / 3.0;
}

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 10.0f;
    // CSV: вторым аргументом можно дать путь или пару «--csv путь».
    const char* csvPath = nullptr;
    if (argc > 2) csvPath = strcmp(argv[2], "--csv") == 0 ? (argc > 3 ? argv[3] : nullptr) : argv[2];
    FILE* csv = csvPath ? fopen(csvPath, "w") : nullptr;
    if (csv) fprintf(csv, "kind,a,b,crowd,boss\n");

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.simulationMode = true;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;

    static const char* wName[7] = {
        "Серебряные клинки", "Огненный шар", "Рунический вихрь",
        "Цепная молния", "Кровавая аура", "Боевой топор", "Ледяное кольцо"
    };
    static const int resonanceRelic[7] = {7, 8, 9, 10, 11, 12, 13};

    int failures = 0;
    printf("Стенд синергии: сборки из двух скиллов, мишени бессмертны, артефактов нет.\n");
    printf("Замер %.0f с на строку, три зерна rand, крит только базовые 8%%.\n", seconds);
    printf("Толпа — %d целей 120–330 px; страж — одна цель 30 px вплотную.\n\n", CROWD);

    // ── 1. Одиночки ──────────────────────────────────────────────────────────
    const int WEAPON_RANK = 8;
    double soloCrowd[7], soloBoss[7];
    printf("одиночки, ранг %d (ДПС по толпе / по стражу):\n", WEAPON_RANK);
    for (int w = 0; w < 7; ++w) {
        const std::vector<int> build = {w, WEAPON_RANK};
        soloCrowd[w] = avgDps(g, build, {}, CROWD, 120.0f, 330.0f, seconds, false);
        soloBoss[w] = avgDps(g, build, {}, 1, 90.0f, 90.0f, seconds, true);
        printf("  %-22s %8.0f  %8.0f\n", wName[w], soloCrowd[w], soloBoss[w]);
        if (csv) fprintf(csv, "solo,%d,-,%.0f,%.0f\n", w, soloCrowd[w], soloBoss[w]);
        if (soloCrowd[w] + soloBoss[w] <= 0.0) {
            printf("    оружие не нанесло урона ни в одном сценарии\n");
            ++failures;
        }
    }

    // ── 2. Пары оружий ───────────────────────────────────────────────────────
    printf("\nпары оружий: ДПС пары и отношение к сумме одиночек (1.00 — сложения нет)\n");
    printf("%-42s %9s %7s %9s %7s\n", "пара", "толпа", "отн.", "страж", "отн.");
    double bestPairCrowd = 0.0, bestPairBoss = 0.0;
    double pairCrowd[7][7] = {}, pairBoss[7][7] = {};
    char bestPairCrowdName[64] = {0}, bestPairBossName[64] = {0};
    for (int a = 0; a < 7; ++a) {
        for (int b = a + 1; b < 7; ++b) {
            const std::vector<int> build = {a, WEAPON_RANK, b, WEAPON_RANK};
            const double crowd = avgDps(g, build, {}, CROWD, 120.0f, 330.0f, seconds, false);
            const double boss = avgDps(g, build, {}, 1, 90.0f, 90.0f, seconds, true);
            const double ratioCrowd = (soloCrowd[a] + soloCrowd[b]) > 1.0 ? crowd / (soloCrowd[a] + soloCrowd[b]) : 0.0;
            const double ratioBoss = (soloBoss[a] + soloBoss[b]) > 1.0 ? boss / (soloBoss[a] + soloBoss[b]) : 0.0;
            char pairName[64];
            snprintf(pairName, sizeof(pairName), "%s + %s", wName[a], wName[b]);
            printf("%-42s %9.0f %7.2f %9.0f %7.2f\n", pairName, crowd, ratioCrowd, boss, ratioBoss);
            if (csv) fprintf(csv, "pair,%d,%d,%.0f,%.0f\n", a, b, crowd, boss);
            pairCrowd[a][b] = crowd; pairBoss[a][b] = boss;
            if (crowd > bestPairCrowd) { bestPairCrowd = crowd; snprintf(bestPairCrowdName, sizeof(bestPairCrowdName), "%s", pairName); }
            if (boss > bestPairBoss) { bestPairBoss = boss; snprintf(bestPairBossName, sizeof(bestPairBossName), "%s", pairName); }

            // Пара не должна быть хуже своей лучшей половины: иначе второй слот
            // сгорел, и игрок заплатил за него уровнем.
            const double betterHalfCrowd = std::max(soloCrowd[a], soloCrowd[b]);
            const double betterHalfBoss = std::max(soloBoss[a], soloBoss[b]);
            if (crowd < betterHalfCrowd * PAIR_MIN_RATIO || boss < betterHalfBoss * PAIR_MIN_RATIO) {
                printf("    пара слабее своей лучшей половины: толпа %.0f против %.0f, страж %.0f против %.0f\n",
                       crowd, betterHalfCrowd, boss, betterHalfBoss);
                ++failures;
            }
            // И не должна перекрывать всё: иначе сборка перестаёт быть выбором.
            if (ratioCrowd > PAIR_MAX_RATIO || ratioBoss > PAIR_MAX_RATIO) {
                printf("    пара даёт больше тройной суммы частей (%.2f / %.2f) — сборка сводится к одной паре\n",
                       ratioCrowd, ratioBoss);
                ++failures;
            }
        }
    }
    printf("лучшая пара по толпе: %s (%.0f ДПС), по стражу: %s (%.0f ДПС)\n",
           bestPairCrowdName, bestPairCrowd, bestPairBossName, bestPairBoss);

    // ── 2а. Какая пара закрывает оба дела ───────────────────────────────────
    // Волна и страж — разные задачи, и сборка из двух слотов должна закрывать
    // обе. Считается худшая из двух долей от лучшей одиночки: у пары, сильной
    // только в одном сценарии, баланс низкий, даже если рекорд высокий.
    {
        double bestSoloCrowd = 0.0, bestSoloBoss = 0.0;
        for (int w = 0; w < 7; ++w) {
            bestSoloCrowd = std::max(bestSoloCrowd, soloCrowd[w]);
            bestSoloBoss = std::max(bestSoloBoss, soloBoss[w]);
        }
        struct Ranked { double balance, crowd, boss; int a, b; };
        std::vector<Ranked> ranked;
        for (int a = 0; a < 7; ++a)
            for (int b = a + 1; b < 7; ++b) {
                const double crowdShare = bestSoloCrowd > 0.0 ? pairCrowd[a][b] / bestSoloCrowd : 0.0;
                const double bossShare = bestSoloBoss > 0.0 ? pairBoss[a][b] / bestSoloBoss : 0.0;
                ranked.push_back({std::min(crowdShare, bossShare), pairCrowd[a][b], pairBoss[a][b], a, b});
            }
        std::sort(ranked.begin(), ranked.end(), [](const Ranked& x, const Ranked& y) {
            return x.balance > y.balance;
        });
        printf("\nпары, закрывающие и волну, и стража (доля от лучшей одиночки в худшем из двух):\n");
        for (size_t i = 0; i < ranked.size() && i < 5; ++i) {
            const Ranked& row = ranked[i];
            printf("  %-42s баланс %.2f  (толпа %.0f, страж %.0f)\n",
                   (std::string(wName[row.a]) + " + " + wName[row.b]).c_str(),
                   row.balance, row.crowd, row.boss);
        }
    }

    // ── 2б. Оружие тиров вместе с основным ──────────────────────────────────
    // Оружие 24-27 — часть сборки, а не отдельная ветка: стенд оружия мерит его
    // одно, и вопрос «а вместе с основным?» оставался без ответа.
    printf("\nоружие тиров вместе с основным (толпа, 8-й уровень):\n");
    // Оружие тира 24–27 задаётся своим полем (itemLevels[24..27], tierWeaponTimers),
    // поэтому для него отдельный вызов, а не общий список оружий.
    static const char* tierName[4] = {"Осколок Кометы", "Рунный Разворот", "Копьё Разлома", "Звезда Пепла"};
    for (int t = 0; t < 4; ++t) {
        const int tierId = 24 + t;
        const double tierAlone = avgDps(g, {}, {}, CROWD, 120.0f, 330.0f, seconds, false, nullptr, tierId, 8);
        const double pairDps = avgDps(g, {0, WEAPON_RANK}, {}, CROWD, 120.0f, 330.0f, seconds, false, nullptr, tierId, 8);
        const double ratio = (soloCrowd[0] + tierAlone) > 1.0 ? pairDps / (soloCrowd[0] + tierAlone) : 0.0;
        printf("  %-20s одиночка %7.0f, с клинками %7.0f (отн. %.2f)\n",
               tierName[t], tierAlone, pairDps, ratio);
        if (csv) fprintf(csv, "tier,%d,-,%.0f,%.0f\n", tierId, tierAlone, pairDps);
        if (tierAlone <= 0.0) {
            printf("    оружие тира не нанесло урона\n");
            ++failures;
        }
        if (ratio > 0.0 && ratio < PAIR_MIN_RATIO) {
            printf("    оружие тира вместе с основным хуже суммы частей: %.2f\n", ratio);
            ++failures;
        }
    }

    // ── 3. Артефакты-связки ──────────────────────────────────────────────────
    // Каждая строка — артефакт, который по описанию требует второго скилла, и
    // сборка, в которой он обязан сработать. Считается прирост к той же сборке
    // без артефакта; ниже 8 % — обещание не выполняется.
    printf("\nартефакты-связки: прирост к той же сборке без артефакта (толпа)\n");
    struct Link {
        const char* artifact;
        int artifactId;
        int artifactRank;
        const char* why;
        double minGain;               // сколько процентов связка обязана принести
        std::vector<int> weapons;     // id, ранг, ...
    };
    // Порог — это обещание артефакта. Призма собрана ради урона и требует трёх
    // оружий в сборке, поэтому 10 %; ожог и откол — добавка к контролю и криту,
    // им хватает 3 %. Остальные — обычные усилители, с них 8 %.
    const std::vector<Link> links = {
        {"Призма Перекрёстного Огня", 33, 3, "три разных оружия по одной цели", 10.0, {0, 8, 3, 8, 1, 8}},
        {"Осколок Оттепели",           35, 3, "нужна заморозка",                 3.0, {6, 8, 5, 8}},
        {"Угольный Фитиль",            34, 3, "нужен крит",                      3.0, {0, 8, 17, 3}},
        {"Печать Разоблачения",        29, 3, "нужны частые удары",              8.0, {0, 8}},
        {"Кровавый Осколок Луны",      17, 3, "крит и суперкрит",                8.0, {0, 8}},
        {"Око Бездны",                 15, 5, "крит и заряд супера",             8.0, {0, 8}},
        {"Печать Пепельного Сердца",   16, 3, "урон и заряд супера",             8.0, {0, 8}},
        {"Линза Разлома",              18, 3, "радиус и частота",                8.0, {6, 8}},
        {"Часы вечности",              12, 6, "частота и радиус",                8.0, {0, 8}}
    };
    for (const Link& link : links) {
        const double base = avgDps(g, link.weapons, {}, CROWD, 120.0f, 330.0f, seconds, false);
        double procs = 0.0;
        const double with = avgDps(g, link.weapons, {link.artifactId, link.artifactRank}, CROWD, 120.0f, 330.0f, seconds, false, &procs);
        const double delta = base > 1.0 ? 100.0 * (with / base - 1.0) : 0.0;
        printf("  %-28s %7.0f -> %7.0f  %+6.1f%% (нужно %+.0f%%, сработок %.1f/с)  %s\n",
               link.artifact, base, with, delta, link.minGain, procs, link.why);
        if (csv) fprintf(csv, "link,%d,-,%.0f,%.0f\n", link.artifactId, base, with);
        if (delta < link.minGain) {
            printf("    связка не срабатывает: обещано «%s» и %+.0f%%, а прирост %+.1f%%\n",
                   link.why, link.minGain, delta);
            ++failures;
        }
    }

    // ── 4. Резонанс: окупается ли эволюция ───────────────────────────────────
    printf("\nрезонанс: оружие 9-го ранга с реликвией 4-го против того же 8-го с реликвией\n");
    for (int w = 0; w < 7; ++w) {
        const std::vector<int> arts = {resonanceRelic[w], 4};
        const double before = avgDps(g, {w, 8}, arts, CROWD, 120.0f, 330.0f, seconds, false);
        const double after = avgDps(g, {w, 9}, arts, CROWD, 120.0f, 330.0f, seconds, false);
        const double gain = before > 1.0 ? after / before : 0.0;
        printf("  %-22s %7.0f -> %7.0f  x%.2f\n", wName[w], before, after, gain);
        if (csv) fprintf(csv, "evo,%d,-,%.0f,%.0f\n", w, before, after);
        if (gain < 1.20) {
            printf("    эволюция не окупается: x%.2f за 9-й ранг и реликвию 4-го\n", gain);
            ++failures;
        }
    }

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе сборки проверены\n");
    return 0;
}
