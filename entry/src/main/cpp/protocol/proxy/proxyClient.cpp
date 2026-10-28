#include "proxyClient.h"
#include "socks5Client.h"
#include "httpConnectClient.h"
#include "jsonAnalyse/cJSON.h"
#include <hilog/log.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>
#include <cstdint>

// Thread-local last proxy error: SSH/SFTP/PF connections run in
// different threads and must not overwrite each other's error code.
static thread_local int g_lastProxyError = PROXY_OK;
// Underlying errno of the last failed proxy connect (0 = none/timeout).
static thread_local int g_lastConnectErrno = 0;

void setLastProxyError(int err) {
    g_lastProxyError = err;
}

int getLastProxyError() {
    return g_lastProxyError;
}

const char* proxyErrorString(int err) {
    switch (err) {
        case PROXY_OK:
            return "成功";
        case PROXY_ERR_GENERAL:
            return "代理配置无效或未启用";
        case PROXY_ERR_SOCKET:
            return "创建代理 Socket 失败";
        case PROXY_ERR_CONNECT:
            return "连接代理服务器失败";
        case PROXY_ERR_HANDSHAKE:
            return "代理协议握手失败";
        case PROXY_ERR_AUTH:
            return "代理鉴权失败";
        case PROXY_ERR_TIMEOUT:
            return "代理连接超时";
        case PROXY_ERR_REJECTED:
            return "代理拒绝连接目标";
        case PROXY_ERR_CONNREFUSED:
            return "连接被拒绝";
        default:
            return "未知代理错误";
    }
}

// Wait until fd becomes readable/writable; timeoutSec is the total
// deadline for this stage, not per-select.
static bool proxyWaitReady(int fd, bool waitWrite, int timeoutSec) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec < 0 ? 0 : timeoutSec);
    while (true) {
        auto remainMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now()).count();
        if (remainMs <= 0) {
            return false; // total timeout
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv;
        tv.tv_sec = static_cast<long>(remainMs / 1000);
        tv.tv_usec = static_cast<long>((remainMs % 1000) * 1000);
        int rc = waitWrite ? select(fd + 1, NULL, &fds, NULL, &tv)
                           : select(fd + 1, &fds, NULL, NULL, &tv);
        if (rc != 0) {
            return rc > 0;
        }
        // This wait expired before the deadline; keep consuming remaining time.
    }
}

bool proxySendAll(int fd, const void* data, size_t len, int timeoutSec) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    size_t sent = 0;
    while (sent < len) {
        if (!proxyWaitReady(fd, true, timeoutSec)) {
            return false;
        }
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) {
            sent += static_cast<size_t>(n);
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

bool proxyRecvExact(int fd, void* buf, size_t n, int timeoutSec) {
    unsigned char* p = static_cast<unsigned char*>(buf);
    size_t got = 0;
    while (got < n) {
        if (!proxyWaitReady(fd, false, timeoutSec)) {
            return false;
        }
        ssize_t r = recv(fd, p + got, n - got, 0);
        if (r > 0) {
            got += static_cast<size_t>(r);
        } else if (r == 0) {
            return false; // peer closed
        } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

// Non-blocking connect to a single address; the select wait is a
// per-address connect timeout.
static int connectAddrNonBlocking(int family, const struct sockaddr* addr, socklen_t addrLen, int timeoutSec) {
    g_lastConnectErrno = 0;
    int fd = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        g_lastConnectErrno = errno;
        return -1;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int cr = connect(fd, addr, addrLen);
    if (cr == 0) {
        fcntl(fd, F_SETFL, flags);
        return fd;
    }
    if (errno != EINPROGRESS) {
        g_lastConnectErrno = errno;
        close(fd);
        return -1;
    }
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    int rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        close(fd);
        return -1; // timeout
    }
    int soerr = 0;
    socklen_t sl = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0 || soerr != 0) {
        g_lastConnectErrno = (soerr != 0) ? soerr : errno;
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, flags); // restore blocking mode
    return fd;
}

// Connect to the proxy server (non-blocking connect + select timeout).
static int connectProxySocket(const std::string& host, int port, int timeoutSec) {
    // Literal IPv4/IPv6 addresses skip getaddrinfo (no blocking DNS);
    // domain names still resolve via getaddrinfo.
    struct sockaddr_storage literal;
    memset(&literal, 0, sizeof(literal));
    socklen_t literalLen = 0;
    int literalFamily = 0;
    if (inet_pton(AF_INET, host.c_str(), &((struct sockaddr_in*)&literal)->sin_addr) == 1) {
        ((struct sockaddr_in*)&literal)->sin_family = AF_INET;
        ((struct sockaddr_in*)&literal)->sin_port = htons(static_cast<uint16_t>(port));
        literalFamily = AF_INET;
        literalLen = sizeof(struct sockaddr_in);
    } else if (inet_pton(AF_INET6, host.c_str(), &((struct sockaddr_in6*)&literal)->sin6_addr) == 1) {
        ((struct sockaddr_in6*)&literal)->sin6_family = AF_INET6;
        ((struct sockaddr_in6*)&literal)->sin6_port = htons(static_cast<uint16_t>(port));
        literalFamily = AF_INET6;
        literalLen = sizeof(struct sockaddr_in6);
    }
    if (literalFamily != 0) {
        int fd = connectAddrNonBlocking(literalFamily, (struct sockaddr*)&literal, literalLen, timeoutSec);
        if (fd < 0) {
            setLastProxyError(g_lastConnectErrno == ECONNREFUSED ? PROXY_ERR_CONNREFUSED : PROXY_ERR_CONNECT);
        }
        return fd;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    int gai = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (gai != 0) {
        setLastProxyError(PROXY_ERR_CONNECT);
        return -1;
    }

    int fd = -1;
    for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
        fd = connectAddrNonBlocking(p->ai_family, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen), timeoutSec);
        if (fd >= 0) {
            break;
        }
    }
    freeaddrinfo(result);

    if (fd < 0) {
        setLastProxyError(g_lastConnectErrno == ECONNREFUSED ? PROXY_ERR_CONNREFUSED : PROXY_ERR_CONNECT);
    }
    return fd;
}

// One attempt: connect to the proxy then run the handshake, handing back a
// tunnel fd (>=0) on success. Transient failures (connect/timeout/handshake)
// are retried once by connectViaProxy; config-class errors are not.
namespace {
struct ProxyAttempt {
    int fd = -1;
    int err = PROXY_ERR_GENERAL;
};

ProxyAttempt tryProxyConnectOnce(const ProxyConfig& cfg, const std::string& targetHost, int targetPort) {
    ProxyAttempt r;
    int fd = connectProxySocket(cfg.host, cfg.port, cfg.timeoutSec);
    if (fd < 0) {
        r.err = getLastProxyError();
        return r;
    }
    int rc = PROXY_ERR_GENERAL;
    if (cfg.type == "SOCKS5") {
        rc = socks5Handshake(fd, cfg.username, cfg.password, targetHost, targetPort, cfg.timeoutSec);
    } else if (cfg.type == "HTTP") {
        rc = httpConnectHandshake(fd, cfg.username, cfg.password, targetHost, targetPort, cfg.timeoutSec);
    } else {
        setLastProxyError(PROXY_ERR_GENERAL);
        close(fd);
        r.err = PROXY_ERR_GENERAL;
        return r;
    }
    if (rc != PROXY_OK) {
        r.err = getLastProxyError();
        close(fd);
        return r;
    }
    setLastProxyError(PROXY_OK);
    r.fd = fd;
    return r;
}

// Errors that may be transient (e.g. cold-start/DNS/TCP jitter): worth a retry.
bool isTransientProxyError(int err) {
    return err == PROXY_ERR_CONNECT || err == PROXY_ERR_TIMEOUT || err == PROXY_ERR_HANDSHAKE ||
           err == PROXY_ERR_SOCKET;
}
} // namespace

int connectViaProxy(const ProxyConfig& cfg, const std::string& targetHost, int targetPort) {
    if (!cfg.enabled || cfg.host.empty() || cfg.port <= 0 || cfg.port > 65535) {
        setLastProxyError(PROXY_ERR_GENERAL);
        return -1;
    }

    // First attempt plus one automatic retry on a transient failure.
    const int maxAttempts = 2;
    ProxyAttempt last;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        last = tryProxyConnectOnce(cfg, targetHost, targetPort);
        if (last.fd >= 0) {
            return last.fd; // tunnel established
        }
        if (attempt < maxAttempts - 1 && isTransientProxyError(last.err)) {
            OH_LOG_INFO(LOG_APP, "proxy connect failed (%d), retrying...", last.err);
            continue; // transient failure -> reconnect once
        }
        break;
    }
    setLastProxyError(last.err);
    return -1;
}

bool parseProxyConfigFromJson(void* cjsonRoot, ProxyConfig& cfg) {
    if (cjsonRoot == nullptr) {
        return false;
    }
    cJSON* root = static_cast<cJSON*>(cjsonRoot);
    cJSON* proxy = cJSON_GetObjectItemCaseSensitive(root, "Proxy");
    if (!cJSON_IsObject(proxy)) {
        return false;
    }

    cfg.enabled = true;
    cfg = ProxyConfig();
    cfg.enabled = true;

    cJSON* type = cJSON_GetObjectItemCaseSensitive(proxy, "type");
    if (cJSON_IsString(type) && type->valuestring) {
        cfg.type = type->valuestring;
    }
    cJSON* host = cJSON_GetObjectItemCaseSensitive(proxy, "host");
    if (cJSON_IsString(host) && host->valuestring) {
        cfg.host = host->valuestring;
    }
    cJSON* port = cJSON_GetObjectItemCaseSensitive(proxy, "port");
    if (cJSON_IsString(port) && port->valuestring) {
        cfg.port = atoi(port->valuestring);
    } else if (cJSON_IsNumber(port)) {
        cfg.port = port->valueint;
    }
    cJSON* user = cJSON_GetObjectItemCaseSensitive(proxy, "username");
    if (cJSON_IsString(user) && user->valuestring) {
        cfg.username = user->valuestring;
    }
    cJSON* pass = cJSON_GetObjectItemCaseSensitive(proxy, "password");
    if (cJSON_IsString(pass) && pass->valuestring) {
        cfg.password = pass->valuestring;
    }
    cJSON* timeout = cJSON_GetObjectItemCaseSensitive(proxy, "timeout");
    if (cJSON_IsNumber(timeout)) {
        cfg.timeoutSec = timeout->valueint;
    } else if (cJSON_IsString(timeout) && timeout->valuestring) {
        // ArkTS may pass timeout as a string (e.g. "10"); accept both.
        int t = atoi(timeout->valuestring);
        if (t > 0) {
            cfg.timeoutSec = t;
        }
    }
    return true;
}
