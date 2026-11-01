// ── Стенд ИИ: headless-прогон боя и сводка по поведению врагов ───────────────
//
// Зачем отдельная программа. Поведение ИИ видно только на длинном отрезке:
// разумный моб появляется в 1 % спавнов, приёмы длятся 0.2–0.8 с, а «моб стоит
// в герое» и «моб держит зазор» отличаются не кадром, а долей времени. Смотреть
// на это глазами по 10 минут — единственный способ, который был раньше.
//
// Стенд собирает ту же игру без окна и звука, гонит её с заданной частотой,
// записывает состояние каждой тактики и печатает сводку:
//
//   • сколько разумных заспавнилось по тактикам против заявленного шанса;
//   • долю времени в контакте с героем (это и был баг: 89 %);
//   • среднюю дистанцию и распределение по кольцам;
//   • число приёмов в минуту: замах/рывок у перехватчика, выскальзывание и
//     отход у стрелка, нырок и круг у увёртливого;
//   • держит ли стрелок огонь в упор;
//   • сколько кадров занимает бой (цена ИИ в мс);
//   • итог забега: убийства, урон, собранное золото.
//
// Сборка (MinGW из build_windows.bat подходит):
//     g++ -O2 -std=c++17 -Isrc tools/ai_probe.cpp -o build/ai_probe
// Запуск:
//     build/ai_probe            # 120 секунд, обычные шансы
//     build/ai_probe 300 1.0    # 300 секунд, все спавны разумные
//     build/ai_probe 300 1.0 3  # плюс только тактика 3 (перехватчик), 1..3
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <map>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

struct TacticStats {
    long long spawned = 0;
    double contactSeconds = 0.0;
    double aliveSeconds = 0.0;
    double distanceSum = 0.0;
    long long distanceSamples = 0;
    int windups = 0, lunges = 0, slips = 0, dives = 0;
    int retreatFrames = 0, fireHeldFrames = 0;
    int shots = 0, pointBlankShots = 0;   // выстрелы и из них сделанные в упор
    std::vector<float> distances;
};

struct Tracked {
    int tactic = 0;
    bool wasWindup = false, wasLunge = false, wasSlip = false, wasDive = false;
    float prevShootTimer = 0.0f;
    bool seen = false;
};

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 120.0f;
    const float smartChance = argc > 2 ? (float)atof(argv[2]) : -1.0f;
    const int onlyTactic = argc > 3 ? atoi(argv[3]) - 1 : -1;   // 0..2 или -1
    bool chase = false;                                         // 4-й аргумент: «chase»
    for (int i = 1; i < argc; ++i) if (!strcmp(argv[i], "chase")) chase = true;

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.cheatGodMode = true;                 // стенд меряет ИИ, а не смертность игрока
    if (smartChance >= 0.0f) {
        for (int i = 0; i < 3; ++i)
            g.smartSpawnChance[i] = (onlyTactic < 0 || onlyTactic == i) ? smartChance : 0.0f;
    }
    g.monsters.clear();

    std::map<int, Tracked> tracked;
    TacticStats stats[3];
    long long spawnedAll = 0, sentByPlayer = 0, nonPlaying = 0;
    double aliveMonstersSum = 0.0;
    long long frames = 0;

    // Автовыбор прокачки: без него прогон встаёт на первом же уровне и вся
    // статистика ниже считается по трём секундам боя, а не по минуте.
    int autoPicks = 0;
    const float dt = 1.0f / 60.0f;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (float t = 0.0f; t < seconds; t += dt) {
        // Игра сама снижает шанс разумных за убийство «в один удар» (/1.1, минимум
        // 0.2 %). В принудительном режиме это обесценило бы замер, поэтому шанс
        // возвращается каждый кадр.
        if (smartChance >= 0.0f) {
            for (int i = 0; i < 3; ++i)
                g.smartSpawnChance[i] = (onlyTactic < 0 || onlyTactic == i) ? smartChance : 0.0f;
        }
        while (g.state == STATE_LEVELUP) {
            g.applyUpgrade(g.upgradeChoices[0]);
            g.state = STATE_PLAYING;
            ++autoPicks;
        }
        // Режим «chase»: агент-игрок сам идёт на ближайшего разумного моба.
        // Без него стенд не проверяет выскальзывание стрелка — герой стоит и
        // дистанция не меняется.
        if (chase && g.state == STATE_PLAYING) {
            const Monster* target = nullptr;
            float best = 1e9f;
            for (const Monster& m : g.monsters) {
                if (!m.isSmart || m.hp <= 0.0f) continue;
                const float d = (m.x - g.playerX) * (m.x - g.playerX) + (m.y - g.playerY) * (m.y - g.playerY);
                if (d < best) { best = d; target = &m; }
            }
            if (target) {
                const float dx = target->x - g.playerX, dy = target->y - g.playerY;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1.0f) {
                    const float playerSpeed = 185.0f;
                    g.playerX += dx / len * playerSpeed * dt;
                    g.playerY += dy / len * playerSpeed * dt;
                }
            }
        }
        g.update(dt);
        ++frames;
        if (g.state != STATE_PLAYING) { ++nonPlaying; if (g.state == STATE_CHEST) { g.state = STATE_PLAYING; } }
        aliveMonstersSum += (double)g.monsters.size();
        for (const Monster& m : g.monsters) {
            auto it = tracked.find(m.uniqueId);
            if (it == tracked.end()) {
                Tracked tr;
                tr.tactic = std::max(0, std::min(2, m.smartTactic));
                it = tracked.emplace(m.uniqueId, tr).first;
                ++spawnedAll;
                if (m.isSmart) ++stats[tr.tactic].spawned;
            }
            if (!m.isSmart) continue;
            TacticStats& st = stats[it->second.tactic];
            const float dx = m.x - g.playerX, dy = m.y - g.playerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            st.aliveSeconds += dt;
            st.distanceSum += dist;
            ++st.distanceSamples;
            if (dist <= m.radius + 14.0f) st.contactSeconds += dt;
            if (m.smartClose.retreating || m.smartClose.recover > 0.0f) ++st.retreatFrames;
            // Выстрел виден по сбросу таймера вверх: игра ставит новый интервал.
            if (it->second.seen && m.shootTimer > it->second.prevShootTimer + 0.01f) {
                ++st.shots;
                if (dist < 104.0f) ++st.pointBlankShots;
            }
            it->second.prevShootTimer = m.shootTimer;
            it->second.seen = true;
            if (m.smartClose.windup > 0.0f && !it->second.wasWindup) ++st.windups;
            if (m.smartClose.lunge > 0.0f && !it->second.wasLunge) ++st.lunges;
            if (m.smartClose.slip > 0.0f && !it->second.wasSlip) ++st.slips;
            if (m.smartClose.dive > 0.0f && !it->second.wasDive) ++st.dives;
            if (dist < 104.0f) ++st.fireHeldFrames;
            it->second.wasWindup = m.smartClose.windup > 0.0f;
            it->second.wasLunge = m.smartClose.lunge > 0.0f;
            it->second.wasSlip = m.smartClose.slip > 0.0f;
            it->second.wasDive = m.smartClose.dive > 0.0f;
        }
    }
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double msPerFrame = std::chrono::duration<double, std::milli>(t1 - t0).count() / (double)frames;

    printf("стенд ИИ: %.0f с, кадров %lld, %.2f мс/кадр\n", seconds, frames, msPerFrame);
    if (smartChance >= 0.0f)
        printf("шанс спавна разумного задан руками: %.2f %s\n", smartChance,
               onlyTactic >= 0 ? "(одна тактика)" : "");
    for (int i = 0; i < 3; ++i) if (stats[i].spawned) sentByPlayer += stats[i].spawned;
    printf("спавнов всего %lld, из них разумных %lld (%.2f%%)\n", spawnedAll, sentByPlayer,
           spawnedAll ? 100.0 * (double)sentByPlayer / (double)spawnedAll : 0.0);
    printf("мобов на экране в среднем: %.1f, кадров вне боя %lld\n\n", aliveMonstersSum / (double)frames, nonPlaying);

    static const char* const names[3] = {"стрелок", "перехватчик", "увёртливый"};
    printf("%-12s %6s %9s %9s %9s %8s %7s %6s %6s %6s %6s %6s %6s\n",
           "тактика", "спавн", "жизнь,с", "контакт,с", "%конт", "средн.д", "отход%", "зам", "рыв", "выск", "нырки", "выстр", "вупор");
    for (int i = 0; i < 3; ++i) {
        TacticStats& st = stats[i];
        if (st.spawned == 0) continue;
        const double contactPct = st.aliveSeconds > 0.0 ? 100.0 * st.contactSeconds / st.aliveSeconds : 0.0;
        printf("%-12s %6lld %9.1f %9.2f %8.1f%% %8.0f %6.0f%% %6d %6d %6d %6d %6d %6d\n",
               names[i], st.spawned, st.aliveSeconds, st.contactSeconds, contactPct,
               st.distanceSamples ? st.distanceSum / (double)st.distanceSamples : 0.0,
               st.aliveSeconds > 0.0 ? 100.0 * (double)st.retreatFrames / (st.aliveSeconds * 60.0) : 0.0,
               st.windups, st.lunges, st.slips, st.dives, st.shots, st.pointBlankShots);
    }
    printf("\nбой: убито %d, золото %d, урон %.0f, HP %.0f/%.0f, автопрокачек %d, уровень %d\n",
           g.kills, g.runGold, g.totalDamageDealt, g.playerHp, g.playerMaxHp, autoPicks, g.playerLevel);
    printf("порог контакта — m.radius + 14; «отход%%» — доля кадров жизни в отходе\n");
    return 0;
}
