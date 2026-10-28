#pragma once
#include <string>
#include "protocol/SessionManager/SessionManager.h"

// 使用extern关键字表示这些是声明而不是定义
extern SessionManager& sessionManager;

// 最近一次 openProtocol 建连失败原因（供 UI 提示映射）
extern std::string g_lastOpenError;

// 函数声明，匹配 .cpp 文件中的定义
int openProtocol(const std::string& napiInput); // 返回 sessionId
bool sendCommand(int sessionId, const std::string& command); // 添加 sessionId
bool closeConnect(int sessionId); // 添加 sessionId