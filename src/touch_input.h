#pragma once
// ── Виртуальное управление для касаний ───────────────────────────────────────
//
// Схема лежит отдельно от Android-кода, потому что её нужно уметь нарисовать и
// проверить без телефона: tools/touch_preview.cpp собирает кадр боя с этим
// управлением и сохраняет картинку, tools/touch_probe.cpp прогоняет её против
// настоящей игры. Одна схема — один код, никаких «на телефоне чуть другое».
//
// Правила игры здесь не меняются: стик ставит те же флаги, что WASD, а тап — те
// же поля мыши, что клик. Поэтому вся логика меню и боя остаётся прежней.
//
// Схем движения три (их выбирает игрок; см. экран «УПРАВЛЕНИЕ» в игре):
//   * стик слева  — по умолчанию: левый палец ведёт стик, правый целится;
//   * стик справа — то же зеркально, для левшей;
//   * крестовина  — четыре кнопки-стрелки слева, диагональ двумя пальцами.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include "renderer.h"

enum TouchScheme {
    TOUCH_SCHEME_STICK_LEFT = 0,    // стик в левой нижней части, кнопки справа
    TOUCH_SCHEME_STICK_RIGHT = 1,   // зеркально: стик справа, кнопки слева
    TOUCH_SCHEME_DPAD_LEFT = 2,     // крестовина слева, кнопки справа
    TOUCH_SCHEME_COUNT = 3
};

enum TouchRole {
    TOUCH_NONE = 0,
    TOUCH_STICK,   // левый палец — движение
    TOUCH_AIM,     // правый палец — курсор, тап — клик
    TOUCH_DASH,    // кнопка рывка
    TOUCH_ULT,     // кнопка супера
    TOUCH_PAUSE,   // пауза
    TOUCH_DEBUG,   // панель отладки
    TOUCH_PAD_UP,  // крестовина: четыре кнопки
    TOUCH_PAD_DOWN,
    TOUCH_PAD_LEFT,
    TOUCH_PAD_RIGHT
};

// Размер кнопок — настройка, а не константа: на пятидюймовом телефоне 92 px
// ложатся под палец, на планшете теряются, а рука с крупным пальцем просит
// больше. Границы: ниже 80 % кнопка (74 px логического кадра) становится
// меньше пальца, а подпись «РЫВОК» в неё уже не влезает; выше 160 % пара
// кнопок перестаёт помещаться в нижнюю четверть экрана.
static constexpr int TOUCH_SIZE_MIN = 80;
static constexpr int TOUCH_SIZE_MAX = 160;
static constexpr int TOUCH_SIZE_DEFAULT = 100;

inline int touchClampSize(int percent) {
    return percent < TOUCH_SIZE_MIN ? TOUCH_SIZE_MIN
                                    : (percent > TOUCH_SIZE_MAX ? TOUCH_SIZE_MAX : percent);
}

// Ступени настройки: игрок жмёт строку в настройках и перебирает их по кругу.
// Шаг нелинейный: от 100 % к 110 % видно на глаз, а 100 → 105 — нет.
inline int touchStepSize(int percent, int direction) {
    static const int steps[] = {TOUCH_SIZE_MIN, 90, TOUCH_SIZE_DEFAULT, 110, 125, 140, TOUCH_SIZE_MAX};
    const int count = (int)(sizeof(steps) / sizeof(steps[0]));
    int nearest = 0;
    for (int i = 1; i < count; ++i)
        if (std::fabs((float)steps[i] - (float)percent) < std::fabs((float)steps[nearest] - (float)percent))
            nearest = i;
    int next = nearest + (direction < 0 ? -1 : 1);
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    return steps[next];
}

struct TouchRect { float x = 0, y = 0, w = 0, h = 0; };

struct TouchLayout {
    TouchRect dash, ult, pause, debug;
    TouchRect padUp, padDown, padLeft, padRight;
    int scheme = TOUCH_SCHEME_STICK_LEFT;
    // Размер кнопок в процентах, каким он реально получился: если экран мал,
    // верхняя граница настройки ужимается, и оверлей рисует то же, что и зона
    // касания. Иначе нарисованная кнопка и кнопка, которая нажимается, — разные.
    int sizePercent = TOUCH_SIZE_DEFAULT;
    float stickBase = 58.0f;   // радиус базы стика
    float stickKnob = 26.0f;   // шайба под пальцем
    float stickMax = 60.0f;    // на сколько шайба отходит от базы

    bool mirrored() const { return scheme == TOUCH_SCHEME_STICK_RIGHT; }
    bool dpad() const { return scheme == TOUCH_SCHEME_DPAD_LEFT; }

    // Пересчитывается на кадр: экран бывает разным (телефон, планшет, эмулятор).
    void layout(int screenW, int screenH, int schemeIndex = TOUCH_SCHEME_STICK_LEFT,
                int requestedPercent = TOUCH_SIZE_DEFAULT) {
        scheme = schemeIndex < 0 || schemeIndex >= TOUCH_SCHEME_COUNT ? TOUCH_SCHEME_STICK_LEFT
                                                                     : schemeIndex;
        const bool mirror = mirrored();
        float scale = (float)touchClampSize(requestedPercent) / 100.0f;
        // Экранная страховка: кнопка не может занимать больше четверти меньшей
        // стороны, а пара кнопок — больше половины ширины. На узком окне
        // настройка 160 % иначе вытолкнула бы кнопки за край или друг на друга.
        const float shortest = (float)(screenW < screenH ? screenW : screenH);
        const float byHeight = std::min(scale, shortest * 0.26f / 92.0f);
        const float byWidth = std::min(byHeight, (float)screenW * 0.55f / (2.0f * 92.0f + 3.0f * 18.0f));
        scale = std::max(0.4f, std::min(scale, byWidth));
        sizePercent = (int)std::lround(scale * 100.0f);
        stickBase = 58.0f * scale;
        stickKnob = 26.0f * scale;
        stickMax = 60.0f * scale;
        const float pad = 18.0f * scale, big = 92.0f * scale, small = 46.0f * scale;
        const float bottom = (float)screenH - pad - big;
        // Кнопки действия стоят под большим пальцем свободной руки.
        if (mirror) {
            ult = { pad, bottom, big, big };
            dash = { pad * 2 + big, bottom, big, big };
        } else {
            ult = { (float)screenW - pad * 2 - big * 2, bottom, big, big };
            dash = { (float)screenW - pad - big, bottom, big, big };
        }
        // Верхний правый угол занят панелью золота и убийств, поэтому служебные
        // кнопки стоят под ней, а не поверх текста. В зеркальной схеме — слева.
        const float topRow = pad + (float)screenH * 0.17f;
        if (mirror) {
            pause = { pad, topRow, small, small };
            debug = { pad * 2 + small, topRow, small, small };
        } else {
            pause = { (float)screenW - pad - small, topRow, small, small };
            debug = { (float)screenW - pad * 2 - small * 2, topRow, small, small };
        }

        // Крестовина: четыре кнопки вокруг точки в нижнем углу. Расстояние между
        // центрами 68 px при кнопке 62 px — между ними остаётся зазор, и палец
        // не задевает соседнюю кнопку, когда ведёт по кругу.
        const float s = 62.0f * scale, step = 68.0f * scale;
        const float cx = pad + step + s * 0.5f;
        const float cy = (float)screenH - pad - step - s * 0.5f;
        padUp = { cx - s * 0.5f, cy - step - s * 0.5f, s, s };
        padDown = { cx - s * 0.5f, cy + step - s * 0.5f, s, s };
        padLeft = { cx - step - s * 0.5f, cy - s * 0.5f, s, s };
        padRight = { cx + step - s * 0.5f, cy - s * 0.5f, s, s };
    }

    static bool inside(const TouchRect& r, float x, float y) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    // Где начинается движение: нижняя половина одной из сторон. Верхняя
    // половина всегда остаётся прицелу — так палец, лежащий на краю экрана,
    // не дёргает героя, а тап по меню остаётся тапом.
    bool inMoveZone(float x, float y) const {
        if (y < (float)SCREEN_H * 0.30f) return false;
        if (mirrored()) return x > (float)SCREEN_W * 0.55f;
        return x < (float)SCREEN_W * 0.45f;
    }
};

struct TouchPointer {
    int32_t id = -1;
    TouchRole role = TOUCH_NONE;
    float x = 0.0f, y = 0.0f;
    float baseX = 0.0f, baseY = 0.0f;   // центр стика — там, где палец коснулся
    float lastY = 0.0f;                 // для прокрутки списков пальцем
    float scrollAccum = 0.0f;
};

static constexpr int TOUCH_MAX_POINTERS = 8;

// Состояния, где вообще нужно движение. В меню палец — это только курсор и
// клик: иначе касание в нижней левой четверти экрана (там, где строки настроек)
// превращалось в невидимый стик и не нажимало ничего. На Windows проблема не
// видна: там мышь кликает в любой точке.
inline bool touchGameplayState(int state) {
    return state == STATE_PLAYING || state == STATE_PAUSED || state == STATE_LEVELUP ||
           state == STATE_CHEST || state == STATE_BOSS_MUSIC_TEST;
}

// Состояния, где палец должен листать список, а не только водить курсором.
inline bool touchScrollableState(int state) {
    return state == STATE_PATCH_NOTES || state == STATE_HELP || state == STATE_MISSION_SELECT;
}

// Всё, что касание делает с игрой. Отдельная функция, чтобы десктопный стенд
// вызывал ровно тот же код, что и телефон.
template <typename GameT>
inline void touchBegin(TouchPointer* pointers, GameT& g, const TouchLayout& layout,
                       int32_t id, float lx, float ly) {
    TouchPointer* slot = nullptr;
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i)
        if (pointers[i].id < 0) { slot = &pointers[i]; break; }
    if (!slot) return;
    slot->id = id;
    slot->x = lx; slot->y = ly;
    slot->baseX = lx; slot->baseY = ly;
    slot->lastY = ly;
    slot->scrollAccum = 0.0f;

    if (TouchLayout::inside(layout.pause, lx, ly)) {
        slot->role = TOUCH_PAUSE;
        g.onKeyPress(27);                   // ESC — пауза и выход из подменю
    } else if (TouchLayout::inside(layout.debug, lx, ly)) {
        slot->role = TOUCH_DEBUG;
        g.debugOverlay = !g.debugOverlay;
    } else if (TouchLayout::inside(layout.dash, lx, ly)) {
        slot->role = TOUCH_DASH;
        g.onKeyPress(32);                   // пробел — рывок
        g.mouseClicked = true;
    } else if (TouchLayout::inside(layout.ult, lx, ly)) {
        slot->role = TOUCH_ULT;
        g.onKeyPress('q');                  // Q — супер
        g.mouseClicked = true;
    } else if (layout.dpad() &&
               (TouchLayout::inside(layout.padUp, lx, ly) || TouchLayout::inside(layout.padDown, lx, ly) ||
                TouchLayout::inside(layout.padLeft, lx, ly) || TouchLayout::inside(layout.padRight, lx, ly))) {
        // Крестовина: каждая кнопка — отдельный палец. Две соседние кнопки
        // вместе дают диагональ, как две клавиши на клавиатуре.
        if (TouchLayout::inside(layout.padUp, lx, ly)) slot->role = TOUCH_PAD_UP;
        else if (TouchLayout::inside(layout.padDown, lx, ly)) slot->role = TOUCH_PAD_DOWN;
        else if (TouchLayout::inside(layout.padLeft, lx, ly)) slot->role = TOUCH_PAD_LEFT;
        else slot->role = TOUCH_PAD_RIGHT;
    } else if (touchGameplayState(g.state) && !layout.dpad() && layout.inMoveZone(lx, ly)) {
        // Стик есть только в двух схемах; в крестовине движение — это кнопки, и
        // палец мимо них не должен превращаться в невидимый стик.
        slot->role = TOUCH_STICK;
    } else {
        slot->role = TOUCH_AIM;
        g.mouseX = (int)lx;
        g.mouseY = (int)ly;
        g.mouseLeftDown = true;
        g.mouseClicked = true;              // в меню это обычный клик
    }
}

template <typename GameT>
inline void touchMove(TouchPointer* pointers, GameT& g, int32_t id, float lx, float ly) {
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].id != id) continue;
        const float dy = ly - pointers[i].lastY;
        pointers[i].x = lx;
        pointers[i].y = ly;
        if (pointers[i].role == TOUCH_AIM) {
            g.mouseX = (int)lx;
            g.mouseY = (int)ly;
            // Списки листаются пальцем: колеса на телефоне нет. Один «щелчок»
            // колеса — 120 единиц; порог 22 px, чтобы дрожание не листало.
            if (touchScrollableState(g.state)) {
                pointers[i].scrollAccum += dy;
                while (pointers[i].scrollAccum <= -22.0f) {
                    g.mouseWheelDelta -= 120;
                    pointers[i].scrollAccum += 22.0f;
                }
                while (pointers[i].scrollAccum >= 22.0f) {
                    g.mouseWheelDelta += 120;
                    pointers[i].scrollAccum -= 22.0f;
                }
            }
        }
        pointers[i].lastY = ly;
        return;
    }
}

template <typename GameT>
inline void touchEnd(TouchPointer* pointers, GameT& g, int32_t id) {
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].id != id) continue;
        if (pointers[i].role == TOUCH_STICK) {
            g.keyW = g.keyA = g.keyS = g.keyD = false;
            g.keyUp = g.keyDown = g.keyLeft = g.keyRight = false;
        }
        if (pointers[i].role == TOUCH_AIM) g.mouseLeftDown = false;
        // Крестовина: снимается только та сторона, которую отпустили, — вторая
        // кнопка (и диагональ) продолжают работать.
        if (pointers[i].role == TOUCH_PAD_UP) { g.keyW = false; g.keyUp = false; }
        if (pointers[i].role == TOUCH_PAD_DOWN) { g.keyS = false; g.keyDown = false; }
        if (pointers[i].role == TOUCH_PAD_LEFT) { g.keyA = false; g.keyLeft = false; }
        if (pointers[i].role == TOUCH_PAD_RIGHT) { g.keyD = false; g.keyRight = false; }
        pointers[i].id = -1;
        pointers[i].role = TOUCH_NONE;
        return;
    }
}

// Каждый кадр: смещение стика и нажатые кнопки крестовины превращаются в те же
// флаги, что ставит клавиатура. Диагональ нормализуется в игре.
//
// Если пальца на движении нет, флаги НЕ обнуляются: их мог поставить физический
// клавиатурный ввод (AKEYCODE_W/A/S/D в main_android.cpp, геймпад на Android TV).
// Отпускание пальца снимает флаги само (touchEnd), зависший палец закрывается
// ACTION_CANCEL.
template <typename GameT>
inline void touchUpdate(GameT& g, const TouchPointer* pointers) {
    bool padAny = false, padUp = false, padDown = false, padLeft = false, padRight = false;
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role == TOUCH_STICK) {
            const float dx = pointers[i].x - pointers[i].baseX;
            const float dy = pointers[i].y - pointers[i].baseY;
            const float dead = 14.0f;   // палец слегка дрожит — это не команда
            const bool left = dx < -dead, right = dx > dead;
            const bool up = dy < -dead, down = dy > dead;
            g.keyW = up;      g.keyS = down;
            g.keyA = left;    g.keyD = right;
            g.keyUp = up;     g.keyDown = down;
            g.keyLeft = left; g.keyRight = right;
            return;
        }
        switch (pointers[i].role) {
            case TOUCH_PAD_UP: padAny = padUp = true; break;
            case TOUCH_PAD_DOWN: padAny = padDown = true; break;
            case TOUCH_PAD_LEFT: padAny = padLeft = true; break;
            case TOUCH_PAD_RIGHT: padAny = padRight = true; break;
            default: break;
        }
    }
    if (!padAny) return;
    g.keyW = padUp;      g.keyS = padDown;
    g.keyA = padLeft;    g.keyD = padRight;
    g.keyUp = padUp;     g.keyDown = padDown;
    g.keyLeft = padLeft; g.keyRight = padRight;
}

// Кнопки видны всегда, стик — пока палец на экране. Рисуется в тот же кадр,
// поэтому масштабирование на устройстве общее с игрой.
template <typename GameT>
inline void drawTouchOverlay(Renderer& r, GameT& g, const TouchPointer* pointers, const TouchLayout& layout) {
    const bool playing = g.state == STATE_PLAYING || g.state == STATE_PAUSED ||
                         g.state == STATE_LEVELUP || g.state == STATE_CHEST ||
                         g.state == STATE_BOSS_MUSIC_TEST;
    if (!playing) return;

    // Подпись не должна вылезать за кнопку: при размере 70 % «РЫВОК» шире самой
    // кнопки. Тогда обе кнопки действия переходят на короткую форму сразу —
    // «СУПЕР» и «РЫВОК» рядом должны читаться одинаково.
    auto fits = [&](const TouchRect& b, const char* label) {
        return r.textWidth(FONT_SMALL, label) <= (int)b.w - 10;
    };
    const bool fullLabels = fits(layout.ult, "СУПЕР") && fits(layout.dash, "РЫВОК");
    auto button = [&](const TouchRect& b, const char* label, const char* shortLabel,
                      uint32_t accent, bool hot) {
        const int x = (int)b.x, y = (int)b.y, w = (int)b.w, h = (int)b.h;
        r.fillRoundedRect(x, y, w, h, 14, hot ? 0xFF3A2A5A : 0xFF2A1E44, hot ? 210 : 150);
        r.drawRoundedRect(x, y, w, h, 14, accent, 2, hot ? 235 : 170);
        r.drawTextCentered(FONT_SMALL, x + w / 2, y + h / 2 - 8,
                           fullLabels || strcmp(label, shortLabel) == 0 ? label : shortLabel,
                           accent, true, 235);
    };

    bool dashHot = false, ultHot = false;
    bool padHot[4] = {false, false, false, false};
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role == TOUCH_DASH) dashHot = true;
        if (pointers[i].role == TOUCH_ULT) ultHot = true;
        if (pointers[i].role == TOUCH_PAD_UP) padHot[0] = true;
        if (pointers[i].role == TOUCH_PAD_DOWN) padHot[1] = true;
        if (pointers[i].role == TOUCH_PAD_LEFT) padHot[2] = true;
        if (pointers[i].role == TOUCH_PAD_RIGHT) padHot[3] = true;
    }
    button(layout.ult, "СУПЕР", "СУП", 0xFFFFD24A, ultHot);
    button(layout.dash, "РЫВОК", "РЫВ", 0xFF6FE0FF, dashHot);
    button(layout.pause, "II", "II", 0xFFC8BEDA, false);
    // «F3» — имя клавиши: на телефоне такой клавиши нет, поэтому кнопка зовётся
    // так, как её видит игрок.
    button(layout.debug, "ОТЛ", "ОТЛ", 0xFF8FD0E8, g.debugOverlay);

    if (layout.dpad()) {
        const TouchRect* pads[4] = {&layout.padUp, &layout.padDown, &layout.padLeft, &layout.padRight};
        const char* arrows[4] = {"^", "v", "<", ">"};
        for (int i = 0; i < 4; ++i) {
            const int x = (int)pads[i]->x, y = (int)pads[i]->y;
            const int w = (int)pads[i]->w, h = (int)pads[i]->h;
            r.fillRoundedRect(x, y, w, h, 12, padHot[i] ? 0xFF3A2A5A : 0xFF231A38,
                              padHot[i] ? 220 : 140);
            r.drawRoundedRect(x, y, w, h, 12, padHot[i] ? 0xFFE8DEFF : 0xFF7868A0, 2,
                              padHot[i] ? 240 : 160);
            r.drawTextCentered(FONT_MED, x + w / 2, y + h / 2 - 12, arrows[i], 0xFFEDE6FF, true, 235);
        }
    }

    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role != TOUCH_STICK) continue;
        const float maxOut = layout.stickMax;
        const float dx = std::max(-maxOut, std::min(maxOut, pointers[i].x - pointers[i].baseX));
        const float dy = std::max(-maxOut, std::min(maxOut, pointers[i].y - pointers[i].baseY));
        const int ring = (int)layout.stickBase, knob = (int)layout.stickKnob;
        r.drawCircleRing((int)pointers[i].baseX, (int)pointers[i].baseY, ring, 3, 0xFFE8DEFF, 90);
        r.fillCircle((int)(pointers[i].baseX + dx), (int)(pointers[i].baseY + dy), knob, 0xFFB9A8E8, 190);
        r.drawCircleRing((int)(pointers[i].baseX + dx), (int)(pointers[i].baseY + dy), knob, 2, 0xFFEDE6FF, 200);
    }
}

// Названия и описания схем — одни и те же на экране выбора и в настройках.
inline const char* touchSchemeName(int scheme) {
    switch (scheme) {
        case TOUCH_SCHEME_STICK_RIGHT: return "СТИК СПРАВА";
        case TOUCH_SCHEME_DPAD_LEFT: return "КРЕСТОВИНА";
        default: return "СТИК СЛЕВА";
    }
}

inline const char* touchSchemeShort(int scheme) {
    switch (scheme) {
        case TOUCH_SCHEME_STICK_RIGHT: return "СПРАВА";
        case TOUCH_SCHEME_DPAD_LEFT: return "КРЕСТ";
        default: return "СЛЕВА";
    }
}
