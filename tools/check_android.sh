#!/bin/sh
# ── Проверка Android-слоя без NDK ───────────────────────────────────────────
#
# Собирает src/main_android.cpp с -fsyntax-only против упрощённых объявлений
# NDK из tools/android_stub. Это ловит опечатки, забытые include и ошибки в
# вызовах игрового ядра, но НЕ заменяет сборку APK: настоящие заголовки и
# линковку даёт только NDK (android/app/src/main/cpp/CMakeLists.txt).
#
#     tools/check_android.sh
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-g++}
echo "проверяю src/main_android.cpp против tools/android_stub ..."
$CXX -std=c++17 -fsyntax-only -Wall -Wextra -Wno-unused-parameter \
    -D__ANDROID__ -Isrc -Itools/android_stub src/main_android.cpp
echo "ядро игры без Windows (тот же кадр, что и на десктопе):"
$CXX -std=c++17 -fsyntax-only -Isrc tools/screen_check.cpp
echo "ok"
