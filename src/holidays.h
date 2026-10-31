#pragma once
// ── Праздничные события: календарь, окна, данные ──────────────────────────────
//
// Игра определяет, какой праздник идёт СЕЙЧАС, по дате в календаре, и на это
// время включает праздничное оформление, персонажей и подарки. Ничего не
// скачивается: «DLC» — это окно дат, внутри которого игра ведёт себя иначе.
//
// Почему отдельным файлом: календарная математика (плавающие даты, окна через
// новый год, високосный февраль) — самая ошибкоопасная часть, и её надо
// проверять стендом без окна, рендера и звука. Здесь нет ни Renderer, ни Game.
//
// Даты проверены:
//   • православная Пасха считается по александрийской пасхалии (юлианский счёт
//     + 13 дней для 1900–2099); 2024 — 5 мая, 2025 — 20 апреля, 2026 — 12
//     апреля, 2027 — 2 мая;
//   • Масленица — за 55 дней до Пасхи (начало сырной недели);
//   • Лунный Новый год — по таблице известных дат; вне таблицы честный фолбэк
//     на 1 февраля, а не «примерно угадали».
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <algorithm>

enum HolidayId {
    HOLIDAY_NONE = -1,
    HOLIDAY_HALLOWEEN = 0,
    HOLIDAY_WINTER,
    HOLIDAY_LOVE,
    HOLIDAY_PATRICK,
    HOLIDAY_EASTER,
    HOLIDAY_MASLENITSA,
    HOLIDAY_LUNAR,
    HOLIDAY_KUPALA,
    HOLIDAY_COUNT
};

// Откуда берётся дата праздника.
enum HolidayKind {
    HOLIDAY_KIND_FIXED = 0,   // окно записано прямо: «20 октября — 5 ноября»
    HOLIDAY_KIND_EASTER = 1,  // окно считается от православной Пасхи
    HOLIDAY_KIND_MASLEN = 2,  // ...и от неё же за 55 дней
    HOLIDAY_KIND_LUNAR = 3    // лунный новый год: таблица лет
};

struct HolidayDef {
    const char* key;          // имя файла/отладки: "halloween"
    const char* name;         // «ХЭЛЛОУИН»
    const char* tagline;      // одна строка, зачем это игроку
    HolidayKind kind;
    int startMonth, startDay; // для HOLIDAY_FIXED
    int endMonth, endDay;
    // Смысловые цвета: символы, персонажи, рамка календаря. Один праздник —
    // одна палитра, чтобы праздник читался цветом раньше надписи.
    uint32_t c1, c2, c3, glow;
    const char* symbols[4];       // 4 символа
    const char* characters[2];    // 2 персонажа
};

// ВАЖНО: порядок окон выбран так, чтобы в году не было «мёртвых» месяцев
// дольше двух недель, а пересечения решались приоритетом (см. holidayForDate).
static const HolidayDef HOLIDAY_DEFS[HOLIDAY_COUNT] = {
    // 31 октября. Окно шире самого дня: в играх Хэллоуин идёт 2–3 недели.
    {"halloween", "ХЭЛЛОУИН", "Тыквы, призраки и ведьмы выходят на арену.",
     HOLIDAY_KIND_FIXED, 10, 20, 11, 5,
     0xFFFF7A1A, 0xFF7A3AB8, 0xFF1A0F22, 0xFFFFA23A,
     {"Фонарь Джека", "Призрак", "Летучая мышь", "Конфета"},
     {"Ведьма на метле", "Тыквоголовый"}},

    // Новый год + Рождество: окно через границу года, самое длинное в календаре.
    {"winter", "НОВЫЙ ГОД", "Ёлка, снег, подарки и Дед Мороз.",
     HOLIDAY_KIND_FIXED, 12, 20, 1, 14,
     0xFF63D6FF, 0xFFE8F4FF, 0xFF0B1826, 0xFF9BE8FF,
     {"Ёлка", "Снежинка", "Звезда", "Подарок"},
     {"Снеговик", "Олень"}},

    // 14 февраля. Окно 11 дней — как в мобильных играх.
    {"love", "ДЕНЬ ВЛЮБЛЁННЫХ", "Сердца, стрелы и слишком меткий Купидон.",
     HOLIDAY_KIND_FIXED, 2, 7, 2, 17,
     0xFFFF4D7A, 0xFFFFC2D6, 0xFF2A0A18, 0xFFFF7AA0,
     {"Сердце", "Стрела Купидона", "Роза", "Конфета-сердце"},
     {"Купидон", "Сердцеед"}},

    // 17 марта. День святого Патрика: зелёный, клевер, лепреконы.
    {"patrick", "ДЕНЬ СВЯТОГО ПАТРИКА", "Клевер, золото и зелёные человечки.",
     HOLIDAY_KIND_FIXED, 3, 12, 3, 20,
     0xFF3FD46A, 0xFFFFD84A, 0xFF0C1E12, 0xFF7CFF9E,
     {"Клевер", "Горшок золота", "Радуга", "Кружка"},
     {"Лепрекон", "Зелёный гоблин"}},

    // Православная Пасха: −3 дня (Страстная) … +7 (Светлая неделя).
    {"easter", "ПАСХА", "Крашенки, кролик и весенний свет.",
     HOLIDAY_KIND_EASTER, 0, 0, 0, 0,
     0xFFFFD36A, 0xFF8AE85C, 0xFF26150C, 0xFFFFE9A0,
     {"Крашенка", "Кролик", "Корзина", "Цыплёнок"},
     {"Пасхальный кролик", "Крашенка-каталка"}},

    // Масленица: сырная неделя, за 56 дней до Пасхи.
    {"maslenitsa", "МАСЛЕНИЦА", "Блины, солнце и проводы зимы.",
     HOLIDAY_KIND_MASLEN, 0, 0, 0, 0,
     0xFFFFB13A, 0xFFFFF0C0, 0xFF2A1608, 0xFFFFD070,
     {"Блин", "Солнце", "Чучело", "Самовар"},
     {"Чучело Зимы", "Блин-катун"}},

    // Лунный Новый год: −5 … +8 дней от самого дня.
    {"lunar", "ЛУННЫЙ НОВЫЙ ГОД", "Дракон, фонари и красные конверты.",
     HOLIDAY_KIND_LUNAR, 0, 0, 0, 0,
     0xFFFF3B30, 0xFFFFC53A, 0xFF2A0A0A, 0xFFFF7A5A,
     {"Хунбао", "Фонарь", "Мандарин", "Хлопушка"},
     {"Лунный дракон", "Лев-страж"}},

    // Иван Купала: ночь на 7 июля, огонь и вода.
    {"kupala", "ИВАН КУПАЛА", "Кострище, венки и цветок папоротника.",
     HOLIDAY_KIND_FIXED, 7, 1, 7, 8,
     0xFF59C8FF, 0xFFFF8A3A, 0xFF0A1420, 0xFF9BE0FF,
     {"Кострище", "Папоротник", "Венок", "Вода"},
     {"Русалка", "Огнецвет"}},
};

inline bool holidayValid(HolidayId id) { return id >= 0 && id < HOLIDAY_COUNT; }
inline const HolidayDef& holidayDef(HolidayId id) {
    static const HolidayDef none{"none", "БУДНИ", "Обычные дни: ничего лишнего.", HOLIDAY_KIND_FIXED, 1, 1, 1, 1,
                                 0xFF808080, 0xFF808080, 0xFF101010, 0xFF808080,
                                 {"", "", "", ""}, {"", ""}};
    return holidayValid(id) ? HOLIDAY_DEFS[id] : none;
}
inline const char* holidayName(HolidayId id) { return holidayDef(id).name; }
inline const char* holidayKey(HolidayId id) { return holidayDef(id).key; }

// Ключ «месяц*100 + день» монотонно растёт внутри года. Сравнение таких ключей
// само разбирается с окном через новый год (20.12 = 1220 > 14.01 = 114), и ему
// не мешает 29 февраля: високосность тут не участвует вовсе.
inline int holidayMonthDayKey(int month, int day) { return month * 100 + day; }

inline bool holidayInWindow(int month, int day, int sm, int sd, int em, int ed) {
    const int k = holidayMonthDayKey(month, day);
    const int ks = holidayMonthDayKey(sm, sd);
    const int ke = holidayMonthDayKey(em, ed);
    if (ks <= ke) return k >= ks && k <= ke;
    return k >= ks || k <= ke; // окно через новый год
}

inline int holidayDaysInMonth(int month, int year) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return (month >= 1 && month <= 12) ? days[month - 1] : 30;
}

// Сдвиг даты на delta дней (может быть отрицательным). Нужен для плавающих
// праздников: «Пасха − 3 дня» и «Пасха + 7 дней».
struct HolidayDate { int month, day; };
// Год важен: 29 февраля бывает не всегда, и «Пасха − 55 дней» в високосном году
// попадает на другой день, если считать по чужому календарю. Ошибка на день
// здесь — это не косметика, а неверное окно события.
inline HolidayDate holidayAddDays(int month, int day, int delta, int year) {
    int m = month, d = day + delta;
    while (d < 1) { --m; if (m < 1) m = 12; d += holidayDaysInMonth(m, year); }
    while (d > holidayDaysInMonth(m, year)) { d -= holidayDaysInMonth(m, year); ++m; if (m > 12) m = 1; }
    return {m, d};
}

// Православная Пасха по александрийской пасхалии. Возвращает дату по
// григорианскому календарю (для 1900–2099 разница 13 дней).
inline HolidayDate holidayOrthodoxEaster(int year) {
    const int a = year % 4;
    const int b = year % 7;
    const int c = year % 19;
    const int d = (19 * c + 15) % 30;
    const int e = (2 * a + 4 * b - d + 34) % 7;
    const int base = d + e + 114;
    int month = base / 31;          // 3 или 4 по юлианскому счёту
    int day = base % 31 + 1;
    // Перевод юлианской даты в григорианскую: +13 дней для XX–XXI века.
    day += 13;
    const int dim = (month == 3) ? 31 : 30;
    if (day > dim) { day -= dim; ++month; }
    return {month, day};
}

// Лунный Новый год. Таблица реальных дат; вне её — 1 февраля (и это прямо
// написано в календаре, а не замаскировано «примерно»).
struct LunarNewYearRow { int year, month, day; };
static const LunarNewYearRow LUNAR_NEW_YEAR[] = {
    {2024, 2, 10}, {2025, 1, 29}, {2026, 2, 17}, {2027, 2, 6},
    {2028, 1, 26}, {2029, 2, 13}, {2030, 2, 3},  {2031, 1, 23},
    {2032, 2, 11}, {2033, 1, 31}, {2034, 2, 19}, {2035, 2, 8},
    {2036, 1, 28}, {2037, 2, 15}, {2038, 2, 4},  {2039, 1, 24},
    {2040, 2, 12},
};
inline HolidayDate holidayLunarNewYear(int year) {
    for (const LunarNewYearRow& row : LUNAR_NEW_YEAR)
        if (row.year == year) return {row.month, row.day};
    return {2, 1};
}
inline bool holidayLunarYearKnown(int year) {
    for (const LunarNewYearRow& row : LUNAR_NEW_YEAR)
        if (row.year == year) return true;
    return false;
}

// Окно праздника в конкретном году. Для плавающих праздников считается здесь,
// чтобы календарь, спавн и стенд пользовались одной функцией.
struct HolidayWindow { int startMonth, startDay, endMonth, endDay; };
inline HolidayWindow holidayWindow(HolidayId id, int year) {
    const HolidayDef& def = holidayDef(id);
    switch (def.kind) {
        case HOLIDAY_KIND_EASTER: {
            const HolidayDate e = holidayOrthodoxEaster(year);
            const HolidayDate s = holidayAddDays(e.month, e.day, -3, year);
            const HolidayDate f = holidayAddDays(e.month, e.day, 7, year);
            return {s.month, s.day, f.month, f.day};
        }
        case HOLIDAY_KIND_MASLEN: {
            const HolidayDate e = holidayOrthodoxEaster(year);
            // Пасха − 55 дней: 2024 — 11 марта, 2025 — 24 февраля, как в
            // настоящем календаре. Ошибка на день тут заметна сразу.
            const HolidayDate s = holidayAddDays(e.month, e.day, -55, year);
            const HolidayDate f = holidayAddDays(e.month, e.day, -49, year);
            return {s.month, s.day, f.month, f.day};
        }
        case HOLIDAY_KIND_LUNAR: {
            const HolidayDate n = holidayLunarNewYear(year);
            const HolidayDate s = holidayAddDays(n.month, n.day, -5, year);
            const HolidayDate f = holidayAddDays(n.month, n.day, 8, year);
            return {s.month, s.day, f.month, f.day};
        }
        default:
            return {def.startMonth, def.startDay, def.endMonth, def.endDay};
    }
}

inline bool holidayIsActive(HolidayId id, int month, int day, int year) {
    const HolidayWindow w = holidayWindow(id, year);
    return holidayInWindow(month, day, w.startMonth, w.startDay, w.endMonth, w.endDay);
}

// Номер дня в году (0-based). Нужен календарю для маркера «сегодня».
inline int holidayDayOfYear(int month, int day, int year) {
    int total = 0;
    for (int m = 1; m < month; ++m) total += holidayDaysInMonth(m, year);
    return total + day - 1;
}

// «Свой день» праздника — тот, ради которого он существует. Окна нужны, чтобы
// праздник длился две недели, но разрешать пересечения по середине окна нельзя:
// у Валентина окно 7–17 февраля, середина — 12-е, и 14 февраля Лунный новый год
// (окно которого шире) выигрывал бы у самого Валентина. Поэтому у каждого
// праздника есть фокус: у плавающих это сам день (Пасха, Лунный новый год),
// у Масленицы — четверг сырной недели, у остальных он записан прямо.
inline HolidayDate holidayFocusDay(HolidayId id, int year) {
    const HolidayDef& def = holidayDef(id);
    switch (def.kind) {
        case HOLIDAY_KIND_EASTER:
            return holidayOrthodoxEaster(year);
        case HOLIDAY_KIND_LUNAR:
            return holidayLunarNewYear(year);
        case HOLIDAY_KIND_MASLEN: {
            // Старт + 3 дня: «разгул» сырной недели, а не её понедельник и не
            // воскресенье — иначе фокус совпадал бы с соседним праздником.
            const HolidayWindow w = holidayWindow(id, year);
            return holidayAddDays(w.startMonth, w.startDay, 3, year);
        }
        default:
            break;
    }
    switch (id) {
        case HOLIDAY_HALLOWEEN: return {10, 31};
        case HOLIDAY_WINTER:    return {1, 1};
        case HOLIDAY_LOVE:      return {2, 14};
        case HOLIDAY_PATRICK:   return {3, 17};
        case HOLIDAY_KUPALA:    return {7, 7};
        default: {
            const HolidayWindow w = holidayWindow(id, year);
            return {w.startMonth, w.startDay};
        }
    }
}

inline int holidayDaysInYear(int year) { return holidayDaysInMonth(2, year) == 29 ? 366 : 365; }

// Дата, с которой работает игра. Объявлена здесь: её возвращает и сдвиг даты,
// который считается раньше остальных календарных помощников.
struct HolidayToday { int year, month, day; };

// Сдвиг даты на N дней (может быть отрицательным), в том числе через Новый год.
// Год меняется на границе: раньше он оставался прежним, и сдвиг «на 30 дней» от
// 20 декабря показывал 19 января того же года — окно Нового года, переходящее
// через 31 декабря, после сдвига считалось по чужому году.
inline HolidayToday holidayShiftDate(int year, int month, int day, int shiftDays) {
    int doy = holidayDayOfYear(month, day, year) + shiftDays;
    while (doy < 0) { --year; doy += holidayDaysInYear(year); }
    while (doy >= holidayDaysInYear(year)) { doy -= holidayDaysInYear(year); ++year; }
    int m = 1, d = doy;
    while (d >= holidayDaysInMonth(m, year)) { d -= holidayDaysInMonth(m, year); ++m; }
    return {year, m, d + 1};
}

// Расстояние по кругу года: окно через 31 декабря не должно «уезжать» на 364.
inline int holidayDayDistance(int doyA, int doyB, int daysInYear) {
    const int d = doyA > doyB ? doyA - doyB : doyB - doyA;
    return std::min(d, daysInYear - d);
}

// Насколько сегодня далеко от «своего дня» праздника.
inline int holidayFocusDistance(HolidayId id, int month, int day, int year) {
    const HolidayDate f = holidayFocusDay(id, year);
    const int daysInYear = holidayDaysInYear(year);
    return holidayDayDistance(holidayDayOfYear(month, day, year),
                              holidayDayOfYear(f.month, f.day, year), daysInYear);
}

// Все активные праздники, ближайший к своему дню — первым. Раньше порядок был
// жёстко задан таблицей, и 14 февраля 2024 года игра показывала Лунный новый
// год: его окно шире и стояло выше. Теперь решает расстояние до фокуса, а
// таблица остаётся только разрешением ничьей (два праздника в один день).
inline int holidayActiveList(int month, int day, int year, HolidayId* out, int maxOut) {
    static const HolidayId order[HOLIDAY_COUNT] = {
        HOLIDAY_EASTER, HOLIDAY_MASLENITSA, HOLIDAY_LUNAR, HOLIDAY_WINTER,
        HOLIDAY_LOVE, HOLIDAY_PATRICK, HOLIDAY_KUPALA, HOLIDAY_HALLOWEEN};
    struct Entry { HolidayId id; int dist; int rank; };
    Entry entries[HOLIDAY_COUNT];
    int n = 0;
    for (int i = 0; i < HOLIDAY_COUNT; ++i) {
        const HolidayId id = order[i];
        if (!holidayIsActive(id, month, day, year)) continue;
        entries[n++] = {id, holidayFocusDistance(id, month, day, year), i};
    }
    // Вставками: элементов не больше восьми, зато сортировка устойчивая и
    // ничья разрешается порядком таблицы, а не случайностью.
    for (int i = 1; i < n; ++i) {
        const Entry e = entries[i];
        int j = i - 1;
        while (j >= 0 && (entries[j].dist > e.dist ||
                          (entries[j].dist == e.dist && entries[j].rank > e.rank))) {
            entries[j + 1] = entries[j];
            --j;
        }
        entries[j + 1] = e;
    }
    const int count = std::min(n, maxOut);
    for (int i = 0; i < count; ++i) out[i] = entries[i].id;
    return count;
}

inline HolidayId holidayForDate(int month, int day, int year) {
    HolidayId list[4];
    const int n = holidayActiveList(month, day, year, list, 4);
    return n > 0 ? list[0] : HOLIDAY_NONE;
}

// Сегодняшняя дата по локальным часам. Отдельная функция, чтобы стенд мог
// подменить дату, не трогая системные часы.
inline HolidayToday holidayRealToday() {
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    return {t.tm_year + 1900, t.tm_mon + 1, t.tm_mday};
}

// Дата, с которой работает игра: либо настоящая, либо сдвинутая настройкой
// «проверка праздника» (сдвиг в днях). Сдвиг нужен, чтобы посмотреть любой
// праздник в любой день, не переводя часы системы.
inline HolidayToday holidayShiftedToday(int shiftDays) {
    const HolidayToday today = holidayRealToday();
    if (shiftDays == 0) return today;
    return holidayShiftDate(today.year, today.month, today.day, shiftDays);
}

// Сколько дней осталось до конца окна включительно (сегодня = 1).
inline int holidayDaysLeft(HolidayId id, int month, int day, int year) {
    const HolidayWindow w = holidayWindow(id, year);
    if (!holidayInWindow(month, day, w.startMonth, w.startDay, w.endMonth, w.endDay)) return 0;
    int left = 0;
    int m = month, d = day;
    for (int guard = 0; guard < 60; ++guard) {
        ++left;
        d += 1;
        if (d > holidayDaysInMonth(m, year)) { d = 1; ++m; if (m > 12) m = 1; }
        if (!holidayInWindow(m, d, w.startMonth, w.startDay, w.endMonth, w.endDay)) break;
    }
    return left;
}

inline const char* holidayMonthName(int month) {
    static const char* names[12] = {"ЯНВАРЯ", "ФЕВРАЛЯ", "МАРТА", "АПРЕЛЯ", "МАЯ", "ИЮНЯ",
                                    "ИЮЛЯ", "АВГУСТА", "СЕНТЯБРЯ", "ОКТЯБРЯ", "НОЯБРЯ", "ДЕКАБРЯ"};
    return (month >= 1 && month <= 12) ? names[month - 1] : "?";
}

// «20 ОКТЯБРЯ — 5 НОЯБРЯ» одной строкой: так окно читается в календаре.
inline void holidayWindowText(HolidayId id, int year, char* out, size_t size) {
    const HolidayWindow w = holidayWindow(id, year);
    snprintf(out, size, "%d %s — %d %s", w.startDay, holidayMonthName(w.startMonth),
             w.endDay, holidayMonthName(w.endMonth));
}
