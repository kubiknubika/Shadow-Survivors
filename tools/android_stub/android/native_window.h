// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
int32_t ANativeWindow_getWidth(ANativeWindow* window);
int32_t ANativeWindow_getHeight(ANativeWindow* window);
