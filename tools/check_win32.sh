#!/bin/sh
# ── Проверка Windows-сборки ──────────────────────────────────────────────────
#
# Все стенды из tools/ — отдельные единицы трансляции: они включают game.h и
# renderer.h, но не src/main_win32.cpp. Поэтому ошибка в самом Windows-файле
# (так и случилось в 2.10.48: в WinMain третий параметр был безымянным, а код
# читал lpCmdLine) не роняла ни один стенд и жила в ветке до первого запуска
# build_windows.bat на Windows.
#
# Этот скрипт собирает ровно тот же TU теми же ключами, что build_windows.bat,
# кросс-компилятором Zig (в нём же заголовки и библиотеки MinGW-w64):
#
#     pip3 install ziglang        # или бинарь zig в PATH
#     tools/check_win32.sh
#
# Запуска на Windows он не заменяет: проверяются компиляция и линковка.
set -e
cd "$(dirname "$0")/.."

ZIG="${ZIG:-zig}"
if ! command -v "$ZIG" >/dev/null 2>&1; then
    if ZIG="$(python3 -c 'import os, ziglang; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))' 2>/dev/null)" && [ -x "$ZIG" ]; then
        :
    else
        echo "ПРОПУСК: zig не найден (pip3 install ziglang) — Windows-сборку проверить нечем"
        exit 0
    fi
fi

VER="$(sed -n 's/^#define GAME_VERSION "\(.*\)"/\1/p' src/version.h)"
NUM="$(sed -n 's/^#define GAME_VERSION_NUM \(.*\)/\1/p' src/version.h)"
if [ -z "$VER" ] || [ -z "$NUM" ]; then
    echo "ОШИБКА: не смог прочитать версию из src/version.h"
    exit 1
fi

OUT="${TMPDIR:-/tmp}/shadow_survivors_${VER}_check.exe"
rm -f "$OUT"

# Ключи — как в build_windows.bat: -O3 -std=c++17 -DGAME_VERSION -DGAME_VERSION_NUM,
# и тот же набор библиотек. -static у zig работает так же, как у mingw-g++.
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O3 -Isrc \
    -DGAME_VERSION="\"$VER\"" -DGAME_VERSION_NUM="$NUM" \
    src/main_win32.cpp -o "$OUT" \
    -mwindows -static -static-libgcc -static-libstdc++ \
    -lwinmm -lgdi32 -luser32 -ldwmapi -pthread

SIZE="$(wc -c < "$OUT" | tr -d ' ')"
echo "OK: Windows-сборка $VER собралась и слинковалась — $OUT ($SIZE байт)"
echo "    запуск на Windows этим скриптом не проверяется"
