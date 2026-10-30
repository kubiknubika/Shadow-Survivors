#pragma once
// ── Виртуальное управление для касаний ───────────────────────────────────────
//
// Схема лежит отдельно от Android-кода, потому что её нужно уметь нарисовать и
// проверить без телефона: tools/touch_preview.cpp собирает кадр боя с этим
// управлением и сохраняет картинку. Одна схема — один код, никаких «на телефоне
// чуть другое».
//
// Правила игры здесь не меняются: стик ставит те же флаги, что WASD, а тап —
// те же поля мыши, что клик. Поэтому вся логика меню и боя остаётся прежней.
#include <cmath>
#include <cstdio>
#include <stdint.h>
#include "renderer.h"

enum TouchRole {
    TOUCH_NONE = 0,
    TOUCH_STICK,   // левый палец — движение
    TOUCH_AIM,     // правый палец — курсор, тап — клик
    TOUCH_DASH,    // кнопка рывка
    TOUCH_ULT,     // кнопка супера
    TOUCH_PAUSE,   // пауза
    TOUCH_DEBUG    // F3
};

struct TouchRect { float x = 0, y = 0, w = 0, h = 0; };

struct TouchLayout {
    TouchRect dash, ult, pause, debug;
    // Пересчитывается на кадр: экран бывает разным (телефон, планшет, эмулятор).
    void layout(int screenW, int screenH) {
        const float pad = 18.0f, big = 92.0f, small = 46.0f;
        const float bottom = (float)screenH - pad - big;
        ult = { (float)screenW - pad * 2 - big * 2, bottom, big, big };
        dash = { (float)screenW - pad - big, bottom, big, big };
        // Верхний правый угол занят панелью золота и убийств, поэтому служебные
        // кнопки стоят под ней, а не поверх текста.
        const float topRow = pad + (float)screenH * 0.17f;
        pause = { (float)screenW - pad - small, topRow, small, small };
        debug = { (float)screenW - pad * 2 - small * 2, topRow, small, small };
    }
    static bool inside(const TouchRect& r, float x, float y) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }
};

struct TouchPointer {
    int32_t id = -1;
    TouchRole role = TOUCH_NONE;
    float x = 0.0f, y = 0.0f;
    float baseX = 0.0f, baseY = 0.0f;   // центр стика — там, где палец коснулся
};

static constexpr int TOUCH_MAX_POINTERS = 8;

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
    } else if (lx < SCREEN_W * 0.45f && ly > SCREEN_H * 0.30f) {
        // Левая нижняя часть экрана — стик. Мёртвая зона сверху, чтобы палец,
        // лежащий на краю, не дёргал героя.
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
        pointers[i].x = lx;
        pointers[i].y = ly;
        if (pointers[i].role == TOUCH_AIM) {
            g.mouseX = (int)lx;
            g.mouseY = (int)ly;
        }
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
        pointers[i].id = -1;
        pointers[i].role = TOUCH_NONE;
        return;
    }
}

// Каждый кадр: смещение стика превращается в те же флаги, что ставит клавиатура.
// Диагональ работает как на клавиатуре — нормализуется в игре.
//
// Если пальца на стике нет, флаги НЕ обнуляются: их мог поставить физический
// клавиатурный ввод (AKEYCODE_W/A/S/D в main_android.cpp, геймпад на Android TV,
// клавиатура планшета). Раньше здесь стоял сброс, и на телефоне любая
// клавиатура работала ровно один кадр — до первого touchUpdate. Отпускание
// пальца снимает флаги само (touchEnd), а зависший палец закрывается ACTION_CANCEL.
template <typename GameT>
inline void touchUpdate(GameT& g, const TouchPointer* pointers) {
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role != TOUCH_STICK) continue;
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
}

// Кнопки видны всегда, стик — пока палец на экране. Рисуется в тот же кадр,
// поэтому масштабирование на устройстве общее с игрой.
template <typename GameT>
inline void drawTouchOverlay(Renderer& r, GameT& g, const TouchPointer* pointers, const TouchLayout& layout) {
    const bool playing = g.state == STATE_PLAYING || g.state == STATE_PAUSED ||
                         g.state == STATE_LEVELUP || g.state == STATE_CHEST ||
                         g.state == STATE_BOSS_MUSIC_TEST;
    if (!playing) return;

    auto button = [&](const TouchRect& b, const char* label, uint32_t accent, bool hot) {
        const int x = (int)b.x, y = (int)b.y, w = (int)b.w, h = (int)b.h;
        r.fillRoundedRect(x, y, w, h, 14, hot ? 0xFF3A2A5A : 0xFF2A1E44, hot ? 210 : 150);
        r.drawRoundedRect(x, y, w, h, 14, accent, 2, hot ? 235 : 170);
        r.drawTextCentered(FONT_SMALL, x + w / 2, y + h / 2 - 8, label, accent, true, 235);
    };

    bool dashHot = false, ultHot = false;
    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role == TOUCH_DASH) dashHot = true;
        if (pointers[i].role == TOUCH_ULT) ultHot = true;
    }
    button(layout.ult, "СУПЕР", 0xFFFFD24A, ultHot);
    button(layout.dash, "РЫВОК", 0xFF6FE0FF, dashHot);
    button(layout.pause, "II", 0xFFC8BEDA, false);
    button(layout.debug, "F3", 0xFF8FD0E8, g.debugOverlay);

    for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) {
        if (pointers[i].role != TOUCH_STICK) continue;
        const float dx = std::max(-60.0f, std::min(60.0f, pointers[i].x - pointers[i].baseX));
        const float dy = std::max(-60.0f, std::min(60.0f, pointers[i].y - pointers[i].baseY));
        r.drawCircleRing((int)pointers[i].baseX, (int)pointers[i].baseY, 58, 3, 0xFFE8DEFF, 90);
        r.fillCircle((int)(pointers[i].baseX + dx), (int)(pointers[i].baseY + dy), 26, 0xFFB9A8E8, 190);
        r.drawCircleRing((int)(pointers[i].baseX + dx), (int)(pointers[i].baseY + dy), 26, 2, 0xFFEDE6FF, 200);
    }
}
