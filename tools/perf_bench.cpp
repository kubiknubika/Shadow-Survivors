// ── Стенд кадра: сколько миллисекунд стоит один кадр игры ─────────────────────
//
// Меряет ровно то, что делает главный цикл: update() + render(). Окно, звук и
// вывод на экран в замере не участвуют — поэтому число здесь это цена игры, а
// не цена Windows. Экран берётся логический: game рисует в 1280×720 (в портрете
// 720×1280) и уже потом растягивает кадр на окно, так что разрешение монитора к
// этому числу отношения не имеет.
//
// Сцена одна и та же на каждом запуске: эндлесс, 8-я минута, god mode (герой не
// умирает и забег не кончается), семь оружий 6-го ранга, клинки эволюционированы,
// 160 мобов на старте. Так кадр нагружен толпой, снарядами и частицами, а не
// пустой ареной.
//
// Важно: simulationMode выключает запись на диск (сейв, логи). Без него хеш
// кадра «плавает» от прогона к прогону и сравнивать версии бессмысленно.
//
// Сборка и запуск:
//     g++ -O2 -std=c++17 -Isrc tools/perf_bench.cpp -o build/perf_bench
//     build/perf_bench                       # 600 кадров, 1280×720
//     build/perf_bench 1200                  # длиннее (стабильнее среднее)
//     build/perf_bench 600 720x1280          # портрет
//     build/perf_bench 600 1280x720 2        # кадр в две полосы
//     build/perf_bench 600 1280x720 1 loot   # сцена с тысячами самоцветов
//     build/perf_bench 600 1280x720 2 trail  # с тлеющей тропой (предмет 40)
//     build/perf_bench 600 1280x720 2 g3     # ступень графики: g1 высокое … g4 эконом
//     build/perf_bench 600 1280x720 1 g1 layout1   # рисунок пола: 0 плиты … 4 осыпь
//
// Пятый аргумент (g0…g4) задаёт уровень графики из настроек: g0 — АВТО,
// g1 — высокое (так рисует 2.10.47 и выше), g2 — среднее, g3 — быстрое,
// g4 — эконом. С ним один и тот же стенд отвечает на вопрос «а что будет на
// слабом железе», не меняя сцену.
//
// Хеш последних 20 кадров печатается специально: если после оптимизации хеш
// совпал, картинка не изменилась ни на пиксель. Если разошёлся — визуальная
// правка была, и её надо смотреть глазами, а не объяснять «стало быстрее».
//
// Код возврата 1, если средний кадр не влезает в 16.7 мс (60 fps) — чтобы
// регрессию производительности было видно в общем прогоне проверок.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <string>
#include "renderer.h"
#include "game.h"

static bool parseSize(const char* s, int& w, int& h) {
    const char* x = strchr(s, 'x');
    if (!x) return false;
    w = atoi(s);
    h = atoi(x + 1);
    return w >= 240 && h >= 240;
}

int main(int argc, char** argv) {
    int frames = 600;
    int w = 1280, h = 720;
    int argi = 1;
    if (argc > 1 && argv[1][0] != '-') {
        if (strchr(argv[1], 'x')) { parseSize(argv[1], w, h); }
        else frames = atoi(argv[1]);
        if (frames < 30) frames = 30;
        ++argi;
    }
    if (argc > argi && strchr(argv[argi], 'x')) parseSize(argv[argi], w, h);
    int threads = 1;
    if (argc > argi + 1) threads = atoi(argv[argi + 1]);
    if (threads < 1) threads = 1;
    // Четвёртый аргумент «лут» добавляет самую тяжёлую сцену: герой стоит на
    // месте 5 минут, и на полу накапливаются тысячи самоцветов. Именно на ней
    // видно, что дорого стоит не толпа, а добыча.
    const bool lootScene = argc > argi + 2 && strcmp(argv[argi + 2], "loot") == 0;
    // Сцена «тропа»: предмет 40 третьего ранга, а герой бежит по кругу — иначе
    // тропе нечего оставлять на полу. Стоящий герой и цена тропы — это ноль
    // проверок, а не ноль цены.
    bool trailScene = false;
    for (int i = argi + 2; i < argc; ++i)
        if (strcmp(argv[i], "trail") == 0) trailScene = true;
    // Пятый аргумент — уровень графики из настроек: g0 авто, g1 высокое,
    // g2 среднее, g3 быстрое, g4 эконом. С ним один и тот же стенд меряет,
    // что игрок реально получит на слабом железе.
    int graphicsLevel = 1;
    for (int i = argi + 2; i < argc; ++i)
        if (argv[i][0] == 'g' && argv[i][1] >= '0' && argv[i][1] <= '4')
            graphicsLevel = argv[i][1] - '0';
    // Вариант карты задаётся явно: у мозаики на кадр приходится больше всего
    // линий (сетка ромбов через весь экран), и её цена — верхняя граница.
    int arenaLayout = -1;
    for (int i = argi + 2; i < argc; ++i)
        if (strncmp(argv[i], "layout", 6) == 0 && argv[i][6] >= '0' && argv[i][6] <= '4')
            arenaLayout = argv[i][6] - '0';

    SCREEN_W = w; SCREEN_H = h;
    Renderer r;
    r.resize(w, h);
    Game g;
    g.simulationMode = true;
    g.ignoreSavedState = true;   // сцена и хеш не зависят от сейва с диска
    g.init();
    g.state = STATE_PLAYING;
    g.gameMode = MODE_ENDLESS;
    g.cheatGodMode = true;
    g.arenaLayoutOverride = arenaLayout;
    g.renderThreads = threads;
    g.options.graphicsLevel = graphicsLevel;
    g.runTime = 480.0f;
    for (int i = 0; i < 7; ++i) g.itemLevels[i] = 6;
    g.itemLevels[0] = 9;          // клинки на максимуме
    g.itemLevels[7] = 4;          // книга IV — условие эволюции
    if (trailScene) g.itemLevels[40] = 3;
    g.checkSymbiosis();
    srand(4242);
    for (int i = 0; i < 160; ++i) g.spawnMonster(i % 10, i % 9 == 0, false);

    const float dt = 1.0f / 60.0f;
    if (lootScene) {
        for (int i = 0; i < 60 * 300; ++i) {
            if (g.state == STATE_LEVELUP) g.state = STATE_PLAYING;
            g.update(dt);
        }
        printf("сцена с лутом: самоцветов на полу %d\n", (int)g.pickups.size());
    }
    double upd = 0.0, ren = 0.0;
    uint32_t hash = 2166136261u;
    for (int f = 0; f < frames; ++f) {
        // Модалка левелапа останавливает бой, а стенду нужен кадр боя.
        if (g.state == STATE_LEVELUP) g.state = STATE_PLAYING;
        if (trailScene) {
            // Круг радиусом ~520 px: скорость выше порога метки, след ложится
            // непрерывно, и в кадре постоянно живёт полный хвост.
            const float t = (float)f / 60.0f * 1.35f;
            g.keyD = cosf(t) > 0.0f;
            g.keyA = cosf(t) < 0.0f;
            g.keyS = sinf(t) > 0.0f;
            g.keyW = sinf(t) < 0.0f;
        }
        const auto t0 = std::chrono::high_resolution_clock::now();
        g.update(dt);
        const auto t1 = std::chrono::high_resolution_clock::now();
        g.render(r);
        const auto t2 = std::chrono::high_resolution_clock::now();
        upd += std::chrono::duration<double, std::milli>(t1 - t0).count();
        ren += std::chrono::duration<double, std::milli>(t2 - t1).count();
        if (f >= frames - 20) {
            for (int i = 0; i < w * h; i += 97) {
                hash ^= r.pixels[i];
                hash *= 16777619u;
            }
        }
    }

    const double updateMs = upd / frames, renderMs = ren / frames, frameMs = updateMs + renderMs;
    printf("%dx%d, %d кадров, потоков %d: update %.2f мс, render %.2f мс, кадр %.2f мс — %.0f fps\n",
           w, h, frames, threads, updateMs, renderMs, frameMs, frameMs > 0.0 ? 1000.0 / frameMs : 0.0);
    printf("сущностей в конце: мобов %d, снарядов %d, частиц %d; хеш кадра %08x\n",
           (int)g.monsters.size(), (int)g.projectiles.size(), (int)g.particles.size(), hash);
    if (trailScene) printf("тропа: меток на полу %d, положено за прогон %d\n",
                           (int)g.ashTrailMarks.size(), g.ashTrailFires);
    if (frameMs > 16.7) {
        printf("НЕ ВЛЕЗАЕТ в 60 fps: %.2f мс при бюджете 16.7 мс\n", frameMs);
        return 1;
    }
    return 0;
}
