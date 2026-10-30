// ── Сверка экранов: контрольная сумма кадра до и после правок рендера ───────
//
// Растеризатор трогать страшно: он рисует всё, а глазами проверяют только то,
// куда посмотрели. Этот стенд печатает хеш десяти экранов (меню, настройки,
// справка, патчноуты, зал славы, выбор героя, бой, пауза, сундук, прокачка).
// Соберите его до правки и после — совпали хеши, значит картинка не изменилась
// ни на пиксель; разошлись — смотрите, где именно.
//
//     g++ -O2 -std=c++17 -Isrc tools/screen_check.cpp -o build/screens_new
//     git stash && g++ -O2 -std=c++17 -Isrc tools/screen_check.cpp -o build/screens_old && git stash pop
//     build/screens_old > old.txt && build/screens_new > new.txt && diff old.txt new.txt
//
// Важно: оба бинарника должны собираться с одним и тем же game.h, иначе
// разойдётся текст (версия в заголовке), а не отрисовка.
#include <cstdio>
#include <cstdint>
#include "renderer.h"
#include "game.h"

static uint32_t checksum(const Renderer& r) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < r.width * r.height; ++i) { h ^= r.pixels[i]; h *= 16777619u; }
    return h;
}

int main() {
    struct Case { const char* name; int state; };
    const Case cases[] = {
        {"меню", STATE_MENU},
        {"настройки", STATE_SETTINGS},
        {"справка", STATE_HELP},
        {"патчноуты", STATE_PATCH_NOTES},
        {"зал славы", STATE_HALL},
        {"выбор героя", STATE_HERO_SELECT},
        {"бой", STATE_PLAYING},
        {"пауза", STATE_PAUSED},
        {"сундук", STATE_CHEST},
        {"прокачка", STATE_LEVELUP},
    };
    SCREEN_W = 1280; SCREEN_H = 720;
    for (const Case& c : cases) {
        Renderer r; r.resize(SCREEN_W, SCREEN_H);
        Game g; g.simulationMode = true; srand(4242); g.init();
        g.gameMode = MODE_ENDLESS;
        g.runTime = 240.0f;
        for (int i = 0; i < 5; ++i) g.itemLevels[i] = 3;
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
