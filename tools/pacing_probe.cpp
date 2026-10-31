// ── Стенд темпа: как часто игра что-то даёт ───────────────────────────────────
//
// Обычные стенды отвечают «сколько»: сколько урона, сколько золота за забег,
// сколько стоит ранг. Этот отвечает «когда»: когда игрок получает следующую
// награду и одинаково ли часто это происходит.
//
// Почему это вообще надо мерить. Уровень стоит 40 + 50·L + 4·L² опыта, а
// спавн растёт как 3.2 + 0.75·t/35 мобов в секунду: требования растут
// квадратично, поток опыта — линейно. Если не смотреть, получается классика
// «первые три минуты по уровню в десять секунд, дальше по уровню в три
// минуты»: подача дофамина обрывается там, где забег только разогрелся.
//
// Стенд играет забег «Бесконечная ночь» ботом (автовыбор прокачки с фокусом,
// автозакрытие сундука, охота за сундуками и самоцветами) и пишет, что
// случилось в каждую минуту:
//   • уровни, сундуки, тиры сборки, стражи — это и есть награды;
//   • убийства и золото — сырьё, из которого они получаются;
//   • прорывы — запланированные всплески, их видно отдельно.
// Дальше печатаются промежутки между уровнями и «засухи» — самый длинный
// отрезок без единой награды, — и кривая кузницы: сколько забегов стоит
// следующий ранг.
//
// Забег по умолчанию БЕССМЕРТНЫЙ, и это осознанно: стенд отвечает на вопрос
// «как часто приходят награды, если игрок доживает», а не «сколько живёт бот».
// Бот не уклоняется, он идёт за добычей и получает по лицу; в смертном режиме
// он стабильно погибает на 3–4-й минуте, и мерился бы его тайминг, а не
// расписание. Поэтому смертный режим включается четвёртым аргументом `mortal`
// и служит нижней границей: столько получает новичок к своей смерти.
//
// Сборка:
//     mkdir -p build && g++ -O2 -std=c++17 -pthread -Isrc tools/pacing_probe.cpp -o build/pacing_probe
// Запуск:
//     build/pacing_probe              # 15 минут, три зерна
//     build/pacing_probe 1200 5       # 20 минут, пять зёрен
// Код возврата 1, если темп неровный: скачок (минута с вдвое большим числом
// наград, чем соседние) или провисание (промежуток без наград дольше трети
// забега при том, что до него награды шли).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

#include "renderer.h"
#include "game.h"

struct MinuteBucket {
    int levels = 0, chests = 0, tiers = 0, bosses = 0, kills = 0, gold = 0;
    int breakthroughs = 0, bossSpawns = 0;
    int pool = 0;               // сколько предметов вообще доступно драфту

    int rewards() const { return levels + chests + tiers; }
};

struct RunResult {
    std::vector<MinuteBucket> minutes;
    std::vector<int> xpMinute;
    std::vector<float> levelTimes;
    std::vector<float> rewardTimes;
    int totalLevels = 0, totalChests = 0, totalBosses = 0, totalGold = 0, totalKills = 0;
    int finalLevel = 0;
    int itemsMaxed = 0;
    int bossSpawns = 0, maxBossesAlive = 0, emptyLevelUps = 0;
    int equipped = 0, buildLimit = 0;
    float longestDrought = 0.0f;
    float droughtStart = 0.0f;
    float longestLevelGap = 0.0f;
    float levelGapStart = 0.0f;
    float survived = 0.0f;
};

// Бот: идёт к ближайшей награде в радиусе видимости, иначе обходит арену по
// окружности. Так он собирает самоцветы (иначе уровень не растёт и стенд
// мерит стояние на месте) и при этом воспроизводим: никакого rand() в боте.
static void driveBot(Game& g, float dt, float& orbitAngle) {
    if (g.state != STATE_PLAYING) return;
    const float speed = g.getMoveSpeed();
    float targetX = g.playerX, targetY = g.playerY;
    bool found = false;

    // Портал выхода — выбор игрока, а не награда расписания: бот его не берёт,
    // иначе забег кончается на 270-й секунде и мерить темп не на чем.
    for (Pickup& p : g.pickups) if (p.type == 7) p.value = 0;

    // Сундук стража — главная награда забега, и он не притягивается: за ним
    // надо идти. Человек идёт. Стенд, который этого не делает, мерил
    // «сундуков 0 при девяти убитых стражах» — то есть свою же ошибку.
    float best = -1.0f;
    for (const Pickup& p : g.pickups) {
        if (p.type != 6 || p.value <= 0) continue;
        const float dx = p.x - g.playerX, dy = p.y - g.playerY;
        const float d2 = dx * dx + dy * dy;
        if (best < 0.0f || d2 < best) { best = d2; targetX = p.x; targetY = p.y; found = true; }
    }
    if (!found) {
        best = 420.0f * 420.0f;
        for (const Pickup& p : g.pickups) {
            if (p.value <= 0 || p.type == 6 || p.type == 7) continue;
            const float dx = p.x - g.playerX, dy = p.y - g.playerY;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; targetX = p.x; targetY = p.y; found = true; }
        }
    }
    if (!found) {
        // Ничего поблизости: обход арены по кругу, а не стояние в точке старта.
        orbitAngle += dt * speed / 320.0f;
        targetX = std::cos(orbitAngle) * 320.0f;
        targetY = std::sin(orbitAngle) * 320.0f;
    }
    const float dx = targetX - g.playerX, dy = targetY - g.playerY;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 2.0f) {
        const float step = std::min(len, speed * dt);
        g.playerX += dx / len * step;
        g.playerY += dy / len * step;
    }
}

static RunResult playRun(Game& g, float seconds, float dt, unsigned seed, bool immortal) {
    RunResult res;
    srand(seed);
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.ignoreSavedState = true;
    g.cheatGodMode = immortal;   // по умолчанию забег смертный: смерть — конец
    g.monsters.clear();
    g.pickups.clear();

    const int budget = (int)(seconds / 60.0f) + 1;
    res.minutes.assign(budget, MinuteBucket());

    int lastLevel = g.playerLevel;
    int lastGold = g.runGold;
    int lastKills = g.kills;
    int lastTier = g.getBuildTier();
    int lastBosses = g.runBossKills;
    float lastLevelTime = 0.0f;
    float orbitAngle = 0.0f;
    std::vector<long long> seenBosses;
    long long xpEarned = 0;                 // сколько опыта стоило всё, что куплено
    std::vector<long long> xpPerMinute;     // и то же по минутам

    for (float t = 0.0f; t < seconds; t += dt) {
        const int minuteNow = std::min(budget - 1, (int)(t / 60.0f));
        // Автовыбор прокачки: без него бой встаёт на первом же уровне.
        while (g.state == STATE_LEVELUP) {
            // Политика бота — «сначала оружие, потом артефакты»: из четырёх
            // карточек берём оружие, а если оружия нет — самый прокачанный
            // предмет. Так играет разумный человек; «первая карточка слева»
            // мерила не игру, а дайс: одно зерно вложило четыре ранга в «Пепел
            // феникса» (возрождение), осталось с нулевым уроном и получило
            // 143 с без наград — но это был выбор сборки, а не расписание.
            int pick = g.upgradeChoices[0];
            int bestWeapon = -1, bestAny = -1, bestAnyLevel = -1;
            for (int k = 0; k < 4; ++k) {
                const int c = g.upgradeChoices[k];
                if (c < 0 || c >= ITEM_COUNT) continue;
                if (g.itemLevels[c] >= g.getItemMaxLevel(c)) continue;
                if (ITEM_DEFS[c].isWeapon && (bestWeapon < 0 || g.itemLevels[c] > g.itemLevels[bestWeapon]))
                    bestWeapon = c;
                if (g.itemLevels[c] > bestAnyLevel) { bestAnyLevel = g.itemLevels[c]; bestAny = c; }
            }
            if (bestWeapon >= 0) pick = bestWeapon;
            else if (bestAny >= 0) pick = bestAny;
            // Мёртвая карточка: выбранное уже на максимуме — уровень не даёт
            // ничего, кроме +12 HP. Это и есть тихая дыра в подаче наград.
            const bool dead = pick < 0 || pick >= ITEM_COUNT || g.itemLevels[pick] >= g.getItemMaxLevel(pick);
            if (dead) ++res.emptyLevelUps;
            // Опыт, который стоил этот уровень: по нему видно, растёт ли поток
            // опыта вместе с ценой уровня или отстаёт от неё.
            xpEarned += g.getXpRequirementForLevel(g.playerLevel);
            if ((int)xpPerMinute.size() < minuteNow + 1) xpPerMinute.resize(minuteNow + 1, 0);
            xpPerMinute[minuteNow] += g.getXpRequirementForLevel(g.playerLevel);
            g.applyUpgrade(pick);
            g.state = STATE_PLAYING;
        }
        driveBot(g, dt, orbitAngle);
        g.update(dt);

        const int minute = minuteNow;
        MinuteBucket& bucket = res.minutes[minute];
        res.survived = t;
        if (g.state == STATE_DEATH || g.state == STATE_GAMEOVER || g.state == STATE_EXTRACTION) break;

        // Уровень: награда с выбором — самый частый «дофамин» в забеге.
        if (g.playerLevel > lastLevel) {
            const int gained = g.playerLevel - lastLevel;
            for (int i = 0; i < gained; ++i) {
                res.levelTimes.push_back(t);
                res.rewardTimes.push_back(t);
                const float gap = t - lastLevelTime;
                if (gap > res.longestLevelGap) { res.longestLevelGap = gap; res.levelGapStart = lastLevelTime; }
                lastLevelTime = t;
                ++bucket.levels;
            }
            lastLevel = g.playerLevel;
        }
        // Сундук стража: заходит в STATE_CHEST сразу после подбора.
        if (g.state == STATE_CHEST) {
            ++bucket.chests;
            res.rewardTimes.push_back(t);
            g.finishTreasureChest();
            if (g.state != STATE_PLAYING) break;      // забег закончился (миссия/портал)
        }
        if (g.buildTierUpTimer > 0.0f && g.getBuildTier() > lastTier) {
            ++bucket.tiers;
            res.rewardTimes.push_back(t);
            lastTier = g.getBuildTier();
        }
        if (g.runBossKills > lastBosses) { ++bucket.bosses; lastBosses = g.runBossKills; }
        // Появление стража считается по новым id: иначе «стражи в минуту» путает
        // приход и убийство, а именно на приходе держится сундук.
        int bossesAlive = 0;
        for (const Monster& m : g.monsters) {
            if (!m.isBoss) continue;
            ++bossesAlive;
            if (std::find(seenBosses.begin(), seenBosses.end(), (long long)m.uniqueId) == seenBosses.end()) {
                seenBosses.push_back((long long)m.uniqueId);
                ++bucket.bossSpawns;
            }
        }
        res.maxBossesAlive = std::max(res.maxBossesAlive, bossesAlive);
        if (g.breakthroughCount > bucket.breakthroughs) bucket.breakthroughs = g.breakthroughCount;

        // Пул драфта: «новые игрушки» тоже награда, и они тоже должны приходить
        // ровно, а не одной пачкой на третьей минуте.
        int unlockedCount = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) if (g.isItemUnlocked(i)) ++unlockedCount;
        bucket.pool = std::max(bucket.pool, unlockedCount);
        const int goldNow = g.runGold;
        bucket.gold += std::max(0, goldNow - lastGold);
        lastGold = goldNow;
        const int killsNow = g.kills;
        bucket.kills += std::max(0, killsNow - lastKills);
        lastKills = killsNow;
    }

    // Засуха — самый длинный отрезок без награды (не считая первых секунд).
    float prev = 0.0f;
    for (float rt : res.rewardTimes) {
        if (rt - prev > res.longestDrought) { res.longestDrought = rt - prev; res.droughtStart = prev; }
        prev = rt;
    }
    const float endTime = std::min(seconds, res.survived > 0.0f ? res.survived : seconds);
    if (endTime - prev > res.longestDrought) { res.longestDrought = endTime - prev; res.droughtStart = prev; }

    res.totalLevels = (int)res.levelTimes.size();
    for (const MinuteBucket& b : res.minutes) {
        res.totalChests += b.chests;
        res.totalBosses += b.bosses;
        res.totalGold += b.gold;
        res.totalKills += b.kills;
    }
    res.finalLevel = g.playerLevel;
    res.bossSpawns = (int)seenBosses.size();
    res.xpMinute.assign(xpPerMinute.begin(), xpPerMinute.end());
    res.equipped = g.countEquippedItems();
    res.buildLimit = g.getBuildLimit();
    for (int i = 0; i < ITEM_COUNT; ++i)
        if (g.itemLevels[i] >= g.getItemMaxLevel(i)) ++res.itemsMaxed;
    return res;
}

// Кривая кузницы: сколько забегов стоит следующий ранг при данной добыче.
// Возвращает число нарушений ровности: ожидание должно расти, но не прыгать.
static int printShopCurve(int goldPerRun) {
    printf("\nкузница: забегов до следующего ранга (добыча %d золота/забег)\n", goldPerRun);
    Game g;
    printf("%-16s", "строка");
    for (int r = 1; r <= 12; ++r) printf(" %5d", r);
    printf("   всего\n");
    double grand = 0.0;
    for (int id = 0; id < 6; ++id) {
        printf("%-16s", META_DEFS[id].name);
        g.save.metaRanks[id] = 0;
        double total = 0.0;
        double prevRuns = 0.0;
        for (int r = 1; r <= META_DEFS[id].maxRank; ++r) {
            const int cost = g.getMetaUpgradeCost(id);
            const double runs = (double)cost / (double)goldPerRun;
            total += runs;
            printf(" %5.1f", runs);
            // Резкий рост времени ожидания между соседними рангами — провисание.
            if (r > 1 && prevRuns > 0.05 && runs / prevRuns > 2.2) printf("(!)");
            prevRuns = runs;
            g.save.metaRanks[id] = r;
        }
        printf("  %6.1f\n", total);
        grand += total;
        g.save.metaRanks[id] = 0;
    }
    printf("%-16s %52.1f\n", "итого", grand);

    // Ровность: первый ранг не бесплатный (иначе пачка покупок в первом же
    // забеге), ожидание растёт, но не более чем в 1,4 раза от ранга к рангу, и
    // последний ранг не превращается в «провисание».

    int failures = 0;
    // Договоры — риск-магазин, и он дешевле по замыслу: там платят не за силу, а
    // за право взять больше золота. Проверка та же — ожидание не должно прыгать.
    printf("\nдоговоры: забегов до следующего ранга\n%-16s", "договор");
    for (int r = 1; r <= 10; ++r) printf(" %5d", r);
    printf("   всего\n");
    for (int id = 0; id < ENEMY_OATH_COUNT; ++id) {
        Game g;
        g.ignoreSavedState = true;
        g.simulationMode = true;
        printf("%-16s", ENEMY_OATHS[id].name);
        double prev = 0.0, total = 0.0;
        for (int r = 0; r < ENEMY_OATHS[id].maxRank; ++r) {
            g.setEnemyOathRank(id, r);
            const double runs = (double)g.getEnemyOathCost(id) / (double)goldPerRun;
            printf(" %5.1f", runs);
            total += runs;
            if (r > 0 && prev > 0.0 && runs / prev > 1.4) {
                printf("(!)");
                ++failures;
            }
            prev = runs;
        }
        printf("  %6.1f\n", total);
    }
    for (int id = 0; id < 6; ++id) {
        Game g;
        g.ignoreSavedState = true;
        g.simulationMode = true;
        double prev = 0.0, first = 0.0, last = 0.0;
        for (int r = 0; r < META_DEFS[id].maxRank; ++r) {
            g.save.metaRanks[id] = r;
            const double runs = (double)g.getMetaUpgradeCost(id) / (double)goldPerRun;
            if (r == 0) first = runs;
            last = runs;
            if (r > 0 && prev > 0.0 && runs / prev > 1.4) {
                printf("  неровно: %s, ранг %d ждать в %.2f раза дольше предыдущего\n",
                       META_DEFS[id].name, r + 1, runs / prev);
                ++failures;
            }
            prev = runs;
        }
        if (first < 0.25) {
            printf("  слишком дёшево: первый ранг %s — %.2f забега\n", META_DEFS[id].name, first);
            ++failures;
        }
        if (last > 4.0) {
            printf("  провисание: последний ранг %s — %.1f забега\n", META_DEFS[id].name, last);
            ++failures;
        }
    }
    return failures;
}

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 900.0f;
    const int seeds = argc > 2 ? std::max(1, atoi(argv[2])) : 3;
    const bool immortal = !(argc > 3 && !strcmp(argv[3], "mortal"));
    const float dt = 1.0f / 60.0f;
    SCREEN_W = 1280; SCREEN_H = 720;

    printf("стенд темпа: %.0f с на забег, зёрен %d, бот %s\n\n", seconds, seeds,
           immortal ? "бессмертный (расписание наград)" : "смертный (нижняя граница новичка)");
    printf("%-6s", "мин");
    for (const char* h : {"ур", "сунд", "тир", "страж", "убийств", "золото"}) printf(" %7s", h);
    printf("\n");

    // Сводка по минутам, усреднённая по зёрнам: один забег — это один бросок.
    const int minutes = (int)(seconds / 60.0f) + 1;
    std::vector<double> avgLevels(minutes, 0.0), avgChests(minutes, 0.0), avgTiers(minutes, 0.0);
    std::vector<double> avgBosses(minutes, 0.0), avgKills(minutes, 0.0), avgGold(minutes, 0.0);
    std::vector<double> avgXp(minutes, 0.0), avgBossSpawns(minutes, 0.0), avgPool(minutes, 0.0);
    double sumLevels = 0, sumGold = 0, sumKills = 0;
    double worstDrought = 0, worstGap = 0, minGold = 1e18, maxGold = 0.0;
    int worstDroughtSeed = 0, worstGapSeed = 0;
    int levelCounts[32] = {0};
    for (int s = 0; s < seeds; ++s) {
        Game g;
        const unsigned seed = 1000u + (unsigned)s * 7717u;
        RunResult res = playRun(g, seconds, dt, seed, immortal);
        for (int m = 0; m < minutes && m < (int)res.minutes.size(); ++m) {
            avgLevels[m] += res.minutes[m].levels;
            avgChests[m] += res.minutes[m].chests;
            avgTiers[m] += res.minutes[m].tiers;
            avgBosses[m] += res.minutes[m].bosses;
            avgKills[m] += res.minutes[m].kills;
            avgGold[m] += res.minutes[m].gold;
            avgBossSpawns[m] += res.minutes[m].bossSpawns;
            avgPool[m] += res.minutes[m].pool;
            if (m < (int)res.xpMinute.size()) avgXp[m] += res.xpMinute[m];
        }
        sumLevels += res.totalLevels;
        sumGold += res.totalGold;
        const double oneRunGold = (double)res.totalGold;
        minGold = std::min(minGold, oneRunGold);
        maxGold = std::max(maxGold, oneRunGold);
        sumKills += res.totalKills;
        if (res.longestDrought > worstDrought) { worstDrought = res.longestDrought; worstDroughtSeed = s; }
        if (res.longestLevelGap > worstGap) { worstGap = res.longestLevelGap; worstGapSeed = s; }
        if (res.totalLevels < 32) ++levelCounts[res.totalLevels];
        printf("зерно %d: %.0f с, уровней %d (ур.%d), сундуков %d, стражей: пришло %d, убито %d, "
               "одновременно до %d; золота %d; пустых уровней %d; предметов %d/%d при слотах %d\n",
               s, res.survived, res.totalLevels, res.finalLevel, res.totalChests, res.bossSpawns, res.totalBosses,
               res.maxBossesAlive, res.totalGold, res.emptyLevelUps, res.equipped, res.itemsMaxed,
               res.buildLimit);
        printf("        засуха %.0f с (с %.0f с), худший шаг между уровнями %.0f с\n",
               res.longestDrought, res.droughtStart, res.longestLevelGap);
    }

    // Таблица по минутам — средняя по зёрнам. Именно она показывает, где подача
    // обрывается: «уровней в минуту» должно убывать плавно, а не падать в ноль.
    printf("\n%-5s", "мин");
    for (const char* h : {"ур/мин", "опыт/мин", "сунд", "тир", "страж", "уб/мин", "зол/мин", "пул"}) printf(" %9s", h);
    printf("\n");
    for (int m = 0; m < minutes; ++m) {
        const double l = avgLevels[m] / (double)seeds;
        if (m == 0 && minutes <= 1) continue;
        printf("%-5d %9.2f %9.0f %9.2f %9.2f %9.2f %9.1f %9.0f %9.0f\n", m,
               l, avgXp[m] / (double)seeds, avgChests[m] / (double)seeds, avgTiers[m] / (double)seeds,
               avgBossSpawns[m] / (double)seeds, avgKills[m] / (double)seeds, avgGold[m] / (double)seeds,
               avgPool[m] / (double)seeds);
    }

    const double goldPerRun = sumGold / (double)seeds;
    const double levelsPerRun = sumLevels / (double)seeds;
    printf("\nнаграды за забег: %.1f уровней, золота %.0f, убийств %.0f\n",
           levelsPerRun, goldPerRun, sumKills / (double)seeds);
    printf("худшая засуха: %.0f с без наград (зерно %d), худший шаг между уровнями: %.0f с (зерно %d)\n",
           worstDrought, worstDroughtSeed, worstGap, worstGapSeed);

    int failures = printShopCurve(goldPerRun > 1.0 ? (int)goldPerRun : 25000);

    // Разброс между забегами — это те же «скачки», только не внутри забега, а
    // между ними: если одно зерно даёт 2 тысячи золота, а другое 37, игрок не
    // знает, чего ждать от следующего вечера.
    const double spread = minGold > 0.0 ? maxGold / minGold : 0.0;
    printf("\nразброс золота между забегами: %.0f…%.0f (в %.2f раза)\n", minGold, maxGold, spread);

    // ── Приговор ────────────────────────────────────────────────────────────
    // Пороги — это требования к расписанию, а не подгонка под замер:
    //   • ни одна минута не должна быть пустой (иначе награда «пропала»);
    //   • самая щедрая минута не может быть в 2,5 раза щедрее обычной
    //     (это и есть «чрезмерная подача» — всплеск, после которого пусто);
    //   • самый длинный отрезок без наград — не дольше двух минут;
    //   • ожидание следующего ранга в магазине растёт не более чем в 1,4 раза
    //     от ранга к рангу (линейная кривая даёт ~1,1).
    printf("\n");
    std::vector<double> perMinute;
    for (int m = 0; m < minutes; ++m) perMinute.push_back(avgLevels[m] + avgChests[m] + avgTiers[m]);
    std::vector<double> sorted = perMinute;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2] / (double)seeds;
    double worstMinute = 1e9, bestMinute = 0.0;
    int worstMinuteIndex = 0;
    for (int m = 0; m < minutes - 1; ++m) {
        const double rewards = perMinute[m] / (double)seeds;
        if (rewards < worstMinute) { worstMinute = rewards; worstMinuteIndex = m; }
        bestMinute = std::max(bestMinute, rewards);
    }
    printf("награды в минуту: медиана %.2f, худшая минута %.2f (%d-я), лучшая %.2f\n",
           median, worstMinute, worstMinuteIndex, bestMinute);
    if (worstMinute < 0.5) {
        printf("ПРОВИСАНИЕ: %d-я минута дала %.2f награды — меньше одной\n", worstMinuteIndex, worstMinute);
        ++failures;
    }
    if (median > 0.0 && bestMinute > 2.5 * median) {
        printf("СКАЧОК: лучшая минута дала %.2f награды при медиане %.2f (в %.1f раза)\n",
               bestMinute, median, bestMinute / median);
        ++failures;
    }
    const double droughtLimit = std::min(120.0, std::max(45.0, seconds * 0.25));
    if (worstDrought > droughtLimit) {
        printf("ПРОВИСАНИЕ: %.0f с без единой награды — дольше %.0f с\n", worstDrought, droughtLimit);
        ++failures;
    }
    if (spread > 2.0) {
        printf("СКАЧОК МЕЖДУ ЗАБЕГАМИ: золото различается в %.2f раза\n", spread);
        ++failures;
    }
    if (levelsPerRun < seconds / 60.0 * 0.8) {
        printf("МАЛО: %.1f уровней за забег — меньше одного в минуту\n", levelsPerRun);
        ++failures;
    }
    if (failures == 0) printf("ОК: темп ровный — ни скачков, ни провисаний\n");
    return failures == 0 ? 0 : 1;
}
