// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "looper.h"
typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;
enum { AINPUT_EVENT_TYPE_KEY = 1, AINPUT_EVENT_TYPE_MOTION = 2 };
int32_t AInputEvent_getType(const AInputEvent* event);
int AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent);
int AInputQueue_preDispatchEvent(AInputQueue* queue, AInputEvent* event);
void AInputQueue_finishEvent(AInputQueue* queue, AInputEvent* event, int handled);
void AInputQueue_attachLooper(AInputQueue* queue, ALooper* looper, int ident, ALooper_callbackFunc callback, void* data);
void AInputQueue_detachLooper(AInputQueue* queue);

enum {
    AMOTION_EVENT_ACTION_MASK = 0xff,
    AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00,
    AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT = 8,
    AMOTION_EVENT_ACTION_DOWN = 0,
    AMOTION_EVENT_ACTION_UP = 1,
    AMOTION_EVENT_ACTION_MOVE = 2,
    AMOTION_EVENT_ACTION_CANCEL = 3,
    AMOTION_EVENT_ACTION_POINTER_DOWN = 5,
    AMOTION_EVENT_ACTION_POINTER_UP = 6
};
int32_t AMotionEvent_getAction(const AInputEvent* event);
size_t AMotionEvent_getPointerCount(const AInputEvent* event);
int32_t AMotionEvent_getPointerId(const AInputEvent* event, size_t pointerIndex);
float AMotionEvent_getX(const AInputEvent* event, size_t pointerIndex);
float AMotionEvent_getY(const AInputEvent* event, size_t pointerIndex);

enum { AKEY_EVENT_ACTION_DOWN = 0, AKEY_EVENT_ACTION_UP = 1, AKEY_EVENT_ACTION_MULTIPLE = 2 };
int32_t AKeyEvent_getAction(const AInputEvent* event);
int32_t AKeyEvent_getKeyCode(const AInputEvent* event);
