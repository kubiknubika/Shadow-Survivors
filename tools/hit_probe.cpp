// ── Стенд попадания: чем легче попасть, а чем сложнее ────────────────────────
//
// Остальные стенды отвечают «сколько урона». Этот — «а попадёт ли вообще».
// Оружие, которое в стенде урона стоит высоко, может промахиваться по бегущей
// цели, по мелкому врагу или по тому, кто стоит вплотную, — и в бою это видно
// как «оружие не работает», хотя цифры в аудите приличные.
//
// Что мерится на каждое оружие в восьми ситуациях:
//   • активации — сколько раз оружие сработало;
//   • попало — в скольких активациях за 2.5 с после срабатывания был хоть один
//     урон (это и есть «попал или промахнулся»);
//   • задержка — сколько в среднем проходит от срабатывания до первого урона
//     (снаряд летит, кольцо расходится, аура ждёт тика);
//   • выстрелов / попаданий — для снарядного оружия: сколько снарядов ушло и
//     сколько из них нашло цель (у клинков залп из пяти, у топора — три).
//
// Ситуации:
//   стоит 220     — базовая: цель стоит на средней дистанции;
//   навстречу 110 — цель идёт на героя по прямой (самый частый случай в игре)
//                   и, дойдя до 40 px, начинает снова с 400;
//   поперёк 110   — цель идёт поперёк луча по кругу 220 px;
//   быстрый 215   — цель бежит 215 px/с (крыса), радиус обхода 300 px;
//   мелкий r=8    — цель с маленьким хитбоксом в 300 px;
//   за спиной     — цель ровно за героем, оружие должно развернуться;
//   далеко 500    — цель на пределе дальности;
//   вплотную 40   — цель в упор: проверка мёртвой середины и перелёта.
//
// Сначала стенд сам ищет дальность попадания каждого оружия (60…560 px, где
// попадает половина срабатываний), и «в своей дальности» считает те же сцены, но
// только те, до которых оружие физически достаёт: вихрь на 200 px и кольцо на
// 560 px нельзя мерить одинаково.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -pthread -Isrc tools/hit_probe.cpp -o build/hit_probe
//     build/hit_probe            # 20 с модельного времени на строку
//     build/hit_probe 30         # дольше
//     build/hit_probe 20 out.csv # плюс CSV
//
// Код возврата 1, если оружие не попадает по СТОЯЩЕЙ цели на 80 % своей
// дальности чаще, чем в 60 % срабатываний (это уже не «сложно», а сломано),
// если меньше 20 % его снарядов находят стоячую цель в 220 px, или если оружие
// не попадает даже в 60 px.
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

struct Scenario {
    const char* name;
    float distance;      // куда поставить цель
    float orbitSpeed;    // 0 — стоит
    float orbitRadius;   // радиус обхода
    float targetRadius;  // хитбокс
    bool behind;         // цель за спиной героя
    float approachSpeed; // идёт на героя по прямой (самый частый случай в игре)
};

struct HitResult {
    int casts = 0;
    int landed = 0;          // активаций, после которых был урон
    int shots = 0;           // снарядов выпущено (только для снарядного оружия)
    int hits = 0;            // попаданий (событий урона) по мишени
    double delaySum = 0.0;   // сумма задержек до первого урона
    int delayCount = 0;
    double seconds = 0.0;
    double damage = 0.0;
    double landedRate() const { return casts > 0 ? (double)landed / casts : 0.0; }
    double shotAccuracy() const { return shots > 0 ? std::min(1.0, (double)hits / shots) : 0.0; }
    double meanDelay() const { return delayCount > 0 ? delaySum / delayCount : 0.0; }
    double dps() const { return seconds > 0.0 ? damage / seconds : 0.0; }
};

// Чистая арена: ни волн, ни отрядов, ни стражей по таймеру, ни резонаторов,
// ни порталов, ни предметов на полу.
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
    g.playerFacing = 1;
}

static HitResult measure(Game& g, int weaponId, int level, const Scenario& sc, float seconds) {
    HitResult res;
    const float dt = 1.0f / 60.0f;

    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
    resetArena(g);
    if (weaponId >= 0 && weaponId < 7) {
        g.itemLevels[weaponId] = level;
        g.weaponTimers[weaponId] = 0.0f;
    } else if (weaponId >= 24 && weaponId <= 27) {
        g.itemLevels[weaponId] = level;
        g.tierWeaponTimers[weaponId - 24] = 0.0f;
    }
    g.playerFacing = sc.behind ? -1 : 1;

    g.spawnMonster(0, false, false);
    if (g.monsters.empty()) return res;
    const int dummyId = g.monsters.back().uniqueId;
    Monster& m = g.monsters.back();
    m.x = sc.behind ? -sc.distance : sc.distance;
    m.y = 0.0f;
    m.vx = m.vy = 0.0f;
    m.hp = m.maxHp = INF_HP;
    m.radius = sc.targetRadius;
    m.isElite = false;

    const int frames = (int)(seconds / dt);
    float pendingSince = -1.0f;      // время срабатывания, после которого ждём урон
    bool pendingLanded = false;

    for (int f = 0; f < frames; ++f) {
        if (sc.approachSpeed > 0.0f) {
            // Идёт на героя по прямой и, дойдя до 40 px, начинает снова с 400:
            // так моб и ведёт себя в игре — подходит, а не стоит.
            const float span = 400.0f - 40.0f;
            const float travelled = fmodf((float)f * dt * sc.approachSpeed, span);
            m.x = 400.0f - travelled;
            m.y = 0.0f;
        } else if (sc.orbitSpeed > 0.0f) {
            const float ang = sc.orbitSpeed / std::max(1.0f, sc.orbitRadius) * (float)f * dt;
            m.x = cosf(ang) * sc.orbitRadius;
            m.y = sinf(ang) * sc.orbitRadius;
        } else {
            m.x = sc.behind ? -sc.distance : sc.distance;
            m.y = 0.0f;
        }
        m.vx = m.vy = 0.0f;
        // Направление шага задаём сами: игра берёт его из lastMoveX/Y, и упреждение
        // огненного шара считается по нему. Без этого шар целился в точку, где цель
        // была на момент выстрела, и по бегущей цели промахивался всегда — стенд
        // показывал бы 0 %, которых в игре нет.
        if (sc.approachSpeed > 0.0f) {
            m.lastMoveX = -1.0f;
            m.lastMoveY = 0.0f;
        } else if (sc.orbitSpeed > 0.0f) {
            const float len = std::max(1.0f, sqrtf(m.x * m.x + m.y * m.y));
            m.lastMoveX = -m.y / len;
            m.lastMoveY = m.x / len;
        } else {
            m.lastMoveX = m.lastMoveY = 0.0f;
        }
        m.hp = INF_HP;
        m.maxHp = INF_HP;

        const float timerBefore = (weaponId >= 0 && weaponId < 7) ? g.weaponTimers[weaponId] : -1.0f;
        const float tierBefore = (weaponId >= 24 && weaponId <= 27) ? g.tierWeaponTimers[weaponId - 24] : -1.0f;
        int playerShotsBefore = 0;
        for (const Projectile& p : g.projectiles)
            if (!p.isEnemy && p.weaponId == weaponId) ++playerShotsBefore;

        g.state = STATE_PLAYING;
        g.update(dt);
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;

        int playerShotsAfter = 0;
        for (const Projectile& p : g.projectiles)
            if (!p.isEnemy && p.weaponId == weaponId) ++playerShotsAfter;
        if (playerShotsAfter > playerShotsBefore) res.shots += playerShotsAfter - playerShotsBefore;

        bool cast = false;
        if (weaponId >= 0 && weaponId < 7 && g.weaponTimers[weaponId] > timerBefore + 1.0e-4f) cast = true;
        if (weaponId >= 24 && weaponId <= 27 && g.tierWeaponTimers[weaponId - 24] > tierBefore + 1.0e-4f) cast = true;
        if (cast) {
            ++res.casts;
            pendingSince = (float)f * dt;
            pendingLanded = false;
        }

        const double damage = (double)INF_HP - (double)m.hp;
        if (damage > 0.0) {
            res.damage += damage;
            ++res.hits;
            if (pendingSince >= 0.0f && !pendingLanded) {
                res.delaySum += (float)f * dt - pendingSince;
                ++res.delayCount;
                ++res.landed;
                pendingLanded = true;
            }
        }
        // Не попало за 2.5 с — активация считается промахом.
        if (pendingSince >= 0.0f && !pendingLanded && (float)f * dt - pendingSince > 2.5f) pendingSince = -1.0f;
        res.seconds += dt;
    }
    return res;
}

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 20.0f;
    const char* csvPath = nullptr;
    if (argc > 2) csvPath = strcmp(argv[2], "--csv") == 0 ? (argc > 3 ? argv[3] : nullptr) : argv[2];
    FILE* csv = csvPath ? fopen(csvPath, "w") : nullptr;
    if (csv) fprintf(csv, "weapon,scenario,casts,landed,landedRate,shots,hits,shotAccuracy,delay,seconds\n");

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
    static const Scenario scenarios[] = {
        {"стоит 220",   220.0f,   0.0f,   0.0f, 18.0f, false,   0.0f},
        {"навстречу 110", 400.0f, 0.0f,   0.0f, 18.0f, false, 110.0f},
        {"поперёк 110", 220.0f, 110.0f, 220.0f, 18.0f, false, 0.0f},
        {"быстрый 215", 300.0f, 215.0f, 300.0f, 18.0f, false, 0.0f},
        {"мелкий r=8",  300.0f,   0.0f,   0.0f,  8.0f, false, 0.0f},
        {"за спиной",   170.0f,   0.0f,   0.0f, 18.0f, true, 0.0f},
        {"далеко 500",  500.0f,   0.0f,   0.0f, 18.0f, false, 0.0f},
        {"вплотную 40",  40.0f,   0.0f,   0.0f, 18.0f, false, 0.0f}
    };
    const int SC_COUNT = (int)(sizeof(scenarios) / sizeof(scenarios[0]));

    int failures = 0;
    printf("Стенд попадания: цель одна, здоровье бесконечное, артефактов нет.\n");
    printf("Замер %.0f с на строку, зерно rand фиксировано. «попало» — доля срабатываний,\n", seconds);
    printf("после которых в течение 2.5 с был хоть один урон по цели.\n\n");

    const int RANK = 8;

    // ── Дальность попадания ──────────────────────────────────────────────────
    // Мерить «попадает ли по цели в 220 px» для кольца вокруг героя бессмысленно:
    // оно и не должно. Сначала находим, докуда оружие реально достаёт.
    double reach[7] = {0.0};
    printf("дальность попадания (стоящая цель, попало в половине срабатываний):\n");
    for (int w = 0; w < 7; ++w) {
        for (float d = 60.0f; d <= 560.0f; d += 20.0f) {
            Scenario sc{"", d, 0.0f, 0.0f, 18.0f, false};
            srand(20260201);
            const HitResult res = measure(g, w, RANK, sc, 6.0f);
            if (res.landedRate() >= 0.5) reach[w] = d;
        }
        printf("  %-24s %5.0f px\n", wName[w], reach[w]);
        if (csv) fprintf(csv, "%s,reach,0,0,%.0f,0,0,0,%.1f,0.0\n", wName[w], reach[w], reach[w]);
        if (reach[w] < 40.0f) {
            printf("    не попадает даже в 60 px — сломано\n");
            ++failures;
        }
    }

    double perWeapon[7] = {0.0};
    double perWeaponInRange[7] = {0.0};
    int perWeaponInRangeCount[7] = {0};
    double delaySum[7] = {0.0};
    int delayCount[7] = {0};
    double perScenario[SC_COUNT] = {0.0};
    int perScenarioCount = 0;

    printf("%-24s", "оружие");
    for (int s = 0; s < SC_COUNT; ++s) printf("%13s", scenarios[s].name);
    printf("   итог  в своей\n");
    printf("%-24s", "");
    for (int s = 0; s < SC_COUNT; ++s) printf("%13s", "попало/задерж");
    printf("\n");

    for (int w = 0; w < 7; ++w) {
        printf("%-24s", wName[w]);
        double sum = 0.0;
        for (int s = 0; s < SC_COUNT; ++s) {
            srand(20260201);
            const HitResult res = measure(g, w, RANK, scenarios[s], seconds);
            printf("%6.0f%%/%5.2fс", 100.0 * res.landedRate(), res.meanDelay());
            sum += res.landedRate();
            if (res.delayCount > 0) { delaySum[w] += res.meanDelay(); ++delayCount[w]; }
            // В «своей» дальности: ближнему оружию сценарии на 300–500 px не база,
            // оно туда и не должно доставать.
            const float scDist = scenarios[s].orbitSpeed > 0.0f ? scenarios[s].orbitRadius : scenarios[s].distance;
            if (scDist <= (float)reach[w] + scenarios[s].targetRadius) {
                perWeaponInRange[w] += res.landedRate();
                ++perWeaponInRangeCount[w];
            }
            perScenario[s] += res.landedRate();
            if (csv) fprintf(csv, "%s,%s,%d,%d,%.3f,%d,%d,%.3f,%.2f,%.1f\n", wName[w], scenarios[s].name,
                             res.casts, res.landed, res.landedRate(), res.shots, res.hits,
                             res.shotAccuracy(), res.meanDelay(), res.seconds);
            // Базовая проверка — на 80 % своей дальности, а не на 220 px: ближнему
            // оружию 220 px не база, оно и не должно туда доставать.
            const float baseDist = std::max(60.0f, std::min(220.0f, (float)reach[w] * 0.8f));
            if (s == 0 && std::fabs(scenarios[s].distance - baseDist) < 1.0f && res.landedRate() < 0.6) {
                printf("\n    не попадает по стоящей цели в %.0f px (своя дальность %.0f): %.0f%% срабатываний\n",
                       scenarios[s].distance, reach[w], 100.0 * res.landedRate());
                ++failures;
            }
            if (s == 0 && res.shots > 0 && res.shotAccuracy() < 0.2) {
                printf("\n    снаряды почти всегда впустую (%.0f%% попаданий) — это уже не «сложно», а мимо\n",
                       100.0 * res.shotAccuracy());
                ++failures;
            }
        }
        perWeapon[w] = sum / SC_COUNT;
        if (perWeaponInRangeCount[w] > 0) perWeaponInRange[w] /= perWeaponInRangeCount[w];
        printf("  %5.0f%%  %5.0f%%\n", 100.0 * perWeapon[w], 100.0 * perWeaponInRange[w]);
    }
    perScenarioCount = 7;
    printf("\nпо ситуациям (среднее по семи оружиям):\n");
    for (int s = 0; s < SC_COUNT; ++s) {
        perScenario[s] /= (double)perScenarioCount;
        printf("  %-16s %5.0f%%\n", scenarios[s].name, 100.0 * perScenario[s]);
    }

    // ── Снарядное оружие: сколько выстрелов уходит впустую ────────────────────
    printf("\nснаряды: выпущено / попаданий, стоящая цель в 220 px\n");
    for (int w : {0, 1, 5, 24, 26}) {
        srand(20260201);
        const HitResult res = measure(g, w, RANK, scenarios[0], seconds);
        if (res.shots <= 0) continue;
        printf("  %-24s %5d / %5d  = %3.0f%% попаданий, задержка %.2f с\n",
               w < 7 ? wName[w] : (w == 24 ? "Осколок Кометы" : "Копьё Разлома"),
               res.shots, res.hits, 100.0 * res.shotAccuracy(), res.meanDelay());
    }

    // ── Лучшие и худшие ──────────────────────────────────────────────────────
    printf("\nв своей дальности (без сценариев, до которых оружие не достаёт):\n");
    for (int w = 0; w < 7; ++w) {
        const double delay = delayCount[w] > 0 ? delaySum[w] / delayCount[w] : 0.0;
        printf("  %-24s %5.0f%%  задержка %.2f с%s\n", wName[w], 100.0 * perWeaponInRange[w], delay,
               reach[w] < 250.0f ? "  • ближний бой" : "");
    }
    std::vector<std::pair<double, const char*>> ranked;
    for (int w = 0; w < 7; ++w) ranked.push_back({perWeaponInRange[w], wName[w]});
    std::sort(ranked.begin(), ranked.end(), [](const std::pair<double, const char*>& a,
                                               const std::pair<double, const char*>& b) {
        return a.first > b.first;
    });
    printf("\nлегче всего попасть:  ");
    for (int i = 0; i < 3; ++i) printf("%s %.0f%%%s", ranked[i].second, 100.0 * ranked[i].first, i < 2 ? ", " : "");
    printf("\nсложнее всего:       ");
    for (int i = 0; i < 3; ++i) printf("%s %.0f%%%s", ranked[ranked.size() - 1 - i].second,
                                       100.0 * ranked[ranked.size() - 1 - i].first, i < 2 ? ", " : "");
    printf("\n");

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе замеры выполнены\n");
    return 0;
}
