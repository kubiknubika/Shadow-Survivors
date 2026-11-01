// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <sys/types.h>
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
typedef struct AAssetDir AAssetDir;
enum { AASSET_MODE_UNKNOWN = 0, AASSET_MODE_RANDOM = 1, AASSET_MODE_STREAMING = 2, AASSET_MODE_BUFFER = 3 };
AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode);
int AAsset_read(AAsset* asset, void* buf, size_t count);
void AAsset_close(AAsset* asset);
AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName);
const char* AAssetDir_getNextFileName(AAssetDir* assetDir);
void AAssetDir_close(AAssetDir* assetDir);
