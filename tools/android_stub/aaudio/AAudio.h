// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
typedef struct AAudioStream AAudioStream;
typedef struct AAudioStreamBuilder AAudioStreamBuilder;
typedef int32_t aaudio_result_t;
typedef int32_t aaudio_data_callback_result_t;
typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
#define AAUDIO_OK 0
#define AAUDIO_CALLBACK_RESULT_CONTINUE 0
#define AAUDIO_CALLBACK_RESULT_STOP 1
enum {
    AAUDIO_DIRECTION_OUTPUT = 0, AAUDIO_DIRECTION_INPUT = 1,
    AAUDIO_SHARING_MODE_EXCLUSIVE = 0, AAUDIO_SHARING_MODE_SHARED = 1,
    AAUDIO_FORMAT_INVALID = -1, AAUDIO_FORMAT_PCM_I16 = 1, AAUDIO_FORMAT_PCM_FLOAT = 2,
    AAUDIO_PERFORMANCE_MODE_NONE = 10, AAUDIO_PERFORMANCE_MODE_POWER_SAVING = 11,
    AAUDIO_PERFORMANCE_MODE_LOW_LATENCY = 12
};
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** builder);
void AAudioStreamBuilder_setDirection(AAudioStreamBuilder* builder, int32_t direction);
void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder* builder, int32_t sharingMode);
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder* builder, int32_t mode);
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder* builder, int32_t format);
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder* builder, int32_t sampleRate);
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder* builder, int32_t channelCount);
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder* builder, AAudioStream_dataCallback callback, void* userData);
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder* builder, AAudioStream** stream);
void AAudioStreamBuilder_delete(AAudioStreamBuilder* builder);
aaudio_result_t AAudioStream_requestStart(AAudioStream* stream);
aaudio_result_t AAudioStream_requestStop(AAudioStream* stream);
aaudio_result_t AAudioStream_close(AAudioStream* stream);
int32_t AAudioStream_getChannelCount(AAudioStream* stream);
int32_t AAudioStream_getSampleRate(AAudioStream* stream);
