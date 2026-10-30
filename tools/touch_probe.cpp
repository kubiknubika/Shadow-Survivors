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
    printf("\nитого: %d проверок, провалов %d\n", g_passed + g_failed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
