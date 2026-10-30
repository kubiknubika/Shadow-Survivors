// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "native_window.h"
#include "input.h"
#include "asset_manager.h"
struct ANativeActivity;
typedef struct ANativeActivity ANativeActivity;

struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity* activity);
    void (*onResume)(ANativeActivity* activity);
    void* (*onSaveInstanceState)(ANativeActivity* activity, size_t* outSize);
    void (*onPause)(ANativeActivity* activity);
    void (*onStop)(ANativeActivity* activity);
    void (*onDestroy)(ANativeActivity* activity);
    void (*onWindowFocusChanged)(ANativeActivity* activity, int hasFocus);
    void (*onNativeWindowCreated)(ANativeActivity* activity, ANativeWindow* window);
    void (*onNativeWindowResized)(ANativeActivity* activity, ANativeWindow* window);
    void (*onNativeWindowRedrawNeeded)(ANativeActivity* activity, ANativeWindow* window);
    void (*onNativeWindowDestroyed)(ANativeActivity* activity, ANativeWindow* window);
    void (*onInputQueueCreated)(ANativeActivity* activity, AInputQueue* queue);
    void (*onInputQueueDestroyed)(ANativeActivity* activity, AInputQueue* queue);
    void (*onContentRectChanged)(ANativeActivity* activity, const void* rect);
    void (*onConfigurationChanged)(ANativeActivity* activity);
    void (*onLowMemory)(ANativeActivity* activity);
};

struct ANativeActivity {
    ANativeActivityCallbacks* callbacks;
    void* vm;
    void* env;
    void* clazz;
    const char* internalDataPath;
    const char* externalDataPath;
    int32_t sdkVersion;
    void* instance;
    AAssetManager* assetManager;
    const char* obbPath;
};

enum {
    AWINDOW_FLAG_ALLOW_LOCK_WHILE_SCREEN_ON = 0x00000001,
    AWINDOW_FLAG_FULLSCREEN = 0x00000400,
    AWINDOW_FLAG_KEEP_SCREEN_ON = 0x00000080
};
void ANativeActivity_setWindowFlags(ANativeActivity* activity, uint32_t addFlags, uint32_t removeFlags);
