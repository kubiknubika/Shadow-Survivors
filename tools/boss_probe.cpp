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
#include <map>
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

// ── Свита ───────────────────────────────────────────────────────────────────
// Что важно в свите: сколько её, докуда она живёт, не растёт ли без предела и
// не съедает ли весь урон игрока. Меряем: вызовы в минуту, призванных мобов,
// среднее и пиковое число живых, элиту в свите и разделение урона между стражем
// и свитой (по падению здоровья каждого моба за кадр).
struct RetinueStats {
    int summonEvents = 0;
    int addsSpawned = 0;
    int eliteAdds = 0;
    double liveSum = 0.0;
    int livePeak = 0;
    int capNow = 0;         // предел свиты в момент пика
    long long frames = 0;
    double damageToBoss = 0.0;
    double damageToAdds = 0.0;
    int aliveAfterBossDeath = 0;      // свита
    int othersAfterBossDeath = 0;     // всё прочее на арене (волны/события)
};

// pinHp > 0: здоровье стража прибито к этой доле, чтобы прогон не обрывался на
// его смерти и мерялся именно вызов свиты в данной фазе. Урон при этом всё равно
// считается по падению HP за кадр (до восстановления).
static RetinueStats runRetinue(Game& g, int bossType, float seconds, float pinHp = 0.0f) {
    RetinueStats st;
    const float dt = 1.0f / 60.0f;
    // Фиксированное зерно: прогоны «до» и «после» должны отличаться кодом, а не
    // удачей rand(). Без него разброс по урону был двукратным.
    srand(20260101 + bossType * 7 + (pinHp > 0.0f ? (int)(pinHp * 100.0f) : 900));
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    // runTime держим ниже 90 с и уводим очередь стражей: иначе эндлес присылает
    // своего босса, и в замер попадает чужая свита (на стенде так и было — рядом
    // с измеряемым стражем вставал второй и звал своих).
    g.runTime = 20.0f;
    g.nextBossIndex = 999;
    g.itemLevels[0] = 3; g.itemLevels[1] = 3; g.itemLevels[2] = 3;
    g.playerX = SCREEN_W * 0.5f; g.playerY = SCREEN_H * 0.5f;
    g.playerHp = g.playerMaxHp;
    // Обычный поток волн выключаем: иначе меряется не свита, а весь бестиарий
    // (в забеге на арене живут десятки мобов, призванных не стражем).
    // RET_WAVES=1 оставляет обычный поток волн: нужно, чтобы проверить, что предел
    // свиты держится и в настоящей толпе, а не только на пустой арене.
    if (!getenv("RET_WAVES")) {
        g.balance.spawnMultiplier = 0.0f;
        g.squadTimer = 1.0e9f;
        g.ratTimer = 1.0e9f;
    }
    g.monsters.clear(); g.projectiles.clear();
    g.spawnMonster(bossType, false, true, bossType);
    if (g.monsters.empty()) return st;
    if (pinHp > 0.0f) for (Monster& m : g.monsters) if (m.isBoss) {
        m.hp = m.maxHp * pinHp;
        m.bossPhase = Game::bossPhaseForHealth(m.hp, m.maxHp);
    }

    std::map<int, float> prevHp;
    bool bossAlive = true;

    for (float t = 0.0f; t < seconds; t += dt) {
        while (g.state == STATE_LEVELUP) { g.applyUpgrade(g.upgradeChoices[0]); g.state = STATE_PLAYING; }
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
        g.nextBossIndex = 999;
        g.update(dt);
        g.playerHp = g.playerMaxHp;
        ++st.frames;

        if (getenv("RET_TRACE")) {
            int bi = 0;
            for (const Monster& m : g.monsters) if (m.isBoss)
                printf("  %6.2f  boss%d type %d hp %.0f  sp %.2f  summon %.2f  abilW %.2f  abilA %.2f  live %d\n",
                       t, bi++, m.bossType, m.hp, m.specialTimer, m.bossSummonWindup,
                       m.abilityWindup, m.abilityActive, g.countBossRetinue());
        }
        const int retinueLive = g.countBossRetinue();
        if (retinueLive > st.livePeak) {
            st.livePeak = retinueLive;
            for (const Monster& m : g.monsters) if (m.isBoss) {
                st.capNow = Game::bossRetinueCap(m.bossType, m.bossPhase, m.bossReinforced);
            }
        }
        st.liveSum += retinueLive;

        int spawnedThisFrame = 0;
        for (const Monster& m : g.monsters) {
            if (!m.isRetinue || m.isBoss) continue;
            auto it = prevHp.find(m.uniqueId);
            if (it == prevHp.end()) {
                ++st.addsSpawned;
                ++spawnedThisFrame;
                if (m.isElite) ++st.eliteAdds;
                prevHp[m.uniqueId] = m.hp;
            } else {
                if (it->second > m.hp + 0.01f) st.damageToAdds += (double)(it->second - m.hp);
                it->second = m.hp;
            }
        }
        if (spawnedThisFrame > 0) {
            ++st.summonEvents;   // один вызов = одна волна
            if (getenv("RET_DEBUG")) {
                int phase = 0;
                for (const Monster& m : g.monsters) if (m.isBoss) phase = m.bossPhase;
                printf("  t=%.2f  волна %d  живых %d  фаза %d\n", t, spawnedThisFrame, retinueLive, phase);
            }
        }

        for (Monster& m : g.monsters) {
            if (!m.isBoss) continue;
            auto it = prevHp.find(m.uniqueId);
            if (it == prevHp.end()) { prevHp[m.uniqueId] = m.hp; }
            else {
                if (it->second > m.hp + 0.01f) st.damageToBoss += (double)(it->second - m.hp);
                it->second = m.hp;
            }
            if (pinHp > 0.0f) m.hp = m.maxHp * pinHp;
        }

        bossAlive = false;
        for (const Monster& m : g.monsters) if (m.isBoss && m.hp > 0.0f) bossAlive = true;
        if (!bossAlive && pinHp <= 0.0f) {
            for (const Monster& m : g.monsters) {
                if (m.isBoss || m.isResonator || m.hp <= 0.0f) continue;
                if (m.isRetinue) ++st.aliveAfterBossDeath;
                else ++st.othersAfterBossDeath;
            }
            break;
        }
    }
    return st;
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
    printf("\nсвита: волны выключены, герой с тремя оружием 3-го ранга стоит в центре.\n");
    printf("Строки 1–3 — по 30 с на фазу при прибитом HP стража (свита видна целиком),\n");
    printf("строка «бой» — обычный бой до смерти стража, затем кто остался на арене.\n");
    printf("%-12s %5s %10s %8s %9s %11s %5s %s\n", "страж", "фаза", "вызовов/м", "призвано", "живых ср", "пик/предел", "элит", "урон в свиту");
    for (int boss = 0; boss < 6; ++boss) {
        if (onlyBoss >= 0 && boss != onlyBoss) continue;
        // Три прогона по 20 с, по одному на фазу: здоровье стража прибито, поэтому
        // видно, сколько свиты положено в каждой фазе и держится ли предел.
        for (int ph = 1; ph <= 3; ++ph) {
            const float pin = ph == 1 ? 0.90f : (ph == 2 ? 0.55f : 0.22f);
            RetinueStats rs = runRetinue(g, boss, 30.0f, pin);
            const double minutes = rs.frames > 0 ? rs.frames / 3600.0 : 1.0;
            const double totalDmg = rs.damageToBoss + rs.damageToAdds;
            printf("%-12s %5d %10.1f %8d %9.1f %6d/%-2d %5d %9.0f%%\n", names[boss], ph,
                   rs.summonEvents / minutes, rs.addsSpawned,
                   rs.frames ? rs.liveSum / rs.frames : 0.0, rs.livePeak, rs.capNow,
                   rs.eliteAdds,
                   totalDmg > 0.0 ? 100.0 * rs.damageToAdds / totalDmg : 0.0);
        }
        // И обычный бой до смерти стража (до 120 с): что остаётся на арене после победы.
        RetinueStats rk = runRetinue(g, boss, 120.0f);
        if (rk.frames < 420) {   // 7 с: страж умер раньше первого вызова
            printf("%-12s %5s %10s %8s %9s %11s %5s %s   бой короче первого вызова (%.1f с)\n",
                   names[boss], "бой", "-", "-", "-", "-", "-", "-", rk.frames / 60.0);
            continue;
        }
        const double kmins = rk.frames > 0 ? rk.frames / 3600.0 : 1.0;
        const double ktotal = rk.damageToBoss + rk.damageToAdds;
        printf("%-12s %5s %10.1f %8d %9.1f %6d/%-2s %5d %9.0f%%   после смерти: свита %d, прочие %d\n",
               names[boss], "бой", rk.summonEvents / kmins, rk.addsSpawned,
               rk.frames ? rk.liveSum / rk.frames : 0.0, rk.livePeak, "-", rk.eliteAdds,
               ktotal > 0.0 ? 100.0 * rk.damageToAdds / ktotal : 0.0,
               rk.aliveAfterBossDeath, rk.othersAfterBossDeath);
    }

    printf("\n«замах+выстр» — доля кадров замаха, в которые страж одновременно стреляет;\n");
    printf("«вплотн» — залпы/приёмы ближе 90/120 px; «попаданий/м» — падения HP героя за минуту,\n");
    printf("«вплотную» — из них полученные ближе 90 px. Герою каждый кадр возвращается полное HP;\n");
    printf("«невидим» — кадры, когда страж был за краем видимого поля.\n");
    return 0;
}
