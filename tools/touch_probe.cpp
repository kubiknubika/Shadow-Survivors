// ── Стенд касаний: схема управления против настоящей игры ────────────────────
//
// tools/touch_preview.cpp отвечает на вопрос «как это выглядит», но не на
// вопрос «работает ли». Здесь то же самое, но с живым Game: касания идут через
// src/touch_input.h, а проверяется то, что видит игрок — герой двинулся, пауза
// встала, тап по кнопке сработал как клик.
//
// Это единственная проверка Android-управления, которую можно сделать без
// телефона: телефон и этот стенд зовут одни и те же функции, поэтому «на
// стенде проходит, на телефоне нет» может означать только ошибку в приёме
// событий Android, а не в логике схемы.
//
//     g++ -O2 -std=c++17 -Isrc tools/touch_probe.cpp -o build/touch_probe
//     build/touch_probe              # все проверки, код 1 при провале
//     build/touch_probe -v           # печатать каждую проверку
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include "renderer.h"
#include "game.h"
#include "touch_input.h"

// ── Мини-фреймворк: проверка печатается одной строкой, провал копится ───────
static int g_failed = 0;
static int g_passed = 0;
static bool g_verbose = false;

static void check(bool ok, const char* what, const char* detail = "") {
    if (ok) {
        ++g_passed;
        if (g_verbose) printf("  ok    %s\n", what);
    } else {
        ++g_failed;
        printf("  ПРОВАЛ %s%s%s\n", what, detail[0] ? " — " : "", detail);
    }
}

static void checkNear(float got, float want, float tol, const char* what) {
    char detail[160];
    snprintf(detail, sizeof(detail), "получено %.1f, ожидалось %.1f ± %.1f", got, want, tol);
    check(std::fabs(got - want) <= tol, what, detail);
}

// ── Сцена: чистая игра в бою, без записи на диск ────────────────────────────
static void freshGame(Game& g, int w, int h) {
    SCREEN_W = w;
    SCREEN_H = h;
    g.simulationMode = true;      // никаких сейвов и логов между прогонами
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.cheatGodMode = true;        // герой не умирает: проверяем управление, не бой
    g.runTime = 30.0f;
}

static void step(Game& g, int frames, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < frames; ++i) {
        if (g.state == STATE_LEVELUP) return;   // модалка останавливает бой
        g.update(dt);
    }
}

// Экранные координаты центра прямоугольника — телефон отдаёт их в пикселях
// окна, стенд считает в логическом кадре, поэтому это одно и то же число.
static void center(const TouchRect& r, float& x, float& y) {
    x = r.x + r.w * 0.5f;
    y = r.y + r.h * 0.5f;
}

// ── 1. Раскладка: кнопки в экране и не наезжают друг на друга ───────────────
static void testLayout() {
    printf("раскладка кнопок\n");
    const int sizes[][2] = {{1600, 720}, {1280, 720}, {720, 1280}, {800, 600}, {1920, 1080}};
    for (auto& s : sizes) {
        const int w = s[0], h = s[1];
        TouchLayout l;
        l.layout(w, h);
        char tag[96];
        snprintf(tag, sizeof(tag), "%dx%d", w, h);

        const TouchRect* all[] = {&l.dash, &l.ult, &l.pause, &l.debug};
        const char* names[] = {"рывок", "супер", "пауза", "F3"};
        bool inside = true;
        for (int i = 0; i < 4; ++i) {
            const TouchRect& r = *all[i];
            if (r.x < 0 || r.y < 0 || r.x + r.w > w || r.y + r.h > h) {
                inside = false;
                char detail[128];
                snprintf(detail, sizeof(detail), "кнопка «%s» вне экрана %s", names[i], tag);
                check(false, "кнопки внутри экрана", detail);
            }
        }
        if (inside) {
            char what[128];
            snprintf(what, sizeof(what), "кнопки внутри экрана %s", tag);
            check(true, what);
        }

        bool overlap = false;
        for (int i = 0; i < 4 && !overlap; ++i)
            for (int j = i + 1; j < 4; ++j) {
                const TouchRect& a = *all[i];
                const TouchRect& b = *all[j];
                if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) {
                    overlap = true;
                    char detail[160];
                    snprintf(detail, sizeof(detail), "%s и %s пересекаются на %s", names[i], names[j], tag);
                    check(false, "кнопки не перекрываются", detail);
                }
            }
        if (!overlap) {
            char what[128];
            snprintf(what, sizeof(what), "кнопки не перекрываются %s", tag);
            check(true, what);
        }
    }
}

// ── 2. Стик: палец двигает героя и отпускает его ────────────────────────────
static void testStick() {
    printf("стик\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    // Палец касается левой нижней четверти — это стик, а не кнопка.
    const float sx = 220.0f, sy = 560.0f;
    touchBegin(p, g, l, 0, sx, sy);
    check(p[0].role == TOUCH_STICK, "касание слева снизу становится стиком");

    // Дрожание пальца на 10 px не должно давать движения: мёртвая зона 14 px.
    touchMove(p, g, 0, sx + 10.0f, sy);
    touchUpdate(g, p);
    check(!g.keyD && !g.keyA, "дрожание 10 px не двигает героя");

    // Ведём вправо на 120 px: герой обязан пойти вправо и только вправо.
    g.playerX = 800.0f;
    g.playerY = 360.0f;
    touchMove(p, g, 0, sx + 120.0f, sy);
    touchUpdate(g, p);
    check(g.keyD && !g.keyA && !g.keyW && !g.keyS, "стик вправо ставит только D");
    const float x0 = g.playerX;
    step(g, 20);
    check(g.playerX > x0 + 5.0f, "герой действительно сдвинулся вправо");
    checkNear(g.playerY, 360.0f, 2.0f, "по вертикали герой не поехал");

    // Отпустили палец: клавиши сняты, герой стоит.
    touchEnd(p, g, 0);
    check(!g.keyD && !g.keyA && !g.keyW && !g.keyS, "после отпускания клавиши сняты");
    const float x1 = g.playerX;
    step(g, 20);
    checkNear(g.playerX, x1, 1.0f, "после отпускания герой стоит");

    // Вниз-вверх: dy<0 — это вверх (клавиша W), dy>0 — вниз (S).
    g.playerX = 800.0f; g.playerY = 360.0f;
    touchBegin(p, g, l, 1, sx, sy);
    touchMove(p, g, 1, sx, sy - 60.0f);
    touchUpdate(g, p);
    check(g.keyW && !g.keyS, "стик вверх ставит W");
    step(g, 20);
    check(g.playerY < 355.0f, "герой пошёл вверх");
    touchEnd(p, g, 1);

    touchBegin(p, g, l, 2, sx, sy);
    touchMove(p, g, 2, sx, sy + 60.0f);
    touchUpdate(g, p);
    check(g.keyS && !g.keyW, "стик вниз ставит S");
    touchEnd(p, g, 2);
    touchUpdate(g, p);

    // Пауза не должна двигать героя даже при нажатом стике: это отдельная
    // проверка того, что стик не обходит правила игры.
    g.state = STATE_PAUSED;
    g.playerX = 800.0f;
    touchBegin(p, g, l, 3, sx, sy);
    touchMove(p, g, 3, sx + 120.0f, sy);
    touchUpdate(g, p);
    const float pausedX = g.playerX;
    step(g, 20);
    checkNear(g.playerX, pausedX, 0.5f, "на паузе стик не двигает героя");
    touchEnd(p, g, 3);
}

// ── 3. Кнопки: рывок, супер, пауза, F3 ──────────────────────────────────────
static void testButtons() {
    printf("кнопки\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);
    float x, y;

    // Рывок: кнопка обязана и рывок дать, и клик, но не тронуть ходьбу.
    center(l.dash, x, y);
    g.dashCooldown = 0.0f;
    g.playerX = 800.0f; g.playerY = 360.0f;
    touchBegin(p, g, l, 10, x, y);
    check(p[0].role == TOUCH_DASH, "касание кнопки «рывок» — это рывок");
    check(g.mouseClicked, "кнопка «рывок» даёт клик (в меню это выбор)");
    step(g, 2);
    check(g.dashTimer > 0.0f || g.playerX != 800.0f, "рывок действительно сработал");
    check(!g.keyW && !g.keyA && !g.keyS && !g.keyD, "кнопка рывка не ставит ходьбу");
    touchEnd(p, g, 10);

    // Супер: заряда нет — он не должен сработать, а кнопка не должна падать.
    center(l.ult, x, y);
    g.superCharge = 0.0f;
    touchBegin(p, g, l, 11, x, y);
    check(p[0].role == TOUCH_ULT, "касание кнопки «супер» — это супер");
    step(g, 2);
    check(g.superCharge >= 0.0f, "супер без заряда не ломает игру");
    touchEnd(p, g, 11);

    // Супер с зарядом обязан включиться.
    g.superCharge = 100.0f;
    touchBegin(p, g, l, 12, x, y);
    step(g, 2);
    check(g.superActive || g.superCharge < 100.0f, "супер с полным зарядом срабатывает");
    touchEnd(p, g, 12);

    // Пауза: ESC — и состояние стало паузой.
    g.state = STATE_PLAYING;
    center(l.pause, x, y);
    touchBegin(p, g, l, 13, x, y);
    check(p[0].role == TOUCH_PAUSE, "касание кнопки «пауза» — это пауза");
    check(g.state == STATE_PAUSED, "пауза встаёт от касания");
    touchEnd(p, g, 13);

    // F3: панель включается и выключается тем же касанием.
    center(l.debug, x, y);
    const bool before = g.debugOverlay;
    touchBegin(p, g, l, 14, x, y);
    check(g.debugOverlay != before, "F3 включает панель отладки");
    touchEnd(p, g, 14);
    touchBegin(p, g, l, 15, x, y);
    check(g.debugOverlay == before, "F3 выключает панель отладки");
    touchEnd(p, g, 15);
}

// ── 4. Прицел: палец справа ведёт курсор и держит кнопку мыши ───────────────
static void testAim() {
    printf("прицел\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    touchBegin(p, g, l, 20, 1100.0f, 300.0f);
    check(p[0].role == TOUCH_AIM, "касание справа — прицел");
    check(g.mouseX == 1100 && g.mouseY == 300, "прицел ставит курсор в точку касания");
    check(g.mouseLeftDown, "прицел держит кнопку мыши — так идёт огонь");
    touchMove(p, g, 20, 1200.0f, 320.0f);
    check(g.mouseX == 1200 && g.mouseY == 320, "ведение пальцем двигает курсор");
    touchEnd(p, g, 20);
    check(!g.mouseLeftDown, "после отпускания кнопка мыши отпущена");
}

// ── 5. Два пальца: стик и прицел одновременно ───────────────────────────────
static void testMultiTouch() {
    printf("два пальца\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    touchBegin(p, g, l, 30, 220.0f, 560.0f);
    touchBegin(p, g, l, 31, 1100.0f, 300.0f);
    touchMove(p, g, 30, 340.0f, 560.0f);
    touchMove(p, g, 31, 1150.0f, 280.0f);
    touchUpdate(g, p);
    check(g.keyD, "стик работает вместе с прицелом");
    check(g.mouseX == 1150 && g.mouseY == 280, "прицел не сбивается стиком");
    check(g.mouseLeftDown, "кнопка мыши держится, пока палец на экране");

    // Отпустили прицел — стик обязан остаться нажатым.
    touchEnd(p, g, 31);
    check(!g.mouseLeftDown, "прицел отпущен");
    touchUpdate(g, p);
    check(g.keyD, "после отпускания прицела стик продолжает работать");

    // Отпустили стик — клавиши сняты, прицел уже не нажат.
    touchEnd(p, g, 30);
    touchUpdate(g, p);
    check(!g.keyD && !g.keyW, "оба пальца отпущены — движения нет");

    // Больше пальцев, чем слотов: девятое касание игнорируется, но не падает.
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) touchBegin(p, g, l, 100 + i, 220.0f, 560.0f);
    touchBegin(p, g, l, 999, 220.0f, 560.0f);
    int busy = 0;
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) if (p[i].id >= 0) ++busy;
    check(busy == TOUCH_MAX_POINTERS, "лишнее касание не занимает слот");
    touchEnd(p, g, 12345);   // чужой id — не должно быть падения
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) touchEnd(p, g, 100 + i);
    touchUpdate(g, p);
    check(!g.keyD, "после всех касаний движение снято");
}

// ── 6. Тап как клик: нажатие по кнопке в меню skutečně переключает экран ────
static void testTapIsClick() {
    printf("тап вместо клика\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    // Зал славы: внизу справа кнопка «В МЕНЮ». Геометрия — та же формула, что
    // в renderHallMenu; щёлкает по ней сама отрисовка, поэтому кадр обязателен.
    g.state = STATE_HALL;
    g.render(r);                       // первый кадр: посчитать наведение
    const int by = SCREEN_H - 76, bh = 52, gap = 12;
    const int bw = std::min(320, (SCREEN_W - 3 * gap) / 2);
    const int leftX = (SCREEN_W - (2 * bw + gap)) / 2;
    const int rightX = leftX + bw + gap;
    const float tx = rightX + bw * 0.5f, ty = by + bh * 0.5f;

    touchBegin(p, g, l, 40, tx, ty);
    g.render(r);                       // клик разбирается в отрисовке
    check(g.state == STATE_MENU, "тап по «В МЕНЮ» возвращает в меню");
    touchEnd(p, g, 40);
    check(g.state == STATE_MENU, "экран не откатывается после отпускания");

    // Промах мимо кнопки не должен ничего нажимать.
    g.state = STATE_HALL;
    g.render(r);
    touchBegin(p, g, l, 41, 40.0f, 40.0f);
    g.render(r);
    check(g.state == STATE_HALL, "тап мимо кнопки ничего не нажимает");
    touchEnd(p, g, 41);
}

// ── 7. Долгий забег под управлением пальца: 3 минуты без падений ────────────
static void testLongRun() {
    printf("долгий забег\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);
    srand(20250);

    touchBegin(p, g, l, 50, 220.0f, 560.0f);
    float dirX = 40.0f, dirY = 0.0f;
    int frames = 0, levelups = 0;
    bool broke = false;
    for (int f = 0; f < 60 * 180; ++f) {
        // Палец ведёт стик по кругу — так проверяются все восемь направлений
        // и одновременно то, что модалки не застревают.
        if (f % 45 == 0) {
            const float a = (float)(f / 45) * 0.7f;
            dirX = cosf(a) * 60.0f;
            dirY = sinf(a) * 60.0f;
        }
        touchMove(p, g, 50, 220.0f + dirX, 560.0f + dirY);
        touchUpdate(g, p);
        if (g.state == STATE_LEVELUP) { ++levelups; g.state = STATE_PLAYING; }
        if (g.state == STATE_CHEST) g.state = STATE_PLAYING;
        g.update(1.0f / 60.0f);
        if (g.state == STATE_MENU || g.state == STATE_GAMEOVER) { broke = true; break; }
        ++frames;
    }
    char detail[160];
    snprintf(detail, sizeof(detail), "кадров %d, уровней %d", frames, levelups);
    check(!broke, "три минуты под управлением пальца не выбили в меню", detail);
    check(frames > 60 * 170, "забег дошёл до конца прогона", detail);
    check(g.playerX > 0.0f && g.playerY > 0.0f, "герой остался на арене");
    touchEnd(p, g, 50);
}

// ── 8. Три схемы движения: раскладка, крестовина, зеркало ───────────────────
static void testSchemes() {
    printf("схемы движения\n");
    Renderer r;
    r.resize(1600, 720);
    for (int scheme = 0; scheme < TOUCH_SCHEME_COUNT; ++scheme) {
        TouchLayout l;
        l.layout(1600, 720, scheme);
        char tag[64];
        snprintf(tag, sizeof(tag), "схема «%s»", touchSchemeName(scheme));

        // Кнопки действия всегда внутри экрана и не перекрывают друг друга.
        const TouchRect* all[] = {&l.dash, &l.ult, &l.pause, &l.debug,
                                  &l.padUp, &l.padDown, &l.padLeft, &l.padRight};
        bool inside = true;
        for (const TouchRect* b : all)
            if (b->x < 0 || b->y < 0 || b->x + b->w > 1600 || b->y + b->h > 720) inside = false;
        check(inside, "все кнопки внутри экрана", tag);

        // Движение и прицел не должны делить одну и ту же точку: палец в
        // «своём» углу обязан стать движением, а в противоположном — прицелом.
        const float moveX = l.mirrored() ? 1500.0f : 120.0f;
        const float aimX = l.mirrored() ? 120.0f : 1500.0f;
        check(l.inMoveZone(moveX, 600.0f), "нижний угол своей стороны — зона движения", tag);
        check(!l.inMoveZone(aimX, 600.0f), "противоположный угол — не зона движения", tag);
        check(!l.inMoveZone(moveX, 100.0f), "верх экрана всегда прицел", tag);
    }

    // Крестовина: одна кнопка — одно направление, две — диагональ.
    {
        Game g;
        freshGame(g, 1600, 720);
        TouchPointer p[TOUCH_MAX_POINTERS];
        TouchLayout l;
        l.layout(1600, 720, TOUCH_SCHEME_DPAD_LEFT);
        float x, y;

        center(l.padRight, x, y);
        touchBegin(p, g, l, 60, x, y);
        check(p[0].role == TOUCH_PAD_RIGHT, "кнопка «вправо» крестовины — это вправо");
        touchUpdate(g, p);
        check(g.keyD && !g.keyA && !g.keyW && !g.keyS, "крестовина вправо ставит только D");
        g.playerX = 800.0f;
        step(g, 20);
        check(g.playerX > 805.0f, "крестовина действительно двигает героя");

        // Вторая кнопка вместе с первой — диагональ, как две клавиши.
        center(l.padUp, x, y);
        touchBegin(p, g, l, 61, x, y);
        touchUpdate(g, p);
        check(g.keyD && g.keyW, "две кнопки крестовины дают диагональ");

        // Отпускаем верх — вправо остаётся нажатым. Проверяем сразу после
        // отпускания: touchEnd не имеет права трогать чужие кнопки.
        touchEnd(p, g, 61);
        check(g.keyD && !g.keyW, "отпускание одной кнопки не снимает вторую");
        touchUpdate(g, p);
        check(g.keyD && !g.keyW, "и на следующем кадре вправо остаётся");
        touchEnd(p, g, 60);
        touchUpdate(g, p);
        check(!g.keyD && !g.keyW, "после отпускания обеих движения нет");

        // В крестовине нет стика: палец в её углу не должен включать стик.
        touchBegin(p, g, l, 62, 40.0f, 640.0f);
        check(p[0].role != TOUCH_STICK, "в схеме с крестовиной стика нет");
        touchEnd(p, g, 62);
    }

    // Зеркальная схема: движение справа, кнопки слева.
    {
        Game g;
        freshGame(g, 1600, 720);
        TouchPointer p[TOUCH_MAX_POINTERS];
        TouchLayout l;
        l.layout(1600, 720, TOUCH_SCHEME_STICK_RIGHT);
        touchBegin(p, g, l, 70, 1400.0f, 600.0f);
        check(p[0].role == TOUCH_STICK, "в зеркальной схеме стик справа");
        touchMove(p, g, 70, 1280.0f, 600.0f);
        touchUpdate(g, p);
        check(g.keyA && !g.keyD, "стик справа, ведём влево — герой идёт влево");
        touchEnd(p, g, 70);
        check(l.dash.x < 1600 / 2, "в зеркальной схеме кнопки рывка слева");
        touchBegin(p, g, l, 71, 200.0f, 300.0f);
        check(p[0].role == TOUCH_AIM, "в зеркальной схеме прицел слева");
        touchEnd(p, g, 71);
    }
}

// ── 8.4. Файл настроек: старый 60-байтный читается, новый пишется ──────────
static void testOptionsFile() {
    printf("файл настроек\n");
    Renderer r;
    r.resize(1600, 720);
    char savedCwd[1024];
    if (!getcwd(savedCwd, sizeof(savedCwd))) savedCwd[0] = 0;
    const char* dir = "/tmp/shadow_touch_probe_cfg";
    mkdir(dir, 0700);
    // Старый файл: ровно раскладка 2.10.52, без полей управления. Обновление
    // игры не должно сбрасывать переключатели игрока.
    {
        LegacyGameOptions60 old{};
        old.magic = 0x53484346;
        old.sfxEnabled = 0;
        old.musicEnabled = 1;
        old.hitEffectsEnabled = 0;
        old.screenShakeEnabled = 1;
        old.advisorEnabled = 0;
        old.hardModeEnabled = 1;
        old.patchNotesHiddenVersion = 3050;
        old.projectileTrailsEnabled = 0;
        old.sfxVolume = 0.31f;
        old.musicVolume = 0.77f;
        old.messageSpeechEnabled = 0;
        old.skillIconStyle = 2;
        old.campaignSnakeMap = 0;
        old.graphicsLevel = 4;
        chdir(dir);
        FILE* f = fopen("shadow_survivors.cfg", "wb");
        bool wrote = f && fwrite(&old, sizeof(old), 1, f) == 1;
        if (f) fclose(f);
        check(wrote && sizeof(old) == 60, "стенд записал настройки 2.10.52 (60 байт)");

        Game g;
        g.simulationMode = true;
        srand(4242);
        g.init();
        check(g.options.sfxEnabled == 0 && g.options.hardModeEnabled == 1,
              "старые настройки прочитаны, а не сброшены");
        check(g.options.graphicsLevel == 4 && g.options.skillIconStyle == 2,
              "хвост старых настроек на месте");
        check(g.options.touchScheme == TOUCH_SCHEME_STICK_LEFT && g.options.touchSetupDone == 0,
              "новых полей в старом файле нет — схема по умолчанию и вопрос при запуске");
        check(sizeof(GameOptions) == 84, "новые настройки занимают 84 байта");
        check(sizeof(LegacyGameOptions68) == 68, "раскладка 2.10.53 осталась 68 байт");
        check(sizeof(LegacyGameOptions72) == 72, "раскладка 2.10.54 осталась 72 байта");
        check(g.options.touchButtonScale == TOUCH_SIZE_DEFAULT,
              "в старом файле нет размера кнопок — он по умолчанию");

        // Запись и чтение нового файла: выбор схемы переживает перезапуск.
        g.options.touchScheme = TOUCH_SCHEME_DPAD_LEFT;
        g.options.touchSetupDone = 1;
        g.simulationMode = false;
        g.saveOptions();
        g.simulationMode = true;
        Game again;
        again.simulationMode = true;
        srand(4242);
        again.init();
        check(again.options.touchScheme == TOUCH_SCHEME_DPAD_LEFT,
              "выбранная схема переживает перезапуск");
        check(again.options.touchSetupDone == 1, "вопрос про схему второй раз не задаётся");
        check(again.options.graphicsLevel == 4 && again.options.sfxEnabled == 0,
              "остальные настройки при этом не поехали");
        remove("shadow_survivors.cfg");
    }
    if (savedCwd[0]) chdir(savedCwd);
}

// ── 8.5. В меню нет стика: касание в нижней левой четверти — это клик ───────
static void testNoStickInMenus() {
    printf("стик только в бою\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    // Строка настроек, которую на телефоне нельзя было нажать: левый нижний угол.
    g.state = STATE_SETTINGS;
    touchBegin(p, g, l, 90, 300.0f, 420.0f);
    check(p[0].role == TOUCH_AIM, "в настройках касание слева-снизу — это клик, а не стик");
    check(g.mouseClicked, "клик в настройках доходит до игры");
    check(!g.keyW && !g.keyA && !g.keyS && !g.keyD, "меню не двигает героя");
    touchEnd(p, g, 90);

    // В бою та же точка — стик: там движение и нужно.
    g.state = STATE_PLAYING;
    touchBegin(p, g, l, 91, 300.0f, 420.0f);
    check(p[0].role == TOUCH_STICK, "в бою та же точка — стик");
    touchEnd(p, g, 91);
}

// ── 8.6. Размер кнопок: настройка, страховка экрана и подписи ───────────────
static void testButtonSize() {
    printf("размер кнопок\n");
    Renderer r;
    r.resize(1600, 720);

    // Ступени: по кругу, без пропусков и без выхода за границы.
    check(touchClampSize(10) == TOUCH_SIZE_MIN, "меньше минимума ужимается до минимума");
    check(touchClampSize(500) == TOUCH_SIZE_MAX, "больше максимума упирается в максимум");
    int walked[16], n = 0, v = TOUCH_SIZE_DEFAULT;
    do { walked[n++] = v; v = touchStepSize(v, 1); } while (v != TOUCH_SIZE_DEFAULT && n < 16);
    check(v == TOUCH_SIZE_DEFAULT && n >= 4, "полный круг возвращает к 100 %, а не к мусору");
    // Туда и обратно: минус после плюса даёт прежний размер.
    check(touchStepSize(touchStepSize(TOUCH_SIZE_DEFAULT, 1), -1) == TOUCH_SIZE_DEFAULT,
          "ступень вверх и обратно — тот же размер");
    // Один круг — от 100 % вверх до максимума, потом снова с минимума: так
    // настройка обходится по кругу, а не упирается в край.
    int seen = 0;
    while (seen < 15 && walked[seen] < walked[seen + 1]) ++seen;
    check(walked[seen] == TOUCH_SIZE_MAX, "по кругу размер доходит до максимума");
    check(walked[seen + 1] == TOUCH_SIZE_MIN, "и с максимума перескакивает на минимум");

    // Геометрия: кнопка растёт вместе с процентом и не выходит за экран.
    for (int percent = TOUCH_SIZE_MIN; percent <= TOUCH_SIZE_MAX; percent += 10) {
        TouchLayout l;
        l.layout(1600, 720, TOUCH_SCHEME_STICK_LEFT, percent);
        char tag[64];
        snprintf(tag, sizeof(tag), "%d %%", percent);
        const bool inside = l.dash.x >= 0 && l.dash.y >= 0 && l.ult.x >= 0 &&
                            l.dash.x + l.dash.w <= 1600 && l.ult.x + l.ult.w <= 1600 &&
                            l.dash.y + l.dash.h <= 720;
        check(inside, "кнопки внутри экрана при любом размере", tag);
        const bool apart = l.dash.x >= l.ult.x + l.ult.w - 0.5f;
        check(apart, "СУПЕР и РЫВОК не наезжают друг на друга", tag);
        check(l.sizePercent >= TOUCH_SIZE_MIN, "итоговый размер не ниже минимума", tag);
    }
    // Больше — значит действительно больше.
    TouchLayout small, big;
    small.layout(1600, 720, TOUCH_SCHEME_STICK_LEFT, 80);
    big.layout(1600, 720, TOUCH_SCHEME_STICK_LEFT, 160);
    check(big.dash.w > small.dash.w * 1.5f, "160 % заметно крупнее 80 %");
    check(big.stickBase > small.stickBase, "вместе с кнопками растёт и стик");

    // Страховка: на очень узком кадре (окно в сплите — логический кадр вроде
    // 320x568) большой процент не выталкивает кнопки за левый край и не
    // сваливает их друг на друга. Без страховки при 160 % они уже не влезают.
    TouchLayout narrow;
    narrow.layout(320, 568, TOUCH_SCHEME_STICK_LEFT, TOUCH_SIZE_MAX);
    check(narrow.dash.x >= 0 && narrow.ult.x >= 0 &&
          narrow.ult.x + narrow.ult.w <= narrow.dash.x + 0.5f &&
          narrow.dash.x + narrow.dash.w <= 320, "на узком кадре кнопки остаются в кадре");
    check(narrow.sizePercent < TOUCH_SIZE_MAX, "и размер честно ужимается, а не рисуется как просили");

    // Подписи: на маленьком размере обе кнопки переходят на короткую форму.
    for (int percent : {TOUCH_SIZE_MIN, TOUCH_SIZE_DEFAULT, TOUCH_SIZE_MAX}) {
        TouchLayout l;
        l.layout(1600, 720, TOUCH_SCHEME_STICK_LEFT, percent);
        const bool fitsFull = r.textWidth(FONT_SMALL, "РЫВОК") <= (int)l.dash.w - 10;
        const bool fitsShort = r.textWidth(FONT_SMALL, "РЫВ") <= (int)l.dash.w - 10;
        char tag[64];
        snprintf(tag, sizeof(tag), "%d %%: «РЫВОК» %d px, кнопка %d px", percent,
                 r.textWidth(FONT_SMALL, "РЫВОК"), (int)l.dash.w);
        check(fitsFull || fitsShort, "подпись кнопки рывка всегда влезает", tag);
    }
}

// ── 8.7. Внешний вид кнопок: стиль, цвет, прозрачность ─────────────────────
static uint32_t overlayHash(Game& g, TouchLayout& layout, TouchPointer* pointers, int style,
                            int theme, int opacity) {
    Renderer r;
    r.resize(1600, 720);
    TouchSkin skin;
    skin.style = style;
    skin.theme = theme;
    skin.opacity = opacity;
    r.clear(0xFF000000);
    g.render(r);
    drawTouchOverlay(r, g, pointers, layout, skin);
    uint32_t h = 2166136261u;
    for (int i = 0; i < 1600 * 720; ++i) h = (h ^ r.pixels[i]) * 16777619u;
    return h;
}

static void testButtonLook() {
    printf("внешний вид кнопок\n");
    Renderer r;
    r.resize(1600, 720);

    // Границы и ступени.
    check(touchClampStyle(-5) == 0 && touchClampStyle(99) == TOUCH_STYLE_COUNT - 1,
          "стиль упирается в границы списка");
    check(touchClampTheme(-1) == 0 && touchClampTheme(9) == TOUCH_THEME_COUNT - 1,
          "цвет упирается в границы списка");
    check(touchClampOpacity(0) == 40 && touchClampOpacity(1000) == 100,
          "прозрачность держится в 40…100 %");
    check(touchStepOpacity(100, 1) == 40, "прозрачность листается по кругу");
    check(touchStepOpacity(40, -1) == 100, "и в обратную сторону тоже");

    // Стили и цвета — это разные слова, а не разные числа на экране.
    bool namesDiffer = true;
    for (int i = 0; i < TOUCH_STYLE_COUNT; ++i)
        for (int j = i + 1; j < TOUCH_STYLE_COUNT; ++j)
            if (strcmp(touchStyleName(i), touchStyleName(j)) == 0) namesDiffer = false;
    check(namesDiffer, "у каждого стиля своё название");
    namesDiffer = true;
    for (int i = 0; i < TOUCH_THEME_COUNT; ++i)
        for (int j = i + 1; j < TOUCH_THEME_COUNT; ++j)
            if (touchPalette(i).ult == touchPalette(j).ult) namesDiffer = false;
    check(namesDiffer, "у каждой темы свой цвет супера");

    // Рисование: разные настройки дают разную картинку.
    Game g;
    freshGame(g, 1600, 720);
    g.touchControls = true;
    g.state = STATE_PLAYING;
    TouchLayout layout;
    layout.layout(1600, 720);
    TouchPointer pointers[TOUCH_MAX_POINTERS];
    const uint32_t glass = overlayHash(g, layout, pointers, TOUCH_STYLE_GLASS, TOUCH_THEME_AMBER, 100);
    const uint32_t solid = overlayHash(g, layout, pointers, TOUCH_STYLE_SOLID, TOUCH_THEME_AMBER, 100);
    const uint32_t outline = overlayHash(g, layout, pointers, TOUCH_STYLE_OUTLINE, TOUCH_THEME_AMBER, 100);
    const uint32_t round = overlayHash(g, layout, pointers, TOUCH_STYLE_ROUND, TOUCH_THEME_AMBER, 100);
    const uint32_t neon = overlayHash(g, layout, pointers, TOUCH_STYLE_GLASS, TOUCH_THEME_NEON, 100);
    const uint32_t faint = overlayHash(g, layout, pointers, TOUCH_STYLE_GLASS, TOUCH_THEME_AMBER, 40);
    check(glass != solid && glass != outline && glass != round && solid != round,
          "каждый стиль рисуется по-своему");
    check(glass != neon, "другая тема — другая картинка");
    check(glass != faint, "прозрачность тоже видна");
    check(glass == overlayHash(g, layout, pointers, TOUCH_STYLE_GLASS, TOUCH_THEME_AMBER, 100),
          "один и тот же внешний вид рисуется одинаково (кадр детерминирован)");

    // В превью и в бою одна и та же функция: превью не может разойтись с игрой.
    // Размер рендерера совпадает с SCREEN_W/SCREEN_H: помощники рисования
    // считают границы от глобального кадра, а не от локального.
    Renderer p1, p2;
    p1.resize(1600, 720);
    p2.resize(1600, 720);
    p1.clear(0xFF000000);
    p2.clear(0xFF000000);
    g.drawSchemePreview(p1, 10, 10, 400, 300, TOUCH_SCHEME_STICK_LEFT, TOUCH_SIZE_DEFAULT);
    TouchSkin skin;
    skin.style = TOUCH_STYLE_GLASS;
    skin.theme = TOUCH_THEME_AMBER;
    skin.opacity = 100;
    g.drawSchemePreview(p2, 10, 10, 400, 300, TOUCH_SCHEME_STICK_LEFT, TOUCH_SIZE_DEFAULT, &skin);
    bool same = true;
    for (int i = 0; i < 1600 * 720; ++i) if (p1.pixels[i] != p2.pixels[i]) same = false;
    check(same, "превью с явным внешним видом совпадает с превью по настройкам");

    // Настройки: файл 84 байта, прежний 72-байтный читается, внешний вид по умолчанию.
    check(sizeof(GameOptions) == 84, "настройки с внешним видом занимают 84 байта");
    check(sizeof(LegacyGameOptions72) == 72, "раскладка 2.10.54 осталась 72 байта");
}

// ── 9. Прокрутка списков пальцем (колеса на телефоне нет) ───────────────────
static void testSwipeScroll() {
    printf("прокрутка пальцем\n");
    Renderer r;
    r.resize(1600, 720);
    Game g;
    freshGame(g, 1600, 720);
    TouchPointer p[TOUCH_MAX_POINTERS];
    TouchLayout l;
    l.layout(1600, 720);

    // В бою прокрутки нет: там палец — это прицел, и колесо ничего не листает.
    g.state = STATE_PLAYING;
    touchBegin(p, g, l, 80, 1200.0f, 400.0f);
    touchMove(p, g, 80, 1200.0f, 260.0f);
    check(g.mouseWheelDelta == 0, "в бою движение пальцем не превращается в колесо");
    touchEnd(p, g, 80);

    // В списках — превращается: свайп вверх листает вниз (как колесо от себя).
    g.state = STATE_HELP;
    g.mouseWheelDelta = 0;
    touchBegin(p, g, l, 81, 1200.0f, 500.0f);
    for (int i = 1; i <= 10; ++i) touchMove(p, g, 81, 1200.0f, 500.0f - i * 10.0f);
    snprintf(nullptr, 0, "");
    check(g.mouseWheelDelta != 0, "свайп вверх по списку листает список");
    const int delta = g.mouseWheelDelta;
    check(delta < 0 || delta > 0, "направление прокрутки определено");
    touchEnd(p, g, 81);

    // Дрожание пальца на 5 px не листает.
    g.mouseWheelDelta = 0;
    touchBegin(p, g, l, 82, 1200.0f, 500.0f);
    for (int i = 1; i <= 6; ++i) touchMove(p, g, 82, 1200.0f, 500.0f - i * 3.0f);
    check(g.mouseWheelDelta == 0, "мелкое дрожание пальца список не листает");
    touchEnd(p, g, 82);
}

// ── 10. Подсказки и первый запуск: телефонный интерфейс без клавиш ──────────
static void testTouchUi() {
    printf("телефонный интерфейс\n");
    Renderer r;
    r.resize(1600, 720);

    // Клавиатурный билд не должен меняться: подсказка остаётся как была.
    {
        Game g;
        freshGame(g, 1600, 720);
        check(strcmp(g.keyHint("НАЗАД [ESC]"), "НАЗАД [ESC]") == 0,
              "на клавиатуре подсказки не трогаются");
    }

    Game g;
    g.simulationMode = true;
    g.touchControls = true;
    g.options.touchSetupDone = 0;
    g.options.touchScheme = 0;
    srand(4242);
    g.init();
    check(g.state == STATE_CONTROLS_SETUP, "первый запуск на Android спрашивает схему движения");

    // Выбор схемы и переход дальше.
    g.setTouchScheme(TOUCH_SCHEME_DPAD_LEFT);
    check(g.options.touchScheme == TOUCH_SCHEME_DPAD_LEFT, "схема выбирается касанием карточки");
    g.render(r);                              // экран должен отрисоваться без падений
    g.onKeyPress('2');
    check(g.options.touchScheme == TOUCH_SCHEME_STICK_RIGHT, "цифра 2 выбирает стик справа");
    g.finishControlsSetup();
    check(g.options.touchSetupDone == 1, "после «дальше» вопрос больше не задаётся");
    check(g.state != STATE_CONTROLS_SETUP, "после выбора экран закрывается");

    // Второй запуск: вопрос уже не задаётся.
    Game g2;
    g2.simulationMode = true;
    g2.touchControls = true;
    g2.options.touchSetupDone = 1;
    srand(4242);
    g2.init();
    check(g2.state != STATE_CONTROLS_SETUP, "при повторном запуске экран не мешает");

    // Подсказки телефона: ни одной клавиши.
    g2.state = STATE_PLAYING;
    const char* hints[] = {
        "НАЗАД [ESC]", "ПРОДОЛЖИТЬ [ESC]", "ОТЛАДКА [F9]", "ЗАБРАТЬ [ПРОБЕЛ / ЛКМ]",
        "В МЕНЮ [ENTER]", "К РАЗВИЛКЕ [TAB]", "НАЗАД К ГЕРОЮ [ESC]",
        "WASD • ДВИЖЕНИЕ   SPACE • РЫВОК   Q • СУПЕР",
        "ПКМ • КУРСОР   M/N • ЗВУК   F11 • ЭКРАН   ESC • ПАУЗА",
    };
    for (const char* hint : hints) {
        const char* converted = g2.keyHint(hint);
        char detail[200];
        snprintf(detail, sizeof(detail), "«%s» -> «%s»", hint, converted);
        const bool noKeys = !strstr(converted, "[ESC]") && !strstr(converted, "[ENTER]") &&
                            !strstr(converted, "[TAB]") && !strstr(converted, "[F9]") &&
                            !strstr(converted, "[F3]") && !strstr(converted, "[F11]") &&
                            !strstr(converted, "[ПРОБЕЛ") && !strstr(converted, "WASD") &&
                            !strstr(converted, "ПКМ") && !strstr(converted, "SPACE") &&
                            !strstr(converted, "M/N") && !strstr(converted, "ESC") &&
                            !strstr(converted, "ENTER") && !strstr(converted, "Q • ");
        check(noKeys && converted[0] != 0, "подсказка телефона без клавиш", detail);
    }
    // И не должна превращать галочку зала славы в клавишу.
    check(strcmp(g2.keyHint("[X] СОТНЯ ВРАГОВ"), "[X] СОТНЯ ВРАГОВ") == 0,
          "галочка «[X] СОТНЯ ВРАГОВ» остаётся галочкой");

    // Раскладка кнопок следует за выбранной схемой.
    TouchLayout l0, l1;
    l0.layout(1600, 720, TOUCH_SCHEME_STICK_LEFT);
    l1.layout(1600, 720, TOUCH_SCHEME_STICK_RIGHT);
    check(l0.dash.x != l1.dash.x, "смена схемы двигает кнопки на другую сторону");
}

int main(int argc, char** argv) {
    g_verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
    printf("стенд касаний: схема из src/touch_input.h против настоящей игры\n\n");
    testLayout();
    testStick();
    testButtons();
    testAim();
    testMultiTouch();
    testTapIsClick();
    testLongRun();
    testSchemes();
    testOptionsFile();
    testNoStickInMenus();
    testButtonSize();
    testButtonLook();
    testSwipeScroll();
    testTouchUi();
    printf("\nитого: %d проверок, провалов %d\n", g_passed + g_failed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
