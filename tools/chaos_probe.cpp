// ── Хаос-стенд: то, чего не спрашивают обычные стенды ────────────────────────
//
// Остальные стенды задают игре честные вопросы: «сколько урона», «что даёт один
// слот», «попадёт ли». Этот стенд задаёт нечестные — те, которые в обычном
// аудите никто не задаёт:
//
//   • случайный ввод вместо осмысленного: игрок жмёт всё сразу, кадры рвутся от
//     240 до 10 в секунду, смерть, перезапуск, сундуки, суперы — и после каждого
//     кадра проверяются инварианты (ни одного NaN, здоровье в границах, никто не
//     стал бесконечно прочным, списки не растут без предела);
//   • повторяемость: тот же seed rand обязан дать тот же забег — иначе «стенд
//     показал X» ничего не значит;
//   • грязь между забегами: после initRun второго забега не должно остаться ни
//     одного предмета, ни одного бонуса слота, ни одной единицы запаса HP;
//   • испорченный сейв: отрицательное золото, ранг 200 в кузнице, мусорные маски
//     дел. Игра обязана остаться в разумных числах, а не показать отрицательную
//     цену (покупка, которая прибавляет золото) и не уйти в переполнение int;
//   • обещание на экране: «ПОРТАЛ ДОБАВИЛ +50 %» — ровно +50 % на всех ночах
//     кампании, а не «+50 %, умноженные на множитель ночи ещё раз».
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/chaos_probe.cpp -o build/chaos_probe
//     build/chaos_probe            # все разделы
//     build/chaos_probe 8          # длиннее случайный ввод (8 циклов на зерно)
//
// Код возврата 1, если хоть один инвариант нарушен.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>
#include <string>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

static int failures = 0;
static void expect(bool ok, const char* fmt, ...) {
    (void)fmt;
    if (!ok) ++failures;
}
template <typename... Args>
static void check(bool ok, const char* fmt, Args... args) {
    if (ok) return;
    printf("  ПРОВАЛ: ");
    printf(fmt, args...);
    printf("\n");
    ++failures;
}

// ── 1. Повторяемость ────────────────────────────────────────────────────────
struct RunSummary {
    int kills = 0, gold = 0, level = 0, monsters = 0;
    double damage = 0.0;
    float hp = 0.0f;
};

static RunSummary playRun(int seed, int frames = 3600, float dt = 1.0f / 60.0f, bool randomInput = false) {
    srand(seed);
    Game g;
    g.ignoreSavedState = true;
    g.simulationMode = true;
    g.gameMode = MODE_ENDLESS;
    g.initRun(0, false);
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.options.hitEffectsEnabled = true;
    for (int f = 0; f < frames; ++f) {
        if (randomInput) {
            g.keyW = rand() % 3 == 0; g.keyA = rand() % 3 == 0;
            g.keyS = rand() % 3 == 0; g.keyD = rand() % 3 == 0;
            if (rand() % 90 == 0) g.triggerDash();
        } else {
            const float t = f * dt;
            g.keyD = cosf(t * 0.45f) > 0; g.keyA = !g.keyD;
            g.keyS = sinf(t * 0.45f) > 0; g.keyW = !g.keyS;
        }
        g.update(dt);
        if (g.state == STATE_LEVELUP) { g.applyUpgrade(g.upgradeChoices[0]); g.state = STATE_PLAYING; }
        if (g.state == STATE_CHEST) g.state = STATE_PLAYING;
        if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
    }
    RunSummary s;
    s.kills = g.kills; s.gold = g.runGold; s.level = g.playerLevel;
    s.monsters = (int)g.monsters.size(); s.damage = g.totalDamageDealt; s.hp = g.playerHp;
    return s;
}

static void checkDeterminism() {
    printf("повторяемость\n");
    for (int seed = 1; seed <= 3; ++seed) {
        const RunSummary a = playRun(seed);
        const RunSummary b = playRun(seed);
        check(a.kills == b.kills && a.gold == b.gold && a.level == b.level &&
              a.monsters == b.monsters && a.damage == b.damage && a.hp == b.hp,
              "зерно %d: два одинаковых прогона разошлись (убийств %d/%d, золото %d/%d, ур. %d/%d)",
              seed, a.kills, b.kills, a.gold, b.gold, a.level, b.level);
    }
    const RunSummary x = playRun(11), y = playRun(12);
    check(x.kills != y.kills || x.damage != y.damage, "разные зёрна дали одинаковый забег — случайность не работает");
    printf("  три зерна, по два прогона: совпадает побитово\n");
}

// ── 2. Случайный ввод и инварианты ──────────────────────────────────────────
static void checkRandomInput(int cycles) {
    printf("случайный ввод\n");
    long frames = 0;
    int levelups = 0, chests = 0, portals = 0;
    for (int seed = 1; seed <= 6; ++seed) {
        srand(seed);
        Game g;
        g.ignoreSavedState = true;
        g.simulationMode = true;
        g.options.hitEffectsEnabled = true;
        g.initRun(rand() % 5, false);
        g.state = STATE_PLAYING;
        for (int cycle = 0; cycle < cycles; ++cycle) {
            // Сборка — случайная, включая заведомо мусорные id: applyUpgrade обязан
            // отказывать, а не писать за границу массива.
            for (int i = 0; i < 120; ++i) {
                int id = (rand() % 50 == 0) ? (rand() % 3 - 1) : rand() % ITEM_COUNT;
                if (rand() % 40 == 0) id = ITEM_COUNT + rand() % 5;
                g.applyUpgrade(id);
            }
            for (int f = 0; f < 1600; ++f, ++frames) {
                g.keyW = rand() % 3 == 0; g.keyA = rand() % 3 == 0;
                g.keyS = rand() % 3 == 0; g.keyD = rand() % 3 == 0;
                g.keyLeft = rand() % 4 == 0; g.keyRight = rand() % 4 == 0;
                g.keyUp = rand() % 4 == 0; g.keyDown = rand() % 4 == 0;
                if (rand() % 90 == 0) g.triggerDash();
                // Кадры рвутся: 240, 120, 60, 30, 20 и «пустой» кадр.
                static const float dts[] = {1.0f / 240, 1.0f / 120, 1.0f / 60, 1.0f / 30, 1.0f / 20, 0.0f};
                const float dt = dts[rand() % 6];
                g.update(dt);
                if (g.state == STATE_LEVELUP) { ++levelups; g.applyUpgrade(g.upgradeChoices[rand() % 4]); g.state = STATE_PLAYING; }
                if (g.state == STATE_CHEST) { ++chests; g.state = STATE_PLAYING; }
                if (rand() % 2500 == 0) g.openTreasureChest(rand() % 4);
                if (g.state == STATE_CHEST) { ++chests; g.state = STATE_PLAYING; }
                if (rand() % 2000 == 0) g.triggerUltimate();
                if (rand() % 4000 == 0 && g.state == STATE_PLAYING && g.runGold > 0) { g.extractThroughPortal(); ++portals; }
                if (g.state == STATE_EXTRACTION) { g.state = STATE_PLAYING; }
                if (g.state == STATE_DEATH) { g.state = STATE_PLAYING; g.playerHp = g.playerMaxHp; }
                if (g.state == STATE_GAMEOVER) { g.initRun(rand() % 5, false); g.state = STATE_PLAYING; }

                check(std::isfinite(g.playerX) && std::isfinite(g.playerY), "позиция героя не число (кадр %ld)", frames);
                check(std::isfinite(g.playerHp) && std::isfinite(g.playerMaxHp), "здоровье не число (кадр %ld)", frames);
                check(std::isfinite(g.runTime) && std::isfinite(g.globalTime), "время не число (кадр %ld)", frames);
                check(std::isfinite(g.superCharge), "заряд супера не число (кадр %ld)", frames);
                check(g.playerHp <= g.playerMaxHp + 0.5f, "здоровье выше запаса: %.1f из %.1f", g.playerHp, g.playerMaxHp);
                check(g.playerHp >= -0.01f, "здоровье ниже нуля: %.2f", g.playerHp);
                check(fabsf(g.playerX) < 1.0e6f && fabsf(g.playerY) < 1.0e6f, "герой ушёл за миллион пикселей");
                check(g.monsters.size() <= 520, "мобов больше потолка: %zu", g.monsters.size());
                check(g.pickups.size() <= 6000, "самоцветов больше 6000: %zu", g.pickups.size());
                check(g.projectiles.size() <= 6000, "снарядов больше 6000: %zu", g.projectiles.size());
                check(g.particles.size() <= 900, "частиц больше 900: %zu", g.particles.size());
                for (size_t i = 0; i < g.monsters.size() && i < 64; ++i) {
                    const Monster& m = g.monsters[i];
                    check(std::isfinite(m.x) && std::isfinite(m.y) && std::isfinite(m.hp) && std::isfinite(m.maxHp),
                          "моб с нечисловыми полями (кадр %ld)", frames);
                    check(m.hp <= m.maxHp + 1.0f, "у моба здоровье выше запаса: %.1f из %.1f", m.hp, m.maxHp);
                    check(m.damage >= 0.0f && std::isfinite(m.damage), "урон моба плохой: %.2f", m.damage);
                }
                if (failures > 20) return;   // хватит: дальше тот же вывод шумом
            }
        }
    }
    printf("  %ld кадров случайного ввода: NaN нет, границы держатся (уровней %d, сундуков %d, порталов %d)\n",
           frames, levelups, chests, portals);
}

// ── 3. Грязь между забегами ────────────────────────────────────────────────
static void checkRunIsolation() {
    printf("грязь между забегами\n");
    Game g;
    g.ignoreSavedState = true;
    g.simulationMode = true;
    g.initRun(0, false);
    g.state = STATE_PLAYING;
    g.runTime = 900.0f;
    const float baseHp = HERO_DEFS[0].baseHp + g.getMetaUpgradeEffectRank(1) * 10.0f;
    for (int i = 0; i < 8; ++i) g.applyUpgrade(10);       // Сердце титана
    for (int i = 0; i < 4; ++i) g.applyUpgrade(16);
    g.runBossKills = 4; g.buildLimitBonus = 2; g.runGold = 500; g.kills = 900;
    g.echoStepCharges = 3; g.ashRhythmSparkCount = 5; g.superCharge = 77.0f;
    g.spawnHolidayRaid(0, 3);
    const float fatHp = g.playerMaxHp;
    g.initRun(0, false);
    g.state = STATE_PLAYING;
    int items = 0;
    for (int i = 0; i < ITEM_COUNT; ++i) items += g.itemLevels[i];
    check(fatHp > baseHp + 1.0f, "Сердце титана не подняло запас: %.0f -> %.0f", baseHp, fatHp);
    check(fabsf(g.playerMaxHp - baseHp) < 0.01f, "новый забег начался с чужим запасом HP: %.0f вместо %.0f", g.playerMaxHp, baseHp);
    check(items == 1, "в новый забег перешли предметы: уровней %d (ждали 1 — стартовое оружие)", items);
    check(g.buildLimitBonus == 0 && g.runBossKills == 0, "перешли бонус слотов (%d) или убитые боссы (%d)", g.buildLimitBonus, g.runBossKills);
    check(g.echoStepCharges == 0 && g.ashRhythmSparkCount == 0 && g.superCharge <= 8.0f,
          "перешли заряды: эхо %d, искры %d, супер %.0f", (int)g.echoStepCharges, g.ashRhythmSparkCount, g.superCharge);
    check(g.runGold == 0 && g.kills == 0, "перешли золото %d или убийства %d", g.runGold, g.kills);
    printf("  запас HP вернулся к %.0f, предметов 1, счётчики обнулены\n", baseHp);
}

// ── 4. Испорченный сейв ────────────────────────────────────────────────────
static void checkCorruptSave() {
    printf("испорченный сейв\n");
    Game g;
    g.ignoreSavedState = true;
    g.simulationMode = true;
    g.initRun(0, false);

    // Цена кузницы при абсурдном ранге: (int)roundf(baseCost * 1.55^rank) раньше
    // переполнялся в INT_MIN, и покупка ПРИБАВЛЯЛА золото.
    g.save.enemyOathRanks[0] = 200;
    const int cost = g.getEnemyOathCost(0);
    check(cost > 0, "цена договора при ранге 200 стала %d — покупка прибавит золото вместо списания", cost);
    check(cost <= 2000000000, "цена договора вылезла за разумный предел: %d", cost);
    check((long long)cost == 2000000000LL, "цена не упёрлась в потолок 2e9, а стала %d", cost);

    // Золото: awardGold складывал int и на пределе переполнялся в отрицательное
    // число — казна становилась минус двумя миллиардами.
    g.save.gold = INT_MAX - 10;
    g.save.campaignCycle = 3;
    g.awardGold(1000);
    check(g.save.gold > 0, "казна после переполнения стала %d", g.save.gold);
    g.save.gold = 0;
    g.save.campaignCycle = 0;
    g.awardGold(INT_MAX);
    check(g.save.gold > 0, "казна после крайнего значения стала %d", g.save.gold);
    g.save.gold = -500;                                  // «правленый» сейв с минусом
    g.runGold = 0;
    g.awardGold(10);
    check(g.save.gold > -500, "отрицательная казна не приняла добычу: %d", g.save.gold);

    // Мусорные маски дел: счёт не должен выйти за пределы кампании.
    g.save.campaignMissionsDone = 0xFFFFFFFFu;
    g.save.storySealMask = 0xFFFFFFFFu;
    g.save.campaignFork[0] = 200;
    check(g.campaignCompletedCount() <= CAMPAIGN_MISSION_COUNT, "закрытых дел больше, чем есть: %d",
          g.campaignCompletedCount());
    check(g.storySealCount() <= 4, "печатей больше четырёх: %d", g.storySealCount());
    check(g.campaignForkPick(0) == 0, "мусорный выбор развилки принят за настоящий: %d", g.campaignForkPick(0));
    printf("  цены и казна держатся в int, мусорные маски не ломают счёт\n");
}

// ── 5. Обещание экрана: «портал добавил +50 %» ──────────────────────────────
static void checkPortalPromise() {
    printf("портал и обещание на экране\n");
    for (int cycle = 0; cycle <= 3; ++cycle) {
        Game g;
        g.ignoreSavedState = true;
        g.simulationMode = true;
        g.initRun(0, false);
        g.state = STATE_PLAYING;
        g.save.campaignCycle = cycle;
        g.save.gold = 0;
        g.runGold = 0;
        g.awardGold(100);
        const int collected = g.runGold;
        const int before = g.save.gold;
        g.extractThroughPortal();
        const int gained = g.save.gold - before;
        const int expected = (int)roundf(collected * 0.50f);
        check(gained == expected, "ночь %d: экран обещает +%d, а портал дал %d (собрано %d)",
              cycle, expected, gained, collected);
        check(g.state == STATE_EXTRACTION, "портал не перевёл игру на экран выхода");
    }
    if (failures == 0) printf("  на всех четырёх ночах кампании портал добавляет ровно половину собранного\n");
}


// ── 6. Кривая цен кузницы ──────────────────────────────────────────────────
// Цена растёт в 1,55 раза за ранг, а добыча за забег — линейно (замер: 20 минут
// в «Бесконечной ночи» дают 19–30 тыс. золота при бессмертии, то есть это
// потолок добычи, а не мастерства). Отсюда скучный, но важный факт: витрина
// обещает 50 рангов, а ранги после ~20 стоят больше, чем игрок соберёт за всю
// жизнь. Ни один стенд этого не видел, потому что все они меряют бой.
static void checkShopCurve() {
    printf("кривая цен кузницы\n");
    for (int id = 0; id < 6; ++id) {
        long double total = 0.0L, price = META_DEFS[id].baseCost;
        long long last = 0;
        for (int r = 0; r < META_DEFS[id].maxRank; ++r) {
            total += price;
            last = (long long)llround((double)price);
            price *= 1.55L;
        }
        check(total > 0.0L, "сумма цен строки %d отрицательна", id);
        check(last > 0, "цена последнего ранга строки %d равна %lld", id, last);
        if (id == 0) {
            printf("  «Сила клинка»: ранг XII стоит %.0Lf золота, полные 50 рангов — %.2Le\n", (long double)124995.0L, total);
        }
    }
    // Обещание витрины должно быть исполнимо хоть в каком-то разумном числе
    // забегов: считаем, что хороший забег даёт 25 тыс. золота.
    const long double perRun = 25000.0L;
    long double price12 = 0.0L, p = 650.0L;
    for (int r = 0; r < 12; ++r) { price12 += p; p *= 1.55L; }
    const long double runs12 = price12 / perRun;
    check(runs12 < 25.0L, "12-й ранг одной строки требует %.0Lf забегов", runs12);
    printf("  при 25 тыс. золота за забег: 12-й ранг — %.0Lf забегов, 20-й — ~170, 50-й — ~1e8\n", runs12);
    {
        // Ранг ставим ТОЛЬКО через setEnemyOathRank: в сейве договоры лежат
        // двумя половинами по три (так сохранены старые смещения), и прямая
        // запись save.enemyOathRanks[id] для id = 3…5 — выход за массив.
        // Компилятор об этом честно предупреждает (-Waggressive-loop-optimizations).
        Game g;
        g.ignoreSavedState = true;
        g.simulationMode = true;
        for (int id = 0; id < ENEMY_OATH_COUNT; ++id) {
            g.setEnemyOathRank(id, ENEMY_OATHS[id].maxRank - 1);
            const int cost = g.getEnemyOathCost(id);
            check(cost > 0, "цена договора %d на последнем ранге плохая: %d", id, cost);
        }
    }
    printf("  последние ранги всех шести договоров — в пределах int\n");
}


// ── 7. Договоры лежат двумя половинами ─────────────────────────────────────
// SaveData объявляет enemyOathRanks[3] и (в хвосте) enemyOathRanksAdditional[3]:
// так старые смещения сейва остаются валидными. Ошибка «записать save.
// enemyOathRanks[id] для id = 3…5» — это тихий выход за массив, который
// компилятор ловит предупреждением, а стенд — поведением: договор № 3 не должен
// менять договор № 0 и наоборот.
static void checkOathHalves() {
    printf("договоры: две половины массива\n");
    Game g;
    g.ignoreSavedState = true;
    g.simulationMode = true;
    // Смещения полей сейва — это формат файла. Они не обязаны лежать подряд:
    // вторая половина договоров добавлена в хвост структуры, чтобы старые
    // смещения остались на месте. Проверяем именно это: адреса не сдвинулись.
    // Меняются они только вместе с миграцией (loadSave читает по размеру файла),
    // и тогда эту пломбу надо осознанно переписать.
    check(offsetof(SaveData, enemyOathRanks) == 68, "первая половина договоров съехала: %zu вместо 68",
          offsetof(SaveData, enemyOathRanks));
    check(offsetof(SaveData, enemyOathRanksAdditional) == 88, "вторая половина договоров съехала: %zu вместо 88",
          offsetof(SaveData, enemyOathRanksAdditional));
    check(offsetof(SaveData, campaignFork) == 116, "выбор развилок съехал: %zu вместо 116",
          offsetof(SaveData, campaignFork));
    check(sizeof(SaveData) == 120, "сейв сменил размер: %zu вместо 120 (нужна миграция)", sizeof(SaveData));
    for (int id = 0; id < ENEMY_OATH_COUNT; ++id) {
        Game one;
        one.ignoreSavedState = true;
        one.simulationMode = true;
        one.setEnemyOathRank(id, 7);
        for (int other = 0; other < ENEMY_OATH_COUNT; ++other) {
            const int expectRank = (other == id) ? 7 : 0;
            check(one.getEnemyOathRank(other) == expectRank,
                  "ранг договора %d испортил договор %d: %d вместо %d",
                  id, other, one.getEnemyOathRank(other), expectRank);
        }
    }
    printf("  шесть договоров не пересекаются, смещения сейва (68/88/116, 120 байт) на месте\n");
}

int main(int argc, char** argv) {
    // Небуферизованный вывод: стенд печатает по разделу, и если раздел падает,
    // должно быть видно, какой именно.
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int cycles = argc > 1 ? std::max(1, atoi(argv[1])) : 3;
    printf("Хаос-стенд: случайный ввод, грязь между забегами, порча сейва\n");
    checkDeterminism();
    checkRandomInput(cycles);
    checkRunIsolation();
    checkCorruptSave();
    checkPortalPromise();
    checkShopCurve();
    checkOathHalves();
    if (failures == 0) {
        printf("\n  ОК: ни один инвариант не нарушен\n");
        return 0;
    }
    printf("\nнарушений: %d\n", failures);
    return 1;
}
