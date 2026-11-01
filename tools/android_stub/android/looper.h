// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
typedef struct ALooper ALooper;
typedef int (*ALooper_callbackFunc)(int fd, int events, void* data);
enum { ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1, ALOOPER_EVENT_INPUT = 1, ALOOPER_EVENT_OUTPUT = 2 };
ALooper* ALooper_prepare(int opts);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
