// ── Стенд пола: плиты, шипы и знаки ──────────────────────────────────────────
//
// Пол в игре перестал быть картинкой: на нём стоят три предмета, и работают они
// от шага, а не от кнопки. Такую вещь нельзя проверить в стоячем стенде пассивок
// (tools/weapon_probe.cpp) — там герой не ходит, а плита срабатывает ровно от
// того, что на неё наступили. Здесь проверяется именно взаимодействие:
//
//   • спавн: не больше FLOOR_DEVICE_MAX, не ближе FLOOR_DEVICE_MIN_PLAYER к
//     герою, не ближе FLOOR_DEVICE_MIN_GAP друг к другу, только в бою;
//   • детерминизм: спавн не тратит rand() и даёт одни и те же точки в двух
//     прогонах — иначе сдвинулся бы общий поток случайности и вместе с ним
//     сцена боя;
//   • плита: срабатывает от шага внутри радиуса, бьёт врагов вокруг, одноразовая;
//   • шипы: сначала замах (за это время не бьют), потом выброс — бьют и героя,
//     и врагов; уклонение спасает; шаг в сторону спасает;
//   • знак: надо простоять FLOOR_SIGIL_CHARGE, выход из круга гасит заряд не
//     мгновенно; лечение и опыт выдаются один раз;
//   • время жизни и смена забега;
//   • вид предмета зависит от рисунка пола.
//
//     g++ -O2 -std=c++17 -Isrc tools/floor_probe.cpp -o build/floor_probe
//     build/floor_probe
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

    Probe() {
        g.simulationMode = true;
        g.ignoreSavedState = true;
        g.init();
        g.gameMode = MODE_ENDLESS;
    }

    // Тихая арена: ни волн, ни отрядов, ни резонаторов. Мерить надо пол,
    // а не случайный спавн, который прибежит и начнёт получать урон сам.
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
        g.clearFloorDevices();
        g.balance.spawnMultiplier = 0.0f;
        g.squadTimer = 1.0e9f;
        g.ratTimer = 1.0e9f;
        g.resonatorSpawnTimer = 1.0e9f;
        g.portalSpawnTimer = 1.0e9f;
        g.nextBossIndex = 999;
        g.eventCooldown = 1.0e9f;
        g.huntActive = false;
        g.state = STATE_PLAYING;
        g.cheatGodMode = true;
        g.runTime = 300.0f;
        g.playerX = 0.0f; g.playerY = 0.0f;
        g.playerHp = g.playerMaxHp;
        g.playerHurtTimer = 0.0f;
        g.dashTimer = 0.0f;
        g.arenaLayoutOverride = -1;
        g.floorDeviceSpawnTimer = 1.0e9f;   // сам по себе спавн в замерах не нужен
        // Звук в стенде живёт по тем же правилам: кулдауны тикают, голоса
        // сбрасываются, иначе вторая проверка ловила бы хвост первой.
        for (int i = 0; i < SFX_COUNT; ++i) g.sound.cooldowns[i] = 0.0f;
        for (ActiveVoice& voice : g.sound.voices) voice.active = false;
        for (int i = 0; i < ITEM_COUNT; ++i) g.itemLevels[i] = 0;
        for (int i = 0; i < 7; ++i) g.evolvedWeapons[i] = false;
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
        return m;
    }

    void tick(float seconds) {
        const int frames = std::max(1, (int)roundf(seconds * 60.0f));
        for (int f = 0; f < frames; ++f) {
            g.state = STATE_PLAYING;
            g.updateFloorDevices(1.0f / 60.0f);
            g.sound.update(1.0f / 60.0f);
            g.playerHurtTimer = std::max(0.0f, g.playerHurtTimer - 1.0f / 60.0f);
        }
    }
};

int main() {
    printf("Стенд пола: плиты, шипы и знаки\n");

    // ── спавн ────────────────────────────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        for (int i = 0; i < 12; ++i) p.g.spawnFloorDevice();
        check((int)p.g.floorDevices.size() == FLOOR_DEVICE_MAX, "не больше FLOOR_DEVICE_MAX устройств за раз");

        bool farEnough = true, closeEnough = true, spread = true;
        for (const FloorDevice& d : p.g.floorDevices) {
            const float dx = d.x - p.g.playerX, dy = d.y - p.g.playerY;
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist < FLOOR_DEVICE_MIN_PLAYER - 1.0f) farEnough = false;
            if (dist > FLOOR_DEVICE_MAX_PLAYER + 1.0f) closeEnough = false;
        }
        for (size_t i = 0; i < p.g.floorDevices.size(); ++i)
            for (size_t j = i + 1; j < p.g.floorDevices.size(); ++j) {
                const float dx = p.g.floorDevices[i].x - p.g.floorDevices[j].x;
                const float dy = p.g.floorDevices[i].y - p.g.floorDevices[j].y;
                if (dx * dx + dy * dy < (FLOOR_DEVICE_MIN_GAP - 1.0f) * (FLOOR_DEVICE_MIN_GAP - 1.0f)) spread = false;
            }
        check(farEnough, "ни один предмет не появляется вплотную к герою");
        check(closeEnough, "ни один предмет не появляется за пределами видимой арены");
        check(spread, "предметы не налезают друг на друга");

        // Детерминизм: тот же герой, та же последовательность — те же точки.
        Probe q;
        q.quietArena();
        for (int i = 0; i < 5; ++i) q.g.spawnFloorDevice();
        bool same = q.g.floorDevices.size() == p.g.floorDevices.size();
        for (size_t i = 0; same && i < q.g.floorDevices.size(); ++i)
            same = fabsf(q.g.floorDevices[i].x - p.g.floorDevices[i].x) < 0.01f &&
                   fabsf(q.g.floorDevices[i].y - p.g.floorDevices[i].y) < 0.01f &&
                   q.g.floorDevices[i].kind == p.g.floorDevices[i].kind;
        check(same, "два одинаковых прогона дают одинаковые точки");

        // Плотное кольцо: три предмета уже стоят в 130 px от героя. Спавн обязан
        // искать место дальше, а не класть новый предмет вплотную к старому.
        Probe ring;
        ring.quietArena();
        for (int i = 0; i < 3; ++i) {
            const float a = (float)i * 2.0943951f;
            ring.g.spawnFloorDeviceAt(FLOOR_DEVICE_PLATE, cosf(a) * 130.0f, sinf(a) * 130.0f);
        }
        for (int i = 0; i < 20; ++i) ring.g.spawnFloorDevice();
        bool ringSpread = true;
        for (size_t i = 0; i < ring.g.floorDevices.size(); ++i)
            for (size_t j = i + 1; j < ring.g.floorDevices.size(); ++j) {
                const float dx = ring.g.floorDevices[i].x - ring.g.floorDevices[j].x;
                const float dy = ring.g.floorDevices[i].y - ring.g.floorDevices[j].y;
                if (dx * dx + dy * dy < (FLOOR_DEVICE_MIN_GAP - 1.0f) * (FLOOR_DEVICE_MIN_GAP - 1.0f)) ringSpread = false;
            }
        check(ringSpread, "уже занятое кольцо не даёт поставить предмет вплотную (20 попыток)");

        // rand() не тронут: после спавна поток случайности там же, где был.
        // Проба создаётся до srand — конструктор сам тратит случайность на init,
        // и это не про пол.
        Probe r;
        r.quietArena();
        srand(9001);
        const int expected = rand();
        srand(9001);
        for (int i = 0; i < 5; ++i) r.g.spawnFloorDevice();
        const int after = rand();
        check(after == expected, "спавн пола не тратит rand() и не сдвигает сцену боя");

        // Пауза не спавнит.
        Probe s;
        s.quietArena();
        s.g.floorDeviceSpawnTimer = 0.01f;
        s.g.state = STATE_PAUSED;
        s.g.updateFloorDevices(1.0f);
        check(s.g.floorDevices.empty(), "на паузе пол ничего не выставляет");
        s.g.state = STATE_PLAYING;
        s.g.updateFloorDevices(0.05f);
        check(!s.g.floorDevices.empty(), "в бою таймер снова работает");
    }

    // ── плита ────────────────────────────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_PLATE, 0.0f, 0.0f);
        p.g.playerX = 0.0f; p.g.playerY = 60.0f;
        p.tick(0.2f);
        check(!p.g.floorDevices[0].used, "плита не срабатывает в шаге от себя");

        Monster& near = p.addDummy(90.0f, 0.0f);
        Monster& far = p.addDummy(320.0f, 0.0f);
        const float nearHp = near.hp, farHp = far.hp;
        p.g.playerY = 40.0f;
        p.tick(0.1f);
        check(p.g.floorDevices[0].used, "плита срабатывает, когда на неё наступили");
        check(p.g.monsters[0].hp < nearHp - 0.5f, "разряд плиты бьёт врагов рядом");
        check(fabsf(p.g.monsters[1].hp - farHp) < 0.5f, "и не задевает тех, кто далеко");
        check(p.g.floorDevicesTriggered == 1, "срабатывание посчитано один раз");
        check(p.g.sound.activeVoiceCount(SFX_FLOOR_PLATE) > 0, "нажатие плиты звучит");

        const float hpAfter = p.g.monsters[0].hp;
        p.tick(0.5f);
        check(p.g.floorDevicesTriggered == 1, "вторая попытка нажать плиту ничего не делает");
        check(fabsf(p.g.monsters[0].hp - hpAfter) < 0.5f, "и повторного урона нет");
    }

    // ── шипы ─────────────────────────────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_SPIKES, 0.0f, 0.0f);
        p.g.playerX = 0.0f; p.g.playerY = 0.0f;
        p.g.playerHp = p.g.playerMaxHp;
        // Срок здесь жёсткий (0,9 с), а не «замах минус кадр»: иначе, укоротив
        // FLOOR_SPIKE_WINDUP, проверка укоротилась бы вместе с ним и всё равно
        // прошла бы. 0,9 с честно меньше настоящего замаха 1,1 с.
        p.tick(0.9f);
        check(fabsf(p.g.playerHp - p.g.playerMaxHp) < 0.01f, "во время замаха шипы не бьют");
        check(p.g.floorDevices[0].stage == 0, "и ещё не выброшены");

        Monster& onTop = p.addDummy(0.0f, 0.0f);
        const float mobHp = onTop.hp;
        p.tick(0.3f);
        check(p.g.playerHp < p.g.playerMaxHp - 0.5f, "выброс бьёт героя, который стоит на шипах");
        check(p.g.monsters[0].hp < mobHp - 0.5f, "и врага, который на них оказался");

        // Уклонение спасает: тот же выброс, но герой в рывке.
        Probe d;
        d.quietArena();
        d.g.spawnFloorDeviceAt(FLOOR_DEVICE_SPIKES, 0.0f, 0.0f);
        d.g.playerX = d.g.playerY = 0.0f;
        d.g.playerHp = d.g.playerMaxHp;
        d.g.dashTimer = 0.4f;
        d.tick(FLOOR_SPIKE_WINDUP + 0.1f);
        check(fabsf(d.g.playerHp - d.g.playerMaxHp) < 0.01f, "рывок проносит над шипами без урона");

        // Шаг в сторону — тоже.
        Probe side;
        side.quietArena();
        side.g.spawnFloorDeviceAt(FLOOR_DEVICE_SPIKES, 0.0f, 0.0f);
        side.g.playerX = 0.0f; side.g.playerY = 0.0f;
        side.g.playerHp = side.g.playerMaxHp;
        side.g.playerY = 150.0f;
        side.tick(FLOOR_SPIKE_WINDUP + 0.3f);
        check(fabsf(side.g.playerHp - side.g.playerMaxHp) < 0.01f, "отошёл — шипы не достают");

        // Шипы уходят в пол и не остаются навсегда.
        p.tick(FLOOR_SPIKE_ACTIVE + 0.5f);
        check(p.g.floorDevices.empty(), "после выброса шипы уходят в пол");
    }

    // ── знак ─────────────────────────────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_SIGIL, 0.0f, 0.0f);
        p.g.playerX = p.g.playerY = 0.0f;
        p.g.playerHp = p.g.playerMaxHp * 0.5f;
        p.g.playerXp = 0;
        const float hurt = p.g.playerHp;
        p.tick(FLOOR_SIGIL_CHARGE - 0.2f);
        check(fabsf(p.g.playerHp - hurt) < 0.01f, "знак не лечит, пока не простоишь свой срок");
        check(p.g.floorDevices[0].timer > 0.5f, "заряд знака идёт");

        p.tick(0.35f);
        check(p.g.playerHp > hurt + 0.5f, "простоял — знак лечит");
        check(p.g.playerXp > 0, "и даёт опыт");
        check(p.g.floorDevicesHealed == 1, "срабатывание знака посчитано");

        const float healed = p.g.playerHp;
        p.tick(0.6f);
        check(fabsf(p.g.playerHp - healed) < 0.01f, "второй раз тот же знак не лечит");

        // Вышел из круга — заряд тает, но не обнуляется мгновенно.
        Probe q;
        q.quietArena();
        q.g.spawnFloorDeviceAt(FLOOR_DEVICE_SIGIL, 0.0f, 0.0f);
        q.g.playerX = q.g.playerY = 0.0f;
        q.tick(0.9f);
        const float charged = q.g.floorDevices[0].timer;
        q.g.playerX = 200.0f;
        q.tick(0.3f);
        const float afterStep = q.g.floorDevices[0].timer;
        check(charged > 0.5f, "стояние в круге копит заряд");
        check(afterStep < charged && afterStep > charged * 0.4f, "выход из круга гасит заряд постепенно");
    }

    // ── время жизни и смена забега ───────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_PLATE, 300.0f, 0.0f);
        p.tick(FLOOR_DEVICE_LIFE - 1.0f);
        check(p.g.floorDevices.size() == 1, "предмет лежит на полу почти весь свой срок");
        p.tick(1.5f);
        check(p.g.floorDevices.empty(), "и исчезает сам, если его не тронули");

        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_SIGIL, 200.0f, 0.0f);
        p.g.clearFloorDevices();
        check(p.g.floorDevices.empty() && p.g.floorDevicesTriggered == 0,
              "новый забег начинается с чистого пола");
    }

    // ── частота встречи с предметами ─────────────────────────────────────────
    {
        // Считаем пик за окно, а не остаток в конце: предмет живёт 19 с, поэтому
        // при редком появлении к концу окна на полу успевает стать пусто.
        auto countAfter = [](int rate, float seconds) {
            Probe p;
            p.quietArena();
            p.g.options.floorDeviceRate = rate;
            p.g.clearFloorDevices();
            p.g.floorDeviceSpawnTimer = p.g.floorDeviceFirstDelay();
            int peak = 0;
            const int frames = (int)(seconds * 60.0f);
            for (int f = 0; f < frames; ++f) {
                p.tick(1.0f / 60.0f);
                peak = std::max(peak, (int)p.g.floorDevices.size());
            }
            return peak;
        };
        check(countAfter(0, 40.0f) == 0, "«выкл»: новых предметов нет вовсе");
        const int rare = countAfter(1, 40.0f);
        const int normal = countAfter(2, 40.0f);
        const int often = countAfter(3, 40.0f);
        // Пик за минуту: предмет живёт 19 с, поэтому даже при пределе 5 на полу
        // одновременно держится меньше. Это и есть «частота встречи».
        check(rare >= 1 && rare <= 2, "«редко»: на полу один-два предмета");
        check(normal >= 3 && normal <= 4, "«обычно»: три-четыре, как раньше");
        check(often >= 4 && often <= 6, "«часто»: четыре-шесть одновременно");
        check(rare < normal && normal < often, "частота растёт по ступеням");

        // Частота — это только частота: урон от неё зависеть не должен, иначе
        // настройка превращалась бы в скрытую сложность.
        Probe p;
        p.quietArena();
        const float damageNormal = p.g.floorDeviceEnemyDamage();
        p.g.options.floorDeviceRate = 3;
        check(fabsf(p.g.floorDeviceEnemyDamage() - damageNormal) < 0.01f,
              "частота встречи не меняет урон пола");

        // Смена настройки пересобирает пол сразу: иначе игрок жмёт переключатель
        // и не видит разницы до следующего предмета.
        p.g.spawnFloorDeviceAt(FLOOR_DEVICE_PLATE, 200.0f, 0.0f);
        p.g.toggleOption(13);
        check(p.g.floorDevices.empty(), "смена частоты пересобирает пол");
        check(p.g.options.floorDeviceRate == 0, "ВЫКЛ идёт сразу после ЧАСТО");
    }

    // ── звук: пол слышно, а не только видно ──────────────────────────────────
    {
        Probe p;
        p.quietArena();
        p.g.spawnFloorDevice();
        check(p.g.sound.activeVoiceCount(SFX_FLOOR_APPEAR) > 0,
              "появление предмета слышно, а не только видно");

        // Второй спавн в тот же кадр не должен накладывать второй голос: подсказка
        // не имеет права превращаться в очередь из трёх «бум» подряд.
        const int voicesAfterFirst = p.g.sound.activeVoiceCount(SFX_FLOOR_APPEAR);
        p.g.spawnFloorDevice();
        check(p.g.sound.activeVoiceCount(SFX_FLOOR_APPEAR) == voicesAfterFirst,
              "подряд идущие появления не копят голоса");

        // «Зов» звучит один раз, когда предмет оказался рядом.
        Probe q;
        q.quietArena();
        q.g.spawnFloorDeviceAt(FLOOR_DEVICE_PLATE, 0.0f, 140.0f);
        q.g.playerX = q.g.playerY = 0.0f;
        q.tick(0.2f);
        check(q.g.sound.activeVoiceCount(SFX_FLOOR_NEAR) > 0, "предмет рядом зовёт игрока");
        check(q.g.floorDevices[0].nudged, "зов не повторяется каждый кадр");

        // Шипы: треск до выброса, удар после.
        Probe w;
        w.quietArena();
        w.g.spawnFloorDeviceAt(FLOOR_DEVICE_SPIKES, 0.0f, 0.0f);
        w.g.playerX = w.g.playerY = 0.0f;
        w.g.playerHp = w.g.playerMaxHp;
        w.tick(0.2f);
        check(w.g.sound.activeVoiceCount(SFX_FLOOR_SPIKE_WARN) > 0, "шипы трещат до выброса");
        check(w.g.sound.activeVoiceCount(SFX_FLOOR_SPIKE_BURST) == 0, "удар шипов не звучит раньше времени");
        w.tick(FLOOR_SPIKE_WINDUP);
        check(w.g.sound.activeVoiceCount(SFX_FLOOR_SPIKE_BURST) > 0, "выброс шипов звучит");

        // Знак: тик заряда, и его высота растёт к концу.
        Probe z;
        z.quietArena();
        z.g.spawnFloorDeviceAt(FLOOR_DEVICE_SIGIL, 0.0f, 0.0f);
        z.g.playerX = z.g.playerY = 0.0f;
        float lowest = 9.0f, highest = 0.0f;
        for (int step = 0; step < 12; ++step) {
            z.g.sound.cooldowns[SFX_FLOOR_SIGIL_TICK] = 0.0f;
            z.tick(0.11f);
            for (const ActiveVoice& voice : z.g.sound.voices)
                if (voice.active && voice.id == SFX_FLOOR_SIGIL_TICK) {
                    lowest = std::min(lowest, voice.speed);
                    highest = std::max(highest, voice.speed);
                }
        }
        check(lowest < 9.0f, "заряд знака тикает");
        check(highest > lowest + 0.15f, "тик знака растёт по высоте вместе с зарядом");
    }

    // ── вид зависит от рисунка пола ──────────────────────────────────────────
    {
        Probe p;
        p.quietArena();
        int counts[ARENA_LAYOUT_COUNT][FLOOR_DEVICE_COUNT] = {};
        for (int layout = 0; layout < ARENA_LAYOUT_COUNT; ++layout) {
            p.g.arenaLayoutOverride = layout;
            for (int seq = 0; seq < 10; ++seq) counts[layout][p.g.pickFloorDeviceKind(seq)]++;
        }
        p.g.arenaLayoutOverride = -1;
        check(counts[ARENA_SLABS][FLOOR_DEVICE_PLATE] >= 5, "на «Плитах» чаще всего плиты");
        check(counts[ARENA_RITUAL][FLOOR_DEVICE_SIGIL] >= 5, "на «Ритуале» чаще знаки");
        check(counts[ARENA_RUBBLE][FLOOR_DEVICE_SPIKES] >= 5, "на «Осыпи» чаще шипы");
        bool allKinds = true;
        for (int layout = 0; layout < ARENA_LAYOUT_COUNT; ++layout) {
            int kinds = 0;
            for (int k = 0; k < FLOOR_DEVICE_COUNT; ++k) if (counts[layout][k] > 0) ++kinds;
            if (kinds < 3) allKinds = false;
        }
        check(allKinds, "на каждом рисунке пола встречаются все три вида");
    }

    printf("\n");
    if (failures == 0) printf("ОК: ошибок нет\n");
    else printf("ПРОВАЛОВ: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
