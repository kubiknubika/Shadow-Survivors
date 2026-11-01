// ── Стенд сорок первого предмета: «Тлеющая Тропа» ─────────────────────────────
//
// Тропа — единственный предмет, который зависит не от удара, крита, лечения или
// разрушения, а от шага. Поэтому её нельзя проверить в стоячем стенде пассивок
// (tools/weapon_probe.cpp честно печатает «не про урон»): там герой не двигается,
// и тропа не появляется вовсе. Здесь проверяется именно то, что отличает её от
// ауры: след остаётся на полу, живёт своим временем, не складывается от
// нескольких меток и вообще не горит, пока герой стоит.
//
//     g++ -O2 -std=c++17 -Isrc tools/trail_probe.cpp -o build/trail_probe
//     build/trail_probe
//
// Что проверяется:
//   • паспорт предмета: id 40, тир IV, три ранга, попадание в арсенал и в пул;
//   • открытие: тир сборки и время эндлеса;
//   • стоя на месте меток нет и урона нет;
//   • при движении метки ложатся с шагом не ближе ASH_TRAIL_SPACING и не чаще
//     ASH_TRAIL_SPAWN_INTERVAL, хвост не длиннее ASH_TRAIL_MAX_MARKS;
//   • метка живёт ASH_TRAIL_MARK_LIFE и гаснет сама;
//   • ожог идёт тактом ранга, урон кратен табличному (значит, крита нет),
//     третий ранг сильнее первого;
//   • резонатор не горит (постройка), страж горит;
//   • без предмета не появляется ничего;
//   • описание карты и советник знают про предмет.
//
// Код возврата 1, если хоть одна проверка провалилась.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

#include "renderer.h"
#include "game.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    printf("  %-4s %s\n", ok ? "OK" : "ПРОВАЛ", what);
    if (!ok) ++failures;
}

struct Probe {
    Game g;
    float dummyX = 0.0f, dummyY = 0.0f;
    float dummyHp = 1.0e6f;
    int dummyCount = 0;

    Probe() {
        g.simulationMode = true;
        g.init();
        g.gameMode = MODE_ENDLESS;
    }

    // Арена без волн, отрядов, крыс, резонаторов и порталов: мерить надо предмет,
    // а не случайный спавн. Герой бессмертен — стенд проверяет урон, а не смерть.
    void quietArena() {
        g.monsters.clear();
        g.projectiles.clear();
        g.pickups.clear();
        g.bossHazards.clear();
        g.particles.clear();
        g.floatTexts.clear();
        g.lightningArcs.clear();
        g.frostWaves.clear();
        g.shockwaves.clear();
        g.skillImpactMarks.clear();
        g.clearAshTrail();
        g.ashTrailFires = 0;
        g.balance.spawnMultiplier = 0.0f;
        g.squadTimer = 1.0e9f;
        g.ratTimer = 1.0e9f;
        g.resonatorSpawnTimer = 1.0e9f;
        g.portalSpawnTimer = 1.0e9f;
        g.nextBossIndex = 999;
        g.eventCooldown = 1.0e9f;
        g.huntActive = false;
        g.pendingResonatorBurst = 0;
        g.pendingMatryoshkaSplits.clear();
        g.state = STATE_PLAYING;
        g.cheatGodMode = true;
        g.runTime = 300.0f;
        g.playerX = 0.0f; g.playerY = 0.0f;
        g.playerHp = g.playerMaxHp;
        g.dashTimer = 0.0f;
        g.keyW = g.keyA = g.keyS = g.keyD = false;
        g.keyUp = g.keyDown = g.keyLeft = g.keyRight = false;
        for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
        for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
        dummyCount = 0;
    }

    Monster& addDummy(float x, float y, float hp = 1.0e6f) {
        g.spawnMonster(0, false, false);
        Monster& m = g.monsters.back();
        m.x = x; m.y = y;
        m.vx = m.vy = 0.0f;
        m.speed = 0.0f;
        m.damage = 0.0f;
        m.hp = m.maxHp = hp;
        m.radius = 18.0f;
        m.isElite = false;
        m.isBoss = false;
        m.isResonator = false;
        dummyX = x; dummyY = y; dummyHp = hp;
        ++dummyCount;
        return m;
    }

    // Мишень стоит там, куда её поставили: ИИ двигать её не должен, иначе
    // «ожог на месте» превращается в «ожог по бегающему мобу».
    void pinDummies() {
        if (dummyCount == 0) return;
        for (Monster& m : g.monsters) {
            if (m.isResonator) continue;
            if (m.isBoss) continue;
            m.x = dummyX; m.y = dummyY;
            m.vx = m.vy = 0.0f;
            m.lastMoveX = m.lastMoveY = 0.0f;
        }
    }

    // Возвращает, сколько раз за прогон здоровье мишени падало.
    int run(int frames, bool right = false, bool up = false) {
        const float dt = 1.0f / 60.0f;
        int burns = 0;
        float prevHp = g.monsters.empty() ? 0.0f : g.monsters.front().hp;
        for (int f = 0; f < frames; ++f) {
            g.state = STATE_PLAYING;
            g.keyD = right;
            g.keyW = up;
            pinDummies();
            g.update(dt);
            if (g.state != STATE_PLAYING) g.state = STATE_PLAYING;
            if (!g.monsters.empty()) {
                const float hp = g.monsters.front().hp;
                if (hp < prevHp - 1.0e-4f) ++burns;
                prevHp = hp;
            }
        }
        g.keyD = g.keyW = false;
        return burns;
    }

    float dummyLost() const { return dummyHp - (g.monsters.empty() ? dummyHp : g.monsters.front().hp); }
};

static float avgSpacing(const std::vector<AshTrailMark>& marks) {
    float worst = 1.0e9f;
    for (size_t i = 1; i < marks.size(); ++i) {
        const float dx = marks[i].x - marks[i - 1].x, dy = marks[i].y - marks[i - 1].y;
        worst = std::min(worst, sqrtf(dx * dx + dy * dy));
    }
    return worst;
}

static bool hasCritText(const Game& g) {
    for (const FloatingText& t : g.floatTexts)
        if (t.text.find('!') != std::string::npos) return true;
    return false;
}

int main() {
    SCREEN_W = 1280; SCREEN_H = 720;
    Renderer r;
    r.resize(SCREEN_W, SCREEN_H);
    srand(20260301);

    printf("стенд «Тлеющая Тропа» (предмет 40)\n\n");

    // ── 1. Паспорт предмета ────────────────────────────────────────────────
    printf("паспорт:\n");
    {
        Probe p;
        p.quietArena();
        check(ITEM_COUNT == 41, "ITEM_COUNT = 41");
        check(ITEM_DEFS[40].id == 40, "ITEM_DEFS[40].id = 40");
        check(!ITEM_DEFS[40].isWeapon, "предмет пассивный (круглая шайба, не пластина)");
        check(std::strlen(ITEM_DEFS[40].name) > 3 && std::strlen(ITEM_DEFS[40].desc) > 20,
              "у предмета есть имя и описание");
        check(p.g.getItemTier(40) == 4, "тир IV");
        check(p.g.getItemMaxLevel(40) == 3, "три ранга");
        check(p.g.getPassiveUpgradePreviewText(40).size() > 20, "описание карты прокачки заполнено");
        check(p.g.getAdvisorRationale(40).find("взвешенный выбор") == std::string::npos,
              "у советника есть своя причина, а не общая фраза");
    }

    // ── 2. Открытие и пул ──────────────────────────────────────────────────
    printf("\nоткрытие и пул:\n");
    {
        Probe p;
        p.quietArena();
        p.g.gameMode = MODE_ENDLESS;
        p.g.runTime = 629.0f;
        check(!p.g.isItemUnlocked(40), "в эндлесе до 630 с закрыт");
        p.g.runTime = 630.0f;
        check(p.g.isItemUnlocked(40), "в эндлесе с 630 с открыт");

        p.g.gameMode = MODE_STORY;
        p.g.runBossKills = 2;
        p.g.runTime = 900.0f;
        const bool lockedAtTier3 = !p.g.isItemUnlocked(40) || p.g.getBuildTier() > 4;
        p.g.runBossKills = 3;
        const bool openAtTier4 = p.g.isItemUnlocked(40) && p.g.getBuildTier() >= 4;
        check(lockedAtTier3, "в кампании на тире сборки 3 ещё закрыт");
        check(openAtTier4, "в кампании на тире сборки 4 открыт");

        p.g.gameMode = MODE_ENDLESS;
        p.g.runTime = 900.0f;
        p.g.itemLevels[40] = 0;
        check(p.g.getBuildLimit() > 0, "слот подбора есть");
        p.g.applyUpgrade(40);
        check(p.g.itemLevels[40] == 1, "applyUpgrade(40) выдаёт ранг");
        bool inPool = false;
        for (int i = 0; i < 200 && !inPool; ++i) {
            p.g.generateUpgradeChoices();
            for (int k = 0; k < 4; ++k) if (p.g.upgradeChoices[k] == 40) inPool = true;
        }
        check(inPool, "предмет встречается в пуле выбора");
        const std::vector<int>& order = p.g.getArsenalItemOrder();
        check((int)order.size() == ITEM_COUNT && std::count(order.begin(), order.end(), 40) == 1,
              "в арсенале предмет ровно один раз");
    }

    // ── 3. Стоя на месте тропы нет ─────────────────────────────────────────
    printf("\nстоя на месте:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.addDummy(0.0f, 0.0f);
        p.run(120);
        check(p.g.ashTrailMarks.empty(), "метки не появляются");
        check(p.dummyLost() < 1.0e-3f, "урона нет");
    }

    // ── 4. Движение оставляет след ─────────────────────────────────────────
    printf("\nдвижение:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.g.playerX = -260.0f;
        p.addDummy(400.0f, 400.0f); // мишень в стороне: след проверяем без урона
        p.run(60, true);
        const int marks = (int)p.g.ashTrailMarks.size();
        check(marks > 0, "на ходу метки ложатся");
        check(marks <= ASH_TRAIL_MAX_MARKS, "хвост не длиннее предела");
        const float spacing = avgSpacing(p.g.ashTrailMarks);
        check(spacing >= ASH_TRAIL_SPACING - 1.0f, "соседние метки не слипаются");
        const int firesAfterWalk = p.g.ashTrailFires;
        p.run(20); // стоим
        check((int)p.g.ashTrailMarks.size() == marks, "на остановке новых меток нет");
        check(p.g.ashTrailFires == firesAfterWalk, "счётчик меток не растёт стоя");
        p.run(300, true);
        check((int)p.g.ashTrailMarks.size() <= ASH_TRAIL_MAX_MARKS, "долгий бег не копит хвост");
    }

    // ── 5. Метка гаснет сама ───────────────────────────────────────────────
    printf("\nвремя жизни метки:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 1;
        p.g.playerX = -40.0f;
        p.addDummy(400.0f, 400.0f);
        p.run(40, true);
        const int laid = p.g.ashTrailFires;
        check(laid > 0, "метки легли");
        const int framesToExpire = (int)((ASH_TRAIL_MARK_LIFE + 0.5f) * 60.0f);
        p.run(framesToExpire);
        check(p.g.ashTrailMarks.empty(), "через время жизни следов не остаётся");
    }

    // ── 6. Ожог: такт, кратность, ранги ────────────────────────────────────
    printf("\nожог:\n");
    {
        float lost[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        int burns[4] = {0, 0, 0, 0};
        for (int rank = 0; rank <= 3; ++rank) {
            Probe p;
            p.quietArena();
            p.g.itemLevels[40] = rank;
            p.g.playerX = -200.0f;
            p.addDummy(0.0f, 0.0f);
            p.run(90, true); // прошли мимо мишени, оставив на ней тропу
            p.run(120);      // стоим: тропа догорает, герой не двигается
            lost[rank] = p.dummyLost();
            burns[rank] = (int)std::lround(p.dummyLost() / std::max(1.0f, p.g.getAshTrailDamage(rank)));
            if (rank > 0) {
                const float tick = p.g.getAshTrailDamage(rank);
                const float remainder = fabsf(lost[rank] - burns[rank] * tick);
                check(remainder < 0.01f, "урон кратен такту (крита нет)");
            }
        }
        printf("  урон по стоящей мишени за 2 с: ранг 1 — %.0f, ранг 2 — %.0f, ранг 3 — %.0f\n",
               lost[1], lost[2], lost[3]);
        check(lost[0] < 1.0e-3f, "без предмета ожога нет");
        check(lost[1] > 0.0f, "первый ранг жжёт");
        check(lost[2] > lost[1] && lost[3] > lost[2], "каждый ранг сильнее предыдущего");
        check(lost[3] >= lost[1] * 1.4f, "третий ранг заметно сильнее первого");

        // Такт: стоя на месте мишень должна получать примерно одно попадание
        // за getAshTrailTickInterval. Считаем попадания, а не урон.
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 1;
        p.g.playerX = -30.0f;
        p.addDummy(0.0f, 0.0f);
        p.run(30, true);
        const float ix = p.g.ashTrailMarks.empty() ? 0.0f : p.g.ashTrailMarks.back().x;
        const float iy = p.g.ashTrailMarks.empty() ? 0.0f : p.g.ashTrailMarks.back().y;
        p.dummyX = ix; p.dummyY = iy;
        if (!p.g.monsters.empty()) { p.g.monsters.front().x = ix; p.g.monsters.front().y = iy; }
        const float interval = p.g.getAshTrailTickInterval(1);
        const int hits = p.run((int)(1.8f * 60.0f));
        const int expected = (int)(1.8f / interval);
        check(hits >= expected - 1 && hits <= expected + 1,
              "частота ожога совпадает с тактом ранга");

        Probe q;
        q.quietArena();
        q.g.itemLevels[40] = 3;
        q.g.playerX = -200.0f;
        q.addDummy(0.0f, 0.0f);
        q.run(90, true);
        q.run(120);
        check(!hasCritText(q.g), "ожог тропы не критует и не пишет «!»");
        // У тика свой угольный цвет попадания (0xFFFF8A36), а не цвет обычного
        // удара: иначе тропа читалась бы как ещё одно попадание героя.
        bool ownColor = false;
        for (const Particle& part : q.g.particles) if (part.color == 0xFFFF8A36) ownColor = true;
        check(ownColor, "попадание тропы красится своим угольным цветом");
    }

    // ── 7. Кого тропа не берёт ─────────────────────────────────────────────
    printf("\nцели ожога:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.g.playerX = -200.0f;
        Monster& res = p.addDummy(0.0f, 0.0f);
        res.isResonator = true;
        p.run(90, true);
        p.run(120);
        check(p.dummyLost() < 1.0e-3f, "резонатор (постройка) не горит");

        Probe b;
        b.quietArena();
        b.g.itemLevels[40] = 3;
        b.g.playerX = -200.0f;
        Monster& boss = b.addDummy(0.0f, 0.0f);
        boss.isBoss = true;
        b.run(90, true);
        b.run(120);
        check(b.dummyLost() > 0.0f, "страж горит — тропа не обходит боссов");
    }

    // ── 8. Тик тропы не запускает чужие срабатывания ───────────────────────
    // Ожог — маленький урон, и он не должен читаться игрой как «удар игрока»:
    // иначе добивающий тик рождал бы «ОСКОЛКИ ПОСЛЕДНЕГО УДАРА» (предмет 31) и
    // тропа сама себя разгоняла бы через чужой предмет.
    printf("\nсверхурон:\n");
    {
        auto shardsSpawned = [](const Game& game) {
            for (const FloatingText& t : game.floatTexts)
                if (t.text.find("ОСКОЛКИ") != std::string::npos) return true;
            return false;
        };
        // Мишень A умирает от тика, B стоит в стороне от следа (70 px), но в
        // радиусе осколков (124 px): если бы тик считался ударом, B получил бы урон.
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.g.itemLevels[31] = 3;
        p.g.overkillShardCooldown = 0.0f;
        p.g.playerX = -200.0f;
        p.addDummy(0.0f, 0.0f, 2.0f);
        const int bystander = (int)p.g.monsters.size();
        p.addDummy(0.0f, -70.0f, 1.0e6f);
        const float bystanderStart = p.g.monsters[bystander].hp;
        for (int f = 0; f < 120; ++f) {
            p.g.state = STATE_PLAYING;
            p.g.keyD = true;
            // Позиции держим по самой мишени, а не по индексу: когда A умирает,
            // вектор сдвигается, и индекс 0 — это уже B. Ошибка стенда, при
            // которой B сам вставал на след, ловилась как «тропа бьёт соседей».
            for (Monster& m : p.g.monsters) {
                const bool dying = m.maxHp <= 100.0f;
                m.x = 0.0f; m.y = dying ? 0.0f : -70.0f;
                m.vx = m.vy = 0.0f;
            }
            p.g.update(1.0f / 60.0f);
        }
        p.g.keyD = false;
        // B — единственный, у кого большое здоровье: он и есть проверяемый сосед.
        float bystanderHp = 0.0f;
        for (const Monster& m : p.g.monsters) if (m.maxHp > 100.0f) bystanderHp = std::max(bystanderHp, m.hp);
        check(bystanderHp >= bystanderStart - 1.0e-3f, "добивающий тик тропы не бьёт соседей осколками");
        check(!shardsSpawned(p.g), "и не пишет «ОСКОЛКИ ПОСЛЕДНЕГО УДАРА»");

        // Контроль: обычный удар с тем же избытком осколки рождает. Без этой
        // проверки предыдущая ничего не стоила бы — она ловила бы и сломанный
        // предмет 31, а не только неверное поведение тропы.
        Probe c;
        c.quietArena();
        c.g.itemLevels[31] = 3;
        c.g.overkillShardCooldown = 0.0f;
        c.addDummy(0.0f, 0.0f, 2.0f);
        c.addDummy(0.0f, -70.0f, 1.0e6f);
        const float secondStart = c.g.monsters[1].hp;
        c.g.damageMonster(0, 100.0f, 0.0f, 0.0f, 0);
        check(c.g.monsters.size() > 1 && c.g.monsters[1].hp < secondStart - 1.0e-3f && shardsSpawned(c.g),
              "контроль: обычный удар с избытком осколки даёт");
    }

    // ── 9. След не доживает до меню ────────────────────────────────────────
    printf("\nэкран меню:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.g.playerX = -60.0f;
        p.addDummy(400.0f, 400.0f);
        p.run(30, true);
        const bool laid = !p.g.ashTrailMarks.empty();
        // Метки рисуются и на фоне меню: застывший след прошлого забега там не
        // должен оставаться.
        p.g.state = STATE_GAMEOVER;
        p.g.update(1.0f / 60.0f);
        check(laid && p.g.ashTrailMarks.empty(), "на экране не-боя метки убираются");
    }

    // ── 10. Отрисовка не падает ────────────────────────────────────────────
    printf("\nотрисовка:\n");
    {
        Probe p;
        p.quietArena();
        p.g.itemLevels[40] = 3;
        p.g.playerX = -120.0f;
        p.addDummy(60.0f, 0.0f);
        p.run(70, true);
        p.g.playerX = 0.0f; p.g.playerY = 0.0f;
        bool drawn = true;
        for (int f = 0; f < 4; ++f) {
            r.clear(0xFF000000);
            p.g.renderWorld(r, -SCREEN_W * 0.5f, -SCREEN_H * 0.5f);
            for (int i = 0; i < SCREEN_W * SCREEN_H; i += 997)
                if (r.pixels[i] == 0) { drawn = false; break; }
        }
        check(drawn, "метки рисуются на полу (кадр не пустой)");
    }

    if (failures) {
        printf("\nПРОВАЛОВ: %d\n", failures);
        return 1;
    }
    printf("\nвсе проверки пройдены\n");
    return 0;
}
