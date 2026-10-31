// ── Стенд календаря праздников: пересечения окон и «свой день» ──────────────
//
// Зачем отдельный стенд. Окна праздников регулярно накладываются друг на друга
// (Лунный новый год накрывает Валентина, Масленица — Патрика и Валентина), и в
// 2.11.0 в такой день побеждал тот, кто выше в таблице: 14 февраля 2024 года
// игра показывала Лунный новый год вместо Дня влюблённых. Глазами это ловится
// только в те годы, когда совпадение есть, поэтому проверка перебирает все дни
// 2024–2060 годов.
//
//     g++ -O2 -std=c++17 -Isrc tools/holiday_probe.cpp -o build/holiday_probe
//     build/holiday_probe
//
// Что проверяется:
//  1. в «свой день» любого праздника побеждает именно он (и он первый в списке);
//  2. в дне не больше трёх активных праздников, и каждый из списка реально активен;
//  3. расстояния до «своего дня» в списке не убывают — список отсортирован;
//  4. порядок детерминирован: два вызова на одну дату дают один ответ;
//  5. известные совпадения прошлых лет (14.02, 17.03, 16–17.02.2026) отдают
//     правильный праздник — те самые даты, на которых ломался выбор;
//  6. окно Нового года, переходящее через 31 декабря, считается как близкое, а
//     не «через 364 дня».
#include <cstdio>
#include <cstring>
#include "holidays.h"

static int failures = 0;

static void expect(bool ok, const char* what) {
    if (!ok) {
        printf("  ПРОВАЛ: %s\n", what);
        ++failures;
    }
}

// Праздник, который игра включает в этот день.
static HolidayId liveOn(int month, int day, int year) {
    return holidayForDate(month, day, year);
}

static void checkFocusDays() {
    for (int year = 2024; year <= 2060; ++year) {
        for (int id = 0; id < HOLIDAY_COUNT; ++id) {
            const HolidayDate focus = holidayFocusDay((HolidayId)id, year);
            HolidayId list[HOLIDAY_COUNT];
            const int count = holidayActiveList(focus.month, focus.day, year, list, HOLIDAY_COUNT);
            char what[160];
            snprintf(what, sizeof(what), "%d: в свой день %s (%02d.%02d) выбран %s",
                     year, holidayName((HolidayId)id), focus.day, focus.month,
                     holidayName(liveOn(focus.month, focus.day, year)));
            expect(liveOn(focus.month, focus.day, year) == (HolidayId)id, what);
            snprintf(what, sizeof(what), "%d: %s не первый в списке своего дня",
                     year, holidayName((HolidayId)id));
            expect(count > 0 && list[0] == (HolidayId)id, what);
        }
    }
}

static void checkEveryDay() {
    int maxActive = 0;
    int tripleDays = 0;
    for (int year = 2024; year <= 2060; ++year) {
        for (int month = 1; month <= 12; ++month) {
            for (int day = 1; day <= holidayDaysInMonth(month, year); ++day) {
                HolidayId list[HOLIDAY_COUNT];
                const int count = holidayActiveList(month, day, year, list, HOLIDAY_COUNT);
                if (count > maxActive) maxActive = count;
                if (count >= 3) ++tripleDays;
                char what[160];
                snprintf(what, sizeof(what), "%02d.%02d.%d: в дне %d активных праздников", day, month, year, count);
                expect(count <= 3, what);
                for (int i = 0; i < count; ++i) {
                    snprintf(what, sizeof(what), "%02d.%02d.%d: %s в списке, но не активен",
                             day, month, year, holidayName(list[i]));
                    expect(holidayIsActive(list[i], month, day, year), what);
                }
                for (int i = 1; i < count; ++i) {
                    snprintf(what, sizeof(what), "%02d.%02d.%d: список не отсортирован по расстоянию",
                             day, month, year);
                    expect(holidayFocusDistance(list[i], month, day, year)
                               >= holidayFocusDistance(list[i - 1], month, day, year), what);
                }
                HolidayId again[HOLIDAY_COUNT];
                const int count2 = holidayActiveList(month, day, year, again, HOLIDAY_COUNT);
                snprintf(what, sizeof(what), "%02d.%02d.%d: порядок не детерминирован", day, month, year);
                expect(count2 == count && memcmp(list, again, sizeof(HolidayId) * count) == 0, what);
            }
        }
    }
    printf("  максимум активных в дне: %d, дней с тремя праздниками: %d\n", maxActive, tripleDays);
}

static void checkKnownCollisions() {
    struct Case { int year, month, day; HolidayId expected; const char* note; };
    const Case cases[] = {
        // Лунный новый год накрывает Валентина — до 2.11.1 побеждал Лунный.
        {2024, 2, 14, HOLIDAY_LOVE, "14.02.2024: Лунный новый год перебивал Валентина"},
        {2027, 2, 14, HOLIDAY_LOVE, "14.02.2027: Лунный новый год перебивал Валентина"},
        {2032, 2, 14, HOLIDAY_LOVE, "14.02.2032: Лунный новый год перебивал Валентина"},
        {2035, 2, 14, HOLIDAY_LOVE, "14.02.2035: Лунный новый год перебивал Валентина"},
        // Масленица накрывает Патрика 17 марта.
        {2024, 3, 17, HOLIDAY_PATRICK, "17.03.2024: Масленица перебивала Патрика"},
        {2040, 3, 17, HOLIDAY_PATRICK, "17.03.2040: Масленица перебивала Патрика"},
        // Тройные дни 2026: 17 февраля — свой день Лунного нового года.
        {2026, 2, 16, HOLIDAY_LUNAR, "16.02.2026: канун лунного нового года"},
        {2026, 2, 17, HOLIDAY_LUNAR, "17.02.2026: сам лунный новый год"},
        // Свой день Нового года в окне, переходящем через 31 декабря.
        {2026, 12, 31, HOLIDAY_WINTER, "31.12.2026: окно Нового года через год"},
        {2027, 1, 1, HOLIDAY_WINTER, "01.01.2027: сам Новый год"},
        {2027, 1, 8, HOLIDAY_WINTER, "08.01.2027: ещё в окне Нового года"},
    };
    for (const Case& c : cases) {
        const HolidayId got = liveOn(c.month, c.day, c.year);
        char what[200];
        snprintf(what, sizeof(what), "%s (выбран %s)", c.note, holidayName(got));
        expect(got == c.expected, what);
    }
}

static void checkNewYearDistance() {
    // 31 декабря до 1 января — один день, а не 364: расстояние считается по кругу.
    const int dist = holidayFocusDistance(HOLIDAY_WINTER, 12, 31, 2026);
    char what[120];
    snprintf(what, sizeof(what), "31.12.2026 до своего дня Нового года %d дн., а не 1", dist);
    expect(dist == 1, what);
    const int left = holidayDaysLeft(HOLIDAY_WINTER, 12, 31, 2026);
    snprintf(what, sizeof(what), "31.12.2026: до конца окна Нового года %d дн. (ждали 15)", left);
    expect(left == 15, what);
}

// Сдвиг даты установкой «проверка праздника»: сдвиг обязан переходить через год,
// иначе окно Нового года проверяется по чужому календарю.
static void checkDateShift() {
    struct Case { int y, m, d, shift; int ey, em, ed; const char* note; };
    const Case cases[] = {
        {2026, 12, 31, 30, 2027, 1, 30, "31.12.2026 + 30 дн."},
        {2027, 1, 1, -30, 2026, 12, 2, "01.01.2027 − 30 дн."},
        {2024, 2, 28, 1, 2024, 2, 29, "28.02.2024 + 1 дн. (високосный)"},
        {2023, 3, 1, -1, 2023, 2, 28, "01.03.2023 − 1 дн."},
        {2026, 1, 1, 365, 2027, 1, 1, "01.01.2026 + 365 дн."},
        {2026, 6, 15, 0, 2026, 6, 15, "нулевой сдвиг"},
    };
    for (const Case& c : cases) {
        const HolidayToday got = holidayShiftDate(c.y, c.m, c.d, c.shift);
        char what[160];
        snprintf(what, sizeof(what), "%s → %04d-%02d-%02d, а вышло %04d-%02d-%02d",
                 c.note, c.ey, c.em, c.ed, got.year, got.month, got.day);
        expect(got.year == c.ey && got.month == c.em && got.day == c.ed, what);
    }
}

int main() {
    printf("Праздники: пересечения окон, 2024–2060\n");
    checkFocusDays();
    checkEveryDay();
    checkKnownCollisions();
    checkNewYearDistance();
    checkDateShift();
    if (failures == 0) {
        printf("  ОК: ошибок нет\n");
        return 0;
    }
    printf("  ошибок: %d\n", failures);
    return 1;
}
