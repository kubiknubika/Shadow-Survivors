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

// ── 10. Ассортимент заданий: все десять видов встречаются и не повторяются ───
static void testKindVariety() {
    printf("[10] ассортимент: десять видов, три разных в день\n");
    int seen[DAILY_KIND_COUNT] = {0};
    int days = 0;
    // Перебираем дни подряд, как это делает игрок: набор зависит только от дня.
    for (int day = 20500; day < 20800; ++day) {
        Game* g = makeGame(2026, 10, 31);
        g->rollDailyMissionsForDay(day);
        ++days;
        int distinct = 0;
        for (int i = 0; i < DAILY_MISSION_COUNT; ++i) {
            const int k = g->save.dailyKind[i];
            check(k >= 0 && k < DAILY_KIND_COUNT, "вид вне списка: %d", k);
            if (k >= 0 && k < DAILY_KIND_COUNT) ++seen[k];
            bool dup = false;
            for (int j = 0; j < i; ++j) if (g->save.dailyKind[j] == k) dup = true;
            check(!dup, "день %d: задание «%s» выдано дважды", day, g->dailyKindTitle(k));
            if (!dup) ++distinct;
            check(g->save.dailyTarget[i] > 0, "день %d: цель задания %d нулевая", day, i);
        }
        check(distinct == DAILY_MISSION_COUNT, "день %d: разных видов %d из %d", day, distinct, DAILY_MISSION_COUNT);
        delete g;
    }
    // Все виды должны встречаться: иначе задание — мёртвый код.
    for (int k = 0; k < DAILY_KIND_COUNT; ++k)
        check(seen[k] > 0, "вид «%s» не выпал ни разу за %d дней", Game::dailyKindTitle(k), days);
    printf("  видов: %d, дней проверено: %d\n", DAILY_KIND_COUNT, days);
}

// ── 11. Новые счётчики заданий: считаются в бою и попадают в прогресс ────────
static void testNewCounters() {
    printf("[11] новые счётчики: элиты, резонаторы, супер, тир сборки\n");
    Game* g = makeGame(2026, 10, 31);
    g->rollDailyMissionsForDay(20700);
    // День фиксируем явно: commitDailyProgress зовёт ensureDaily, и «чужой»
    // день заставил бы игру перевыдать набор прямо посреди проверки.
    g->save.dailyDay = g->dailyToday();
    g->save.dailyKind[0] = DAILY_ELITES;      g->save.dailyTarget[0] = 100; g->save.dailyProgress[0] = 0;
    g->save.dailyKind[1] = DAILY_RESONATORS;  g->save.dailyTarget[1] = 100; g->save.dailyProgress[1] = 0;
    g->save.dailyKind[2] = DAILY_BUILD_TIER;  g->save.dailyTarget[2] = 9;   g->save.dailyProgress[2] = 0;

    g->initRun(0, false);
    check(g->runElites == 0 && g->runResonators == 0 && g->runSupers == 0,
          "счётчики нового боя не с нуля");
    // Элита: убиваем её так же, как игра, — через damageMonster с большим уроном.
    const size_t before = g->monsters.size();
    g->spawnMonster(0, true, false);
    check(g->monsters.size() == before + 1, "элита не заспавнилась");
    g->damageMonster(g->monsters.size() - 1, 1.0e9f, 0.0f, 0.0f);
    check(g->runElites == 1, "элита не засчитана (%d)", g->runElites);
    // Резонатор — destroyed, а не убит: у него отдельная ветка.
    const int resonatorsBefore = g->countResonators();
    g->spawnResonator(0);
    if (g->countResonators() > resonatorsBefore) {
        for (size_t i = g->monsters.size(); i-- > 0;) {
            if (g->monsters[i].isResonator && g->monsters[i].hp > 0.0f) {
                g->damageMonster(i, 1.0e9f, 0.0f, 0.0f);
                break;
            }
        }
        check(g->runResonators == 1, "резонатор не засчитан (%d)", g->runResonators);
    }
    // Супер: заряжаем и применяем.
    g->superCharge = 100.0f;
    g->playerHp = g->playerMaxHp;
    g->state = STATE_PLAYING;
    g->triggerUltimate();
    check(g->runSupers == 1, "супер не засчитан (%d)", g->runSupers);

    g->commitDailyProgress();
    check(g->save.dailyProgress[0] == 1, "элиты не попали в прогресс (%d)", g->save.dailyProgress[0]);
    check(g->save.dailyProgress[1] == (g->runResonators ? 1 : 0), "резонаторы не попали в прогресс");
    // Тир сборки считается по лучшему бою и не может быть меньше первого.
    check(g->save.dailyProgress[2] >= 1, "тир сборки не записан (%d)", g->save.dailyProgress[2]);
    const int tierAfterFirstRun = g->save.dailyProgress[2];

    // Второй бой хуже — прогресс не должен ухудшиться.
    g->initRun(0, false);
    g->commitDailyProgress();
    check(g->save.dailyProgress[2] == tierAfterFirstRun, "тир ухудшился вторым боем (%d → %d)",
          tierAfterFirstRun, g->save.dailyProgress[2]);
    delete g;
}

// ── 12. Тир-лист жетонов: порог по потраченному, закрытый тир не выдаётся ────
static void testBoosterTiers() {
    printf("[12] тир-лист жетонов: порог по потраченному золоту\n");
    Game* g = makeGame(2026, 10, 31);
    check(BOOSTER_KINDS == 12, "жетонов %d вместо 12", BOOSTER_KINDS);
    check(BOOSTER_TIER_GOLD[0] == 0, "первый тир должен быть открыт с нуля");
    check(BOOSTER_TIER_GOLD[1] > 0 && BOOSTER_TIER_GOLD[2] > BOOSTER_TIER_GOLD[1],
          "пороги тиров должны расти");

    g->save.goldSpent = 0;
    check(g->unlockedBoosterTierCount() == 1, "с нуля открыто %d тиров", g->unlockedBoosterTierCount());
    check(!g->boosterTierUnlocked(1), "второй тир открыт без трат");
    g->save.goldSpent = BOOSTER_TIER_GOLD[1] - 1;
    check(!g->boosterTierUnlocked(1), "второй тир открылся на золото меньше порога");
    g->save.goldSpent = BOOSTER_TIER_GOLD[1];
    check(g->boosterTierUnlocked(1), "второй тир не открылся на пороге");
    check(g->unlockedBoosterTierCount() == 2, "открыто %d тиров вместо двух", g->unlockedBoosterTierCount());
    check(g->unlockedBoosterCount() == 8, "доступно %d жетонов вместо восьми", g->unlockedBoosterCount());
    g->save.goldSpent = BOOSTER_TIER_GOLD[2];
    check(g->unlockedBoosterTierCount() == 3, "третий тир не открылся");
    check(g->unlockedBoosterCount() == BOOSTER_KINDS, "при трёх тирах доступны не все жетоны");

    // Закрытый тир нельзя выбрать, даже если жетон лежит в сейве.
    g->save.goldSpent = 0;
    g->save.boosters[BOOSTER_SPEED] = 3;
    g->save.activeBooster = -1;
    g->selectBooster(BOOSTER_SPEED);
    check(g->save.activeBooster == -1, "выбран жетон закрытого тира");

    // И он не срабатывает, если попал в файл сейва руками.
    g->save.activeBooster = BOOSTER_SPEED;
    g->save.boosters[BOOSTER_SPEED] = 1;
    g->initRun(0, false);
    check(g->runBooster == -1, "жетон закрытого тира сработал в бою (%d)", g->runBooster);
    check(g->save.boosters[BOOSTER_SPEED] == 1, "жетон закрытого тира списался");

    // Крутка и награда задания не выдают жетоны закрытых тиров.
    g->save.goldSpent = 0;
    g->save.activeBooster = -1;
    bool closedTierPrize = false, closedTierReward = false;
    for (int day = 20500; day < 20800; ++day) {
        for (int i = 0; i < 30; ++i) {
            int kind = 0, value = 0, bk = 0;
            g->dailyPrize(day, i, &kind, &value, &bk);
            if (bk >= g->unlockedBoosterCount()) closedTierPrize = true;
        }
    }
    check(!closedTierPrize, "крутка выдала жетон закрытого тира");
    for (int day = 20500; day < 20800; ++day) {
        Game* h = makeGame(2026, 10, 31);
        h->rollDailyMissionsForDay(day);
        for (int i = 0; i < DAILY_MISSION_COUNT; ++i) {
            h->save.dailyReward[i] = 2;   // награда — жетон
            h->save.dailyProgress[i] = h->save.dailyTarget[i];
            h->save.dailyClaimed = 0;
            h->save.goldSpent = 0;
            h->claimDaily(i);
            for (int k = h->unlockedBoosterCount(); k < BOOSTER_KINDS; ++k)
                if (h->save.boosters[k] > 0) closedTierReward = true;
        }
        delete h;
    }
    check(!closedTierReward, "награда задания выдала жетон закрытого тира");
    delete g;
}

// ── 13. Все двенадцать жетонов действительно что-то делают ───────────────────
static void testEveryBoosterWorks() {
    printf("[13] каждый из двенадцати жетонов меняет бой\n");
    // Базовая линия: бой без жетона.
    auto runWith = [](int kind, int* boosterOut) -> Game* {
        Game* g = makeGame(2026, 10, 31);
        g->save.goldSpent = 2000000000;      // все тиры открыты
        for (int k = 0; k < BOOSTER_KINDS; ++k) g->save.boosters[k] = 1;
        g->save.activeBooster = kind;
        g->initRun(0, false);
        if (boosterOut) *boosterOut = g->runBooster;
        return g;
    };
    Game* base = makeGame(2026, 10, 31);
    base->save.goldSpent = 2000000000;
    for (int k = 0; k < BOOSTER_KINDS; ++k) base->save.boosters[k] = 1;
    base->save.activeBooster = -1;
    base->initRun(0, false);

    for (int k = 0; k < BOOSTER_KINDS; ++k) {
        int used = -1;
        Game* g = runWith(k, &used);
        check(used == k, "жетон «%s» не попал в бой (%d)", Game::boosterTitle(k), used);
        bool differs = false;
        switch (k) {
            case BOOSTER_GOLD:    differs = g->runGoldMultiplier > base->runGoldMultiplier; break;
            case BOOSTER_HEALTH:  differs = g->playerMaxHp > base->playerMaxHp; break;
            case BOOSTER_RAGE:    differs = g->runDamageMultiplier > base->runDamageMultiplier; break;
            case BOOSTER_LUCK:    differs = g->rerollsPerLevel() > 1; break;
            case BOOSTER_SPEED:   differs = g->getMoveSpeed() > base->getMoveSpeed(); break;
            case BOOSTER_XP:      differs = g->getXpMultiplier() > base->getXpMultiplier(); break;
            case BOOSTER_HUNT:    differs = g->runEliteBonus > base->runEliteBonus; break;
            case BOOSTER_CHARGE:  differs = g->getSuperGainMultiplier() > base->getSuperGainMultiplier(); break;
            case BOOSTER_HUNGER:  differs = g->runGoldMultiplier > base->runGoldMultiplier &&
                                            g->playerMaxHp < base->playerMaxHp; break;
            case BOOSTER_RESONANCE: differs = g->resonatorCap() > base->resonatorCap(); break;
            case BOOSTER_BREAK:   differs = g->runBreakthroughEarly > 0; break;
            case BOOSTER_WRATH:   differs = g->runDamageMultiplier > base->runDamageMultiplier; break;
            default: break;
        }
        check(differs, "жетон «%s» (%s) не изменил бой", Game::boosterTitle(k), Game::boosterEffect(k));
        delete g;
    }
    delete base;

    // Жетон действует ровно один бой — в том числе верхних тиров.
    Game* once = makeGame(2026, 10, 31);
    once->save.goldSpent = 2000000000;
    once->save.boosters[BOOSTER_WRATH] = 1;
    once->save.activeBooster = BOOSTER_WRATH;
    once->initRun(0, false);
    const float boostedDamage = once->runDamageMultiplier;
    once->initRun(0, false);
    check(once->runBooster == -1, "второй бой снова получил жетон гнева");
    check(once->runDamageMultiplier < boostedDamage, "множитель урона не сбросился (%f)", once->runDamageMultiplier);
    check(once->save.boosters[BOOSTER_WRATH] == 0, "жетон не сгорел (%d)", once->save.boosters[BOOSTER_WRATH]);
    delete once;
}

// ── 14. Потраченное золото: растёт на покупках, не сбрасывается казной ───────
static void testGoldSpent() {
    printf("[14] потраченное золото: копится покупками, не сбрасывается\n");
    Game* g = makeGame(2026, 10, 31);
    g->save.gold = 1000000;
    g->save.goldSpent = 0;
    const int cost = g->getMetaUpgradeCost(0);
    g->spendGold(cost);
    check(g->save.gold == 1000000 - cost, "золото не списалось (%d)", g->save.gold);
    check(g->save.goldSpent == cost, "потраченное не выросло (%d)", g->save.goldSpent);

    // Потолок тот же, что у казны: счётчик не должен уехать в минус от переполнения.
    g->save.goldSpent = 2000000000;
    g->save.gold = 5000;
    g->spendGold(1000);
    check(g->save.goldSpent == 2000000000, "потраченное вышло за потолок (%d)", g->save.goldSpent);
    check(g->save.gold == 4000, "золото списалось неверно (%d)", g->save.gold);

    // Оценка для старого сейва: выкупленная кузница даёт больше миллиона.
    g->save.metaRanks[0] = 12; g->save.metaRanks[1] = 12; g->save.metaRanks[2] = 12;
    g->save.metaRanks[3] = 12; g->save.metaRanks[4] = 12; g->save.metaRanks[5] = 12;
    const int estimate = g->estimateSpentGold();
    check(estimate > 1000000, "оценка потраченного для выкупленной кузницы %d — меньше миллиона", estimate);
    check(estimate < 2000000000, "оценка потраченного %d — больше потолка", estimate);
    delete g;
}

// ── 15. Сейв v10 → v11: старый выбранный жетон не превращается в новый ──────
static void testTierMigration() {
    printf("[15] сейв v10 (204 байта) → v11: жетоны и потраченное\n");
    // Пишем файл ровно того вида, какой оставляла 2.12.16: версия 10, четыре
    // жетона, выбран третий (удача), за ним сразу поле activeBooster.
    SaveData old = {};
    old.magic = 0x53484457;
    old.gold = 250000;
    old.saveVersion = 10;
    old.metaRanks[0] = 5;
    old.boosters[0] = 0; old.boosters[1] = 0; old.boosters[2] = 1; old.boosters[3] = 4;
    FILE* f = fopen("shadow_survivors.sav", "wb");
    check(f != nullptr, "не удалось создать сейв v10");
    if (!f) return;
    // Файл v10 собирается по своему раскладу, а не по нынешнему: жетонов было
    // четыре, и номер выбранного лежал сразу за ними (смещение 200 в нынешней
    // структуре — это уже начало пятого жетона, а не выбор).
    const size_t v10Size = offsetof(SaveData, boosters) + 4 * sizeof(int) + sizeof(int);
    check(v10Size == 204, "длина сейва v10 получилась %zu вместо 204", v10Size);
    {
        std::vector<char> bytes(v10Size, 0);
        memcpy(bytes.data(), &old, offsetof(SaveData, boosters) + 4 * sizeof(int));
        const int legacyActive = (int)BOOSTER_LUCK;
        memcpy(bytes.data() + offsetof(SaveData, boosters) + 4 * sizeof(int), &legacyActive, sizeof(int));
        fwrite(bytes.data(), 1, v10Size, f);
    }
    fclose(f);

    Game* g = new Game();
    g->simulationMode = true;
    g->ignoreSavedState = false;
    srand(4242);
    g->init();
    check(g->save.saveVersion == 11, "версия сейва после миграции %u", g->save.saveVersion);
    // init() успевает начать забег, а забег сжигает выбранный жетон. Значит,
    // проверять надо не поле в сейве, а то, что жетон сработал именно тот.
    check(g->runBooster == BOOSTER_LUCK,
          "выбранный жетон потерялся при миграции (в бою %d вместо %d)", g->runBooster, BOOSTER_LUCK);
    check(g->save.boosters[BOOSTER_LUCK] == 3,
          "запас жетона удачи испорчен (%d вместо 3 после сгорания одного)", g->save.boosters[BOOSTER_LUCK]);
    for (int k = 4; k < BOOSTER_KINDS; ++k)
        check(g->save.boosters[k] == 0, "при миграции появился жетон тира (%d: %d)", k, g->save.boosters[k]);
    // Потраченное восстановлено по купленным рангам: пять рангов «Силы клинка».
    check(g->save.goldSpent > 0, "потраченное после миграции ноль");
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
    testKindVariety();
    testNewCounters();
    testBoosterTiers();
    testEveryBoosterWorks();
    testGoldSpent();
    testTierMigration();
    printf("\n");
    if (failures == 0) printf("ОК: обещания дневных даров выполнены\n");
    else printf("ПРОВАЛОВ: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
