#pragma once
#ifdef ANDROID
#include <android/log.h>
#include <csignal>

#define LOG_TAG "docscanner"
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define BREAK() raise(SIGTRAP)
#define LOGE_AND_BREAK(...) LOGE(__VA_ARGS__); BREAK()
#endif