#pragma once

namespace sk {
void LogWrite(const char* level, const char* fmt, ...);
void SetLogLevel(int lv);
}

#define LOG_APP 0
#define LOG_CORE 1

#define OH_LOG_INFO(tag, ...) sk::LogWrite("I", __VA_ARGS__)
#define OH_LOG_WARN(tag, ...) sk::LogWrite("W", __VA_ARGS__)
#define OH_LOG_ERROR(tag, ...) sk::LogWrite("E", __VA_ARGS__)
#define OH_LOG_DEBUG(tag, ...) sk::LogWrite("D", __VA_ARGS__)
#define OH_LOG_FATAL(tag, ...) sk::LogWrite("F", __VA_ARGS__)

#define SK_LOGI(...) sk::LogWrite("I", __VA_ARGS__)
#define SK_LOGW(...) sk::LogWrite("W", __VA_ARGS__)
#define SK_LOGE(...) sk::LogWrite("E", __VA_ARGS__)
