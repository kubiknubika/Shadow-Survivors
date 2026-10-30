// ── Стенд стратегии: сборки, герои и стиль игры ──────────────────────────────
//
// Стенд «сколько урона» (weapon_probe) и «разные ли варианты» (variety_probe)
// уже ответили на свои вопросы. Этот отвечает на третий: **есть ли у игрока
// стратегия** — то есть приводят ли разные вложения к разному результату.
//
// Финитная волна для этого не годится: на пике сборки сносят её за 3 с и
// упираются в потолок замера (проверено: 9 разных сборок дали 0.2–4.8 с при
// почти одинаковом уроне). Поэтому здесь живой поток врагов:
//
//   1. «Напор» — герой бессмертен, поток 8 мобов/с в течение 60 с, здоровье
//      мобов как на 10-й минуте. Меряется, сколько убито и сколько врагов
//      остаётся жить: если очередь растёт, сборка не держит темп.
//   2. «Живучесть» — тот же поток, но герой смертен. Меряется, сколько секунд
//      он живёт. Это единственный замер, где защитные вложения что-то значат.
//   3. «Герои» — один и тот же билд и поток за пять героев.
//   4. «Стиль» — стоять или кайтить по кругу на одном билде.
//   5. «Решения» — сколько прокачек успевает дать забег (сбор опыта).
//   6. «Матрица разнообразия» — 7 оружий × 3 способа добрать третий слот.
//   7. «Артефакты» — что ускоряет зачистку живой волны.
//   8. «Каталог билдов» — что реально собирается в окне 0–5 / 5–10 / 10–15 мин.
//      Каждая сборка считается дважды: все ранги сразу и по одной прокачке,
//      когда предмет уже открыт. Смертность здесь выключена: вопрос про состав,
//      а не про «сколько прожил». Картинка — weapon_chart --catalog.
//
// Все сборки получают одинаковый бюджет — 17 рангов. Это не «сильная» сборка
// против «слабой», а разные способы потратить одно и то же.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/build_probe.cpp -o build/build_probe
//     build/build_probe                 # 60 с напора и 90 с живучести на строку
//     build/build_probe 60 90 out.csv   # свои лимиты и CSV для картинки
//
// Код возврата 1, если какая-то сборка за 60 с напора не убила никого.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

static const float STREAM_RATE = 8.0f;      // мобов в секунду
static const int STREAM_CAP = 220;          // предел живых, чтобы стенд не «кипел»

struct ItemRank { int id; int rank; };
struct Build {
    const char* name;
    const char* note;
    std::vector<ItemRank> items;
};

struct RunResult {
    double survived = 0.0;      // сколько секунд герой продержался
    int kills = 0;
    double damage = 0.0;
    double avgAlive = 0.0;
    int peakAlive = 0;
    int aliveEnd = 0;
    double hpLeft = 0.0;        // доля здоровья на конец замера
    int levelUps = 0;           // сколько прокачек (решений) выдал забег
};

// realSpawns=true оставляет штатный спавн игры (сложность растёт сама),
// false — выключает его, чтобы подавать врагов руками.
static void resetArena(Game& g, bool realSpawns) {
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.bossHazards.clear();
    g.particles.clear();
    g.lightningArcs.clear();
    g.frostWaves.clear();
    g.balance.spawnMultiplier = realSpawns ? 1.15f : 0.0f;   // 1.15 — штатное значение игры
    g.squadTimer = 1.0e9f;
    g.ratTimer = 1.0e9f;
    g.resonatorSpawnTimer = 1.0e9f;
    g.portalSpawnTimer = 1.0e9f;
    g.nextBossIndex = 999;
    g.eventCooldown = 1.0e9f;
    g.huntActive = false;
    g.runTime = 0.0f;
    g.pendingResonatorBurst = 0;
    g.pendingMatryoshkaSplits.clear();
    g.nextBreakthroughLevel = 999999;
    g.breakthroughActive = false;
    g.breakthroughTimer = 0.0f;
    g.state = STATE_PLAYING;
    g.playerX = 0.0f;
    g.playerY = 0.0f;
    g.playerHurtTimer = 0.0f;
    g.kills = 0;
    g.totalDamageDealt = 0.0;
    g.playerLevel = 1;                  // уровень считаем от нуля, а не от прошлого блока
    g.playerXp = 0;
    g.xpToNext = g.getXpRequirementForLevel(1);
    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
}

// Поток врагов. hpMinute — минута, по здоровью которой замеряется моб.
static RunResult runStream(Game& g, const Build& build, float seconds, bool mortal,
                           float kiteSpeed, float hpMinute) {
    RunResult res;
    const float dt = 1.0f / 60.0f;
    resetArena(g, false);
    g.runTime = hpMinute;
    g.cheatGodMode = !mortal;
    g.playerHp = g.playerMaxHp;
    for (const ItemRank& item : build.items) g.itemLevels[item.id] = item.rank;
    for (int i = 0; i < 7; ++i) g.weaponTimers[i] = 0.0f;
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;
    g.checkSymbiosis();                 // симбиоз включается там же, где в игре

    const int frames = (int)(seconds / dt);
    int levelUps = 0;
    float spawnAccum = 0.0f;
    int spawned = 0;
    long long aliveSum = 0;
    int frameCount = 0;
    float orbit = 0.0f;
    for (int f = 0; f < frames; ++f) {
        spawnAccum += dt * STREAM_RATE;
        while (spawnAccum >= 1.0f) {
            spawnAccum -= 1.0f;
            if ((int)g.monsters.size() >= STREAM_CAP) continue;
            const bool elite = (spawned % 12 == 11);
            g.spawnMonster(spawned % 12, elite, false);
            ++spawned;
        }
        if (kiteSpeed > 0.0f) {
            // Герой обходит арену по кругу радиусом 120 px: проверяется, окупается
            // ли движение, а не только «стою и стреляю».
            orbit += kiteSpeed / 120.0f * dt;
            g.playerX = cosf(orbit) * 120.0f;
            g.playerY = sinf(orbit) * 120.0f;
        }
        // Модальное окно прокачки засчитывается как решение, но не применяется:
        // иначе заданная сборка расползлась бы случайными картами и сравнение
        // «равный бюджет» перестало бы быть равным.
        if (g.state == STATE_LEVELUP) { ++levelUps; g.state = STATE_PLAYING; }
        g.state = STATE_PLAYING;
        g.update(dt);
        aliveSum += (long long)g.monsters.size();
        res.peakAlive = std::max(res.peakAlive, (int)g.monsters.size());
        ++frameCount;
        res.survived += dt;
        if (mortal && g.playerHp <= 0.0f) break;
    }
    res.kills = g.kills;
    res.damage = g.totalDamageDealt;
    res.avgAlive = frameCount > 0 ? (double)aliveSum / frameCount : 0.0;
    res.aliveEnd = (int)g.monsters.size();
    res.hpLeft = g.playerMaxHp > 0.0f ? std::max(0.0f, g.playerHp) / g.playerMaxHp : 0.0f;
    res.levelUps = levelUps;
    return res;
}

// Честная сборка: ранг из списка выдаётся только тогда, когда игра сама дала
// прокачку, предмет уже открыт по времени и его не держит потолок ранга
// (девятый ранг оружия требует резонансной реликвии IV). Если пункт списка
// заблокирован — попытка уходит следующему, а не сгорает.
static void tryRampStep(Game& g, const std::vector<ItemRank>& order) {
    for (const ItemRank& ir : order) {
        if (g.itemLevels[ir.id] >= ir.rank) continue;
        if (!g.isItemUnlocked(ir.id)) return;                  // ещё не открылся — ждём
        if (g.itemLevels[ir.id] >= g.getItemMaxLevel(ir.id)) continue;   // потолок — ниже по списку
        const int before = g.itemLevels[ir.id];
        g.applyUpgrade(ir.id);
        if (g.itemLevels[ir.id] > before) return;
    }
}

// Настоящий забег: штатный спавн игры, герой смертен и не двигается сам.
// Это единственный замер, где сложность растёт так же, как в игре, поэтому он
// и годен для ответа «какая сборка доживает дальше».
static RunResult runReal(Game& g, const Build& build, float seconds, bool mortal,
                         float moveSpeed, bool collect = false, bool ramp = false) {
    RunResult res;
    const float dt = 1.0f / 60.0f;
    resetArena(g, true);
    g.cheatGodMode = !mortal;
    g.playerHp = g.playerMaxHp;
    if (!ramp) for (const ItemRank& item : build.items) g.itemLevels[item.id] = item.rank;
    // Первый предмет — тот, с которым игрок начинает забег (первый же выбор драфта).
    else if (!build.items.empty()) g.applyUpgrade(build.items[0].id);
    for (int i = 0; i < 7; ++i) g.weaponTimers[i] = 0.0f;
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;
    g.checkSymbiosis();

    const int frames = (int)(seconds / dt);
    long long aliveSum = 0;
    int frameCount = 0;
    int levelUps = 0;
    float orbit = 0.0f;
    for (int f = 0; f < frames; ++f) {
        if (ramp) {
            // Слоты сборки растут от убитых стражей. Здесь стражи не спавнятся
            // (nextBossIndex=999), поэтому их вклад подаётся по расписанию игры:
            // босс приходит на 90-й, 180-й, 270-й секунде и т.д.
            g.runBossKills = std::min(4, (int)(g.runTime / 90.0f));
        }
        if (collect) {
            // Идёт к ближайшему самоцвету: без сбора опыта забег не даёт прокачек,
            // а вопрос «сколько решений успевает принять игрок» без этого не измерить.
            const Pickup* best = nullptr;
            float bestD2 = 1.0e18f;
            for (const Pickup& p : g.pickups) {
                const float dx = p.x - g.playerX, dy = p.y - g.playerY;
                const float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) { bestD2 = d2; best = &p; }
            }
            if (best) {
                const float d = sqrtf(std::max(1.0f, bestD2));
                const float step = std::min(d, moveSpeed * dt);
                g.playerX += (best->x - g.playerX) / d * step;
                g.playerY += (best->y - g.playerY) / d * step;
            }
        } else if (moveSpeed > 0.0f) {
            // Обход арены по кругу 120 px со скоростью героя: движение вместо стойки.
            orbit += moveSpeed / 120.0f * dt;
            g.playerX = cosf(orbit) * 120.0f;
            g.playerY = sinf(orbit) * 120.0f;
        }
        // Модальное окно прокачки засчитывается как решение, но не применяется:
        // иначе заданная сборка расползлась бы случайными картами и сравнение
        // «равный бюджет» перестало бы быть равным.
        if (g.state == STATE_LEVELUP) {
            ++levelUps;
            g.state = STATE_PLAYING;
            if (ramp) tryRampStep(g, build.items);
        }
        g.state = STATE_PLAYING;
        g.update(dt);
        aliveSum += (long long)g.monsters.size();
        res.peakAlive = std::max(res.peakAlive, (int)g.monsters.size());
        ++frameCount;
        res.survived += dt;
        if (mortal && g.playerHp <= 0.0f) break;
    }
    res.kills = g.kills;
    res.damage = g.totalDamageDealt;
    res.avgAlive = frameCount > 0 ? (double)aliveSum / frameCount : 0.0;
    res.aliveEnd = (int)g.monsters.size();
    res.hpLeft = g.playerMaxHp > 0.0f ? std::max(0.0f, g.playerHp) / g.playerMaxHp : 0.0f;
    res.levelUps = levelUps;
    return res;
}

// Волна из 40 живых мобов с настоящим здоровьем 5-й минуты. Нужна для артефактов,
// которые срабатывают по событию: на бессмертных мишенях добивание, ожог и откол
// не видны, а на живой волне видно, ускоряет ли артефакт зачистку.
struct WaveResult { double seconds = 0.0; int kills = 0; bool cleared = false; };

static WaveResult runWave(Game& g, int weaponId, int weaponLevel, int artifactId,
                          float limit, int wave = 40) {
    WaveResult res;
    const float dt = 1.0f / 60.0f;
    resetArena(g, false);
    g.runTime = 300.0f;
    g.cheatGodMode = true;
    g.playerHp = g.playerMaxHp;
    g.playerX = 0.0f; g.playerY = 0.0f;
    g.itemLevels[weaponId] = weaponLevel;
    if (artifactId > 0) g.itemLevels[artifactId] = g.getItemMaxLevel(artifactId);
    for (int i = 0; i < 7; ++i) g.weaponTimers[i] = 0.0f;
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;
    g.checkSymbiosis();
    for (int i = 0; i < wave; ++i) {
        const float t = wave > 1 ? (float)i / (float)(wave - 1) : 0.0f;
        const float r = 240.0f + 180.0f * t;
        const float a = i * 2.39996323f;
        g.spawnMonster(i % 12, i % 8 == 7, false);
        if (g.monsters.empty()) break;
        Monster& m = g.monsters.back();
        m.x = cosf(a) * r;
        m.y = sinf(a) * r;
        m.vx = m.vy = 0.0f;
    }
    const int frames = (int)(limit / dt);
    for (int f = 0; f < frames; ++f) {
        if (g.state == STATE_LEVELUP) g.state = STATE_PLAYING;
        g.state = STATE_PLAYING;
        g.update(dt);
        res.seconds += dt;
        if (g.monsters.empty()) { res.cleared = true; break; }
    }
    res.kills = g.kills;
    return res;
}

// Один и тот же забег на трёх зёрнах. Одно зерно давало разброс до ±40 % на
// «сколько прожил» у оружия, которое зависит от крит-бросков (топор, шар), и
// выдавало удачу за стратегию. Теперь в таблицах — среднее, а разброс печатается.
struct SeedAvg {
    RunResult mean;
    double survivedMin = 1e9, survivedMax = 0.0;
    int lastLevelUps = 0;      // прокачки последнего зерна — с ним и печатается состав
};

static SeedAvg runRealSeeds(Game& g, const Build& build, float seconds, float moveSpeed,
                            bool collect = false, int seeds = 3, bool ramp = false,
                            bool mortal = true) {
    SeedAvg out;
    for (int s = 0; s < seeds; ++s) {
        srand(20260201 + s);
        RunResult r = runReal(g, build, seconds, mortal, moveSpeed, collect, ramp);
        out.mean.survived += r.survived / seeds;
        out.mean.kills += (double)r.kills / seeds;
        out.mean.damage += r.damage / seeds;
        out.mean.avgAlive += r.avgAlive / seeds;
        out.mean.peakAlive += (double)r.peakAlive / seeds;
        out.mean.levelUps += (double)r.levelUps / seeds;
        out.mean.hpLeft += r.hpLeft / seeds;
        out.survivedMin = std::min(out.survivedMin, r.survived);
        out.survivedMax = std::max(out.survivedMax, r.survived);
        out.lastLevelUps = r.levelUps;
    }
    return out;
}

// ── Каталог билдов: что можно собрать в каждом окне забега ────────────────────
//
// Бюджет — не выдуманный: это замеренное число прокачек на 5-й, 10-й и 15-й
// минуте (17 / 30 / 39, блок 5). Уровни предметов ограничены тем, что открыто
// по времени: до 5-й минуты почти всё — тир I, к 10-й открываются тиры II–III,
// к 15-й — IV–V.
struct CatalogBuild {
    const char* name;
    const char* idea;
    std::vector<ItemRank> items;
};
struct Epoch {
    const char* title;
    float seconds;
    std::vector<CatalogBuild> builds;
};

static std::vector<Epoch> makeCatalog() {
    // Правило отбора: предмет должен открыться не позже чем за 2 минуты до конца
    // окна, иначе его ранги физически не успеть получить. В эпоху 1 открыто
    // только базовое (тир I, id 0..15 плюс 16 на 3-й минуте), в эпоху 2 — тиры
    // II–III, в эпоху 3 — всё, кроме Звезды Пепла (27 открывается на 13-й
    // минуте, на её ранги остаётся меньше двух минут).
    //
    // Порядок внутри сборки — порядок вложений: оружие, его резонанс, затем
    // остальное по времени открытия. Поздний тир в начале списка означал бы,
    // что прокачки до его открытия сгорают впустую (в игре окно выбора не ждёт).
    return {
        {"ЭПОХА 1: 0–5 мин, 17 прокачек, тир I и Печать Сердца на 3-й минуте", 300.0f, {
            {"Клинки-симбиоз",   "глубина в одно оружие",       {{0, 9}, {7, 4}, {11, 4}}},
            {"Клинки-темп",      "клинки + частота",            {{0, 8}, {11, 4}, {12, 5}}},
            {"Молния-заряд",     "клинки молний + супер",       {{3, 8}, {11, 2}, {12, 2}, {15, 5}}},
            {"Аура-танк",        "HP + урон вблизи",            {{4, 9}, {10, 4}, {11, 4}}},
            {"Ширина",           "два оружия и книга",          {{0, 6}, {1, 6}, {7, 5}}},
            {"Феникс-страховка", "воскрешение + HP",            {{0, 8}, {14, 2}, {10, 4}, {11, 3}}},
            {"Экономист",        "опыт, золото, магнит",        {{0, 8}, {7, 4}, {9, 2}, {13, 3}}},
        }},
        {"ЭПОХА 2: 5–10 мин, 30 прокачек, открылись тиры II–III", 600.0f, {
            {"Гроза",            "молния + урон элитам",        {{3, 9}, {10, 4}, {11, 5}, {15, 3}, {12, 3}, {16, 3}, {20, 3}}},
            {"Ожог",             "вихрь + поджог критом",       {{2, 9}, {9, 4}, {11, 5}, {16, 3}, {12, 3}, {17, 3}, {34, 3}}},
            {"Перекрёстный",     "два оружия + рикошет",        {{0, 6}, {1, 6}, {11, 4}, {12, 4}, {15, 4}, {17, 3}, {33, 3}}},
            {"Переполнение",     "аура + щит из лечения",       {{4, 9}, {11, 4}, {10, 4}, {12, 4}, {16, 3}, {22, 3}, {30, 3}}},
            {"Топор-пробитие",   "топор + радиус и линза",      {{5, 9}, {12, 4}, {11, 5}, {10, 4}, {20, 3}, {17, 2}, {18, 3}}},
            {"Лёд-контроль",     "кольцо + линза и руна",       {{6, 9}, {13, 4}, {12, 4}, {11, 4}, {10, 4}, {21, 2}, {18, 3}}},
            {"Вихрь-линза",      "вихрь + радиус и крит",       {{2, 9}, {9, 4}, {11, 5}, {12, 4}, {15, 2}, {17, 3}, {18, 3}}},
            {"Супер-цикл",       "супер каждые полминуты",      {{3, 9}, {10, 4}, {11, 5}, {15, 5}, {12, 1}, {16, 3}, {17, 3}}},
        }},
        {"ЭПОХА 3: 10–15 мин, 39 прокачек, открылись тиры IV–V", 900.0f, {
            {"Казнь",            "молния + добивание Палача",   {{3, 9}, {10, 6}, {11, 6}, {12, 6}, {15, 3}, {16, 3}, {17, 3}, {37, 3}}},
            {"Возмездие",        "топор + контрудар за HP",     {{5, 9}, {12, 6}, {11, 6}, {10, 6}, {17, 3}, {19, 3}, {23, 3}, {38, 3}}},
            {"Морозный взрыв",   "лёд + откол Оттепели",        {{6, 9}, {13, 5}, {12, 6}, {11, 6}, {10, 4}, {18, 3}, {35, 3}}},
            {"Симбиоз-печать",   "клинки + Печать Разоблачения", {{0, 9}, {7, 4}, {11, 6}, {12, 6}, {15, 3}, {16, 3}, {17, 3}, {29, 3}}},
            {"Крит-суперкрит",   "крит, ожог, Осколок Луны",    {{0, 9}, {7, 4}, {11, 6}, {12, 6}, {15, 5}, {16, 3}, {17, 3}, {34, 3}}},
            {"Танк-возмездие",   "аура, щиты, контрудар",       {{4, 9}, {11, 6}, {10, 6}, {12, 6}, {22, 3}, {23, 3}, {30, 3}, {38, 3}}},
        }},
    };
}

static Build bladeBuild() {
    return {"Клинки: симбиоз", "0:9 7:4 11:4", {{0, 9}, {7, 4}, {11, 4}}};
}

static std::vector<Build> makeBuilds() {
    // Бюджет у всех одинаковый — 17 рангов вложений.
    // Первые девять строк — «своё оружие на максимум + его резонанс + урон»;
    // последние три — те же 17 рангов, потраченные иначе (ширина, оборона).
    return {
        {"Клинки: симбиоз",      "0:9 7:4 11:4",  {{0, 9}, {7, 4}, {11, 4}}},
        {"Клинки: ширина",       "0:6 1:6 7:5",   {{0, 6}, {1, 6}, {7, 5}}},
        {"Клинки: оборона",      "0:9 10:4 19:4", {{0, 9}, {10, 4}, {19, 4}}},
        {"Шар: симбиоз",         "1:9 8:4 11:4",  {{1, 9}, {8, 4}, {11, 4}}},
        {"Вихрь: симбиоз",       "2:9 9:4 11:4",  {{2, 9}, {9, 4}, {11, 4}}},
        {"Молния: симбиоз",      "3:9 10:4 11:4", {{3, 9}, {10, 4}, {11, 4}}},
        {"Аура: симбиоз",        "4:9 11:4 12:4", {{4, 9}, {11, 4}, {12, 4}}},
        {"Топор: симбиоз",       "5:9 12:4 11:4", {{5, 9}, {12, 4}, {11, 4}}},
        {"Лёд: симбиоз",         "6:9 13:4 11:4", {{6, 9}, {13, 4}, {11, 4}}},
        {"Шар: оборона",         "1:9 8:4 19:4",  {{1, 9}, {8, 4}, {19, 4}}},
        {"Топор: оборона",       "5:9 12:4 19:4", {{5, 9}, {12, 4}, {19, 4}}},
        {"Аура-танк",            "4:9 10:5 11:3", {{4, 9}, {10, 5}, {11, 3}}},
    };
}

int main(int argc, char** argv) {
    const float pressure = argc > 1 ? (float)atof(argv[1]) : 60.0f;
    const float runLimit = argc > 2 ? (float)atof(argv[2]) : 300.0f;
    const char* outCsv = argc > 3 ? argv[3] : nullptr;
    FILE* csv = outCsv ? fopen(outCsv, "w") : nullptr;
    if (csv) fprintf(csv, "kind,name,survived,kills,damage,avgAlive,peakAlive,aliveEnd,hpLeft\n");

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.simulationMode = true;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;

    int failures = 0;
    std::vector<Build> builds = makeBuilds();

    printf("Стенд стратегии. Бюджет у всех сборок один — 17 рангов вложений.\n");
    printf("Поток в «напоре»: %.0f мобов/с, здоровье как на 10-й минуте (моб ~87 HP).\n\n", STREAM_RATE);

    printf("1. НАПОР: %.0f с бессмертия, держит ли сборка темп\n", pressure);
    printf("%-20s %11s %7s %9s %8s %7s\n", "сборка", "вложения", "убито", "урон", "живых", "в конце");
    for (const Build& build : builds) {
        srand(20260201);
        RunResult res = runStream(g, build, pressure, false, 0.0f, 600.0f);
        printf("%-20s %11s %7d %9.0f %8.1f %7d\n", build.name, build.note,
               res.kills, res.damage, res.avgAlive, res.aliveEnd);
        if (csv) fprintf(csv, "pressure,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", build.name,
                         res.survived, res.kills, res.damage, res.avgAlive,
                         res.peakAlive, res.aliveEnd, res.hpLeft);
        if (res.kills == 0) { printf("    сборка не убила никого\n"); ++failures; }
    }

    printf("\n2. НАСТОЯЩИЙ ЗАБЕГ: %.0f с штатного спавна, герой смертен и стоит\n", runLimit);
    printf("%-20s %9s %7s %9s %8s\n", "сборка", "прожил", "убито", "урон", "пик живых");
    for (const Build& build : builds) {
        SeedAvg avg = runRealSeeds(g, build, runLimit, 0.0f);
        RunResult res = avg.mean;
        printf("%-20s %6.1f с %7.0f %9.0f %8.0f  (разброс %.0f–%.0f с)\n", build.name, res.survived,
               (double)res.kills, res.damage, (double)res.peakAlive, avg.survivedMin, avg.survivedMax);
        if (csv) fprintf(csv, "real,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", build.name,
                         res.survived, res.kills, res.damage, res.avgAlive,
                         res.peakAlive, res.aliveEnd, res.hpLeft);
    }

    static const char* heroName[5] = {"ВИКТОР", "СЕЛЕНА", "БЬЁРН", "АЛАРИК", "НИКС"};
    Build blade = bladeBuild();
    Build bare = {"Старт: один клинок", "0:1", {{0, 1}}};
    printf("\n3. ГЕРОИ: %.0f с штатного спавна, смерть настоящая, две сборки\n", runLimit);
    struct HeroCase { const Build* build; const char* title; };
    const HeroCase heroCases[2] = {{&bare, "стартовая сборка (0:1)"}, {&blade, "сильная сборка (0:9 7:4 11:4)"}};
    for (const HeroCase& hc : heroCases) {
        printf("  %s:\n", hc.title);
        double heroMin = 1e9, heroMax = 0.0;
        for (int h = 0; h < 5; ++h) {
            g.selectedHero = h;
            SeedAvg avg = runRealSeeds(g, *hc.build, runLimit, 0.0f);
            RunResult res = avg.mean;
            printf("    %-8s прожил %6.1f с  убито %4.0f  урон %7.0f  HP %3.0f%%  (%.0f–%.0f с)\n",
                   heroName[h], res.survived, (double)res.kills, res.damage,
                   res.hpLeft * 100.0f, avg.survivedMin, avg.survivedMax);
            if (csv) {
                char label[96];
                snprintf(label, sizeof(label), "%s → %s", hc.title, heroName[h]);
                fprintf(csv, "hero,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", label,
                        res.survived, res.kills, res.damage, res.avgAlive,
                        res.peakAlive, res.aliveEnd, res.hpLeft);
            }
            heroMin = std::min(heroMin, res.survived);
            heroMax = std::max(heroMax, res.survived);
        }
        printf("    разброс: %.1f–%.1f с (x%.2f)\n", heroMin, heroMax, heroMax / std::max(0.1, heroMin));
    }
    g.selectedHero = 0;

    printf("\n4. СТИЛЬ: %.0f с штатного спавна, стоять или обходить кругом\n", runLimit);
    const float heroSpeed = HERO_DEFS[0].moveSpeed;
    struct StyleRow { const Build* build; const char* name; bool move; };
    const StyleRow styles[4] = {
        {&bare, "старт: стоит", false},
        {&bare, "старт: обходит", true},
        {&blade, "сильная: стоит", false},
        {&blade, "сильная: обходит", true},
    };
    for (const StyleRow& style : styles) {
        SeedAvg avg = runRealSeeds(g, *style.build, runLimit, style.move ? heroSpeed : 0.0f);
        RunResult res = avg.mean;
        printf("  %-16s прожил %6.1f с  убито %4.0f  урон %7.0f  HP %3.0f%%  (%.0f–%.0f с)\n",
               style.name, res.survived, (double)res.kills, res.damage, res.hpLeft * 100.0f,
               avg.survivedMin, avg.survivedMax);
        if (csv) fprintf(csv, "style,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", style.name,
                         res.survived, res.kills, res.damage, res.avgAlive,
                         res.peakAlive, res.aliveEnd, res.hpLeft);
    }

    printf("\n5. РЕШЕНИЯ: сколько прокачек успевает дать забег (герой собирает опыт)\n");
    for (const HeroCase& hc : heroCases) {
        SeedAvg avg = runRealSeeds(g, *hc.build, runLimit, HERO_DEFS[0].moveSpeed, true);
        RunResult res = avg.mean;
        printf("  %-28s прокачек %3d  прожил %6.1f с  убито %4.0f\n",
               hc.title, res.levelUps, res.survived, (double)res.kills);
    }

    printf("\n6. МАТРИЦА РАЗНООБРАЗИЯ: 7 оружий × 3 способа добрать третий слот\n");
    printf("   (оружие 9 + свой резонанс IV + один из трёх артефактов; 300 с штатного спавна)\n");
    printf("%-16s %-18s %9s %7s %8s\n", "оружие", "третий слот", "прожил", "убито", "пик живых");
    static const char* weaponShort[7] = {"Клинки", "Шар", "Вихрь", "Молния", "Аура", "Топор", "Лёд"};
    struct Filler { const char* name; int id; int rank; };
    const Filler fillers[3] = {{"урон (берсерк)", 11, 4}, {"оборона (клятва)", 19, 4}, {"частота (часы)", 12, 4}};
    for (int w = 0; w < 7; ++w) {
        for (const Filler& filler : fillers) {
            Build build = {weaponShort[w], "", {{w, 9}, {7 + w, 4}, {filler.id, filler.rank}}};
            SeedAvg avg = runRealSeeds(g, build, runLimit, 0.0f);
            RunResult res = avg.mean;
            printf("%-10s %-18s %6.1f с %6.0f %7.0f  (%.0f–%.0f с)\n", weaponShort[w], filler.name,
                   res.survived, (double)res.kills, (double)res.peakAlive, avg.survivedMin, avg.survivedMax);
            if (csv) fprintf(csv, "matrix,%s + %s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n",
                             weaponShort[w], filler.name, res.survived, res.kills, res.damage,
                             res.avgAlive, res.peakAlive, res.aliveEnd, res.hpLeft);
        }
    }

    printf("\n7. АРТЕФАКТЫ ПО ЖИВОЙ ВОЛНЕ: клинки 4-го ранга, 40 мобов с настоящим HP\n");
    printf("   (сюда попадают добивание, ожог и откол — на бессмертных мишенях их не видно)\n");
    srand(20260201);
    WaveResult baseWave = runWave(g, 0, 4, 0, 90.0f);
    printf("  без артефакта: %.2f с%s\n", baseWave.seconds, baseWave.cleared ? "" : " (не зачистил)");
    double bestDelta = 0.0, worstDelta = 0.0;
    std::string bestName, worstName;
    for (int p = 7; p < ITEM_COUNT; ++p) {
        if (ITEM_DEFS[p].isWeapon) continue;
        if (g.getItemMaxLevel(p) <= 0) continue;
        srand(20260201);
        WaveResult wave = runWave(g, 0, 4, p, 90.0f);
        const double delta = 100.0 * (baseWave.seconds - wave.seconds) / std::max(0.01, baseWave.seconds);
        if (delta > bestDelta) { bestDelta = delta; bestName = ITEM_DEFS[p].name; }
        if (delta < worstDelta) { worstDelta = delta; worstName = ITEM_DEFS[p].name; }
        if (csv) fprintf(csv, "artifact,%s,%.2f,%d,%.0f,%.1f,%d,%d,%.2f\n", ITEM_DEFS[p].name,
                         wave.seconds, wave.kills, delta, 0.0, 0, 0, 0.0);
    }
    printf("  лучший: %s — зачистка быстрее на %.0f%%\n", bestName.c_str(), bestDelta);
    printf("  худший: %s — медленнее на %.0f%%\n", worstName.c_str(), -worstDelta);

    printf("\n8. КАТАЛОГ БИЛДОВ: что реально можно собрать в каждом окне забега\n");
    printf("   бюджет = замеренное число прокачек (17 / 30 / 39), предметы — из открытых к этому времени тиров\n");
    printf("   слева — сборка стоит с нуля секунды; справа — те же ранги, но выдаются по одной прокачке,\n");
    printf("   только когда предмет открылся и игра сама дала уровень: так игрок и собирает\n");
    for (const Epoch& epoch : makeCatalog()) {
        printf("\n  %s\n", epoch.title);
        printf("  %-24s %-30s %7s %7s | %7s %7s %8s\n", "билд", "идея", "убито", "урон", "убито", "урон", "пик живых");
        for (const CatalogBuild& cb : epoch.builds) {
            Build build = {cb.name, cb.idea, cb.items};
            // Сборка, которая не ходит за опытом, не получает прокачек — поэтому
            // в каталоге обе версии собирают опыт с той же скоростью героя:
            // разница между ними только в том, когда приходят ранги.
            // Смертность здесь выключена намеренно: вопрос каталога — успевает ли
            // сборка собраться и сколько она сносит к концу окна, а не «сколько
            // прожил». Выживание замерено отдельно (герои, стиль, 2.10.44):
            // подсадка смертности на неполной сборке мерила бы первые 15 секунд
            // забега, а не силу билда.
            const float collectSpeed = HERO_DEFS[0].moveSpeed;
            SeedAvg instant = runRealSeeds(g, build, epoch.seconds, collectSpeed, true, 3, false, false);
            SeedAvg honest = runRealSeeds(g, build, epoch.seconds, collectSpeed, true, 3, true, false);
            RunResult ri = instant.mean, rh = honest.mean;
            int ranks = 0;
            for (int i = 0; i < ITEM_COUNT; ++i) ranks += g.itemLevels[i];
            printf("  %-24s %-30s %7.0f %7.0f | %7.0f %7.0f %8.0f  (ур. %d, рангов %d)\n",
                   cb.name, cb.idea, (double)ri.kills, (double)ri.damage,
                   (double)rh.kills, (double)rh.damage, (double)rh.peakAlive,
                   honest.lastLevelUps, ranks);
            printf("      собрано к концу:");
            for (int i = 0; i < ITEM_COUNT; ++i)
                if (g.itemLevels[i] > 0) printf(" %d:%d", i, g.itemLevels[i]);
            printf("\n");
            if (csv) fprintf(csv, "catalog,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", cb.name,
                             ri.survived, ri.kills, ri.damage, ri.avgAlive,
                             ri.peakAlive, ri.aliveEnd, ri.hpLeft);
            if (csv) fprintf(csv, "catalogramp,%s,%.1f,%d,%.0f,%.1f,%d,%d,%.2f\n", cb.name,
                             rh.survived, rh.kills, rh.damage, rh.avgAlive,
                             rh.peakAlive, rh.aliveEnd, rh.hpLeft);
        }
    }

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе замеры выполнены\n");
    return 0;
}
