#pragma once
#include "../Platform.h"

#define LOG_APP 0
#define LOG_CORE 1
#define LOG_SERVICE 2
#define LOG_DOMAIN 0

typedef int LogType;

#define OH_LOG_DEBUG(dom, ...) skLogRaw(0, __VA_ARGS__)
#define OH_LOG_INFO(dom, ...) skLogRaw(1, __VA_ARGS__)
#define OH_LOG_WARN(dom, ...) skLogRaw(2, __VA_ARGS__)
#define OH_LOG_ERROR(dom, ...) skLogRaw(3, __VA_ARGS__)
#define OH_LOG_FATAL(dom, ...) skLogRaw(4, __VA_ARGS__)
