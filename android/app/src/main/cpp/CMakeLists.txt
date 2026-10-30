# Единственный исходник Android-слоя. Вся игра (src/game.h, src/renderer.h,
# src/audio.h) — заголовочные файлы, они попадают в сборку через #include.
cmake_minimum_required(VERSION 3.18)
project(shadow_survivors CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Корень репозитория на пять уровней выше: android/app/src/main/cpp -> корень.
get_filename_component(SHADOW_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src" ABSOLUTE)

add_library(shadow_survivors SHARED "${SHADOW_SRC}/main_android.cpp")

target_include_directories(shadow_survivors PRIVATE "${SHADOW_SRC}")

target_compile_options(shadow_survivors PRIVATE
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-variable
    -Wno-missing-field-initializers)

target_link_options(shadow_survivors PRIVATE -Wl,--gc-sections)

# android  — ANativeActivity, ввод, ассеты, лог
# EGL/GLESv2 — показать кадр
# aaudio   — звук (API 26+)
target_link_libraries(shadow_survivors
    android
    log
    EGL
    GLESv2
    aaudio)
