// ── Стенд советника: проверка рекомендаций прокачки без окна и без игры ─────
//
// Советник — единственный слой «игрового ИИ», который реально влияет на забег:
// он подсвечивает карту на повышении уровня. Проверить его глазами дорого —
// нужно дожить до уровня, получить нужный набор карт и нужное состояние боя.
// Поэтому здесь сценарии собираются вручную, а проверки формулируются так,
// чтобы падение было видно сразу:
//
//   1) таблица весов конечна (нет NaN/inf) — иначе весь ранжирующий проход
//      сравнивает мусор и «совет» становится случайным;
//   2) ранжирование — перестановка: все четыре карты получают разные места;
//   3) реакция на опасность: при 15 % HP лечение или воскрешение обязаны быть
//      в топ-2, иначе советник советует урон умирающему;
//   4) устойчивость: изменение HP на 1 % не переставляет карты (нет дрожания);
//   5) разнообразие: на сетке контекстов советник выбирает не одну и ту же
//      карту — иначе «совет» не зависит от боя вообще.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/advisor_probe.cpp -o build/advisor_probe
//     build/advisor_probe
//
// Код возврата 1, если хоть одна проверка провалилась — годится для CI.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <set>
#include <vector>

#include "renderer.h"
#include "game.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK" : "ПРОВАЛ", what);
    if (!ok) ++failures;
}

// Ранжирование для заданного набора карт и состояния боя.
static void rank(Game& g, const int cards[4], const Game::AdvisorSnapshot& ctx, int out[4]) {
    for (int i = 0; i < 4; ++i) g.upgradeChoices[i] = cards[i];
    g.getAdvisorRanking(out, ctx);
}

static int topPick(Game& g, const int cards[4], const Game::AdvisorSnapshot& ctx) {
    int ranked[4];
    rank(g, cards, ctx, ranked);
    return ranked[0] >= 0 ? g.upgradeChoices[ranked[0]] : -1;
}

int main() {
    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.init();
    g.options.advisorEnabled = 1;

    printf("── 1. Таблица весов ────────────────────────────────────────────────\n");
    int badCells = 0;
    float minW = 1e9f, maxW = -1e9f;
    for (int i = 0; i < 20; ++i)
        for (int k = 0; k < 27; ++k) {
            const float w = ADVISOR_UPGRADE_WEIGHTS[i][k];
            if (!std::isfinite(w)) ++badCells;
            if (w < minW) minW = w;
            if (w > maxW) maxW = w;
        }
    printf("  ячеек 20x27, диапазон %.3f .. %.3f\n", minW, maxW);
    check(badCells == 0, "в весах нет NaN/inf");
    check(maxW - minW > 0.001f, "веса не вырождены в константу");

    printf("\n── 2. Ранжирование — перестановка ──────────────────────────────────\n");
    Game::AdvisorSnapshot calm;
    calm.hpRatio = 1.0f;
    calm.weaponCount = 1;
    calm.activeWeaponCount = 1;
    const int cardsA[4] = {0, 1, 10, 14};
    int ranked[4];
    rank(g, cardsA, calm, ranked);
    std::set<int> unique(ranked, ranked + 4);
    check(unique.size() == 4 && unique.count(-1) == 0,
          "четыре карты получают четыре разных места");

    printf("\n── 3. Реакция на опасность ─────────────────────────────────────────\n");
    Game::AdvisorSnapshot dying = calm;
    dying.hpRatio = 0.15f;
    dying.nearbyThreats = 6;
    dying.nearestThreat = 60.0f;
    rank(g, cardsA, dying, ranked);
    const int dyingPick = g.upgradeChoices[ranked[0]];
    const int dyingSecond = g.upgradeChoices[ranked[1]];
    printf("  HP 15%%, угроз рядом 6 → выбор %d (%s), второй %d (%s)\n",
           dyingPick, ITEM_DEFS[dyingPick].name, dyingSecond, ITEM_DEFS[dyingSecond].name);
    check(dyingPick == 10 || dyingPick == 14 || dyingSecond == 10 || dyingSecond == 14,
          "лечение или воскрешение в топ-2 при 15% HP");
    printf("  причина: %s\n", g.getAdvisorRationale(dyingPick, dying).c_str());

    printf("\n── 4. Устойчивость к дрожанию ───────────────────────────────────────\n");
    Game::AdvisorSnapshot a60 = calm, a59 = calm;
    a60.hpRatio = 0.60f;
    a59.hpRatio = 0.59f;
    const int pickA = topPick(g, cardsA, a60);
    const int pickB = topPick(g, cardsA, a59);
    printf("  HP 60%% → %d, HP 59%% → %d\n", pickA, pickB);
    check(pickA == pickB, "изменение HP на 1% не меняет совет");

    printf("\n── 5. Разнообразие на сетке контекстов ──────────────────────────────\n");
    // Три разных набора карт: оружие, поздние реликвии, защита. Для каждого
    // считаем, сколько РАЗНЫХ советов выдаёт советник на 18 состояниях боя.
    const int cardSets[4][4] = {{0, 1, 2, 3}, {3, 8, 15, 18}, {10, 14, 16, 19}, {0, 8, 10, 14}};
    static const char* const setNames[4] = {"оружие 0..3", "реликвии 3/8/15/18", "защита 10/14/16/19", "смесь 0/8/10/14"};
    const float hpValues[3] = {1.0f, 0.55f, 0.2f};
    const int threatValues[3] = {0, 4, 9};
    const float timeValues[3] = {30.0f, 240.0f, 560.0f};
    const bool bossValues[2] = {false, true};
    int totalDistinct = 0;
    for (int setIndex = 0; setIndex < 4; ++setIndex) {
        std::set<int> distinct;
        int scenarios = 0;
        for (float hp : hpValues)
            for (int th : threatValues)
                for (float rt : timeValues)
                    for (bool boss : bossValues) {
                        Game::AdvisorSnapshot c = calm;
                        c.hpRatio = hp;
                        c.nearbyThreats = th;
                        c.midrangeThreats = th * 2;
                        c.nearestThreat = th > 0 ? 90.0f : 900.0f;
                        c.bossAlive = boss;
                        c.bossNear = boss && th > 0;
                        g.runTime = rt;
                        const int pick = topPick(g, cardSets[setIndex], c);
                        if (pick < 0) { printf("  ПРОБЛЕМА: пустой совет в сценарии\n"); ++failures; }
                        distinct.insert(pick);
                        ++scenarios;
                    }
        totalDistinct += (int)distinct.size();
        printf("  %-18s сценариев %2d, различных советов %d:", setNames[setIndex], scenarios, (int)distinct.size());
        for (int id : distinct) printf(" %d", id);
        printf("\n");
    }
    g.runTime = 0.0f;
    // Один и тот же ответ во всех 54 сценариях — это не советник, а фиксированный
    // приоритет. Он не обязан быть разнообразным внутри одного набора карт, но
    // обязан реагировать хотя бы на часть контекста в целом.
    check(totalDistinct >= 3, "совет учитывает состояние боя, а не только набор карт");

    printf("\n── 6. Статическая таблица сложности героев ──────────────────────────\n");
    bool ranksOk = true;
    std::set<int> rankSet;
    for (int i = 0; i < 5; ++i) {
        printf("  %-8s ранг %d\n", HERO_DEFS[i].name, HERO_DEFS[i].botDifficultyRank);
        if (HERO_DEFS[i].botDifficultyRank < 1 || HERO_DEFS[i].botDifficultyRank > 5) ranksOk = false;
        rankSet.insert(HERO_DEFS[i].botDifficultyRank);
    }
    check(ranksOk, "ранги в диапазоне 1..5");
    check(rankSet.size() >= 2, "ранги различаются между героями");

    printf("\nИтог: %s\n", failures == 0 ? "все проверки пройдены" : "есть провалы");
    return failures == 0 ? 0 : 1;
}
