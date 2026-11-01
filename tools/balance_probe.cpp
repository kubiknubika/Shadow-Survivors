// ── Стенд баланса: у каждого предмета своя ниша, и ни одна не решает всё ──────
//
// Остальные стенды смотрят на один вопрос: «сколько урона», «сколько попаданий»,
// «как складывается пара». Этот отвечает на вопрос игрока: **что даёт один слот**.
// Если один предмет лучше другого сразу везде, выбора нет: слабый — мусор,
// сильный — обязателен. Поэтому здесь каждый предмет меряется по семи осям, и
// проверяется не «кто сильнее», а «у кого какая ниша и нет ли такого, кто
// выигрывает на всех».
//
// Оси (все — «чем больше, тем лучше»):
//   темп      — поток 8 мобов/с, 30 с, герой бессмертен: сколько нанесено;
//   орда      — поток, 60 с, герой кайтит по кругу: сколько нанесено;
//   кайт      — тот же кайт: сколько пути прошёл герой (скорость и рывок);
//   живучесть — один моб бьёт на 20 каждые 0,42 с, герой смертен: сколько прожито;
//   опыт      — поле из 40 самоцветов, 60 с, герой идёт спиралью: сумма опыта;
//   золото    — тот же забег: сколько золота собрано (runGold);
//   босс      — ДПС по мишени с флагом босса (30 px);
//   элита     — ДПС по мишени-элите (30 px).
//
// Носитель — серебряные клинки 4-го ранга: единственное оружие, которое бьёт и
// одну цель, и толпу, поэтому на нём видно вклад любого артефакта. Предмет
// ставится на максимум своего ранга (у тира I — 6, у 13–15 — 5, у остальных — 3).
//
// Как читать вывод:
//   • «% к базе» — прирост к забегу без артефакта;
//   • «ниша» — ось, где предмет сильнее всего, и его прирост там;
//   • «широта» — средний прирост по всем осям (насколько предмет полезен везде);
//   • «лучших осей» — на скольких осях предмет первый. Два и больше — это уже
//     однозначный выбор, стенд печатает предупреждение.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -pthread -Isrc tools/balance_probe.cpp -o build/balance_probe
//     build/balance_probe              # все предметы, короткие замеры
//     build/balance_probe 30           # дольше замер = меньше шума
//     build/balance_probe 30 0         # только список без замеров (быстрая проверка)
//     build/balance_probe 30 out.csv   # плюс CSV
//
// Код возврата 1, если один предмет первый на трёх и более осях (однозначно
// сильный) или если предмет ни на одной оси не даёт хотя бы +10 % (однозначно
// слабый). Предметы, которым эти сцены не дают триггера, печатаются отдельным
// списком и в приговор не входят — их проверяют build_probe и точечные сцены.
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
static const float STREAM_RATE = 8.0f;
static const int STREAM_CAP = 220;

enum Axis { AX_TEMPO, AX_CROWD, AX_KITE, AX_SURVIVE, AX_XP, AX_GOLD, AX_BOSS, AX_ELITE, AX_COUNT };
static const char* axisName[AX_COUNT] = {"темп", "орда", "кайт", "живучесть", "опыт", "золото", "босс", "элита"};
// Темп и орда меряются уроном по живой волне, а не числом убийств: убийство —
// ступенька, и три зерна давали на нём ±20 % там, где сила сборки не менялась.

struct Profile {
    double v[AX_COUNT] = {0.0};
};

// Чистая арена. realSpawns=true оставляет штатный спавн игры.
static void resetArena(Game& g, bool realSpawns) {
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.bossHazards.clear();
    g.particles.clear();
    g.lightningArcs.clear();
    g.frostWaves.clear();
    g.balance.spawnMultiplier = realSpawns ? 1.15f : 0.0f;
    g.squadTimer = 1.0e9f;
    g.ratTimer = 1.0e9f;
    // Волна праздника приходит на 30-й секунде и подкидывает мобов: их убийства
    // роняют самоцветы, и «опыт» начинал зависеть от урона, а не от экономики.
    g.holidayWaveTimer = 1.0e9f;
    g.resonatorSpawnTimer = 1.0e9f;
    g.portalSpawnTimer = 1.0e9f;
    g.nextBossIndex = 999;
    // Сюжетный страж приходит по runTime и на входе вычищает арену
    // (clearArenaForBoss). В замере мишень должна жить, поэтому вход стража
    // закрыт: без этого весь замер ДПС превращался в ноль.
    g.storyMissionBossSpawned = true;
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
    g.runGold = 0;
    g.playerLevel = 1;
    g.playerXp = 0;
    g.xpToNext = g.getXpRequirementForLevel(1);
    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
    for (int i = 0; i < 7; ++i) g.weaponTimers[i] = 0.0f;
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;
}

// Носитель: клинки 4-го ранга (бьют и одну цель, и толпу) плюс проверяемый предмет.
// Ранги выдаём через applyUpgrade, а не записью в itemLevels: только этот путь
// применяет побочные эффекты предмета («Сердце титана» растит запас здоровья).
static void equipCarrier(Game& g, int itemId) {
    resetArena(g, false);
    // Запас здоровья — заново от базы героя: «Сердце титана» прибавляет +40 к
    // playerMaxHp на каждом ранге, и без сброса его прибавка переходила бы всем
    // следующим предметам (все после него «жили» до потолка замера).
    g.playerMaxHp = HERO_DEFS[g.selectedHero].baseHp + g.getMetaUpgradeEffectRank(1) * 10.0f;
    g.playerHp = g.playerMaxHp;
    g.runTime = 900.0f;                 // всё открыто: иначе applyUpgrade откажет
    for (int i = 0; i < 4; ++i) g.applyUpgrade(0);
    if (itemId >= 0) {
        for (int i = 0; i < 8; ++i) {
            const int before = g.itemLevels[itemId];
            g.applyUpgrade(itemId);
            if (g.itemLevels[itemId] == before) break;
        }
    }
    for (int i = 0; i < 7; ++i) g.weaponTimers[i] = 0.0f;
    g.checkSymbiosis();
}

// ── Живая сцена: поток врагов руками, положение героя по режиму ─────────────
// Штатный спавн игры здесь не годится: он тянет позиции из rand(), и любой
// предмет, который тратит лишний бросок (крит, ожог, эхо), сдвигал бы ВСЮ
// последовательность спавнов. Тогда разница между предметами — это разная
// расстановка толпы, а не сила предмета: «Покров» показывал +51 % урона, хотя
// урона не даёт. Поэтому поток подаём сами, а точку спавна считаем от счётчика.
//
// mode: 0 — кайт по кругу 120 px (темп и орда), 1 — сбор опыта (опыт и золото),
//       2 — смертный и стоит (живучесть), 3 — стоя в куче, бессмертный: считается
//       впитанный урон и лечение (из них выводится «сколько секунд до смерти»).
struct StreamResult {
    double damage = 0.0;
    double survived = 0.0;
    double taken = 0.0;                 // впитанный урон (режим 3)
    double healed = 0.0;                // восстановленное здоровье (режим 3)
    double xp = 0.0;
    double gold = 0.0;
    double distance = 0.0;              // путь за кайт (ось «кайт»)
    int kills = 0;
};

static StreamResult sceneStream(Game& g, int itemId, float seconds, int mode, float hpMinute) {
    StreamResult out;
    const float dt = 1.0f / 60.0f;
    equipCarrier(g, itemId);
    // Здоровье мобов задаётся минутой забега: 300 — середина, 600 — поздняя игра.
    g.runTime = hpMinute;
    g.playerLevel = 20;                 // уровень прибит, см. комментарий выше
    g.xpToNext = g.getXpRequirementForLevel(g.playerLevel);
    g.playerXp = 0;
    // Бессмертие только там, где смерть не измеряется: в режиме 3 (защита) и в
    // режиме 2 (живучесть) здоровье должно падать по-настоящему.
    g.cheatGodMode = (mode == 0 || mode == 1);
    g.playerHp = g.playerMaxHp;
    // Режим 3 — «куча вплотную»: один моб прибит к герою и бьёт ровно на 20.
    // Так защита меряется одинаково для всех: без толпы, которая решает сама,
    // кого ударить, и без хаоса «когда именно наступит смерть».
    // Удар в 20 не давал процентной защите шанса: «Покров» (делит удар пополам)
    // снимал 10 HP и не менял «сколько прожито» вовсе, а «Клятва» с +15 брони
    // растягивала замер в четыре раза. 35 ближе к поздним волнам: и броня, и
    // процентное поглощение видны, но ни одно не решает всё.
    const float HIT_DAMAGE = 20.0f;
    if (mode == 1) {
        // Прокачки в сцене экономики отключены: «опыт» — это собранный опыт, а
        // не сумма порогов уровней. Иначе +5 опыта от одного самоцвета решало,
        // попали ли вы в следующий уровень, и предмет без влияния на опыт
        // показывал ±11 %.
        g.xpToNext = 2000000000;
        // Опыт и золото без боя: 24 самоцвета на 8 и 16 золотых монет по 10.
        // Так замер не зависит от того, кого и где убили, и «Книга мудрости»
        // видна как множитель опыта, а «Магнит» — как скорость сбора.
        for (int i = 0; i < 72; ++i) {
            Pickup p{};
            const float ang = i * 2.39996323f;
            const float radius = 70.0f + (float)((i * 47) % 330);
            p.x = cosf(ang) * radius;
            p.y = sinf(ang) * radius;
            p.type = (i % 4 == 3) ? 3 : (i % 3);
            p.value = (i % 4 == 3) ? 10 : 5;
            p.bobTime = 0.0f;
            p.magnetized = false;
            p.age = 0.0f;
            g.pickups.push_back(p);
        }
    }
    if (mode == 3) {
        g.spawnMonster(0, false, false);
        if (!g.monsters.empty()) g.monsters.back().hp = g.monsters.back().maxHp = INF_HP;
    }
    const float moveSpeed = HERO_DEFS[0].moveSpeed;
    float orbit = 0.0f;
    float spawnAccum = 0.0f;
    int spawnIndex = 0;
    const int frames = (int)(seconds / dt);
    for (int f = 0; f < frames; ++f) {
        // Поток: 8 мобов в секунду, положение — золотой угол и ступенчатый радиус.
        if (mode == 3) {
            for (Monster& m : g.monsters) {
                m.x = g.playerX;
                m.y = g.playerY;
                m.vx = m.vy = 0.0f;
                m.damage = HIT_DAMAGE;
                m.hp = m.maxHp = INF_HP;
            }
        }
        if (mode == 0) spawnAccum += STREAM_RATE * dt;
        while (mode == 0 && spawnAccum >= 1.0f && (int)g.monsters.size() < STREAM_CAP) {
            spawnAccum -= 1.0f;
            g.spawnMonster((spawnIndex % 6 == 5) ? 3 : 0, spawnIndex % 29 == 28, false);
            if (!g.monsters.empty()) {
                Monster& m = g.monsters.back();
                const float ang = spawnIndex * 2.39996323f;
                const float radius = 260.0f + (float)((spawnIndex * 37) % 200);
                m.x = cosf(ang) * radius;
                m.y = sinf(ang) * radius;
                m.vx = m.vy = 0.0f;
            }
            ++spawnIndex;
        }
        if (mode == 1) {
            // Герой идёт по расширяющейся спирали через всё поле, а не к
            // ближайшему самоцвету: «дойти до каждого» делало радиус сбора
            // бесполезным, и «Магнитный амулет» (+45 % радиуса за ранг) мерился
            // теми же +10 %, что и предмет без радиуса. Со спиралью видно, что
            // радиус — это сколько добычи попало в карман по дороге.
            // Два прохода через поле (туда и обратно), а не спираль по всему кругу:
            // спираль собирала почти всё даже с базовым радиусом 95 px, и «Магнит»
            // мерился нулём. Два коридора дают базовому радиусу примерно половину
            // поля, а увеличенному — почти всё. И это не делает радиус вечным
            // победителем: опыт и золото всё равно копятся по своим множителям.
            const float progress = (float)f / (float)std::max(1, frames);
            if (progress < 0.45f) {
                g.playerX = -380.0f + 760.0f * (progress / 0.45f);
                g.playerY = -140.0f;
            } else if (progress < 0.9f) {
                g.playerX = 380.0f - 760.0f * ((progress - 0.45f) / 0.45f);
                g.playerY = 140.0f;
            } else {
                g.playerX = -380.0f;
                g.playerY = 140.0f;
            }
        } else if (mode == 0) {
            orbit += moveSpeed / 120.0f * dt;
            g.playerX = cosf(orbit) * 120.0f;
            g.playerY = sinf(orbit) * 120.0f;
        }
        // Рывок — как у осторожного игрока: когда враг уже вплотную. Готовность
        // берём у самой игры, а не своим таймером: иначе «Руна Возврата»
        // (рывок возвращается за убийства) не могла проявиться вовсе — стенд
        // всегда ждал свои 2,5 с и не замечал, что рывок уже готов.
        if (mode != 3) {
            bool close = false;
            for (const Monster& m : g.monsters) {
                const float dx = m.x - g.playerX, dy = m.y - g.playerY;
                if (dx * dx + dy * dy < 70.0f * 70.0f) { close = true; break; }
            }
            if (close) g.triggerDash();
        }
        if (g.state == STATE_LEVELUP) {
            out.xp += g.xpToNext;
            g.state = STATE_PLAYING;
        }
        g.state = STATE_PLAYING;
        const float hpBefore = g.playerHp;
        const float pxBefore = g.playerX, pyBefore = g.playerY;
        g.update(dt);
        // Путь считаем по реальному смещению героя: так ось видит и скорость,
        // и рывок (он сдвигает героя внутри update, а не формулой сцены).
        out.distance += hypot((double)g.playerX - pxBefore, (double)g.playerY - pyBefore);
        out.survived += dt;
        // Урон и лечение считаем по изменению здоровья: так метрика видит и
        // броню, и щит, и регенерацию, не зная, откуда они взялись.
        if (g.playerHp < hpBefore) out.taken += (double)(hpBefore - g.playerHp);
        if (g.playerHp > hpBefore) out.healed += (double)(g.playerHp - hpBefore);
        if ((mode == 2 || mode == 3) && g.playerHp <= 0.0f) break;
    }
    out.damage = g.totalDamageDealt;
    out.survived = out.survived;
    out.kills = g.kills;
    out.xp += g.playerXp;
    out.gold = g.runGold;
    return out;
}

// ── Оси «босс» и «элита»: ДПС по одной мишени 30 px ─────────────────────────
// Мишень — обычный моб с нужным флагом: урон по боссу намеренно берётся по
// цели, помеченной isBoss, иначе отдельная ветка «+20 % боссам» (Клятва)
// мерилась бы нулём, а флаг элиты — никак не влиял бы на «элиту».
static double sceneDps(Game& g, int itemId, float seconds, bool elite, bool boss) {
    const float dt = 1.0f / 60.0f;
    equipCarrier(g, itemId);
    g.runTime = 300.0f;
    g.cheatGodMode = true;
    g.spawnMonster(0, elite, false);
    if (g.monsters.empty()) return 0.0;
    if (boss) {
        Monster& dummy = g.monsters.back();
        dummy.isBoss = true;
        dummy.bossType = 0;
        dummy.bossPhase = 1;
        dummy.abilityTimer = 1.0e9f;
        dummy.specialTimer = 1.0e9f;
    }
    // Мишень ищем по uniqueId, а не по «последнему в векторе»: за время замера
    // в monsters могут появиться чужие записи (осколки, свита), и разница
    // «бесконечное здоровье минус чужое» выдавала абсурдные проценты.
    const int dummyId = g.monsters.back().uniqueId;
    double damage = 0.0;
    const int frames = (int)(seconds / dt);
    auto touchDummy = [&](bool count) {
        for (Monster& t : g.monsters) {
            if (t.uniqueId != dummyId) continue;
            if (count && t.hp < INF_HP) damage += (double)INF_HP - (double)t.hp;
            t.x = 90.0f; t.y = 0.0f;
            t.vx = t.vy = 0.0f;
            t.hp = t.maxHp = INF_HP;
            t.lastMoveX = t.lastMoveY = 0.0f;
        }
    };
    touchDummy(false);
    for (int f = 0; f < frames; ++f) {
        if (g.state == STATE_LEVELUP) g.state = STATE_PLAYING;
        g.state = STATE_PLAYING;
        g.update(dt);
        touchDummy(true);
    }
    return damage / std::max(0.001f, seconds);
}

// ── Предметы, которым эти сцены не дают триггера ───────────────────────────
// Стенд мерит восемь «общих» осей: урон по волне, путь, живучесть, экономику и
// ДПС по крупной цели. У части предметов сила включается только от события,
// которого в этих сценах нет (снаряд врага, резонатор, заморозка, три оружия).
// Ноль на таких осях — свойство сцены, а не приговор предмету: их проверяют
// build_probe (сборки «Казнь», «Возмездие», «Морозный взрыв») и точечные сцены.
// Печатаем их отдельным списком, чтобы приговор «слабый» не был приговором стенду.
struct SceneBlind { int id; const char* reason; };
static const SceneBlind SCENE_BLIND[] = {
    {22, "лечение за убийство элиты/босса: в потоке элит почти нет, стражей нет"},
    {30, "избыток лечения: носитель не лечится, лечить нечем"},
    {33, "нужны три разных оружия: у носителя одно"},
    {35, "нужна заморозка: у носителя клинки, а не лёд"},
    {37, "казнь ниже порога HP: мишень с бесконечным здоровьем"},
    {38, "потеря HP героем: в сценах урона герой бессмертен"},
    {36, "вражеский снаряд: в потоке стрелков мало, выстрелы редко долетают"},
    {31, "нужен добивающий удар: в потоке мобы 5-й минуты с 8 тыс. HP, перебор почти не случается"},
    {39, "резонатор: в сценах он не спавнится"},
};
static const char* sceneBlindReason(int id) {
    for (const SceneBlind& b : SCENE_BLIND) if (b.id == id) return b.reason;
    return nullptr;
}

static Profile measureItem(Game& g, int itemId, float seconds) {
    Profile p;
    // Длительность сцен нарочно разная: «живучесть» за 12 с не набирает сигнала
    // (герой ещё жив), а поток за 240 с упирается в предел живых врагов.
    const float tempoSec = std::max(10.0f, seconds * 2.0f);      // 40 с при 20
    const float crowdSec = std::max(20.0f, seconds * 4.0f);      // 80 с при 20
    const float surviveSec = std::max(30.0f, seconds * 3.0f);    // 60 с при 20
    const float dpsSec = std::max(8.0f, seconds * 0.90f);        // 18 с при 20
    // Три зерна: одно зерно давало ±14 % на «темпе» у артефакта, который вообще
    // ничего не делает, — просто из-за последовательности критов.
    static const unsigned seeds[] = {20260201u, 20260203u, 20260205u, 20260207u, 20260209u};
    const int seedCount = (int)(sizeof(seeds) / sizeof(seeds[0]));
    for (unsigned sd : seeds) {
        srand((int)sd);
        p.v[AX_TEMPO] += sceneStream(g, itemId, tempoSec, 0, 300.0f).damage / seedCount;
    }
    for (unsigned sd : seeds) {
        srand((int)sd);
        const StreamResult crowd = sceneStream(g, itemId, crowdSec, 0, 600.0f);
        p.v[AX_CROWD] += crowd.damage / seedCount;
        // Мобильность: за тот же кайт считается пройденный путь. Иначе предметы
        // «скорость/рывок» (сапоги, руна возврата) не попадали ни на одну ось и
        // выглядели мусором только потому, что стенд их не мерил.
        p.v[AX_KITE] += crowd.distance / seedCount;
    }
    for (unsigned sd : seeds) {
        srand((int)sd);
        // Сколько секунд герой держится под ровным обстрелом. Метрика видит
        // броню, щит, запас здоровья и воскрешение сразу, а не по отдельности.
        p.v[AX_SURVIVE] += sceneStream(g, itemId, surviveSec, 3, 300.0f).survived / seedCount;
    }
    for (unsigned sd : seeds) {
        srand((int)sd);
        const StreamResult econ = sceneStream(g, itemId, crowdSec, 1, 300.0f);
        p.v[AX_XP] += econ.xp / seedCount;
        p.v[AX_GOLD] += econ.gold / seedCount;
    }
    for (unsigned sd : seeds) {
        srand((int)sd);
        p.v[AX_BOSS] += sceneDps(g, itemId, dpsSec, false, true) / seedCount;
        srand((int)sd);
        p.v[AX_ELITE] += sceneDps(g, itemId, dpsSec, true, false) / seedCount;
    }
    return p;
}

int main(int argc, char** argv) {
    float seconds = argc > 1 ? (float)atof(argv[1]) : 20.0f;
    FILE* csv = nullptr;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv = fopen(argv[++i], "w");
        else if (argv[i][0] != '-') csv = fopen(argv[i], "w");
    }
    Game g;
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;

    printf("Стенд баланса: у каждого предмета своя ниша, ни один не решает всё.\n");
    printf("Носитель — серебряные клинки 4-го ранга, предмет на максимуме ранга.\n");
    printf("Замер %.0f с на забег, два зерна rand на строку.\n\n", seconds);

    srand(20260201);
    const Profile base = measureItem(g, -1, seconds);

    struct Row {
        int id;
        Profile p;
        double gain[AX_COUNT] = {0.0};
        int bestAxis = 0;
        double bestGain = -1e9;
        double width = 0.0;
        int wins = 0;
    };
    std::vector<Row> rows;
    for (int id = 7; id < ITEM_COUNT; ++id) {
        if (ITEM_DEFS[id].isWeapon) continue;            // оружие тиров — стенд оружия
        if (g.getItemMaxLevel(id) <= 0) continue;
        Row r;
        r.id = id;
        srand(20260201);
        r.p = measureItem(g, id, seconds);
        for (int a = 0; a < AX_COUNT; ++a) {
            const double b = std::max(1e-6, base.v[a]);
            r.gain[a] = 100.0 * (r.p.v[a] / b - 1.0);
            r.width += r.gain[a] / AX_COUNT;
            if (r.gain[a] > r.bestGain) { r.bestGain = r.gain[a]; r.bestAxis = a; }
        }
        rows.push_back(r);
    }

    printf("база без артефакта:");
    for (int a = 0; a < AX_COUNT; ++a) printf("  %s %.0f", axisName[a], base.v[a]);
    printf("\n\n");

    // Кто первый на каждой оси. Считаем до печати: колонка «лучших осей» нужна
    // в той же таблице.
    for (int a = 0; a < AX_COUNT; ++a) {
        double best = -1e9;
        for (const Row& r : rows) best = std::max(best, r.gain[a]);
        for (Row& r : rows) if (r.gain[a] >= best - 0.5) ++r.wins;
    }

    printf("%-30s", "предмет");
    for (int a = 0; a < AX_COUNT; ++a) printf("%9s", axisName[a]);
    printf("%9s %8s %11s\n", "ниша", "широта", "лучших осей");
    for (const Row& r : rows) {
        printf("%-30s", ITEM_DEFS[r.id].name);
        for (int a = 0; a < AX_COUNT; ++a) printf("%8.0f%%", r.gain[a]);
        printf("%9s %7.0f%%%11d\n", axisName[r.bestAxis], r.width, r.wins);
        if (csv) {
            fprintf(csv, "%s", ITEM_DEFS[r.id].name);
            for (int a = 0; a < AX_COUNT; ++a) fprintf(csv, ",%.1f", r.gain[a]);
            fprintf(csv, ",%.1f,%.1f,%d\n", r.bestGain, r.width, r.wins);
        }
    }
    printf("\nпервые на оси:\n");
    for (int a = 0; a < AX_COUNT; ++a) {
        double best = -1e9;
        for (const Row& r : rows) best = std::max(best, r.gain[a]);
        printf("  %-10s ", axisName[a]);
        bool first = true;
        for (const Row& r : rows) if (r.gain[a] >= best - 0.5) {
            printf("%s%s %+.0f%%", first ? "" : ", ", ITEM_DEFS[r.id].name, r.gain[a]);
            first = false;
        }
        printf("\n");
    }

    // Проверки: «однозначно сильный» — первый на трёх и более осях;
    // «однозначно слабый» — ни на одной оси не догоняет лучшего хотя бы наполовину.
    int failures = 0;
    printf("\nоднозначно сильные (первый на 3+ осях из %d):\n", AX_COUNT);
    bool anyStrong = false;
    for (const Row& r : rows) {
        if (r.wins >= 3) {
            printf("  %-30s первый на %d осях\n", ITEM_DEFS[r.id].name, r.wins);
            anyStrong = true;
            ++failures;
        }
    }
    if (!anyStrong) printf("  нет\n");

    // Порог приговора — абсолютный, а не «половина лучшего»: иначе «Клеймо»
    // (+42 % к элите) оказывалось «слабым» только потому, что универсальная
    // перчатка даёт на той же оси +91 %. Предмет слаб, если нигде не даёт
    // хотя бы 10 % — то есть слота не чувствует вообще.
    const double WEAK_GAIN = 10.0;
    printf("однозначно слабые (нигде не дают +%.0f %%):\n", WEAK_GAIN);
    bool anyWeak = false;
    for (const Row& r : rows) {
        if (sceneBlindReason(r.id)) continue;
        bool niche = false;
        for (int a = 0; a < AX_COUNT; ++a) if (r.gain[a] >= WEAK_GAIN) niche = true;
        if (!niche) {
            printf("  %-30s лучшая ось %s %+.0f%%\n", ITEM_DEFS[r.id].name, axisName[r.bestAxis], r.bestGain);
            anyWeak = true;
            ++failures;
        }
    }
    if (!anyWeak) printf("  нет\n");

    printf("\nне меряется этими сценами (нужен триггер, стенд не приговор):\n");
    for (const Row& r : rows) {
        const char* why = sceneBlindReason(r.id);
        if (!why) continue;
        printf("  %-30s %s\n", ITEM_DEFS[r.id].name, why);
    }

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе замеры выполнены\n");
    return 0;
}
