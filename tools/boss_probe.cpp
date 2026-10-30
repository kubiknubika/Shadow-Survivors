// ── Стенд стражей: поведение ИИ боссов без окна ─────────────────────────────
//
// Шесть стражей ведут себя по-разному, но проверить это в игре дорого: чтобы
// увидеть фазу III, надо снять 67 % здоровья, а чтобы увидеть приём — дождаться
// таймера. Здесь каждый страж ставится один на арену с бессмертным героем, и
// прогон меряет то, что глазами не измерить:
//
//   • долю времени в контакте с героем (страж, стоящий в герое, — это не бой,
//     а непрерывный урон касанием);
//   • среднюю дистанцию;
//   • выстрелы в минуту и из них сделанные вплотную (< 90 px) — залп без
//     дистанции не читается;
//   • приёмы в минуту (замах способности) и сколько из них начато вплотную;
//   • кадры, в которые страж стрелял ОДНОВРЕМЕННО с замахом способности:
//     телеграф приёма в этот момент не читается, потому что экран занят
//     снарядами;
//   • кадры вне видимости (страж ушёл за край), кадры без движения (застрял);
//   • попадания по герою за минуту и попадания вплотную.
//
// Фаза задаётся принудительно (здоровье пересчитывается каждый кадр), поэтому
// I, II и III меряются отдельно.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/boss_probe.cpp -o build/boss_probe
//     build/boss_probe            # 60 с на стража, все три фазы
//     build/boss_probe 120        # дольше
//     build/boss_probe 60 2       # только Кардинал (0..5)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

struct BossStats {
    double seconds = 0.0;
    double contactSeconds = 0.0;
    double distanceSum = 0.0;
    long long distanceSamples = 0;
    int volleys = 0, closeVolleys = 0;          // залпы по числу новых снарядов
    int abilities = 0, closeAbilities = 0;
    int framesDuringWindup = 0, framesShootingDuringWindup = 0;
    int framesInvisible = 0, framesStuck = 0, framesTotal = 0;
    int playerHits = 0, playerHitsClose = 0;    // попадания по герою (по падению HP)
    int summons = 0;
};

// Наблюдение за одним стражем: тип, фаза, длительность.
static BossStats runBoss(Game& g, int bossType, int phase, float seconds) {
    BossStats st;
    const float dt = 1.0f / 60.0f;
    g.monsters.clear();
    g.projectiles.clear();
    g.bossHazards.clear();
    g.playerX = SCREEN_W * 0.5f;
    g.playerY = SCREEN_H * 0.5f;
    g.playerHp = g.playerMaxHp;
    g.spawnMonster(bossType, false, true, bossType);
    if (g.monsters.empty()) return st;
    Monster* boss = &g.monsters.back();
    // Фаза: выставляем здоровье под нужную треть и держим его там, иначе страж
    // перейдёт в следующую фазу по ходу замера.
    const float phaseRatio = phase == 1 ? 0.90f : (phase == 2 ? 0.55f : 0.25f);
    const float keepHp = boss->maxHp * phaseRatio;
    float prevShootTimer = boss->shootTimer;
    bool prevWindup = false;
    float prevHp = g.playerHp;
    int prevMonsters = (int)g.monsters.size();

    for (float t = 0.0f; t < seconds; t += dt) {
        // Держим фазу и здоровье.
        if (!g.monsters.empty()) {
            Monster& b = g.monsters.front();
            b.hp = keepHp;
            b.bossPhase = phase;
            b.phaseInvulnTimer = 0.0f;
        }
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
        g.update(dt);
        if (g.monsters.empty()) break;
        Monster& b = g.monsters.front();

        const float dx = b.x - g.playerX, dy = b.y - g.playerY;
        const float dist = std::sqrt(dx * dx + dy * dy);
        ++st.framesTotal;
        st.seconds += dt;
        st.distanceSum += dist;
        ++st.distanceSamples;
        if (dist <= b.radius + 16.0f) st.contactSeconds += dt;

        // Залп — это скачок таймера перезарядки вверх. Придержка огня поднимает
        // его максимум до 0.15, поэтому порог 0.3 отличает одно от другого.
        if (b.shootTimer > prevShootTimer + 0.3f) {
            ++st.volleys;
            if (dist < 90.0f) ++st.closeVolleys;
        }
        prevShootTimer = b.shootTimer;

        // Новый замах способности.
        const bool windup = b.abilityWindup > 0.0f;
        if (windup && !prevWindup) {
            ++st.abilities;
            if (dist < 120.0f) ++st.closeAbilities;
        }
        if (windup) {
            ++st.framesDuringWindup;
            if (b.shootTimer < 0.02f) ++st.framesShootingDuringWindup;
        }
        prevWindup = windup;

        if (!g.isWorldPointVisible(b.x, b.y, 10.0f)) ++st.framesInvisible;
        if (std::fabs(b.lastMoveX) + std::fabs(b.lastMoveY) < 0.05f) ++st.framesStuck;

        if (g.playerHp < prevHp - 0.01f) {
            ++st.playerHits;
            if (dist < 90.0f) ++st.playerHitsClose;
        }
        prevHp = g.playerHp;
        g.playerHp = g.playerMaxHp;          // герой в стенде бессмертен постфактум
        if (g.playerHp <= 0.0f) g.playerHp = g.playerMaxHp;

        // Призывы: считаем по прибавке мобов, но сразу убираем их с арены,
        // чтобы мерился страж, а не его свита.
        const int now = (int)g.monsters.size();
        if (now > prevMonsters) st.summons += now - prevMonsters;
        prevMonsters = now;
        if (g.monsters.size() > 1) g.monsters.erase(g.monsters.begin() + 1, g.monsters.end());
    }
    return st;
}

// Время до убийства стража обычным билдом: стража не должно стать невозможно
// достать, если он держит дистанцию. Игрок с тремя оружием 3-го ранга,
// автопрокачка на повышениях уровня, 90 секунд максимум.
static double timeToKill(Game& g, int bossType, float limit, int* hitsTaken, bool chase) {
    const float dt = 1.0f / 60.0f;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.runTime = 120.0f;
    g.itemLevels[0] = 3; g.itemLevels[1] = 3; g.itemLevels[2] = 3;
    g.playerHp = g.playerMaxHp;
    g.playerX = SCREEN_W * 0.5f; g.playerY = SCREEN_H * 0.5f;
    g.monsters.clear(); g.projectiles.clear();
    g.spawnMonster(bossType, false, true, bossType);
    if (g.monsters.empty()) return -1.0;
    float prevHp = g.playerHp;
    int hits = 0;
    double elapsed = 0.0;
    for (float t = 0.0f; t < limit; t += dt) {
        while (g.state == STATE_LEVELUP) { g.applyUpgrade(g.upgradeChoices[0]); g.state = STATE_PLAYING; }
        // Без этого прогон замирает, как только герой получает уровень или умирает,
        // и «время до убийства» превращается в «до первого события».
        // «Идёт на стража»: герой сам сокращает дистанцию, как живой игрок.
        // Стоящего героя страж не обязан подпускать — это его преимущество.
        if (chase) {
            for (const Monster& m : g.monsters) {
                if (!m.isBoss || m.hp <= 0.0f) continue;
                const float ddx = m.x - g.playerX, ddy = m.y - g.playerY;
                const float len = std::sqrt(ddx * ddx + ddy * ddy);
                if (len > 20.0f) {
                    g.playerX += ddx / len * 185.0f * dt;
                    g.playerY += ddy / len * 185.0f * dt;
                }
                break;
            }
        }
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
        g.update(dt);
        elapsed += dt;
        if (g.playerHp < prevHp - 0.01f) ++hits;
        prevHp = g.playerHp;
        g.playerHp = g.playerMaxHp;      // измерительный прогон: смертность не мешает
        bool alive = false;
        for (const Monster& m : g.monsters) if (m.isBoss && m.hp > 0.0f) alive = true;
        if (!alive) break;
    }
    *hitsTaken = hits;
    return elapsed;
}

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 60.0f;
    const int onlyBoss = argc > 2 ? atoi(argv[2]) : -1;

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.runTime = 120.0f;

    static const char* const names[6] = {"МОРГАР", "МАЛАКОР", "КАРДИНАЛ", "АЗАРОТ", "ПРОТОКОЛИСТ", "ДВОЙНИК"};

    printf("стенд стражей: %.0f с на стража и фазу, герой стоит в центре\n\n", seconds);
    printf("%-12s %3s %8s %8s %8s %7s %7s %7s %8s %8s %8s %6s\n",
           "страж", "фз", "контакт%", "средн.д", "залпов/м", "вплотн", "приём/м", "вплотн",
           "замах+выстр", "попаданий/м", "вплотную", "невидим");

    for (int boss = 0; boss < 6; ++boss) {
        if (onlyBoss >= 0 && boss != onlyBoss) continue;
        for (int phase = 1; phase <= 3; ++phase) {
            // Прогон на чистой игре: состояние стража не переносится между фазами.
            g.init();
            g.state = STATE_PLAYING;
            g.gameMode = MODE_ENDLESS;
            g.runTime = 120.0f;
            BossStats st = runBoss(g, boss, phase, seconds);
            const double minutes = st.seconds > 0.0 ? st.seconds / 60.0 : 1.0;
            printf("%-12s %3d %7.1f%% %8.0f %8.1f %7d %7.1f %7d %7d%% %8.1f %8d %6d\n",
                   phase == 1 ? names[boss] : "", phase,
                   st.seconds > 0.0 ? 100.0 * st.contactSeconds / st.seconds : 0.0,
                   st.distanceSamples ? st.distanceSum / (double)st.distanceSamples : 0.0,
                   st.volleys / minutes,
                   st.closeVolleys,
                   st.abilities / minutes,
                   st.closeAbilities,
                   st.framesDuringWindup ? (int)(100.0 * st.framesShootingDuringWindup / st.framesDuringWindup) : 0,
                   st.playerHits / minutes,
                   st.playerHitsClose,
                   st.framesInvisible);
        }
        printf("\n");
    }
    printf("\nвремя до убийства обычным билдом (три оружия 3-го ранга):\n");
    printf("%-12s %12s %10s %12s %10s\n", "страж", "стоя, с", "попаданий", "идёт, с", "попаданий");
    for (int boss = 0; boss < 6; ++boss) {
        if (onlyBoss >= 0 && boss != onlyBoss) continue;
        int hitsStill = 0, hitsChase = 0;
        const double tStill = timeToKill(g, boss, 90.0f, &hitsStill, false);
        const double tChase = timeToKill(g, boss, 90.0f, &hitsChase, true);
        printf("%-12s %12.1f %10d %12.1f %10d\n", names[boss], tStill, hitsStill, tChase, hitsChase);
    }
    printf("\n«замах+выстр» — доля кадров замаха, в которые страж одновременно стреляет;\n");
    printf("«вплотн» — залпы/приёмы ближе 90/120 px; «попаданий/м» — падения HP героя за минуту,\n");
    printf("«вплотную» — из них полученные ближе 90 px. Герою каждый кадр возвращается полное HP;\n");
    printf("«невидим» — кадры, когда страж был за краем видимого поля.\n");
    return 0;
}
