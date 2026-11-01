#!/bin/sh
# ── Проверка Android-слоя без NDK и телефона ────────────────────────────────
#
# Три ступени, от дешёвой к дорогой:
#
#   1. -fsyntax-only против упрощённых объявлений NDK из tools/android_stub:
#      ловит опечатки, забытые include и неверные вызовы игрового ядра;
#   2. check_android_bat.py: батник, gradle, CMake, манифест, иконки и то, что
#      инструкция не врёт про файлы;
#   3. исполняющие стенды (если есть g++): touch_probe гоняет схему касаний
#      против настоящей игры, android_smoke исполняет src/main_android.cpp на
#      поддельном NDK — поток игры, EGL, AAudio, ассеты, пауза, сейв.
#
#     tools/check_android.sh              # всё
#     tools/check_android.sh --quick      # только 1 и 2 (без исполнения)
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-g++}

echo "проверяю src/main_android.cpp против tools/android_stub ..."
$CXX -std=c++17 -fsyntax-only -Wall -Wextra -Wno-unused-parameter \
    -D__ANDROID__ -Isrc -Itools/android_stub src/main_android.cpp

echo "ядро игры без Windows (тот же кадр, что и на десктопе):"
$CXX -std=c++17 -fsyntax-only -Isrc tools/screen_check.cpp

echo "батник, проект, ресурсы и инструкция (кодировка, метки, версии, lib_name, XML, иконки):"
python3 tools/check_android_bat.py --strict

if [ "$1" = "--quick" ]; then
    echo "ok (без исполняющих стендов)"
    exit 0
fi

mkdir -p build

echo "стенд касаний: схема против настоящей игры ..."
$CXX -O2 -std=c++17 -pthread -Isrc tools/touch_probe.cpp -o build/touch_probe
./build/touch_probe

echo "стенд Android-слоя: main_android.cpp на поддельном NDK ..."
$CXX -O2 -std=c++17 -pthread -D__ANDROID__ -Isrc -Itools/android_stub -Itools/android_host \
    tools/android_smoke.cpp tools/android_host/fake_ndk.cpp -o build/android_smoke
./build/android_smoke

echo "ok"
