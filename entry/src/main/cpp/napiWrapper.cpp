#include "napiWrapper.hpp"
#include "command.h"
#include "protocol/ssh/SshSession.h"
#include "protocol/telnetSession/telnetSession.h"

// 变量定义
SessionManager& sessionManager = SessionManager::getInstance();
// 移除全局SessionHandle

// 最近一次建连失败原因（供 UI 提示映射）
std::string g_lastOpenError;

// 新接口：返回sessionId
int openProtocol(const std::string& napiInput) {
    PROTOCOL protocolType = analyseProtocol(napiInput);
    auto session = sessionManager.createSession(protocolType);
    if (!session) {
        OH_LOG_ERROR(LOG_APP, "openProtocol: Failed to create session for type %{public}d", protocolType);
        g_lastOpenError = "CONNECT_ERR:1";
        return -1;
    }
    sessionManager.applyPendingCallbacks(session);
    int sessionId = sessionManager.getSessionId(session);
    if (!session->openConnect(napiInput)) {
        // 提取具体建连失败原因（如代理错误）
        g_lastOpenError.clear();
        if (auto* sshSession = dynamic_cast<SshSession*>(session.get())) {
            g_lastOpenError = sshSession->getLastConnectError();
        } else if (auto* telnetSession = dynamic_cast<TelnetSession*>(session.get())) {
            g_lastOpenError = telnetSession->getLastConnectError();
        }
        if (g_lastOpenError.empty()) {
            g_lastOpenError = "CONNECT_ERR:0";
        }
        sessionManager.destroySession(sessionId);
        return -1;
    }
    g_lastOpenError.clear();
    return sessionId;
}

bool sendCommand(int sessionId, const std::string& command) {
    try {
        auto session = sessionManager.getSession(sessionId);
        if (!session) {
            OH_LOG_ERROR(LOG_APP, "sendCommand: Session ID %{public}d not found.", sessionId);
            return false;
        }
        return session->sendData(command);
    } catch (...) {
        OH_LOG_ERROR(LOG_APP, "sendCommand: Caught unknown exception for ID %{public}d.", sessionId);
        return false;
    }
}

bool closeConnect(int sessionId) {
    try {
        auto session = sessionManager.getSession(sessionId);
        if (!session) {
            OH_LOG_ERROR(LOG_APP, "closeConnect: Failed to get session for ID %{public}d", sessionId);
            return false;
        }

        OH_LOG_INFO(LOG_APP, "closeConnect: Calling session->disconnect() for ID %{public}d", sessionId);
        bool disconnectResult = session->disconnect();
        OH_LOG_INFO(LOG_APP, "closeConnect: session->disconnect() for ID %{public}d returned: %{public}d", sessionId, disconnectResult);

        if (!disconnectResult) {
            OH_LOG_ERROR(LOG_APP, "closeConnect: disconnectResult is false for ID %{public}d, returning false.", sessionId);
            return false;
        }

        OH_LOG_INFO(LOG_APP, "closeConnect: disconnectResult is true for ID %{public}d. Destroying session...", sessionId);
        sessionManager.destroySession(sessionId);

        OH_LOG_INFO(LOG_APP, "closeConnect: About to return true for ID %{public}d", sessionId);
        return true;
    } catch (const std::exception& e) {
        OH_LOG_ERROR(LOG_APP, "closeConnect: Caught std::exception for ID %{public}d: %{public}s. Returning false.", sessionId, e.what());
        return false;
    } catch (...) {
        OH_LOG_ERROR(LOG_APP, "closeConnect: Caught unknown exception for ID %{public}d. Returning false.", sessionId);
        return false;
    }
}