// ── Стенд оружия и способностей: урон без окна и без чужих бонусов ────────────
//
// Семь активных оружий, четыре оружия тиров (24–27) и суперумение проверяются
// в одинаковых условиях: герой стоит в центре, мишени держатся на месте с
// бесконечным запасом здоровья, крит-артефакты не надеты. Тогда видно то, что
// в игре тонет в шуме: сколько урона даёт оружие на ранг, сколько целей задевает
// одно срабатывание, растёт ли урон от ранга, и не «воюет» ли оружие само с
// собой (молния бьёт одну цель шесть раз вместо цепи из шести).
//
//   • 220 px — одна мишень на дистанции: урон «по площади» на средней дистанции;
//   • 90 px — одна мишень вплотную: проверка, достаёт ли оружие ближнего боя;
//   • 20 px — живой моб в упор (не прибит к точке): сюда попадают только те,
//     у кого нет «мёртвой середины». Именно этого столбца не хватило прошлому
//     аудиту: вихрь на 4-м ранге бил в 90 px и не бил в 20;
//   • в движ. — одна мишень обходит героя по кругу 110 px/с: проверка упреждения
//     и наведения (стоячую цель любое оружие бьёт, идущую — не всякое);
//   • толпа — 24 мишени на кольце 120–330 px: урон по площади;
//   • за удар — сколько урона приносит одна активация;
//   • целей за удар — сколько разных мишеней задела одна активация;
//   • сраб/м — сколько раз в минуту оружие реально срабатывает.
//
// Отдельные блоки: оружие тиров 24–27, пассивки (усреднение по трём зёрнам,
// иначе крит-бросок выдаёт шум за вклад артефакта) и суперумение.
//
// Важно про цифры: мишени бессмертны, поэтому DPS — это потолок урона, а не
// «сколько нужно ударов». В игре обычный моб на 5-й минуте имеет около 50 HP,
// так что там решает не потолок, а охват (сколько целей задевает активация).
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/weapon_probe.cpp -o build/weapon_probe
//     build/weapon_probe                       # 20 с на замер, все оружия и ранги
//     build/weapon_probe 10                    # быстрее
//     build/weapon_probe 20 3                  # только «Цепная молния»
//     build/weapon_probe 20 -1 --csv out.csv   # та же таблица в CSV для графика
//
// Код возврата 1, если оружие не сработало ни разу, если урон падает от ранга,
// если цепь бьёт меньше двух разных целей при живой толпе, если оружие не
// попадает по движущейся цели, если пассивка на максимуме ухудшает урон или
// если суперумение не задело никого.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

// Запас здоровья мишени. 1e9 для float не годится: шаг float на 1e9 — 64 единицы,
    // и удар на 27 урона «не менял» hp, хотя реально проходил (стенд показывал 0 DPS).
static const float INF_HP = 1.0e6f;
static const int CROWD = 24;

struct Result {
    int casts = 0;
    int shots = 0;
    double damage = 0.0;
    double seconds = 0.0;
    double targetsPerCast = 0.0;   // сколько разных мишеней задела одна активация
    int castFrames = 0;
    int intervals = 0;
    double dps() const { return seconds > 0.0 ? damage / seconds : 0.0; }
    double perCast() const { return casts > 0 ? damage / casts : 0.0; }
};

// Мишени: тип 0 (обычный моб), бесконечное здоровье, стоят там, куда их поставили.
struct Dummy { float x, y; };

static std::vector<Dummy> layoutTargets(int count, float inner, float outer) {
    std::vector<Dummy> out;
    for (int i = 0; i < count; ++i) {
        const float t = count > 1 ? (float)i / (float)(count - 1) : 0.5f;
        const float r = inner + (outer - inner) * t;
        const float a = i * 2.39996323f;             // золотой угол: без колец и слипания
        out.push_back({cosf(a) * r, sinf(a) * r});
    }
    return out;
}

// Один замер: ставит оружие, мишени и гоняет игру, пока не выйдет время.
static Result measure(Game& g, int itemId, int level, bool evolved, int targetCount,
                      float inner, float outer, float seconds, float orbitSpeed = 0.0f,
                      int passiveId = -1, bool pinTargets = true) {
    Result res;
    const float dt = 1.0f / 60.0f;

    // Чистая арена: ни волн, ни отрядов, ни стражей, ни резонаторов, ни порталов.
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.bossHazards.clear();
    g.particles.clear();
    g.lightningArcs.clear();
    g.frostWaves.clear();
    g.balance.spawnMultiplier = 0.0f;
    g.squadTimer = 1.0e9f;
    g.ratTimer = 1.0e9f;
    g.resonatorSpawnTimer = 1.0e9f;
    g.portalSpawnTimer = 1.0e9f;
    g.nextBossIndex = 999;
    g.eventCooldown = 1.0e9f;        // «тихая охота» не должна подмешивать своих мобов
    g.huntActive = false;
    // Предметы пола — не оружие: их урон подмешивался в замер и давал разброс
    // ±3 %, из-за которого артефакт выглядел «хуже базы». Стенд меряет оружие.
    g.clearFloorDevices();
    g.floorDeviceSpawnTimer = 1.0e9f;
    g.pendingResonatorBurst = 0;
    g.pendingMatryoshkaSplits.clear();
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.playerX = 0.0f;
    g.playerY = 0.0f;
    g.playerHp = g.playerMaxHp;
    g.playerHurtTimer = 0.0f;
    g.runTime = 0.0f;

    for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
    for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
    g.itemLevels[itemId] = level;
    if (passiveId >= 0) g.itemLevels[passiveId] = g.getItemMaxLevel(passiveId);
    if (itemId >= 0 && itemId < 7) {
        g.evolvedWeapons[itemId] = evolved;
        g.weaponTimers[itemId] = 0.0f;          // первый удар — сразу, а не через таймер
    }
    for (int i = 0; i < 4; ++i) g.tierWeaponTimers[i] = 0.0f;

    std::vector<Dummy> spots = layoutTargets(targetCount, inner, outer);
    std::vector<int> dummyIds;
    std::vector<int> dummySlot;   // uniqueId -> индекс в spots
    for (size_t i = 0; i < spots.size(); ++i) {
        const Dummy& d = spots[i];
        g.spawnMonster(0, false, false);
        if (g.monsters.empty()) break;
        Monster& m = g.monsters.back();
        m.x = d.x; m.y = d.y;
        m.vx = m.vy = 0.0f;
        m.hp = m.maxHp = INF_HP;
        m.radius = 18.0f;
        m.isElite = false;
        dummyIds.push_back(m.uniqueId);
        dummySlot.push_back((int)i);
    }

    const int frames = (int)(seconds / dt);
    // «Целей за удар» считаем по интервалу между активациями: снаряд долетает позже
    // выстрела, поэтому смотреть надо не кадр выстрела, а всё, что задето до
    // следующей активации.
    std::vector<char> hitMark(spots.size(), 0);
    int intervalHits = 0;
    int pendingSinceCast = 0;
    for (int f = 0; f < frames; ++f) {
        // Движущаяся мишень: моб обходит героя по кругу со своей скоростью —
        // так проверяется, попадает ли оружие по цели, которая не ждёт.
        if (orbitSpeed > 0.0f) {
            const float ang = orbitSpeed / std::max(1.0f, inner) * (float)f * dt;
            spots[0] = {cosf(ang) * inner, sinf(ang) * inner};
        }
        // Мишени стоят; здоровье восстанавливаем, поэтому «урон» = сколько сняли.
        for (Monster& m : g.monsters) {
            auto it = std::find(dummyIds.begin(), dummyIds.end(), m.uniqueId);
            if (it == dummyIds.end()) continue;
            const size_t slot = (size_t)(it - dummyIds.begin());
            const Dummy& d = spots[dummySlot[slot]];
            // В сценарии «в упор» мишень не прибита к точке: живой моб подходит
            // и упирается в героя сам. Иначе оружие, которое выталкивает врага
            // из мёртвой середины (вихрь), выглядело бы сломанным.
            if (pinTargets || slot != 0) {
                m.x = d.x; m.y = d.y;
                m.vx = m.vy = 0.0f;
            }
            m.hp = INF_HP;
            m.maxHp = INF_HP;
            // Мишень двигаем мы, а не ИИ, поэтому направление шага задаём сами:
            // игра берёт его из lastMoveX/Y (упреждение оружия считается по нему).
            if (orbitSpeed > 0.0f && slot == 0) {
                const float len = std::max(1.0f, sqrtf(d.x * d.x + d.y * d.y));
                m.lastMoveX = -d.y / len;
                m.lastMoveY = d.x / len;
            } else {
                m.lastMoveX = m.lastMoveY = 0.0f;
            }
        }
        const float timerBefore = (itemId >= 0 && itemId < 7) ? g.weaponTimers[itemId] : -1.0f;
        const float tierBefore[4] = {g.tierWeaponTimers[0], g.tierWeaponTimers[1],
                                     g.tierWeaponTimers[2], g.tierWeaponTimers[3]};
        g.state = STATE_PLAYING;
        g.update(dt);
        if (g.state != STATE_PLAYING) { g.state = STATE_PLAYING; }

        bool cast = false;
        if (itemId >= 0 && itemId < 7) {
            const bool fired = g.weaponTimers[itemId] > timerBefore + 1.0e-4f;
            if (fired) cast = true;
        } else if (itemId >= 24 && itemId <= 27) {
            const int slot = itemId - 24;
            if (g.tierWeaponTimers[slot] > tierBefore[slot] + 1.0e-4f) cast = true;
        }

        double frameDamage = 0.0;
        int touched = 0;
        for (const Monster& m : g.monsters) {
            auto it = std::find(dummyIds.begin(), dummyIds.end(), m.uniqueId);
            if (it == dummyIds.end()) continue;
            const double d = (double)INF_HP - (double)m.hp;
            if (d > 0.0) {
                frameDamage += d;
                ++touched;
                const size_t slot = (size_t)(it - dummyIds.begin());
                if (!hitMark[slot]) { hitMark[slot] = 1; ++intervalHits; }
            }
        }
        res.damage += frameDamage;
        if (cast) {
            ++res.casts;
            ++res.castFrames;
            // Интервал закрылся: записываем, сколько разных целей задела прошлая активация.
            if (pendingSinceCast > 0) { res.targetsPerCast += intervalHits; ++res.intervals; }
            ++pendingSinceCast;
            intervalHits = 0;
            std::fill(hitMark.begin(), hitMark.end(), 0);
        }
        res.seconds += dt;
    }
    if (res.intervals > 0) res.targetsPerCast /= (double)res.intervals;
    return res;
}

// ── Суперумение: сколько целей и урона даёт одна активация ───────────────────
static void measureSuper(Game& g, int targets, float radius, double* damage, int* touched) {
    g.clearFloorDevices();
    g.floorDeviceSpawnTimer = 1.0e9f;
    g.monsters.clear();
    g.projectiles.clear();
    g.pickups.clear();
    g.state = STATE_PLAYING;
    g.cheatGodMode = true;
    g.playerX = 0.0f; g.playerY = 0.0f;
    g.playerHp = g.playerMaxHp;
    for (int i = 0; i < targets; ++i) {
        const float a = i * 2.39996323f;
        const float r = radius;
        g.spawnMonster(0, false, false);
        if (g.monsters.empty()) break;
        Monster& m = g.monsters.back();
        m.x = cosf(a) * r; m.y = sinf(a) * r;
        m.hp = m.maxHp = INF_HP;
        m.radius = 18.0f;
    }
    // Супер меряем на 25-м уровне героя: на первом уровне он и не должен
    // впечатлять, а к середине забега у него уже есть цена в 12% здоровья.
    g.playerLevel = 25;
    g.superCharge = 100.0f;
    g.triggerUltimate();
    *damage = 0.0; *touched = 0;
    for (const Monster& m : g.monsters) {
        const double d = (double)INF_HP - (double)m.hp;
        if (d > 0.0) { *damage += d; ++*touched; }
    }
}

// Почему артефакт не даёт урона в этом стенде. Пустая строка — даёт.
// Стенд стоячий и с бессмертными мишенями, поэтому срабатывания «по событию»
// здесь не видны, и это надо писать прямо, а не выдавать за нулевой вклад.
static const char* passiveReason(int id) {
    switch (id) {
        case 7:  return "опыт: +10% за ранг";
        case 8:  return "скорость шага";
        case 9:  return "радиус сбора";
        case 10: return "HP и регенерация";
        case 13: return "золото";
        case 14: return "воскрешение";
        case 15: return "заряд супера";
        case 19: return "урон боссам, боссов нет";
        case 20: return "урон элитам, элит нет";
        case 21: return "возврат рывка";
        case 22: return "лечение с элит";
        case 23: return "щит";
        case 28: return "эхо рывка";
        case 30: return "щит из лечения";
        case 31: return "цепь от перебора урона";
        case 32: return "искры от самоцветов";
        case 33: return "нужно второе оружие";
        case 34: return "ожог 4-6 по 3-7 урона";
        case 35: return "откол после заморозки";
        case 36: return "отражение снаряда";
        case 37: return "добивание, мишени бессмертны";
        case 38: return "нужно потерять HP";
        case 39: return "заряд супера";
        case 40: return "след от шага, стенд стоячий";
        default: return "";
    }
}

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? (float)atof(argv[1]) : 20.0f;
    const int onlyWeapon = argc > 2 ? atoi(argv[2]) : -1;
    const char* csvPath = (argc > 4 && !strcmp(argv[3], "--csv")) ? argv[4] : nullptr;
    FILE* csv = csvPath ? fopen(csvPath, "w") : nullptr;
    if (csv) fprintf(csv, "weapon,level,far220,close90,near20,moving,crowd,perCast,castsPerMin,targetsPerCast\n");

    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r; r.resize(SCREEN_W, SCREEN_H);
    Game g;
    g.simulationMode = true;        // стенд не пишет сейв
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;

    static const char* weaponName[7] = {
        "Серебряные клинки", "Огненный шар", "Рунический вихрь",
        "Цепная молния", "Кровавая аура", "Боевой топор", "Ледяное кольцо"
    };
    static const char* tierName[4] = {
        "Кристальный залп (24)", "Рунический импульс (25)",
        "Разломное копьё (26)", "Пепельная звезда (27)"
    };

    int failures = 0;
    printf("Стенд оружия: герой стоит, мишени с бесконечным здоровьем, артефактов нет.\n");
    printf("Замер %.0f с на строку, зерно rand фиксировано, крит только базовые 8%%.\n", seconds);
    printf("Мишени: 220 px и 90 px — одна цель на месте; «в движ.» — одна цель по кругу 110 px/с;\n");
    printf("толпа — %d целей на кольце 120–330 px. Урон = снятое здоровье мишеней.\n\n", CROWD);
    printf("%-30s %5s %8s %8s %8s %8s %8s %9s %7s\n",
           "оружие", "ранг", "220 px", "90 px", "20 px", "в движ.", "толпа", "за удар", "сраб/м");

    struct Row {
        const char* name; int level;
        Result far, close, near, moving, crowd;
    };
    std::vector<Row> rows;

    for (int w = 0; w < 7; ++w) {
        if (onlyWeapon >= 0 && onlyWeapon != w) continue;
        for (int rankIdx = 0; rankIdx < 4; ++rankIdx) {
            static const int levels[4] = {1, 4, 8, 9};
            const int level = levels[rankIdx];
            const bool evolved = (rankIdx == 3);
            srand(20260201);
            Result far = measure(g, w, level, evolved, 1, 220.0f, 220.0f, seconds);
            srand(20260201);
            Result close = measure(g, w, level, evolved, 1, 90.0f, 90.0f, seconds);
            srand(20260201);
            Result near = measure(g, w, level, evolved, 1, 20.0f, 20.0f, seconds, 0.0f, -1, false);
            srand(20260201);
            Result moving = measure(g, w, level, evolved, 1, 220.0f, 220.0f, seconds, 110.0f);
            srand(20260201);
            Result crowd = measure(g, w, level, evolved, CROWD, 120.0f, 330.0f, seconds);
            char label[64];
            if (evolved) snprintf(label, sizeof(label), "%s (эволюция)", weaponName[w]);
            else snprintf(label, sizeof(label), "%s", weaponName[w]);
            printf("%-30s %5d %8.0f %8.0f %8.0f %8.0f %8.0f %9.0f %7.1f\n",
                   label, level, far.dps(), close.dps(), near.dps(), moving.dps(), crowd.dps(),
                   crowd.perCast(), crowd.casts / std::max(0.001, crowd.seconds) * 60.0);
            if (csv) fprintf(csv, "%s,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.1f,%.1f\n", weaponName[w], level,
                             far.dps(), close.dps(), near.dps(), moving.dps(), crowd.dps(), crowd.perCast(),
                             crowd.casts / std::max(0.001, crowd.seconds) * 60.0,
                             crowd.targetsPerCast);
            rows.push_back({weaponName[w], level, far, close, near, moving, crowd});
        }
    }

    // ── Рост от ранга ────────────────────────────────────────────────────────
    printf("\nрост от ранга 1 к рангу 8 (у одной цели в 90 px / в толпе):\n");
    for (int w = 0; w < 7; ++w) {
        if (onlyWeapon >= 0 && onlyWeapon != w) continue;
        const Row* row1 = nullptr; const Row* row8 = nullptr;
        for (const Row& row : rows) {
            if (strcmp(row.name, weaponName[w]) != 0) continue;
            if (row.level == 1) row1 = &row;
            if (row.level == 8) row8 = &row;
        }
        if (!row1 || !row8) continue;
        auto ratio = [](double a, double b, char* buf, size_t n, const char* where) {
            if (a < 1.0) snprintf(buf, n, "нет урона %s", where);
            else snprintf(buf, n, "x%.1f", b / a);
        };
        char closeText[48], crowdText[48];
        ratio(row1->close.dps(), row8->close.dps(), closeText, sizeof(closeText), "в 90 px");
        ratio(row1->crowd.dps(), row8->crowd.dps(), crowdText, sizeof(crowdText), "в толпе");
        printf("  %-22s %-10s  толпа %s\n", weaponName[w], closeText, crowdText);
        if (row8->close.dps() + row8->crowd.dps() <= 0.0) {
            printf("    оружие на 8-м ранге не наносит урона ни в одном сценарии\n");
            ++failures;
        }
        if (row1->close.dps() > 5.0 && row8->close.dps() < row1->close.dps() * 0.9) {
            printf("    урон падает от ранга: ранг 1 %.0f, ранг 8 %.0f\n", row1->close.dps(), row8->close.dps());
            ++failures;
        }
    }

    // ── Сколько целей задевает одна активация ───────────────────────────────
    printf("\nцелей за одну активацию (толпа %d, ранг 8 / эволюция):\n", CROWD);
    for (const Row& row : rows) {
        if (row.level != 8 && row.level != 9) continue;
        if (onlyWeapon >= 0 && strcmp(row.name, weaponName[std::max(0, onlyWeapon)]) != 0) continue;
        printf("  %-22s ранг %d: %.1f\n", row.name, row.level, row.crowd.targetsPerCast);
    }
    // Оружие, которое не бьёт того, кто стоит в упор, — сломанное оружие:
    // именно так вихрь на 4-м ранге пропускал врагов в мёртвой середине.
    for (const Row& row : rows) {
        if (row.level != 4 && row.level != 8) continue;
        if (row.near.dps() < 1.0) {
            printf("  %s на ранге %d не наносит урона мишени в 20 px\n", row.name, row.level);
            ++failures;
        }
    }

    // Обещание «цепной» молнии: одна активация должна задевать больше одной цели.
    for (const Row& row : rows) {
        if (strcmp(row.name, weaponName[3]) != 0 || row.level != 8) continue;
        if (row.crowd.targetsPerCast < 1.5) {
            printf("  цепная молния бьёт %.1f целей за активацию — это не цепь\n", row.crowd.targetsPerCast);
            ++failures;
        }
    }
    // Обещание «самонаводящихся» клинков: по движущейся цели урон не должен
    // проваливаться относительно стоящей (снаряд обязан доворачивать).
    for (const Row& row : rows) {
        if (strcmp(row.name, weaponName[0]) != 0 || row.level != 8) continue;
        if (row.moving.dps() < row.far.dps() * 0.8) {
            printf("  клинки теряют %.0f%% урона по движущейся цели (%.0f против %.0f) — не наводятся\n",
                   100.0 * (1.0 - row.moving.dps() / std::max(1.0, row.far.dps())),
                   row.moving.dps(), row.far.dps());
            ++failures;
        }
    }
    // Огненный шар обязан попадать и по движущейся цели: раньше снаряд, которому
    // не хватало жизни, исчезал в воздухе без взрыва.
    for (const Row& row : rows) {
        if (strcmp(row.name, weaponName[1]) != 0 || row.level != 8) continue;
        if (row.moving.dps() < 1.0) {
            printf("  огненный шар не наносит урона по движущейся цели\n");
            ++failures;
        }
    }

    // ── Оружие тиров ────────────────────────────────────────────────────────
    printf("\nоружие тиров (уровень 8):\n");
    printf("%-30s %8s %8s %8s %8s %8s %9s %7s\n", "оружие", "220 px", "90 px", "20 px", "в движ.", "толпа", "за удар", "сраб/м");
    for (int t = 0; t < 4; ++t) {
        const int id = 24 + t;
        if (onlyWeapon >= 0 && onlyWeapon != id) continue;
        if (onlyWeapon >= 0 && onlyWeapon < 7) break;
        srand(20260201);
        Result far = measure(g, id, 8, false, 1, 220.0f, 220.0f, seconds);
        srand(20260201);
        Result close = measure(g, id, 8, false, 1, 90.0f, 90.0f, seconds);
        srand(20260201);
        Result moving = measure(g, id, 8, false, 1, 220.0f, 220.0f, seconds, 110.0f);
        srand(20260201);
        Result crowd = measure(g, id, 8, false, CROWD, 120.0f, 330.0f, seconds);
        printf("%-30s %8.0f %8.0f %8.0f %8.0f %9.0f %7.1f\n",
               tierName[t], far.dps(), close.dps(), moving.dps(), crowd.dps(), crowd.perCast(),
               crowd.casts / std::max(0.001, crowd.seconds) * 60.0);
        if (csv) fprintf(csv, "tier%d,8,%.0f,%.0f,%.0f,%.0f,%.0f,%.1f,%.1f\n", id,
                         far.dps(), close.dps(), moving.dps(), crowd.dps(), crowd.perCast(),
                         crowd.casts / std::max(0.001, crowd.seconds) * 60.0, crowd.targetsPerCast);
        if (crowd.casts == 0) { printf("%-30s   не сработало ни разу\n", ""); ++failures; }
    }

    // ── Пассивки: измеримый вклад каждого артефакта на максимуме ─────────────
    if (onlyWeapon < 0) {
        printf("\nпассивки на максимуме (клинки 4-го ранга, цель в 90 px; прирост урона к «без артефактов»):\n");
        // Крит — это бросок rand, поэтому одно зерно даёт на маленькой выборке
        // разброс в несколько процентов. Усредняем три фиксированных зерна:
        // тогда «+0 %» и «−6 %» — это вклад артефакта, а не удачный бросок.
        auto avgDps = [&](int passiveId) {
            static const unsigned seeds[3] = {20260201u, 20260202u, 20260203u};
            double sum = 0.0;
            for (unsigned sd : seeds) {
                srand((int)sd);
                sum += measure(g, 0, 4, false, 1, 90.0f, 90.0f, seconds, 0.0f, passiveId).dps();
            }
            return sum / 3.0;
        };
        const double baseDps = std::max(1.0, avgDps(-1));
        printf("  база без артефактов: %.0f DPS\n", baseDps);
        for (int p = 7; p < ITEM_COUNT; ++p) {
            if (ITEM_DEFS[p].isWeapon) continue;   // оружие тиров меряется в своей таблице
            if (g.getItemMaxLevel(p) <= 0) continue;
            const double dps = avgDps(p);
            const double delta = 100.0 * (dps / baseDps - 1.0);
            const char* reason = passiveReason(p);
            const char* verdict = delta >= 5.0 ? "урон"
                                : (delta > -3.0 ? (reason[0] ? reason : "не про урон")
                                                : "ХУЖЕ БАЗЫ");
            printf("  %-28s %6.0f DPS %+6.1f%%  %s\n", ITEM_DEFS[p].name, dps, delta, verdict);
            if (delta <= -3.0) ++failures;
        }
        printf("  «не про урон» — артефакт про опыт, золото, сбор, выживание или\n");
        printf("  срабатывание по событию (смерть, удар, потеря HP): в стоячем стенде\n");
        printf("  с бессмертными мишенями такого события нет, поэтому вклад не виден.\n");
    }

    // ── Суперумение ─────────────────────────────────────────────────────────
    if (onlyWeapon < 0) {
        double dmg = 0.0; int touched = 0;
        measureSuper(g, 12, 300.0f, &dmg, &touched);
        printf("\nсуперумение (25-й уровень героя): 12 целей в 300 px — урон %.0f, задето %d из 12,\n",
               dmg, touched);
        printf("  цена 12%% здоровья; на цель %.0f урона.\n", touched > 0 ? dmg / touched : 0.0);
        // С чем сравнивать: обычный моб на 5-й минуте забега.
        g.runTime = 300.0f;
        g.monsters.clear();
        g.spawnMonster(0, false, false);
        const float refHp = g.monsters.empty() ? 0.0f : g.monsters.back().maxHp;
        if (refHp > 1.0f && touched > 0)
            printf("  обычный моб на 5-й минуте: %.0f HP — супер снимает %.0f%% запаса,\n",
                   refHp, 100.0 * (dmg / touched) / refHp);
        printf("  для сравнения: ранг-1 оружие бьёт 40–80 урона в секунду, ранг-8 — 500–3000.\n");
        if (touched == 0) { printf("  суперумение не задело никого\n"); ++failures; }
    }

    if (csv) fclose(csv);
    if (failures) { printf("\nПРОВАЛОВ: %d\n", failures); return 1; }
    printf("\nвсе замеры выполнены\n");
    return 0;
}
