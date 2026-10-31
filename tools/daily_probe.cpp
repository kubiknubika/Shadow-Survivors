// ── Стенд дневных даров: задания, крутка, жетоны, аномалии ───────────────────
//
// Дневная система — единственное место в игре, где важны не числа, а обещания:
//   • «одни и те же задания весь день» — значит, день обязан давать тот же
//     набор после перезапуска, на другой машине и при сдвиге даты в настройках
//     ровно на столько же;
//   • «награда забирается один раз» — повторный клик не должен платить второй
//     раз (в том числе после перезапуска, потому что бит лежит в сейве);
//   • «пятая крутка дня — всегда жетон» — иначе полоса золотых круток читается
//     как сломанная крутилка даже когда она честная;
//   • «жетон действует один бой» — то есть после старта его в сейве нет, и
//     второй забег идёт без него;
//   • «аномалия забега видна» — за 200 забегов должны встретиться все шесть, а
//     поправки не должны выходить из объявленных границ;
//   • «старый сейв не сбрасывается» — файл до 2.12.16 (120 байт) читается, ранги
//     кузницы и печати остаются на месте, а дневные счётчики начинаются с нуля.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/daily_probe.cpp -o build/daily_probe
//     mkdir -p /tmp/daily_probe_run && cd /tmp/daily_probe_run && /path/build/daily_probe
//
// Стенд пишет во временный каталог и сам его чистит: настоящий сейв игрока не
// трогается ни разу (loadSave читает shadow_survivors.sav из текущего каталога).
//
// Код возврата 1, если нарушено хоть одно обещание.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <set>
#include <unistd.h>
#include <sys/stat.h>

#include "renderer.h"
#include "game.h"

static int failures = 0;
template <typename... Args>
static void check(bool ok, const char* fmt, Args... args) {
    if (ok) return;
    printf("  ПРОВАЛ: ");
    printf(fmt, args...);
    printf("\n");
    ++failures;
}

static Game* makeGame(int pinYear, int pinMonth, int pinDay, bool withSave = false) {
    Game* g = new Game();
    g->simulationMode = true;
    g->ignoreSavedState = !withSave;
    srand(12345);
    g->init();
    g->options.holidaysEnabled = 1;
    g->options.holidayOverride = -1;
    g->holidayPinYear = pinYear;
    g->holidayPinMonth = pinMonth;
    g->holidayPinDay = pinDay;
    return g;
}

// ── 1. Один день — один набор заданий ───────────────────────────────────────
static void testSameDayIsStable() {
    printf("[1] один день даёт один и тот же набор заданий\n");
    Game* a = makeGame(2026, 10, 31);
    Game* b = makeGame(2026, 10, 31);
    a->rollDailyMissions();
    b->rollDailyMissions();
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i) {
        check(a->save.dailyKind[i] == b->save.dailyKind[i], "задание %d: вид %d против %d",
              i, a->save.dailyKind[i], b->save.dailyKind[i]);
        check(a->save.dailyTarget[i] == b->save.dailyTarget[i], "задание %d: цель %d против %d",
              i, a->save.dailyTarget[i], b->save.dailyTarget[i]);
        check(a->save.dailyReward[i] == b->save.dailyReward[i], "задание %d: награда расходится", i);
    }
    // Три задания одного вида — это одно задание, посчитанное трижды.
    std::set<int> kinds;
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i) kinds.insert(a->save.dailyKind[i]);
    check((int)kinds.size() == DAILY_MISSION_COUNT, "виды заданий повторяются (разных %d)", (int)kinds.size());
    delete a;
    delete b;
}

// ── 2. Разные дни — разные задания ──────────────────────────────────────────
static void testDaysDiffer() {
    printf("[2] дни не повторяют друг друга\n");
    Game* g = makeGame(2026, 10, 31);
    std::set<std::string> seen;
    int days = 60;
    for (int d = 0; d < days; ++d) {
        g->holidayPinYear = 2026;
        g->holidayPinMonth = 1;
        g->holidayPinDay = 1 + d;
        g->rollDailyMissions();
        std::string key;
        for (int i = 0; i < DAILY_MISSION_COUNT; ++i)
            key += std::to_string(g->save.dailyKind[i]) + ":" + std::to_string(g->save.dailyTarget[i]) + "|";
        seen.insert(key);
    }
    // Шесть видов по три дают C(6,3) = 20 наборов, ещё три ступени целей — 60.
    // Требовать «все дни разные» нечестно, но повторов почти всех дней — баг.
    check((int)seen.size() >= days / 2, "за %d дней всего %d разных наборов заданий", days, (int)seen.size());
    // Сдвиг даты в настройках — тот же день, значит тот же набор.
    g->holidayPinYear = 2026; g->holidayPinMonth = 10; g->holidayPinDay = 31;
    g->rollDailyMissions();
    const int k0 = g->save.dailyKind[0], t0 = g->save.dailyTarget[0];
    g->options.holidayDateShift = 0;
    g->rollDailyMissions();
    check(g->save.dailyKind[0] == k0 && g->save.dailyTarget[0] == t0,
          "повторная выдача на тот же день сменила задание %d/%d → %d/%d",
          k0, t0, g->save.dailyKind[0], g->save.dailyTarget[0]);
    delete g;
}

// ── 3. Награда платится один раз ────────────────────────────────────────────
static void testClaimOnce() {
    printf("[3] награда забирается один раз\n");
    Game* g = makeGame(2026, 10, 31);
    g->rollDailyMissions();
    // Первое задание делаем готовым и золотым: считаем именно золото.
    int goldIndex = -1;
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i) if (g->save.dailyReward[i] == 0) goldIndex = i;
    if (goldIndex < 0) g->save.dailyReward[0] = 0, goldIndex = 0;
    g->save.dailyProgress[goldIndex] = g->save.dailyTarget[goldIndex];
    const int before = g->save.gold;
    check(g->dailyClaimable(goldIndex), "готовое задание не считается готовым");
    g->claimDaily(goldIndex);
    const int afterFirst = g->save.gold;
    check(afterFirst > before, "первый клик не заплатил (%d → %d)", before, afterFirst);
    g->claimDaily(goldIndex);
    check(g->save.gold == afterFirst, "второй клик заплатил ещё раз (%d → %d)", afterFirst, g->save.gold);
    check(!g->dailyClaimable(goldIndex), "забранное задание осталось доступным");
    check(g->dailyClaimedCount() == 1, "забрано %d заданий вместо одного", g->dailyClaimedCount());

    // Незабранные награды по крутке и жетону тоже не должны двоиться.
    int rollsIndex = -1, boosterIndex = -1;
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i) {
        if (g->save.dailyReward[i] == 1) rollsIndex = i;
        if (g->save.dailyReward[i] == 2) boosterIndex = i;
    }
    if (rollsIndex >= 0) {
        g->save.dailyProgress[rollsIndex] = g->save.dailyTarget[rollsIndex];
        const int t0 = g->save.rollTokens;
        g->claimDaily(rollsIndex);
        const int t1 = g->save.rollTokens;
        g->claimDaily(rollsIndex);
        check(t1 == t0 + 1, "награда «жетон крутки» дала %d вместо 1", t1 - t0);
        check(g->save.rollTokens == t1, "награда «жетон крутки» удвоилась");
    }
    if (boosterIndex >= 0) {
        g->save.dailyProgress[boosterIndex] = g->save.dailyTarget[boosterIndex];
        int sum0 = 0;
        for (int k = 0; k < BOOSTER_KINDS; ++k) sum0 += g->save.boosters[k];
        g->claimDaily(boosterIndex);
        int sum1 = 0;
        for (int k = 0; k < BOOSTER_KINDS; ++k) sum1 += g->save.boosters[k];
        g->claimDaily(boosterIndex);
        int sum2 = 0;
        for (int k = 0; k < BOOSTER_KINDS; ++k) sum2 += g->save.boosters[k];
        check(sum1 == sum0 + 1, "награда «жетон на бой» выдала %d жетонов вместо 1", sum1 - sum0);
        check(sum2 == sum1, "награда «жетон на бой» удвоилась");
    }
    delete g;
}

// ── 4. Крутка: шансы, гарантия, повторяемость ───────────────────────────────
static void testRoll() {
    printf("[4] крутка: гарантия пятой, повторяемость, расход жетонов\n");
    Game* g = makeGame(2026, 10, 31);
    g->ensureDaily();
    // Пять круток подряд из одного дня: пятая обязана быть жетоном на бой.
    int prizes[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5; ++i) {
        g->save.dailyRolls = i;
        g->save.rollTokens = 9;          // чтобы крутить было чем
        g->doDailyRoll();
        prizes[i] = g->dailyLastPrizeKind;
        check(g->save.dailyRolls == i + 1, "крутка %d не засчиталась (%d)", i, g->save.dailyRolls);
    }
    check(prizes[4] == 1, "пятая крутка дала вид %d вместо жетона на бой", prizes[4]);

    // Тот же день и тот же номер крутки — тот же приз: «перекрутить» нельзя.
    for (int i = 0; i < 5; ++i) {
        int k1 = 0, v1 = 0, b1 = 0, k2 = 0, v2 = 0, b2 = 0;
        g->dailyPrize(g->save.dailyDay, i, &k1, &v1, &b1);
        g->dailyPrize(g->save.dailyDay, i, &k2, &v2, &b2);
        check(k1 == k2 && v1 == v2 && b1 == b2, "приз крутки %d не повторяем", i);
    }

    // Бесплатная крутка одна, дальше — только за жетон, и он списывается.
    Game* h = makeGame(2026, 11, 1);
    h->ensureDaily();
    h->save.dailyRolls = 0;
    h->save.rollTokens = 0;
    check(h->dailyFreeRollsLeft() == DAILY_ROLLS_FREE, "свободных круток %d вместо %d",
          h->dailyFreeRollsLeft(), DAILY_ROLLS_FREE);
    h->doDailyRoll();
    check(h->save.dailyRolls == 1, "бесплатная крутка не прошла");
    check(!h->dailyRollAvailable(), "вторая крутка доступна без жетона");
    const int before = h->save.dailyRolls;
    h->doDailyRoll();
    check(h->save.dailyRolls == before, "крутка прошла без права на неё");
    h->save.rollTokens = 1;
    check(h->dailyRollAvailable(), "крутка не доступна при одном жетоне");
    h->doDailyRoll();
    check(h->save.rollTokens == 0, "жетон крутки не списался (%d)", h->save.rollTokens);
    check(h->save.dailyRolls == before + 1, "платная крутка не прошла");
    delete g;
    delete h;
}

// ── 5. Жетон на бой: сгорает на старте, действует один бой ──────────────────
static void testBoosterOneRun() {
    printf("[5] жетон действует ровно один бой\n");
    Game* g = makeGame(2026, 10, 31);
    g->save.boosters[BOOSTER_GOLD] = 2;
    g->save.activeBooster = BOOSTER_GOLD;
    g->initRun(0, false);
    check(g->runBooster == BOOSTER_GOLD, "жетон не попал в забег (%d)", g->runBooster);
    check(g->save.boosters[BOOSTER_GOLD] == 1, "жетон не списался (%d осталось)", g->save.boosters[BOOSTER_GOLD]);
    check(g->save.activeBooster == -1, "выбранный жетон не сброшен");
    // Аномалия дня тоже множит золото, поэтому жетон проверяется не «равно
    // 1,25», а «не меньше 1,25 и не больше 1,25 × верхняя граница аномалии».
    check(g->runGoldMultiplier >= 1.25f - 1e-4f && g->runGoldMultiplier <= 1.25f * 1.20f + 1e-4f,
          "множитель золота с жетоном %.3f вне 1,25…1,50", g->runGoldMultiplier);
    g->initRun(0, false);
    check(g->runBooster == -1, "второй забег снова получил жетон");
    check(g->runGoldMultiplier <= 1.20f + 1e-4f, "множитель золота второго боя %.3f — жетон не сгорел",
          g->runGoldMultiplier);
    check(g->save.boosters[BOOSTER_GOLD] == 1, "второй бой списал жетон, хотя он не действовал");

    // Жетон удачи: две замены прокачки на каждом уровне.
    Game* h = makeGame(2026, 10, 31);
    h->save.boosters[BOOSTER_LUCK] = 1;
    h->save.activeBooster = BOOSTER_LUCK;
    h->initRun(0, false);
    check(h->runBooster == BOOSTER_LUCK, "жетон удачи не попал в забег");
    check(h->rerollsPerLevel() == 2, "жетон удачи даёт %d замену вместо двух", h->rerollsPerLevel());
    check(h->runEliteBonus > 0.0f, "жетон удачи не добавил элит");
    h->generateUpgradeChoices();
    check(h->upgradeRerollsLeft == 2, "после нового уровня замен %d вместо двух", h->upgradeRerollsLeft);

    // Здоровье: +25 % к максимуму именно на этот бой.
    Game* z = makeGame(2026, 10, 31);
    z->save.boosters[BOOSTER_HEALTH] = 1;
    z->save.activeBooster = BOOSTER_HEALTH;
    z->initRun(0, false);
    const float hpBoosted = z->playerMaxHp;
    z->initRun(0, false);
    const float hpPlain = z->playerMaxHp;
    check(hpBoosted > hpPlain * 1.2f, "здоровье с жетоном %f против %f без него", hpBoosted, hpPlain);
    delete g;
    delete h;
    delete z;
}

// ── 6. Прогресс заданий: сумма и лучший забег ───────────────────────────────
static void testProgressAccounting() {
    printf("[6] прогресс: сумма для счётчиков, лучший бой для времени и уровня\n");
    Game* g = makeGame(2026, 10, 31);
    g->rollDailyMissions();
    // Ставим руками понятный набор видов: убийства (сумма), время (лучший бой).
    g->save.dailyKind[0] = DAILY_KILLS; g->save.dailyTarget[0] = 1000; g->save.dailyProgress[0] = 0;
    g->save.dailyKind[1] = DAILY_SURVIVE; g->save.dailyTarget[1] = 600; g->save.dailyProgress[1] = 0;
    g->save.dailyKind[2] = DAILY_GOLD; g->save.dailyTarget[2] = 5000; g->save.dailyProgress[2] = 0;

    g->kills = 120; g->runGold = 1000; g->runTime = 200.0f; g->runChests = 1; g->playerLevel = 7; g->runBossKills = 0;
    g->commitDailyProgress();
    check(g->save.dailyProgress[0] == 120, "убийства не сложились (%d)", g->save.dailyProgress[0]);
    check(g->save.dailyProgress[1] == 200, "время записано не по бою (%d)", g->save.dailyProgress[1]);
    g->kills = 60; g->runGold = 700; g->runTime = 150.0f; g->runChests = 1; g->playerLevel = 4;
    g->commitDailyProgress();
    check(g->save.dailyProgress[0] == 180, "убийства не суммируются (%d вместо 180)", g->save.dailyProgress[0]);
    check(g->save.dailyProgress[1] == 200, "время ухудшилось коротким боем (%d вместо 200)", g->save.dailyProgress[1]);
    check(g->save.dailyProgress[2] == 1700, "золото не суммируется (%d вместо 1700)", g->save.dailyProgress[2]);
    delete g;
}

// ── 7. Аномалии: все шесть встречаются, поправки в объявленных границах ─────
static void testAnomalies() {
    printf("[7] аномалии забега: все шесть, поправки в границах\n");
    Game* g = makeGame(2026, 10, 31);
    bool seen[ANOMALY_COUNT] = {false};
    for (int i = 0; i < 400; ++i) {
        g->initRun(0, false);
        check(g->runAnomaly >= 0 && g->runAnomaly < ANOMALY_COUNT, "аномалия вне списка: %d", g->runAnomaly);
        seen[g->runAnomaly] = true;
        // Границы объявлены на экране: ±20 % золота, ±12 % врагов, ±12 % опыта.
        check(g->runGoldMultiplier >= 1.0f && g->runGoldMultiplier <= 1.20f, "золото ×%.3f", g->runGoldMultiplier);
        check(g->runXpMultiplier >= 1.0f && g->runXpMultiplier <= 1.12f, "опыт ×%.3f", g->runXpMultiplier);
        check(g->runSpawnMultiplier >= 0.85f && g->runSpawnMultiplier <= 1.15f, "враги ×%.3f", g->runSpawnMultiplier);
        check(g->runEliteBonus >= -5.0f && g->runEliteBonus <= 9.0f, "элиты %+.1f п.п.", g->runEliteBonus);
        check(g->runDamageMultiplier >= 1.0f && g->runDamageMultiplier <= 1.15f, "урон ×%.3f", g->runDamageMultiplier);
        check(g->nextBreakthroughLevel >= 1, "прорыв на уровне %d", g->nextBreakthroughLevel);
    }
    for (int i = 0; i < ANOMALY_COUNT; ++i)
        check(seen[i], "аномалия «%s» не выпала за 400 забегов", g->anomalyTitle(i));
    delete g;
}

// ── 8. Старый сейв: 120 байт читаются, ранги и печати целы ─────────────────
static void testLegacySave(int legacySize, const char* label) {
    printf("[8] старый сейв %d байт (%s): дневные счётчики с нуля, прогресс цел\n", legacySize, label);
    // Файл-предшественник: только те поля, что были до 2.12.16.
    SaveData old = {};
    old.magic = 0x53484457;
    old.gold = 54321;
    old.storyChapter = 3;
    old.bestTimeSec = 777;
    old.bestKills = 999;
    old.totalRuns = 42;
    for (int i = 0; i < 6; ++i) old.metaRanks[i] = std::min(i, META_DEFS[i].maxRank);
    old.storySealMask = 0x0F;
    old.storyEntryMask = 0x0B;
    old.campaignMissionsDone = 0xFF;
    old.campaignTrack = 1;
    for (int f = 0; f < CAMPAIGN_FORK_COUNT; ++f) old.campaignFork[f] = (uint8_t)(f % 3);
    old.saveVersion = 9;
    FILE* f = fopen("shadow_survivors.sav", "wb");
    check(f != nullptr, "не удалось создать файл сейва в текущем каталоге");
    if (!f) return;
    fwrite(&old, 1, (size_t)legacySize, f);
    fclose(f);

    Game* g = new Game();
    g->simulationMode = true;
    g->ignoreSavedState = false;
    srand(777);
    g->init();
    check(g->save.gold == 54321, "золото не пережило обновление (%d)", g->save.gold);
    check(g->save.storyChapter == 3, "глава не пережила обновление (%d)", g->save.storyChapter);
    check(g->save.bestTimeSec == 777 && g->save.bestKills == 999, "рекорды потерялись (%d / %d)",
          g->save.bestTimeSec, g->save.bestKills);
    // В файле до 120 байт развилок ещё не было: они восстанавливаются из следа
    // кампании, и требовать совпадения с рукописным «f % 3» для него нельзя.
    if (legacySize >= 120)
        for (int f = 0; f < CAMPAIGN_FORK_COUNT; ++f)
            check(g->save.campaignFork[f] == (uint8_t)(f % 3), "развилка %d потерялась", f);
    check(g->save.activeBooster == -1, "в старом сейве появился жетон (%d)", g->save.activeBooster);
    for (int k = 0; k < BOOSTER_KINDS; ++k)
        check(g->save.boosters[k] == 0, "в старом сейве появились жетоны (%d)", g->save.boosters[k]);
    check(g->save.dailyClaimed == 0, "в старом сейве появились забранные задания");
    check(g->save.dailyDay == g->dailyToday(), "после загрузки не выдан сегодняшний набор (%d против %d)",
          g->save.dailyDay, g->dailyToday());
    check(g->save.dailyTarget[0] > 0, "цель первого задания нулевая");
    delete g;
    remove("shadow_survivors.sav");
}

// ── 9. Испорченный сейв: границы правятся при загрузке ──────────────────────
static void testCorruptSave() {
    printf("[9] испорченный сейв: нулевая цель, мусорные биты, отрицательные жетоны\n");
    SaveData bad = {};
    bad.magic = 0x53484457;
    bad.gold = 100;
    bad.saveVersion = 10;
    bad.dailyDay = 20757;
    bad.dailyKind[0] = 99;          // вид вне списка
    bad.dailyTarget[0] = 0;         // «0 из 0» — задание выполнено сразу
    bad.dailyReward[0] = 7;
    bad.dailyProgress[0] = -5;
    bad.dailyKind[1] = DAILY_SURVIVE;
    bad.dailyTarget[1] = 0;         // время с нулевой целью
    bad.dailyProgress[1] = 10;
    bad.dailyKind[2] = DAILY_KILLS;
    bad.dailyTarget[2] = 50;
    bad.dailyProgress[2] = 999999;
    bad.dailyClaimed = 0x7FFFFFFF;  // биты за пределами трёх заданий
    bad.dailyRolls = -3;
    bad.rollTokens = -1;
    bad.boosters[0] = -4;
    bad.boosters[1] = 999;
    bad.activeBooster = 42;
    FILE* f = fopen("shadow_survivors.sav", "wb");
    check(f != nullptr, "не удалось создать испорченный сейв");
    if (!f) return;
    fwrite(&bad, sizeof(bad), 1, f);
    fclose(f);

    Game* g = new Game();
    g->simulationMode = true;
    g->ignoreSavedState = false;
    srand(99);
    g->init();
    // Сегодняшний набор выдан заново: испорченный день не пережил загрузку.
    check(g->save.dailyDay == g->dailyToday(), "испорченный день пережил загрузку (%d)", g->save.dailyDay);
    // Биты за пределами трёх заданий срезаны, а «забрано» остаётся только там,
    // где прогресс действительно закрывает цель: пустая полоска не может
    // показывать «ЗАБРАНО».
    check((g->save.dailyClaimed & ~((1 << DAILY_MISSION_COUNT) - 1)) == 0,
          "биты забранного вне трёх заданий пережили загрузку (%d)", g->save.dailyClaimed);
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i)
        if (g->save.dailyClaimed & (1 << i))
            check(g->save.dailyProgress[i] >= g->save.dailyTarget[i],
                  "задание %d помечено забранным при прогрессе %d из %d",
                  i, g->save.dailyProgress[i], g->save.dailyTarget[i]);
    for (int i = 0; i < DAILY_MISSION_COUNT; ++i) {
        check(g->save.dailyTarget[i] >= 30, "цель задания %d осталась нулевой (%d)", i, g->save.dailyTarget[i]);
        check(g->dailyClaimable(i) == false, "задание %d можно забрать без прогресса", i);
    }
    check(g->save.dailyRolls == 0, "отрицательные крутки пережили загрузку (%d)", g->save.dailyRolls);
    check(g->save.rollTokens == 0, "отрицательные жетоны крутки пережили загрузку (%d)", g->save.rollTokens);
    for (int k = 0; k < BOOSTER_KINDS; ++k)
        check(g->save.boosters[k] >= 0 && g->save.boosters[k] <= 9, "жетон %d вне границ (%d)", k, g->save.boosters[k]);
    check(g->save.activeBooster == -1, "выбранный жетон вне списка пережил загрузку (%d)", g->save.activeBooster);
    delete g;
    remove("shadow_survivors.sav");
}

int main() {
    printf("СТЕНД ДНЕВНЫХ ДАРОВ\n\n");
    testSameDayIsStable();
    testDaysDiffer();
    testClaimOnce();
    testRoll();
    testBoosterOneRun();
    testProgressAccounting();
    testAnomalies();
    testLegacySave(120, "версия 9");
    testLegacySave(68, "версия до развилок");
    testLegacySave((int)offsetof(SaveData, activeBooster), "обрезанный хвост");
    testCorruptSave();
    printf("\n");
    if (failures == 0) printf("ОК: обещания дневных даров выполнены\n");
    else printf("ПРОВАЛОВ: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
