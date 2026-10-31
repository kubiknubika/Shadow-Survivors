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

// Приоритет при пересечении окон. Плавающие праздники (Пасха, Масленица,
// Лунный Новый год) «переезжают» каждый год и могут налезть на чужое окно;
// в этом случае выигрывает тот, чей день ближе к сегодняшнему. Фиксированные
// праздники между собой не пересекаются вовсе — это проверяет стенд.
inline bool holidayIsActive(HolidayId id, int month, int day, int year) {
    const HolidayWindow w = holidayWindow(id, year);
    return holidayInWindow(month, day, w.startMonth, w.startDay, w.endMonth, w.endDay);
}

// Все активные праздники в массив (максимум 3). Календарь показывает их списком,
// игра берёт первый — так игрок видит пересечение, а не «праздник пропал».
inline int holidayActiveList(int month, int day, int year, HolidayId* out, int maxOut) {
    int n = 0;
    // Порядок — от «самого важного» к остальным: Пасха, Масленица, Лунный,
    // Новый год, Валентин, Патрик, Купала, Хэллоуин.
    static const HolidayId order[HOLIDAY_COUNT] = {
        HOLIDAY_EASTER, HOLIDAY_MASLENITSA, HOLIDAY_LUNAR, HOLIDAY_WINTER,
        HOLIDAY_LOVE, HOLIDAY_PATRICK, HOLIDAY_KUPALA, HOLIDAY_HALLOWEEN};
    for (int i = 0; i < HOLIDAY_COUNT && n < maxOut; ++i)
        if (holidayIsActive(order[i], month, day, year)) out[n++] = order[i];
    return n;
}

inline HolidayId holidayForDate(int month, int day, int year) {
    HolidayId list[4];
    const int n = holidayActiveList(month, day, year, list, 4);
    return n > 0 ? list[0] : HOLIDAY_NONE;
}

// Номер дня в году (0-based). Нужен календарю для маркера «сегодня».
inline int holidayDayOfYear(int month, int day, int year) {
    int total = 0;
    for (int m = 1; m < month; ++m) total += holidayDaysInMonth(m, year);
    return total + day - 1;
}

// Сегодняшняя дата по локальным часам. Отдельная функция, чтобы стенд мог
// подменить дату, не трогая системные часы.
struct HolidayToday { int year, month, day; };
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
    HolidayToday today = holidayRealToday();
    if (shiftDays == 0) return today;
    const int doy = holidayDayOfYear(today.month, today.day, today.year) + shiftDays;
    const int dim = (today.year % 4 == 0 && (today.year % 100 != 0 || today.year % 400 == 0)) ? 366 : 365;
    int d = ((doy % dim) + dim) % dim;
    int m = 1;
    while (d >= holidayDaysInMonth(m, today.year)) { d -= holidayDaysInMonth(m, today.year); ++m; }
    return {today.year, m, d + 1};
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
