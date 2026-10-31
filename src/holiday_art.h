#pragma once
// ── Праздничная графика: 32 символа и 16 персонажей ────────────────────────────
//
// У каждого праздника четыре символа и два персонажа — всего 48 рисунков.
// Они рисуются примитивами, а не запекаются в спрайты, по двум причинам:
//   • их видно в трёх местах с разным размером (арена, карточка календаря,
//     строка меню), а запекать 48 картинок ради двухнедельного окна — дорого;
//   • силуэт должен читаться на 24 px, и это проще проверить, когда форма
//     задана в долях от размера, а не пикселями конкретного спрайта.
//
// Вся геометрия задана в единичных координатах: (−1,−1) — левый верх, (1,1) —
// правый низ. Поэтому один и тот же рисунок работает и на 22 px в слоте, и на
// 200 px в календаре.
#include "renderer.h"
#include "holidays.h"

struct HolidayArt {
    Renderer& r;
    float t = 0.0f;      // время для анимации
    int cx = 0, cy = 0;  // центр
    int S = 16;          // половина размера в пикселях
    bool flash = false;  // попадание: фигура вспыхивает
    int alpha = 255;

    HolidayArt(Renderer& renderer, int x, int y, int size, float anim = 0.0f, bool hitFlash = false)
        : r(renderer), t(anim), cx(x), cy(y), S(std::max(3, size / 2)), flash(hitFlash) {}

    uint32_t tint(uint32_t c) const {
        if (!flash) return c;
        const int rr = (c >> 16) & 0xFF, gg = (c >> 8) & 0xFF, bb = c & 0xFF;
        const int mr = (rr + 255 * 3) / 4, mg = (gg + 255 * 3) / 4, mb = (bb + 255 * 3) / 4;
        return 0xFF000000u | (uint32_t)(mr << 16) | (uint32_t)(mg << 8) | (uint32_t)mb;
    }
    int P(float u) const { return (int)lroundf(u * (float)S); }
    int R(float u) const { return std::max(1, (int)lroundf(u * (float)S)); }
    int A(int a) const { return std::max(0, std::min(255, a * alpha / 255)); }

    void d(float x, float y, float rad, uint32_t c, int a = 255) {
        r.fillCircle(cx + P(x), cy + P(y), R(rad), tint(c), A(a));
    }
    void ring(float x, float y, float rad, float thick, uint32_t c, int a = 235) {
        r.drawCircleRing(cx + P(x), cy + P(y), R(rad), std::max(1, R(thick)), tint(c), A(a), false);
    }
    void l(float x1, float y1, float x2, float y2, uint32_t c, float w = 0.12f, int a = 255) {
        r.drawLine(cx + P(x1), cy + P(y1), cx + P(x2), cy + P(y2), tint(c), std::max(1, R(w)), A(a));
    }
    void box(float x, float y, float w, float h, uint32_t c, int a = 255) {
        const int px = cx + P(x - w), py = cy + P(y - h);
        r.fillRect(px, py, std::max(1, P(w * 2)), std::max(1, P(h * 2)), tint(c), A(a));
    }
    // Треугольник через три линии плюс заливка по строкам: у Renderer нет
    // полигона, а для символов хватает этой формы.
    void tri(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c, int a = 255) {
        const int X1 = cx + P(x1), Y1 = cy + P(y1);
        const int X2 = cx + P(x2), Y2 = cy + P(y2);
        const int X3 = cx + P(x3), Y3 = cy + P(y3);
        const int minY = std::min(Y1, std::min(Y2, Y3)), maxY = std::max(Y1, std::max(Y2, Y3));
        for (int y = minY; y <= maxY; ++y) {
            float lo = 1e9f, hi = -1e9f;
            const int xs[3] = {X1, X2, X3}, ys[3] = {Y1, Y2, Y3};
            for (int k = 0; k < 3; ++k) {
                const int a2 = (k + 1) % 3;
                const float ax = (float)xs[k], ay = (float)ys[k];
                const float bx = (float)xs[a2], by = (float)ys[a2];
                if ((ay <= y && by >= y) || (by <= y && ay >= y)) {
                    const float ax2 = (fabsf(by - ay) < 0.001f) ? ax : ax + (bx - ax) * ((float)y - ay) / (by - ay);
                    lo = std::min(lo, ax2);
                    hi = std::max(hi, ax2);
                }
            }
            if (hi >= lo) r.blendSpan(y, (int)lroundf(lo), (int)lroundf(hi), tint(c), A(a));
        }
    }
    // Сердце: два круга и клин. Нужно трём праздникам, поэтому в хелперах.
    void heart(float x, float y, float s, uint32_t c, int a = 255) {
        d(x - s * 0.38f, y - s * 0.30f, s * 0.52f, c, a);
        d(x + s * 0.38f, y - s * 0.30f, s * 0.52f, c, a);
        tri(x - s * 0.92f, y - s * 0.10f, x + s * 0.92f, y - s * 0.10f, x, y + s * 0.95f, c, a);
    }
    void star5(float x, float y, float rad, uint32_t c, int a = 255) {
        for (int i = 0; i < 5; ++i) {
            const float a1 = -1.5707963f + (float)i * 1.2566371f;
            const float a2 = a1 + 2.5132742f;
            tri(x, y, x + cosf(a1) * rad, y + sinf(a1) * rad,
                x + cosf(a2) * rad * 0.45f, y + sinf(a2) * rad * 0.45f, c, a);
            tri(x, y, x + cosf(a2) * rad, y + sinf(a2) * rad,
                x + cosf(a1) * rad * 0.45f, y + sinf(a1) * rad * 0.45f, c, a);
        }
    }

    // ── Символы ────────────────────────────────────────────────────────────────
    void symbol(HolidayId id, int index, int sizeHint = 0) {
        (void)sizeHint;
        switch (id) {
        case HOLIDAY_HALLOWEEN:
            switch (index) {
            case 0: { // Фонарь Джека
                const uint32_t p = 0xFFFF8A1E, pd = 0xFFB0500C, stem = 0xFF3F7A2A;
                box(0.0f, -0.85f, 0.10f, 0.18f, stem);
                d(-0.55f, 0.0f, 0.55f, pd); d(0.55f, 0.0f, 0.55f, pd);
                d(0.0f, 0.05f, 0.72f, p);
                tri(-0.45f, -0.22f, -0.10f, -0.22f, -0.28f, -0.50f, 0xFF2A0F05);
                tri(0.45f, -0.22f, 0.10f, -0.22f, 0.28f, -0.50f, 0xFF2A0F05);
                box(0.0f, 0.42f, 0.42f, 0.09f, 0xFF2A0F05);
                for (int i = 0; i < 3; ++i) box(-0.24f + i * 0.24f, 0.42f, 0.05f, 0.20f, p);
                break;
            }
            case 1: { // Призрак
                const uint32_t w = 0xFFEDEDF7, s = 0xFFB9B9D6;
                d(0.0f, -0.25f, 0.62f, w);
                box(0.0f, 0.18f, 0.60f, 0.30f, w);
                d(-0.52f, 0.45f, 0.14f, w); d(-0.18f, 0.45f, 0.14f, w);
                d(0.18f, 0.45f, 0.14f, w); d(0.52f, 0.45f, 0.14f, w);
                d(-0.22f, -0.35f, 0.11f, s); d(0.22f, -0.35f, 0.11f, s);
                d(0.0f, -0.02f, 0.08f, s);
                break;
            }
            case 2: { // Летучая мышь
                const uint32_t b = 0xFF6A4A8A, bd = 0xFF3A2450, e = 0xFFFFD84A;
                tri(-0.12f, -0.18f, -1.00f, -0.50f, -0.62f, 0.42f, b);
                tri(0.12f, -0.18f, 1.00f, -0.50f, 0.62f, 0.42f, b);
                // Перепонки темнее крыла: без них силуэт на 26 px сливался.
                l(-0.55f, -0.30f, -0.70f, 0.25f, bd, 0.07f);
                l(0.55f, -0.30f, 0.70f, 0.25f, bd, 0.07f);
                d(0.0f, 0.0f, 0.32f, bd);
                tri(-0.26f, -0.20f, 0.0f, -0.22f, -0.18f, -0.68f, bd);
                tri(0.26f, -0.20f, 0.0f, -0.22f, 0.18f, -0.68f, bd);
                d(-0.12f, -0.04f, 0.08f, e); d(0.12f, -0.04f, 0.08f, e);
                break;
            }
            default: { // Конфета
                const uint32_t w = 0xFFFF9A3A, s = 0xFFF4F0FF;
                tri(-0.30f, 0.0f, -1.00f, -0.45f, -1.00f, 0.45f, w);
                tri(0.30f, 0.0f, 1.00f, -0.45f, 1.00f, 0.45f, w);
                d(0.0f, 0.0f, 0.40f, s);
                l(-0.22f, -0.30f, 0.22f, 0.30f, w, 0.14f);
                l(-0.30f, 0.10f, 0.10f, -0.28f, w, 0.10f);
                break;
            }
            }
            break;
        case HOLIDAY_WINTER:
            switch (index) {
            case 0: { // Ёлка
                const uint32_t g = 0xFF2F9E5A, gd = 0xFF17603A, tr = 0xFF6A4424;
                box(0.0f, 0.84f, 0.14f, 0.20f, tr);
                tri(0.0f, -0.88f, -0.52f, -0.28f, 0.52f, -0.28f, gd);
                tri(0.0f, -0.58f, -0.70f, 0.06f, 0.70f, 0.06f, g);
                tri(0.0f, -0.28f, -0.90f, 0.68f, 0.90f, 0.68f, gd);
                star5(0.0f, -0.98f, 0.22f, 0xFFFFD84A);
                d(-0.30f, -0.20f, 0.11f, 0xFFFF4A4A);
                d(0.34f, -0.08f, 0.11f, 0xFF5AC8FF);
                d(-0.10f, 0.26f, 0.11f, 0xFFFFE070);
                d(0.45f, 0.44f, 0.11f, 0xFF5AC8FF);
                d(-0.48f, 0.48f, 0.11f, 0xFFFF4A4A);
                break;
            }
            case 1: { // Снежинка
                const uint32_t i = 0xFFD8F4FF;
                for (int k = 0; k < 3; ++k) {
                    const float a = (float)k * 1.0471976f; // 0, 60, 120 градусов
                    l(-cosf(a), -sinf(a), cosf(a), sinf(a), i, 0.14f);
                    const float bx = cosf(a) * 0.55f, by = sinf(a) * 0.55f;
                    const float px = -sinf(a) * 0.22f, py = cosf(a) * 0.22f;
                    l(bx, by, bx + px + cosf(a) * 0.18f, by + py + sinf(a) * 0.18f, i, 0.10f);
                    l(bx, by, bx - px + cosf(a) * 0.18f, by - py + sinf(a) * 0.18f, i, 0.10f);
                    l(-bx, -by, -bx + px - cosf(a) * 0.18f, -by + py - sinf(a) * 0.18f, i, 0.10f);
                    l(-bx, -by, -bx - px - cosf(a) * 0.18f, -by - py - sinf(a) * 0.18f, i, 0.10f);
                }
                d(0.0f, 0.0f, 0.12f, 0xFFFFFFFF);
                break;
            }
            case 2: { // Звезда
                star5(0.0f, 0.0f, 0.95f, 0xFFFFD84A);
                star5(0.0f, 0.0f, 0.45f, 0xFFFFF6D0);
                break;
            }
            default: { // Подарок
                const uint32_t b = 0xFFD23A3A, r = 0xFFFFD84A, rl = 0xFFFFF0A8;
                box(0.0f, 0.15f, 0.72f, 0.62f, b);
                box(0.0f, -0.62f, 0.78f, 0.20f, 0xFFB02030);
                box(-0.10f, 0.15f, 0.13f, 0.62f, r);
                box(0.0f, 0.15f, 0.72f, 0.10f, r);
                d(-0.26f, -0.78f, 0.22f, rl); d(0.26f, -0.78f, 0.22f, rl);
                break;
            }
            }
            break;
        case HOLIDAY_LOVE:
            switch (index) {
            case 0: heart(0.0f, 0.05f, 1.0f, 0xFFFF3D68); heart(-0.10f, -0.10f, 0.35f, 0xFFFF9BB4); break;
            case 1: { // Стрела Купидона
                const uint32_t w = 0xFFF4E4C0, g = 0xFFD02040, f = 0xFFFFFFFF;
                l(-0.85f, 0.85f, 0.72f, -0.62f, w, 0.12f);
                tri(0.95f, -0.85f, 0.52f, -0.42f, 0.80f, -0.30f, g);
                l(-0.85f, 0.55f, -0.55f, 0.85f, f, 0.10f);
                l(-0.68f, 0.85f, -0.85f, 0.65f, f, 0.10f);
                break;
            }
            case 2: { // Роза
                const uint32_t rd = 0xFFD81E4A, rl = 0xFFFF6A8A, st = 0xFF2F7A3A;
                l(0.0f, 0.20f, 0.0f, 0.95f, st, 0.12f);
                d(-0.30f, 0.55f, 0.22f, st); d(0.30f, 0.70f, 0.22f, st);
                d(-0.30f, -0.25f, 0.30f, rd); d(0.30f, -0.25f, 0.30f, rd);
                d(0.0f, -0.05f, 0.42f, rd);
                ring(0.0f, -0.20f, 0.28f, 0.10f, rl);
                ring(0.0f, -0.20f, 0.13f, 0.08f, rl);
                break;
            }
            default: { // Конфета-сердце
                const uint32_t w = 0xFFFFC2D6, c = 0xFFFF3D68;
                heart(0.0f, 0.0f, 0.78f, c);
                l(-0.35f, -0.20f, 0.22f, 0.28f, w, 0.10f);
                tri(-0.55f, 0.55f, -0.95f, 0.30f, -0.95f, 0.85f, w);
                tri(0.55f, 0.55f, 0.95f, 0.30f, 0.95f, 0.85f, w);
                break;
            }
            }
            break;
        case HOLIDAY_PATRICK:
            switch (index) {
            case 0: { // Клевер
                const uint32_t g = 0xFF3FD46A, gd = 0xFF1F8A44;
                l(0.0f, 0.10f, 0.0f, 0.95f, gd, 0.13f);
                for (int i = 0; i < 3; ++i) {
                    const float a = -1.5707963f + (float)i * 2.0943951f;
                    heart(cosf(a) * 0.48f, sinf(a) * 0.48f - 0.10f, 0.50f, g);
                }
                d(0.0f, -0.06f, 0.14f, gd);
                break;
            }
            case 1: { // Горшок золота
                const uint32_t pot = 0xFF3B2A1A, gl = 0xFFFFD84A, gl2 = 0xFFFFF0A0;
                for (int i = 0; i < 5; ++i) d(-0.55f + i * 0.28f, -0.45f + (i % 2) * 0.12f, 0.20f, gl);
                tri(-0.75f, -0.10f, 0.75f, -0.10f, 0.55f, 0.85f, pot);
                tri(-0.55f, 0.85f, 0.55f, 0.85f, 0.0f, 1.00f, pot);
                box(0.0f, -0.10f, 0.78f, 0.10f, 0xFF241A10);
                d(0.55f, -0.55f, 0.14f, gl2);
                break;
            }
            case 2: { // Радуга
                const uint32_t cs[4] = {0xFFFF5A5A, 0xFFFFD84A, 0xFF5AE07A, 0xFF5AC8FF};
                for (int i = 0; i < 4; ++i) {
                    for (int k = -8; k <= 8; ++k) {
                        const float a = -3.14159f * (float)(k + 8) / 16.0f;
                        d(cosf(a) * (0.95f - i * 0.14f), sinf(a) * (0.95f - i * 0.14f) + 0.35f,
                          0.075f, cs[i]);
                    }
                }
                d(-0.85f, 0.35f, 0.22f, 0xFFF0F4FF); d(0.85f, 0.35f, 0.22f, 0xFFF0F4FF);
                break;
            }
            default: { // Кружка
                const uint32_t m = 0xFF2F7A3A, f = 0xFFF0F8E8, gd = 0xFFFFD84A;
                ring(0.72f, 0.15f, 0.30f, 0.12f, m);
                box(0.0f, 0.20f, 0.55f, 0.55f, m);
                box(0.0f, -0.40f, 0.55f, 0.16f, f);
                d(-0.22f, -0.62f, 0.16f, f); d(0.22f, -0.62f, 0.16f, f);
                d(0.0f, 0.20f, 0.20f, gd);
                break;
            }
            }
            break;
        case HOLIDAY_EASTER:
            switch (index) {
            case 0: { // Крашенка
                const uint32_t e = 0xFFF2E8D0, s1 = 0xFFD03A5A, s2 = 0xFF3A7AD0, s3 = 0xFFFFC53A;
                d(0.0f, 0.05f, 0.72f, e);
                box(0.0f, -0.30f, 0.62f, 0.10f, s1);
                box(0.0f, 0.05f, 0.70f, 0.10f, s2);
                box(0.0f, 0.40f, 0.58f, 0.10f, s3);
                for (int i = 0; i < 3; ++i) d(-0.30f + i * 0.30f, 0.22f, 0.07f, s1);
                break;
            }
            case 1: { // Кролик
                const uint32_t b = 0xFFF8F5FB, p = 0xFFFFA8C4, e = 0xFF2A1A24;
                const uint32_t bd = 0xFFD8CEDE;
                // Уши — высокие столбики от самой макушки. Кружки над головой
                // с просветом читались как антенны, а не как заячьи уши.
                box(-0.24f, -0.60f, 0.09f, 0.30f, b);
                box(-0.24f, -0.60f, 0.04f, 0.22f, p);
                box(0.24f, -0.60f, 0.09f, 0.30f, b);
                box(0.24f, -0.60f, 0.04f, 0.22f, p);
                d(-0.24f, -0.90f, 0.10f, b); d(0.24f, -0.90f, 0.10f, b);
                d(0.0f, 0.26f, 0.50f, b);
                d(0.0f, 0.30f, 0.34f, bd, 90);
                d(-0.18f, 0.16f, 0.07f, e); d(0.18f, 0.16f, 0.07f, e);
                d(0.0f, 0.34f, 0.09f, p);
                l(-0.10f, 0.42f, 0.10f, 0.42f, e, 0.04f);
                l(-0.30f, 0.30f, -0.62f, 0.20f, bd, 0.04f);
                l(0.30f, 0.30f, 0.62f, 0.20f, bd, 0.04f);
                d(-0.34f, 0.72f, 0.14f, b); d(0.34f, 0.72f, 0.14f, b);
                break;
            }
            case 2: { // Корзина
                const uint32_t w = 0xFFB07A3A, wd = 0xFF7A5020, eg1 = 0xFFFF7A8A, eg2 = 0xFF7AD0FF;
                for (int k = -6; k <= 6; ++k) {
                    const float a = -3.14159f * (float)(k + 6) / 12.0f;
                    d(cosf(a) * 0.62f, sinf(a) * 0.62f - 0.10f, 0.07f, wd);
                }
                d(-0.28f, -0.30f, 0.20f, eg1); d(0.28f, -0.30f, 0.20f, eg2);
                d(0.0f, -0.42f, 0.18f, 0xFFFFD84A);
                tri(-0.80f, -0.05f, 0.80f, -0.05f, 0.55f, 0.85f, w);
                box(0.0f, 0.10f, 0.72f, 0.08f, wd);
                box(0.0f, 0.45f, 0.62f, 0.08f, wd);
                break;
            }
            default: { // Цыплёнок
                const uint32_t y = 0xFFFFD84A, yd = 0xFFD0A020, sh = 0xFFF0EDE4, bk = 0xFF2A1A24;
                d(0.0f, -0.10f, 0.55f, y);
                tri(-0.08f, -0.55f, 0.10f, -0.55f, 0.0f, -0.85f, y);
                d(-0.20f, -0.20f, 0.08f, bk); d(0.20f, -0.20f, 0.08f, bk);
                tri(-0.05f, 0.05f, 0.25f, 0.05f, 0.10f, 0.28f, yd);
                d(-0.45f, 0.10f, 0.18f, y); d(0.45f, 0.10f, 0.18f, y);
                tri(-0.62f, 0.62f, 0.62f, 0.62f, 0.0f, 0.20f, sh);
                break;
            }
            }
            break;
        case HOLIDAY_MASLENITSA:
            switch (index) {
            case 0: { // Блин
                const uint32_t b = 0xFFE8A648, bd = 0xFFB87820, bs = 0xFF8A5410;
                const uint32_t bu = 0xFFFFE08A, jam = 0xFFD83050;
                // Вид сверху: круглый золотой блин с маслом и вареньем. Стопка
                // в три круга читалась как бочонок или буханка.
                d(0.0f, 0.05f, 0.92f, bd);
                d(0.0f, 0.0f, 0.86f, b);
                ring(0.0f, 0.0f, 0.50f, 0.05f, bs, 150);
                ring(0.0f, 0.0f, 0.74f, 0.04f, bs, 110);
                box(0.0f, -0.05f, 0.20f, 0.18f, bu);
                d(-0.36f, 0.16f, 0.16f, jam);
                d(0.30f, 0.22f, 0.13f, jam);
                d(-0.12f, 0.46f, 0.10f, jam, 200);
                break;
            }
            case 1: { // Солнце
                const uint32_t s = 0xFFFFC53A, sc = 0xFFFFF0B0;
                for (int i = 0; i < 12; ++i) {
                    const float a = (float)i * 0.5235988f;
                    l(cosf(a) * 0.62f, sinf(a) * 0.62f, cosf(a) * 1.0f, sinf(a) * 1.0f, s, 0.12f);
                }
                d(0.0f, 0.0f, 0.62f, s);
                d(0.0f, 0.0f, 0.38f, sc);
                break;
            }
            case 2: { // Чучело
                const uint32_t st = 0xFFC8A040, cl = 0xFF8A5A8A, h = 0xFFE8C878;
                l(0.0f, 0.35f, 0.0f, 1.00f, 0xFF6A4A20, 0.10f);
                l(-0.65f, -0.18f, 0.65f, -0.18f, 0xFF6A4A20, 0.09f);
                tri(-0.42f, -0.45f, 0.42f, -0.45f, 0.0f, 0.65f, cl);
                d(0.0f, -0.62f, 0.30f, h);
                d(-0.12f, -0.66f, 0.05f, 0xFF2A1A24); d(0.12f, -0.66f, 0.05f, 0xFF2A1A24);
                box(0.0f, -0.90f, 0.32f, 0.09f, 0xFFB03060);
                break;
            }
            default: { // Самовар
                const uint32_t m = 0xFFC8902A, md = 0xFF8A5A1A, gl = 0xFFFFE8A0;
                d(0.0f, 0.15f, 0.55f, m);
                box(0.0f, -0.35f, 0.30f, 0.18f, md);
                box(0.0f, 0.72f, 0.42f, 0.10f, md);
                l(-0.55f, 0.30f, -0.90f, 0.05f, md, 0.12f);
                ring(0.62f, 0.20f, 0.22f, 0.09f, md);
                d(0.0f, 0.10f, 0.14f, gl);
                l(-0.10f, -0.62f, -0.20f, -0.95f, 0xFFF0F4FF, 0.07f, 130);
                l(0.15f, -0.70f, 0.10f, -1.00f, 0xFFF0F4FF, 0.07f, 130);
                break;
            }
            }
            break;
        case HOLIDAY_LUNAR:
            switch (index) {
            case 0: { // Хунбао — красный конверт с золотой монетой
                const uint32_t rd = 0xFFD81E1E, rd2 = 0xFF9A0C0C, gd = 0xFFFFC53A;
                const uint32_t gl = 0xFFFFF0A8;
                // Дракона в мелком слоте заменить нечем: любые попытки нарисовать
                // голову дракона в 86 px читались как кот, рыба или ёжик, поэтому
                // дракон остался персонажем (он там крупный), а символом стал
                // хунбао — красный конверт, самый узнаваемый предмет Лунного года.
                box(0.0f, 0.02f, 0.62f, 0.72f, rd);
                box(0.0f, -0.62f, 0.62f, 0.14f, rd2);
                box(0.0f, 0.02f, 0.50f, 0.05f, rd2, 160);
                d(0.0f, 0.06f, 0.26f, gd);
                d(0.0f, 0.06f, 0.18f, gl);
                ring(0.0f, 0.06f, 0.11f, 0.04f, rd2, 220);
                l(-0.62f, -0.20f, -0.86f, -0.34f, gd, 0.06f);
                l(0.62f, -0.20f, 0.86f, -0.34f, gd, 0.06f);
                d(-0.86f, -0.34f, 0.08f, gl); d(0.86f, -0.34f, 0.08f, gl);
                break;
            }
            case 1: { // Фонарь
                const uint32_t rd = 0xFFE82A2A, gd = 0xFFFFC53A;
                l(0.0f, -1.00f, 0.0f, -0.82f, gd, 0.07f);
                box(0.0f, -0.82f, 0.30f, 0.06f, gd);
                d(0.0f, 0.05f, 0.60f, rd);
                box(0.0f, 0.05f, 0.14f, 0.60f, gd, 190);
                box(0.0f, 0.05f, 0.60f, 0.12f, gd, 190);
                box(0.0f, 0.70f, 0.28f, 0.08f, gd);
                l(0.0f, 0.78f, 0.0f, 1.00f, gd, 0.08f);
                break;
            }
            case 2: { // Мандарин
                const uint32_t o = 0xFFFF8A1A, od = 0xFFD05A0A, lf = 0xFF3FA85A;
                d(0.0f, 0.15f, 0.68f, o);
                d(-0.22f, -0.10f, 0.14f, od); d(0.28f, 0.28f, 0.11f, od);
                d(0.22f, -0.05f, 0.09f, od);
                l(0.0f, -0.45f, 0.0f, -0.70f, 0xFF5A3A20, 0.08f);
                tri(0.0f, -0.68f, 0.55f, -0.85f, 0.30f, -0.45f, lf);
                break;
            }
            default: { // Хлопушка
                const uint32_t rd = 0xFFE82A2A, gd = 0xFFFFC53A;
                box(0.0f, 0.20f, 0.24f, 0.55f, rd);
                box(0.0f, -0.25f, 0.26f, 0.10f, gd);
                l(0.0f, -0.35f, 0.0f, -0.62f, gd, 0.07f);
                for (int i = 0; i < 6; ++i) {
                    const float a = -2.6f + i * 0.42f;
                    l(0.0f, -0.45f, cosf(a) * 0.85f, -0.45f + sinf(a) * 0.55f, gd, 0.07f);
                    d(cosf(a) * 0.92f, -0.45f + sinf(a) * 0.60f, 0.08f, 0xFFFFF0A0);
                }
                break;
            }
            }
            break;
        default: { // HOLIDAY_KUPALA
            switch (index) {
            case 0: { // Кострище
                const uint32_t f1 = 0xFFE03A0A, f2 = 0xFFFF8A1E, f3 = 0xFFFFE070;
                const uint32_t wd = 0xFF5A3A20, wl = 0xFF8A5A30;
                // Пламя из кругов, а не из треугольника: ровный треугольник
                // читался как палатка, а рваный край узнаётся сразу.
                d(-0.42f, 0.48f, 0.34f, f1); d(0.42f, 0.48f, 0.34f, f1);
                d(0.0f, 0.52f, 0.42f, f1);
                d(-0.26f, 0.16f, 0.30f, f2); d(0.26f, 0.16f, 0.30f, f2);
                d(0.0f, 0.10f, 0.36f, f2);
                d(0.02f, -0.34f, 0.22f, f2);
                d(0.0f, 0.30f, 0.26f, f3);
                d(0.0f, 0.06f, 0.18f, f3);
                l(-0.90f, 0.92f, 0.72f, 0.66f, wd, 0.17f);
                l(0.90f, 0.92f, -0.72f, 0.66f, wl, 0.17f);
                d(-0.90f, 0.92f, 0.09f, wd); d(0.90f, 0.92f, 0.09f, wl);
                for (int i = 0; i < 5; ++i) {
                    const float a = -2.8f + (float)i * 0.40f;
                    d(cosf(a) * 0.72f, -0.55f + sinf(a) * 0.55f, 0.06f, f3, 210);
                }
                break;
            }
            case 1: { // Папоротник
                const uint32_t st = 0xFF2F7A3A, lf = 0xFF5AC85A, fl = 0xFFB8F0FF;
                l(0.0f, 0.90f, 0.0f, -0.35f, st, 0.10f);
                for (int i = 0; i < 4; ++i) {
                    const float y = 0.75f - i * 0.30f;
                    l(0.0f, y, -0.55f - i * 0.05f, y - 0.30f, lf, 0.07f);
                    l(0.0f, y, 0.55f + i * 0.05f, y - 0.30f, lf, 0.07f);
                }
                star5(0.0f, -0.55f, 0.38f, fl);
                d(0.0f, -0.55f, 0.12f, 0xFFF4FFFF);
                break;
            }
            case 2: { // Венок
                const uint32_t g1 = 0xFF3FA85A, g2 = 0xFFFF8AB0, g3 = 0xFFFFD84A;
                ring(0.0f, 0.0f, 0.72f, 0.20f, g1, 220);
                for (int i = 0; i < 8; ++i) {
                    const float a = (float)i * 0.7853982f;
                    d(cosf(a) * 0.72f, sinf(a) * 0.72f, 0.13f, (i % 3 == 0) ? g3 : g2);
                }
                d(0.0f, 0.0f, 0.16f, 0xFFF0E8D0);
                d(0.0f, 0.0f, 0.08f, 0xFFFF8A3A);
                break;
            }
            default: { // Вода
                const uint32_t w1 = 0xFF3AA8E8, w2 = 0xFF9BE0FF;
                for (int k = 0; k < 3; ++k) {
                    const float y = -0.30f + k * 0.40f;
                    for (int i = -6; i <= 6; ++i) {
                        const float x = (float)i / 7.0f;
                        d(x, y + sinf(x * 6.0f + t * 2.0f) * 0.10f, 0.11f, k == 1 ? w2 : w1);
                    }
                }
                d(0.0f, -0.70f, 0.14f, w2);
                break;
            }
            }
            break;
        }
        }
    }

    // ── Персонажи ──────────────────────────────────────────────────────────────
    // bob — общий для всех: фигуры дышат, иначе толпа выглядит наклейками.
    void character(HolidayId id, int index, float bob = 0.0f) {
        cy += P(bob);
        switch (id) {
        case HOLIDAY_HALLOWEEN:
            if (index == 0) { // Ведьма на метле
                const uint32_t cl = 0xFF5A3A86, cl2 = 0xFF7A54A8, hat = 0xFF3A2458;
                const uint32_t sk = 0xFF9BE88A, br = 0xFF9A7040, band = 0xFFFFC53A;
                for (int i = 0; i < 5; ++i)
                    l(0.55f, 0.30f, 1.05f, 0.10f + i * 0.10f, 0xFFD8B060, 0.06f);
                l(-1.00f, 0.55f, 0.55f, 0.30f, br, 0.10f);
                tri(-0.42f, -0.35f, 0.30f, -0.10f, 0.0f, 0.65f, cl);
                d(-0.05f, 0.10f, 0.22f, cl2, 120);
                tri(-0.72f, -0.42f, 0.44f, -0.42f, 0.08f, -1.02f, hat);
                box(0.0f, -0.44f, 0.56f, 0.06f, band);
                d(-0.12f, -0.20f, 0.22f, sk);
                d(-0.19f, -0.22f, 0.05f, 0xFF14300E); d(-0.01f, -0.22f, 0.05f, 0xFF14300E);
                tri(0.02f, -0.12f, 0.20f, -0.12f, 0.10f, 0.00f, 0xFF14300E);
                d(0.42f, 0.05f, 0.11f, 0xFFFFC53A);
            } else { // Тыквоголовый
                const uint32_t body = 0xFF2E2030, p = 0xFFFF8A1E;
                box(0.0f, 0.45f, 0.42f, 0.45f, body);
                l(-0.42f, 0.20f, -0.70f, 0.65f, body, 0.14f);
                l(0.42f, 0.20f, 0.70f, 0.65f, body, 0.14f);
                d(-0.22f, 0.98f, 0.16f, body); d(0.22f, 0.98f, 0.16f, body);
                d(-0.28f, -0.45f, 0.32f, p); d(0.28f, -0.45f, 0.32f, p);
                d(0.0f, -0.42f, 0.40f, p);
                tri(-0.35f, -0.55f, -0.10f, -0.55f, -0.22f, -0.72f, 0xFF2A0F05);
                tri(0.35f, -0.55f, 0.10f, -0.55f, 0.22f, -0.72f, 0xFF2A0F05);
                box(0.0f, -0.25f, 0.26f, 0.05f, 0xFF2A0F05);
                box(0.0f, -0.95f, 0.07f, 0.14f, 0xFF3F7A2A);
            }
            break;
        case HOLIDAY_WINTER:
            if (index == 0) { // Снеговик
                const uint32_t sn = 0xFFF0F6FF, sd = 0xFFC8D8EC, carrot = 0xFFFF8A2A;
                const uint32_t sc = 0xFFD03A4A, coal = 0xFF1A1A22;
                d(0.0f, 0.60f, 0.55f, sd); d(0.0f, 0.55f, 0.50f, sn);
                d(0.0f, -0.15f, 0.38f, sd); d(0.0f, -0.20f, 0.34f, sn);
                d(0.0f, -0.70f, 0.28f, sd); d(0.0f, -0.74f, 0.25f, sn);
                d(-0.09f, -0.80f, 0.05f, coal); d(0.09f, -0.80f, 0.05f, coal);
                tri(0.0f, -0.70f, 0.30f, -0.66f, 0.0f, -0.62f, carrot);
                for (int i = 0; i < 3; ++i) d(0.0f, -0.30f + i * 0.14f, 0.05f, coal);
                box(0.0f, -0.42f, 0.30f, 0.07f, sc);
                l(-0.30f, -0.20f, -0.62f, -0.45f, 0xFF7A5030, 0.06f);
                l(0.30f, -0.20f, 0.62f, -0.45f, 0xFF7A5030, 0.06f);
                tri(-0.24f, -0.98f, 0.24f, -0.98f, 0.0f, -1.25f, 0xFF2A2A3A);
            } else { // Олень
                const uint32_t fur = 0xFF8A5230, fur2 = 0xFF6A3A20, ant = 0xFFD8C8A8;
                const uint32_t nose = 0xFFFF3A4A;
                d(0.0f, 0.35f, 0.55f, fur2);
                d(0.45f, -0.10f, 0.32f, fur);
                l(0.70f, -0.20f, 1.00f, -0.30f, fur, 0.16f);
                l(0.30f, -0.35f, 0.20f, -0.80f, ant, 0.07f);
                l(0.20f, -0.80f, 0.05f, -0.95f, ant, 0.06f);
                l(0.20f, -0.80f, 0.35f, -0.95f, ant, 0.06f);
                l(0.45f, -0.40f, 0.50f, -0.88f, ant, 0.07f);
                l(0.50f, -0.88f, 0.38f, -1.00f, ant, 0.06f);
                l(0.50f, -0.88f, 0.65f, -1.00f, ant, 0.06f);
                d(0.98f, -0.32f, 0.10f, nose);
                d(0.55f, -0.18f, 0.05f, 0xFF1A1010);
                for (int i = 0; i < 4; ++i) {
                    const float x = -0.45f + (float)i * 0.30f;
                    l(x, 0.78f, x + (i < 2 ? -0.05f : 0.05f), 1.00f, fur2, 0.10f);
                    d(x + (i < 2 ? -0.05f : 0.05f), 1.00f, 0.07f, 0xFF2A1A10);
                }
            }
            break;
        case HOLIDAY_LOVE:
            if (index == 0) { // Купидон
                const uint32_t sk = 0xFFFFD8C0, skd = 0xFFE8B090, wg = 0xFFF8F4FF;
                const uint32_t wd = 0xFFD8C8EC, bow = 0xFFC8A060, ar = 0xFFFF3D68;
                l(-0.10f, -0.10f, -0.55f, 0.30f, sk, 0.09f);
                l(0.10f, -0.10f, 0.55f, 0.30f, sk, 0.09f);
                // Крылья: два пера сверху и одно снизу с тёмной кромкой — иначе
                // на светлом фоне они сливались с телом.
                for (int side = -1; side <= 1; side += 2) {
                    const float sx = 0.22f * (float)side;
                    tri(sx, -0.30f, sx + 0.55f * side, -0.55f, sx + 0.20f * side, 0.05f, wg);
                    tri(sx, -0.15f, sx + 0.48f * side, 0.05f, sx + 0.15f * side, 0.28f, wg);
                    l(sx + 0.10f * side, -0.20f, sx + 0.45f * side, -0.45f, wd, 0.03f);
                }
                tri(-0.16f, -0.12f, 0.16f, -0.12f, 0.0f, 0.42f, sk);
                d(0.0f, 0.45f, 0.14f, skd);
                d(0.0f, -0.32f, 0.22f, sk);
                d(-0.09f, -0.30f, 0.05f, 0xFF3A2030); d(0.09f, -0.30f, 0.05f, 0xFF3A2030);
                ring(0.0f, -0.70f, 0.17f, 0.04f, 0xFFFFD84A, 190);
                // Лук и стрела: без них Купидон — просто младенец с крыльями.
                for (int k = 0; k <= 6; ++k) {
                    const float a = -1.4f + (float)k * 0.4f;
                    d(0.58f + cosf(a) * 0.30f, -0.05f + sinf(a) * 0.30f, 0.05f, bow);
                }
                l(0.20f, -0.05f, 0.95f, -0.05f, ar, 0.05f);
                tri(1.00f, -0.05f, 0.82f, -0.15f, 0.82f, 0.05f, ar);
            } else { // Сердцеед
                const uint32_t c = 0xFFD81E4A, cd = 0xFF8A0A28, leg = 0xFF5A0A1A;
                heart(0.0f, -0.20f, 0.80f, c);
                d(-0.22f, -0.25f, 0.09f, 0xFFFFFFFF); d(0.22f, -0.25f, 0.09f, 0xFFFFFFFF);
                d(-0.22f, -0.25f, 0.04f, cd); d(0.22f, -0.25f, 0.04f, cd);
                box(0.0f, 0.60f, 0.10f, 0.20f, cd);
                l(-0.10f, 0.75f, -0.35f, 1.00f, leg, 0.12f);
                l(0.10f, 0.75f, 0.35f, 1.00f, leg, 0.12f);
                l(-0.60f, 0.10f, -0.95f, 0.40f, leg, 0.10f);
                l(0.60f, 0.10f, 0.95f, 0.40f, leg, 0.10f);
            }
            break;
        case HOLIDAY_PATRICK:
            if (index == 0) { // Лепрекон
                const uint32_t co = 0xFF2F9E4A, cod = 0xFF1A6A30, hat = 0xFF1F7A3A;
                const uint32_t sk = 0xFFFFD8B0, gd = 0xFFFFD84A, beard = 0xFFFF8A2A;
                box(0.0f, 0.40f, 0.38f, 0.45f, co);
                box(0.0f, 0.40f, 0.10f, 0.45f, cod);
                d(0.0f, -0.25f, 0.30f, sk);
                d(0.0f, 0.02f, 0.26f, beard);
                d(-0.10f, -0.30f, 0.04f, 0xFF102410); d(0.10f, -0.30f, 0.04f, 0xFF102410);
                box(0.0f, -0.58f, 0.38f, 0.20f, hat);
                box(0.0f, -0.42f, 0.44f, 0.06f, 0xFF102410);
                box(0.0f, -0.42f, 0.07f, 0.06f, gd);
                l(-0.38f, 0.30f, -0.62f, 0.62f, co, 0.12f);
                l(0.38f, 0.30f, 0.62f, 0.62f, co, 0.12f);
                d(0.66f, 0.62f, 0.14f, gd);
            } else { // Зелёный гоблин
                const uint32_t sk = 0xFF5AC85A, skd = 0xFF2F8A3A, eye = 0xFFFFE04A;
                box(0.0f, 0.40f, 0.34f, 0.40f, skd);
                d(0.0f, -0.10f, 0.42f, sk);
                tri(-0.35f, -0.35f, -0.90f, -0.55f, -0.40f, -0.05f, sk);
                tri(0.35f, -0.35f, 0.90f, -0.55f, 0.40f, -0.05f, sk);
                d(-0.15f, -0.15f, 0.07f, eye); d(0.15f, -0.15f, 0.07f, eye);
                box(0.0f, 0.10f, 0.18f, 0.04f, 0xFF102410);
                l(0.40f, 0.40f, 0.85f, 0.10f, 0xFF7A5A2A, 0.14f);
                d(0.88f, 0.06f, 0.16f, 0xFF5A3A1A);
            }
            break;
        case HOLIDAY_EASTER:
            if (index == 0) { // Пасхальный кролик
                const uint32_t b = 0xFFF6F2F8, bd = 0xFFD8D0E0, p = 0xFFFFA8C0;
                const uint32_t egg = 0xFF6AD0FF, eye = 0xFF2A1A24;
                // Порядок важен: уши, голова, тело, лапы. Раньше голова и тело
                // рисовались одним пятном, и кролик читался снеговиком.
                box(-0.20f, -0.86f, 0.10f, 0.34f, b);
                box(-0.20f, -0.86f, 0.05f, 0.26f, p);
                box(0.22f, -0.86f, 0.10f, 0.34f, b);
                box(0.22f, -0.86f, 0.05f, 0.26f, p);
                d(-0.22f, -0.94f, 0.11f, b); d(0.10f, -0.94f, 0.11f, b);
                d(0.0f, -0.44f, 0.34f, b);
                d(-0.12f, -0.48f, 0.06f, eye); d(0.12f, -0.48f, 0.06f, eye);
                d(0.0f, -0.32f, 0.08f, p);
                d(0.0f, 0.25f, 0.46f, b);
                d(0.0f, 0.30f, 0.28f, bd, 80);
                d(-0.46f, 0.10f, 0.16f, b); d(0.46f, 0.10f, 0.16f, b);
                d(-0.28f, 0.74f, 0.16f, b); d(0.28f, 0.74f, 0.16f, b);
                d(-0.60f, 0.18f, 0.20f, egg);
                d(-0.60f, 0.18f, 0.12f, 0xFFFF5A8A, 210);
                d(-0.60f, 0.18f, 0.06f, 0xFF6AD0FF);
                l(-0.60f, -0.02f, -0.26f, 0.16f, bd, 0.07f);
                break;
            } else { // Крашенка-каталка
                const uint32_t e = 0xFFF2E8D0, s1 = 0xFF6AD0FF, s2 = 0xFFFF5A8A;
                d(0.0f, 0.05f, 0.72f, e);
                box(0.0f, -0.20f, 0.60f, 0.10f, s1);
                box(0.0f, 0.25f, 0.58f, 0.10f, s2);
                d(-0.24f, 0.02f, 0.10f, s2); d(0.10f, -0.45f, 0.08f, s1);
                l(-0.20f, 0.55f, -0.35f, 0.95f, 0xFF8A7030, 0.14f);
                l(0.20f, 0.55f, 0.35f, 0.95f, 0xFF8A7030, 0.14f);
                d(-0.55f, 0.50f, 0.14f, 0xFFB09060); d(0.55f, 0.50f, 0.14f, 0xFFB09060);
            }
            break;
        case HOLIDAY_MASLENITSA:
            if (index == 0) { // Чучело Зимы
                const uint32_t st = 0xFFD8B45A, cl = 0xFF8A4A6A, h = 0xFFE8C878;
                const uint32_t fire = 0xFFFF6A1A;
                l(0.0f, 0.45f, 0.0f, 1.10f, 0xFF6A4A20, 0.12f);
                l(-0.80f, -0.10f, 0.80f, -0.10f, 0xFF6A4A20, 0.10f);
                tri(-0.50f, -0.50f, 0.50f, -0.50f, 0.0f, 0.75f, cl);
                d(0.0f, -0.70f, 0.30f, h);
                d(-0.11f, -0.74f, 0.05f, 0xFF2A1A24); d(0.11f, -0.74f, 0.05f, 0xFF2A1A24);
                box(0.0f, -0.96f, 0.34f, 0.09f, 0xFFB03060);
                for (int i = 0; i < 3; ++i) d(-0.45f + i * 0.45f, 0.80f, 0.14f, fire, 220);
                d(0.0f, 0.86f, 0.18f, 0xFFFFC53A, 200);
            } else { // Блин-катун
                const uint32_t b = 0xFFE0A040, bd = 0xFFB07820, bu = 0xFFFFF0C0;
                d(0.0f, 0.0f, 0.72f, bd);
                d(0.0f, 0.0f, 0.66f, b);
                d(-0.20f, -0.18f, 0.12f, bu); d(0.18f, 0.10f, 0.09f, bu);
                d(-0.22f, -0.05f, 0.08f, 0xFF2A1A0A); d(0.22f, -0.05f, 0.08f, 0xFF2A1A0A);
                l(-0.12f, 0.20f, 0.12f, 0.20f, 0xFF2A1A0A, 0.06f);
                l(-0.30f, 0.60f, -0.55f, 0.95f, bd, 0.14f);
                l(0.30f, 0.60f, 0.55f, 0.95f, bd, 0.14f);
            }
            break;
        case HOLIDAY_LUNAR:
            if (index == 0) { // Лунный дракон
                const uint32_t rd = 0xFFE82A2A, gd = 0xFFFFC53A, dk = 0xFF7A1010;
                for (int i = 4; i >= 1; --i) {
                    const float x = -i * 0.35f;
                    d(x, sinf(t * 3.0f + (float)i) * 0.20f, 0.30f - i * 0.03f, i % 2 ? rd : gd);
                }
                d(0.45f, -0.25f, 0.45f, rd);
                tri(0.30f, -0.60f, 0.50f, -0.60f, 0.38f, -1.00f, gd);
                tri(0.62f, -0.55f, 0.80f, -0.50f, 0.72f, -0.92f, gd);
                d(0.35f, -0.30f, 0.10f, 0xFFFFFFFF); d(0.60f, -0.30f, 0.10f, 0xFFFFFFFF);
                d(0.35f, -0.30f, 0.05f, dk); d(0.60f, -0.30f, 0.05f, dk);
                l(0.20f, 0.10f, -0.10f, 0.35f, gd, 0.07f);
                d(0.85f, -0.20f, 0.10f, dk);
            } else { // Лев-страж
                const uint32_t mn = 0xFFD84A1A, mn2 = 0xFFFFC53A, fc = 0xFFE8B04A;
                const uint32_t eye = 0xFFFFFFFF;
                for (int i = 0; i < 10; ++i) {
                    const float a = (float)i * 0.6283185f;
                    d(cosf(a) * 0.62f, sinf(a) * 0.55f, 0.22f, i % 2 ? mn : mn2);
                }
                d(0.0f, -0.05f, 0.48f, fc);
                d(-0.20f, -0.15f, 0.13f, eye); d(0.20f, -0.15f, 0.13f, eye);
                d(-0.20f, -0.15f, 0.06f, 0xFF1A0A0A); d(0.20f, -0.15f, 0.06f, 0xFF1A0A0A);
                box(0.0f, 0.20f, 0.16f, 0.05f, 0xFF7A1010);
                tri(-0.30f, -0.40f, -0.10f, -0.40f, -0.20f, -0.62f, mn);
                tri(0.30f, -0.40f, 0.10f, -0.40f, 0.20f, -0.62f, mn);
            }
            break;
        default:
            if (index == 0) { // Русалка
                const uint32_t sk = 0xFFC8F0D8, hr = 0xFF2F8A8A, hr2 = 0xFF55C8B8;
                const uint32_t tl = 0xFF3AA8C8, tl2 = 0xFF9BE0FF;
                // Волосы отдельной массой и хвост с плавником: без этого фигура
                // читалась как палочка с головой.
                d(0.0f, -0.62f, 0.32f, hr);
                d(0.0f, -0.58f, 0.23f, sk);
                d(-0.30f, -0.45f, 0.22f, hr); d(0.30f, -0.45f, 0.22f, hr);
                d(-0.09f, -0.58f, 0.04f, 0xFF102020); d(0.09f, -0.58f, 0.04f, 0xFF102020);
                box(0.0f, -0.22f, 0.20f, 0.16f, sk);
                d(-0.18f, -0.20f, 0.12f, sk); d(0.18f, -0.20f, 0.12f, sk);
                tri(-0.30f, -0.10f, 0.30f, -0.10f, 0.0f, 0.45f, tl);
                tri(0.0f, 0.40f, 0.0f, 0.95f, 0.55f, 0.62f, tl2);
                tri(0.0f, 0.40f, 0.0f, 0.95f, -0.55f, 0.62f, tl);
                d(0.0f, 0.48f, 0.12f, tl2, 170);
                for (int i = 0; i < 3; ++i)
                    d(-0.62f + i * 0.10f, -0.30f + i * 0.10f, 0.09f, hr2, 150);
            } else { // Огнецвет
                const uint32_t f1 = 0xFFFF6A1A, f2 = 0xFFFFC53A, core = 0xFFF4FFFF;
                for (int i = 0; i < 6; ++i) {
                    const float a = (float)i * 1.0471976f + t * 0.6f;
                    d(cosf(a) * 0.55f, sinf(a) * 0.45f - 0.10f, 0.26f, i % 2 ? f1 : f2, 235);
                }
                d(0.0f, -0.10f, 0.30f, core);
                d(0.0f, -0.10f, 0.16f, 0xFF8AF0FF);
                for (int i = 0; i < 4; ++i) {
                    const float a = -2.4f + i * 0.6f;
                    d(cosf(a) * 0.85f, -0.30f + sinf(a) * 0.60f, 0.06f, f2, 200);
                }
                l(0.0f, 0.40f, 0.0f, 1.00f, 0xFF2F7A3A, 0.10f);
                l(0.0f, 0.70f, -0.30f, 0.90f, 0xFF2F7A3A, 0.07f);
            }
            break;
        }
    }
};
