// ── Сверка экранов: контрольная сумма кадра до и после правок рендера ───────
//
// Растеризатор трогать страшно: он рисует всё, а глазами проверяют только то,
// куда посмотрели. Этот стенд печатает хеш десяти экранов (меню, настройки,
// справка, патчноуты, зал славы, выбор героя, бой, пауза, сундук, прокачка).
// Соберите его до правки и после — совпали хеши, значит картинка не изменилась
// ни на пиксель; разошлись — смотрите, где именно.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/screen_check.cpp -o build/screens_new
//     build/screens_new 2          # тот же набор, но кадр в две полосы
//     build/screens_new 2 3        # и на ступени графики 3 («быстрое»)
//
// Второй аргумент — ступень графики 1..4. На ступени 1 картинка обязана
// совпадать со сборкой до правок; 2–4 меняют её осознанно, и там проверяется
// другое: что один, два и четыре потока дают один и тот же кадр.
//     git stash && g++ -O2 -std=c++17 -Isrc tools/screen_check.cpp -o build/screens_old && git stash pop
//     build/screens_old > old.txt && build/screens_new > new.txt && diff old.txt new.txt
//
// Важно: оба бинарника должны собираться с одним и тем же game.h, иначе
// разойдётся текст (версия в заголовке), а не отрисовка.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "renderer.h"
#include "game.h"

static uint32_t checksum(const Renderer& r) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < r.width * r.height; ++i) { h ^= r.pixels[i]; h *= 16777619u; }
    return h;
}

int main(int argc, char** argv) {
    // Второй аргумент — число потоков: с ним тот же стенд проверяет, что
    // полосный кадр совпадает с однопоточным пиксель в пиксель.
    const int threads = argc > 1 ? std::max(1, atoi(argv[1])) : 1;
    // Второй аргумент — уровень графики (1 высокое … 4 эконом): три нижних
    // уровня меняют картинку осознанно, и их надо смотреть глазами.
    const int graphicsLevel = argc > 2 ? std::max(1, std::min(4, atoi(argv[2]))) : 1;
    // Третий аргумент «touch» включает телефонный интерфейс: подсказки без
    // названий клавиш и кнопка «УПРАВЛЕНИЕ» в настройках. Тот же стенд с теми же
    // экранами, только глазами телефона.
    const bool touchUi = argc > 3 && strcmp(argv[3], "touch") == 0;
    // Праздники задаются явно, а не берутся из системной даты: иначе хеши боя
    // и меню менялись бы 20 октября, 31 декабря и в любой другой день, когда
    // календарь включает окно. -1 — праздников нет, 0..7 — принудительный
    // праздник, -2 — календарь на закреплённой дате 31 октября 2026.
    struct Case { const char* name; int state; int holiday; };
    const Case cases[] = {
        {"меню", STATE_MENU, -1},
        {"настройки", STATE_SETTINGS, -1},
        {"справка", STATE_HELP, -1},
        {"патчноуты", STATE_PATCH_NOTES, -1},
        {"зал славы", STATE_HALL, -1},
        {"выбор героя", STATE_HERO_SELECT, -1},
        {"бой", STATE_PLAYING, -1},
        {"пауза", STATE_PAUSED, -1},
        {"сундук", STATE_CHEST, -1},
        {"прокачка", STATE_LEVELUP, -1},
        {"управление", STATE_CONTROLS_SETUP, -1},
        {"вид кнопок", STATE_BUTTON_LOOK, -1},
        {"праздники", STATE_HOLIDAY, -2},
        {"бой праздник", STATE_PLAYING, 0},
    };
    SCREEN_W = 1280; SCREEN_H = 720;
    for (const Case& c : cases) {
        Renderer r; r.resize(SCREEN_W, SCREEN_H);
        Game g; g.simulationMode = true;
        g.touchControls = touchUi;      // до init(): от него зависит первый экран
        srand(4242); g.init();
        // Праздничная часть прибита гвоздями: выключена или выбрана вручную.
        g.options.holidayDateShift = 0;
        g.options.holidaysEnabled = c.holiday >= 0 ? 1 : 0;
        g.options.holidayOverride = c.holiday >= 0 ? c.holiday : -1;
        if (c.holiday == -2) {
            g.options.holidaysEnabled = 1;
            g.holidayPinYear = 2026; g.holidayPinMonth = 10; g.holidayPinDay = 31;
        }
        g.activeHoliday = g.resolveActiveHoliday();
        g.holidayBannerShown = false;
        g.holidayWaveTimer = 40.0f;
        g.gameMode = MODE_ENDLESS;
        g.runTime = 240.0f;
        for (int i = 0; i < 5; ++i) g.itemLevels[i] = 3;
        g.renderThreads = threads;
        g.options.graphicsLevel = graphicsLevel;
        g.state = (GameState)c.state;
        if (c.state == STATE_PLAYING || c.state == STATE_LEVELUP || c.state == STATE_CHEST || c.state == STATE_PAUSED) {
            for (int i = 0; i < 60; ++i) g.spawnMonster(i % 8, i % 9 == 0, false);
        }
        for (int f = 0; f < 3; ++f) {
            if (g.state == STATE_LEVELUP) break;
            g.update(1.0f / 60.0f);
        }
        g.state = (GameState)c.state;
        r.clear(0xFF000000);
        g.render(r);
        printf("%-14s %08x\n", c.name, checksum(r));
    }
    return 0;
}
