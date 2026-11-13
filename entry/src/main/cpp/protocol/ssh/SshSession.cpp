#include "SshSession.h"
#include <numeric>
#include <sys/stat.h>
#include "LogManager.h"
#include <fcntl.h> // Include for fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <errno.h> // Likely needed for error checking with fcntl/sockets
#include <sys/socket.h> // For socket operations if not already included
#include <functional> // Ensure functional is included
#include <sys/types.h> // For off_t
#include <sstream>     // 添加 sstream 用于 URL 解码 (虽然当前实现可能不需要，但保留)
#include <string>      // 确保 string 包含
#include <atomic> // 需要包含 atomic 头文件
#include <netdb.h> // For getaddrinfo and gai_strerror
#include <openssl/sha.h>

static std::string base64NoPad(const unsigned char* data, size_t len) {
    static const char TBL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i + 2 < len) {
        unsigned int v = (static_cast<unsigned int>(data[i]) << 16) |
                         (static_cast<unsigned int>(data[i + 1]) << 8) |
                         static_cast<unsigned int>(data[i + 2]);
        out.push_back(TBL[(v >> 18) & 63]);
        out.push_back(TBL[(v >> 12) & 63]);
        out.push_back(TBL[(v >> 6) & 63]);
        out.push_back(TBL[v & 63]);
        i += 3;
    }
    if (len - i == 1) {
        unsigned int v = static_cast<unsigned int>(data[i]) << 16;
        out.push_back(TBL[(v >> 18) & 63]);
        out.push_back(TBL[(v >> 12) & 63]);
    } else if (len - i == 2) {
        unsigned int v = (static_cast<unsigned int>(data[i]) << 16) |
                         (static_cast<unsigned int>(data[i + 1]) << 8);
        out.push_back(TBL[(v >> 18) & 63]);
        out.push_back(TBL[(v >> 12) & 63]);
        out.push_back(TBL[(v >> 6) & 63]);
    }
    return out;
}

bool SshSession::verifyHostKey() {
    size_t keyLen = 0;
    int keyType = 0;
    const char* rawKey = libssh2_session_hostkey(session.get(), &keyLen, &keyType);
    if (rawKey == nullptr) {
        OH_LOG_ERROR(LOG_APP, "verifyHostKey: host key unavailable, skip verification");
        return true;
    }

    switch (keyType) {
        case LIBSSH2_HOSTKEY_TYPE_RSA: hostKeyTypeName = "ssh-rsa"; break;
        case LIBSSH2_HOSTKEY_TYPE_DSS: hostKeyTypeName = "ssh-dss"; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: hostKeyTypeName = "ecdsa-sha2-nistp256"; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: hostKeyTypeName = "ecdsa-sha2-nistp384"; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: hostKeyTypeName = "ecdsa-sha2-nistp521"; break;
        case LIBSSH2_HOSTKEY_TYPE_ED25519: hostKeyTypeName = "ssh-ed25519"; break;
        default: hostKeyTypeName = "unknown"; break;
    }

    const char* hash = libssh2_hostkey_hash(session.get(), LIBSSH2_HOSTKEY_HASH_SHA256);
    if (hash != nullptr) {
        hostKeyFingerprint = "SHA256:" + base64NoPad(reinterpret_cast<const unsigned char*>(hash), 32);
    } else {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(rawKey), keyLen, digest);
        hostKeyFingerprint = "SHA256:" + base64NoPad(digest, SHA256_DIGEST_LENGTH);
    }

    OH_LOG_INFO(LOG_APP, "host key: type=%{public}s fp=%{public}s", hostKeyTypeName.c_str(), hostKeyFingerprint.c_str());

    if (expectedHostKey.empty()) {
        return true;
    }
    if (expectedHostKey == hostKeyFingerprint) {
        return true;
    }
    OH_LOG_ERROR(LOG_APP, "host key mismatch: expected=%{public}s actual=%{public}s",
                 expectedHostKey.c_str(), hostKeyFingerprint.c_str());
    return false;
}

SshSession::SshSession()
    : session(nullptr, &libssh2_session_free),
      channelShell(nullptr, &libssh2_channel_free),
      knownHosts(nullptr, &libssh2_knownhost_free),
      sock(std::make_unique<libssh2_socket_t>(-1)), // 恢复初始化
      threadPool(std::make_unique<ThreadPool>(8)) // 恢复初始化 - Increase size to 8
{
    OH_LOG_INFO(LOG_APP, "SshSession构造函数: 初始化 (已移除 osc7Regex 初始化)");
    // 其他初始化代码...
}

// --- 新增 RAII Helper ---
namespace {
    class ShellConfigGuard {
        std::atomic<bool>& flag_;
    public:
        explicit ShellConfigGuard(std::atomic<bool>& flag) : flag_(flag) {
            OH_LOG_INFO(LOG_APP, "ShellConfigGuard: Setting config flag to true.");
            flag_.store(true);
        }
        ~ShellConfigGuard() {
            OH_LOG_INFO(LOG_APP, "ShellConfigGuard: Setting config flag to false.");
            flag_.store(false);
        }
        // Prevent copying/moving
        ShellConfigGuard(const ShellConfigGuard&) = delete;
        ShellConfigGuard& operator=(const ShellConfigGuard&) = delete;
        ShellConfigGuard(ShellConfigGuard&&) = delete;
        ShellConfigGuard& operator=(ShellConfigGuard&&) = delete;
    };
}
// ----------------------

void SshSession::runAsyncInThreadPool(std::function<void()> task) {
    std::thread([future = threadPool->enqueue(std::move(task))]() mutable {
        future.get();
    }).detach();
}

SshSession::~SshSession() {
    OH_LOG_INFO(LOG_APP, "SshSession destructor started.");

    // 首先停止所有线程
    OH_LOG_INFO(LOG_APP, "Setting stopThread flag in destructor.");
    stopThread.store(true);
    callBackDataCV.notify_one(); // 确保 callBackSendData 也能退出

    // 清理资源前加锁
    {
        std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
        OH_LOG_INFO(LOG_APP, "Acquired libssh2Mutex in destructor.");

        // --- 新增：在 session 之前清理 knownHosts ---
        if (knownHosts) {
             OH_LOG_INFO(LOG_APP, "Resetting knownHosts in destructor.");
             knownHosts.reset();
        }
        // -----------------------------------------

        // 重置并确保 SFTP 会话清理
        if (interactiveSftpSession) {
            OH_LOG_INFO(LOG_APP, "Resetting interactive SFTP session in destructor.");
            interactiveSftpSession.reset();
        }
        if (transferSftpSession) {
            OH_LOG_INFO(LOG_APP, "Resetting transfer SFTP session in destructor.");
            transferSftpSession.reset();
        }

        // 重置端口转发
        if (portForward) {
            portForward.reset();
        }

        // 关闭并清理 channelShell
        if (channelShell) {
            try {
                if (!libssh2_channel_eof(channelShell.get())) {
                    // 尝试正常关闭通道
                    int closeAttempts = 0;
                    while (closeAttempts < 3 &&
                           libssh2_channel_close(channelShell.get()) == LIBSSH2_ERROR_EAGAIN) {
                        waitSocket();
                        closeAttempts++;
                    }
                }
            } catch (...) {
                OH_LOG_ERROR(LOG_APP, "关闭Shell通道时出现异常");
            }
            channelShell.reset();
        }

        // 关闭SSH会话
        if (session) {
            try {
                int disconnectAttempts = 0;
                while (disconnectAttempts < 3 &&
                       libssh2_session_disconnect(session.get(), "Normal shutdown") == LIBSSH2_ERROR_EAGAIN) {
                    waitSocket();
                    disconnectAttempts++;
                }
            } catch (...) {
                OH_LOG_ERROR(LOG_APP, "断开SSH会话时发生异常");
            }
            session.reset(); // session 现在在 knownHosts 之后 reset
        }

        // 关闭SSH会话
        if (session) {
            try {
                int disconnectAttempts = 0;
                while (disconnectAttempts < 3 &&
                       libssh2_session_disconnect(session.get(), "Normal shutdown") == LIBSSH2_ERROR_EAGAIN) {
                    waitSocket();
                    disconnectAttempts++;
                }
            } catch (...) {
                OH_LOG_ERROR(LOG_APP, "断开SSH会话时出现异常");
            }
            session.reset(); // session 现在在 knownHosts 之后 reset
        }

         OH_LOG_INFO(LOG_APP, "Releasing libssh2Mutex in destructor.");
    } // 锁在这里释放

    // 关闭socket
    if (sock && *sock != LIBSSH2_INVALID_SOCKET) {
        try {
            shutdown(*sock, SHUT_RDWR);
            close(*sock);
        } catch (...) {
            OH_LOG_ERROR(LOG_APP, "关闭socket时出现异常");
        }
        *sock = LIBSSH2_INVALID_SOCKET;
    }

    // 等待线程池完成清理 - 注意：确保线程池在 keepAliveThread join 之后清理
     OH_LOG_INFO(LOG_APP, "Resetting threadPool in destructor.");
    threadPool.reset(); // 确保这个在 join 之后

    OH_LOG_INFO(LOG_APP, "SshSession destructor finished.");
}

int SshSession::waitSocket() {
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;
    libssh2_socket_t current_sock = LIBSSH2_INVALID_SOCKET; // 本地变量存储socket

    timeout.tv_sec = 0;
    timeout.tv_usec = 50000; // 保持50ms超时

    FD_ZERO(&fd);

    // --- 在锁内获取必要的共享信息 ---
    {
        std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
        if (!sock || *sock == LIBSSH2_INVALID_SOCKET) {
            OH_LOG_ERROR(LOG_APP, "waitSocket: Invalid socket descriptor inside lock.");
            return -1; // Socket无效
        }
        current_sock = *sock; // 复制 socket descriptor 到本地变量

        if (!session) {
            OH_LOG_ERROR(LOG_APP, "waitSocket: Invalid session pointer inside lock.");
            return -1; // Session 无效
        }
        dir = libssh2_session_block_directions(session.get()); // 获取方向
    }
    // --- 锁已释放 ---

    // 检查 socket descriptor 是否有效（在锁外检查本地副本）
    if (current_sock == LIBSSH2_INVALID_SOCKET) {
        OH_LOG_ERROR(LOG_APP, "waitSocket: Socket descriptor became invalid after lock release.");
        return -1;
    }

    // --- 设置 fd_set (不需要锁) ---
    FD_SET(current_sock, &fd);

    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;
    // ---------------------------

    // --- 调用 select (不需要锁) ---
    // OH_LOG_DEBUG(LOG_APP, "waitSocket: Calling select (socket=%d, read=%d, write=%d)", current_sock, readfd != NULL, writefd != NULL); // Optional debug log
    do {
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;
        rc = select((int)(current_sock + 1), readfd, writefd, NULL, &timeout);
    } while (rc < 0 && errno == EINTR);
    // OH_LOG_DEBUG(LOG_APP, "waitSocket: select returned %d", rc); // Optional debug log
    // ---------------------------

    // 检查 select 错误
    if (rc < 0) {
        int select_errno = errno;
        OH_LOG_ERROR(LOG_APP, "waitSocket: select error %{public}d (%{public}s)", select_errno, strerror(select_errno));
    }

    if (rc > 0 && readfd != NULL) {
        lastRxMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    return rc;
}

int SshSession::probeInbound(int waitMs) {
    libssh2_socket_t current_sock = LIBSSH2_INVALID_SOCKET;
    {
        std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
        if (!sock || *sock == LIBSSH2_INVALID_SOCKET) {
            return -1;
        }
        current_sock = *sock;
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(current_sock, &rd);
    struct timeval timeout;
    timeout.tv_sec = waitMs / 1000;
    timeout.tv_usec = (waitMs % 1000) * 1000;
    int rc = select((int)(current_sock + 1), &rd, nullptr, nullptr, &timeout);
    if (rc <= 0) {
        return rc;
    }

    char c = 0;
    int n = (int)recv(current_sock, &c, 1, MSG_PEEK);
    if (n > 0) {
        return 1;
    }
    if (n == 0) {
        return -1;
    }
    return 0;
}

bool SshSession::openConnect(const std::string& napiInput) {
    uint32_t hostaddr {};
    int ret = 0;
    string userAuthList {};

    SessionAnalyse* SessionAnalyseHandle = encapsulateSSHFromJson(napiInput);
    if (SessionAnalyseHandle == NULL) {
        OH_LOG_INFO(LOG_APP, "Failed to parse JSON input.");
        lastConnectError = "JSON_PARSE_FAILED";
        return false;
    }

    // --- 解析代理配置 ---
    cJSON* proxyRootJson = cJSON_Parse(napiInput.c_str());
    if (proxyRootJson != NULL) {
        parseProxyConfigFromJson(proxyRootJson, proxyConfig);
        if (proxyConfig.type == "None") {
            proxyConfig.enabled = false;
        }
        cJSON_Delete(proxyRootJson);
    }

    // 保存认证信息到成员变量，用于SFTP连接
    if (SessionAnalyseHandle->hostName) {
        hostname = SessionAnalyseHandle->hostName;
    } else {
        OH_LOG_ERROR(LOG_APP, "主机名为空");
        return false;
    }

    port = SessionAnalyseHandle->port;

    if (SessionAnalyseHandle->userName) {
        username = SessionAnalyseHandle->userName;
    } else {
        OH_LOG_ERROR(LOG_APP, "用户名为空");
        return false;
    }

    if (SessionAnalyseHandle->password) {
        password = SessionAnalyseHandle->password;
    }

    useKeyAuth = (SessionAnalyseHandle->authType == AUTH_PUBLICKEY);
    if (useKeyAuth) {
        privateKeyPath = ""; // 不再使用路径
        this->privateKeyData = ""; // 初始化
        if (SessionAnalyseHandle->password) {
            passphrase = SessionAnalyseHandle->password;
        }
    }

    // 获取保活参数
    keepAliveInterval = SessionAnalyseHandle->keepAlive;
    if (keepAliveInterval == 0) {
        keepAliveInterval = 60; // 默认值为60秒
        OH_LOG_INFO(LOG_APP, "保活间隔为0，使用默认值: %{public}d秒", keepAliveInterval);
    } else if (keepAliveInterval < 0) {
        keepAliveInterval = 0;
        OH_LOG_INFO(LOG_APP, "保活已显式禁用");
    }

    expectedHostKey = SessionAnalyseHandle->expectedHostKey ? std::string(SessionAnalyseHandle->expectedHostKey) : std::string();
    expectedHostKeyType = SessionAnalyseHandle->expectedHostKeyType ? std::string(SessionAnalyseHandle->expectedHostKeyType) : std::string();

    OH_LOG_INFO(LOG_APP, "SSH连接配置: %{public}s@%{public}s:%{public}d, 保活间隔: %{public}d秒",
               username.c_str(), hostname.c_str(), port, keepAliveInterval);

    ret = libssh2_init(0);
    if (ret) {
        OH_LOG_INFO(LOG_APP, "libssh2 initialization failed (%{public}d)", ret);
        return false;
    }

    // --- Socket 创建移到这里之后 ---
    // sock = std::make_unique<libssh2_socket_t>(socket(AF_INET, SOCK_STREAM, 0));
    // if (*sock.get() == LIBSSH2_INVALID_SOCKET) {
    //    OH_LOG_INFO(LOG_APP, "failed to create socket.");
    //    return false;
    // }

    OH_LOG_INFO(LOG_APP, "准备解析的主机名/IP: '%{public}s'", SessionAnalyseHandle->hostName);

    if (!proxyConfig.enabled) {
    // ============ 直连路径（原有逻辑）============
    struct sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = 0;
    int pton_ret = 0; // 用于存储 inet_pton 的返回值
    bool parsedAsIp = false;

    // 先尝试将输入作为 IPv4/IPv6 字面量解析
    pton_ret = inet_pton(AF_INET, SessionAnalyseHandle->hostName,
                         &(((struct sockaddr_in *)&addr)->sin_addr));
    if (pton_ret == 1) {
        OH_LOG_INFO(LOG_APP, "地址被识别为 IPv4");
        struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
        sin->sin_family = AF_INET;
        sin->sin_port = htons(SessionAnalyseHandle->port);
        addr_len = sizeof(struct sockaddr_in);
        parsedAsIp = true;
    } else {
        if (pton_ret == -1) {
            OH_LOG_ERROR(LOG_APP, "inet_pton(AF_INET) 失败: errno=%{public}d (%{public}s)", errno, strerror(errno));
        }

        pton_ret = inet_pton(AF_INET6, SessionAnalyseHandle->hostName,
                             &(((struct sockaddr_in6 *)&addr)->sin6_addr));
        if (pton_ret == 1) {
            OH_LOG_INFO(LOG_APP, "地址被识别为 IPv6");
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = htons(SessionAnalyseHandle->port);
            addr_len = sizeof(struct sockaddr_in6);
            parsedAsIp = true;
        } else {
            if (pton_ret == -1) {
                OH_LOG_ERROR(LOG_APP, "inet_pton(AF_INET6) 失败: errno=%{public}d (%{public}s)", errno, strerror(errno));
            } else {
                OH_LOG_INFO(LOG_APP,
                            "inet_pton 未将 '%{public}s' 识别为 IPv4/IPv6 字面量，尝试作为域名解析",
                            SessionAnalyseHandle->hostName);
            }
        }
    }

    // 如果既不是合法 IPv4 也不是合法 IPv6，则按域名解析
    if (!parsedAsIp) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // 允许 IPv4 或 IPv6
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* result = nullptr;
        std::string portStr = std::to_string(SessionAnalyseHandle->port);

        int gaiRet = getaddrinfo(SessionAnalyseHandle->hostName, portStr.c_str(), &hints, &result);
        if (gaiRet != 0) {
            OH_LOG_ERROR(LOG_APP, "getaddrinfo 解析域名失败: %{public}s", gai_strerror(gaiRet));
            return false;
        }

        bool foundAddr = false;
        for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6) {
                if (ptr->ai_addrlen <= sizeof(addr)) {
                    memcpy(&addr, ptr->ai_addr, ptr->ai_addrlen);
                    addr_len = static_cast<socklen_t>(ptr->ai_addrlen);
                    OH_LOG_INFO(LOG_APP, "域名解析成功，使用地址族 %{public}d", ptr->ai_family);
                    foundAddr = true;
                    break;
                }
            }
        }

        freeaddrinfo(result);

        if (!foundAddr || addr_len == 0) {
            OH_LOG_ERROR(LOG_APP, "getaddrinfo 未返回可用的 IPv4/IPv6 地址");
            return false;
        }
    } else if (addr_len == 0) {
        // 理论上不会发生，但为了安全性保留检查
        OH_LOG_ERROR(LOG_APP, "内部错误：地址解析后 addr_len 仍为 0");
        return false;
    }

    // --- 在确定地址族后创建 Socket ---
    OH_LOG_INFO(LOG_APP, "使用地址族 %{public}d 创建套接字", addr.ss_family);
    sock = std::make_unique<libssh2_socket_t>(socket(addr.ss_family, SOCK_STREAM, 0));
    if (*sock.get() == LIBSSH2_INVALID_SOCKET) {
        OH_LOG_ERROR(LOG_APP, "创建套接字失败: errno=%{public}d (%{public}s)", errno, strerror(errno));
        libssh2_exit(); // 清理 libssh2
        return false;
    }
    OH_LOG_INFO(LOG_APP, "套接字创建成功 (fd=%{public}d)", *sock.get());

    int retryCount = 0;
    const int maxRetries = 3;
    const int timeoutSec = SessionAnalyseHandle->timeout > 0 ? SessionAnalyseHandle->timeout : 10;
    const int connectTimeout = timeoutSec >= 3 ? (timeoutSec / 3) : 1;

    int flags = fcntl(*sock.get(), F_GETFL, 0);

    fcntl(*sock.get(), F_SETFL, flags | O_NONBLOCK);

    bool connected = false;

    // --- 修改非阻塞连接逻辑 ---
    OH_LOG_INFO(LOG_APP, "发起非阻塞连接...");
    int connect_ret = connect(*sock.get(), (struct sockaddr *)&addr, addr_len);

    if (connect_ret == 0) {
        // 立即连接成功 (虽然对于非阻塞不常见)
        OH_LOG_INFO(LOG_APP, "连接立即成功");
        connected = true;
        fcntl(*sock.get(), F_SETFL, flags); // 恢复阻塞模式
    } else if (errno == EINPROGRESS) {
        OH_LOG_INFO(LOG_APP, "连接正在进行中 (EINPROGRESS)，进入 select 等待循环 (最多 %{public}d 次，每次 %{public}d 秒)", maxRetries, connectTimeout);
        while (retryCount < maxRetries) {
            retryCount++;
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(*sock.get(), &writefds);

            // readfds 和 exceptfds 设为 NULL，我们只关心写就绪（连接完成）
            fd_set readfds = writefds; // 也可以同时检查读和错误
            fd_set exceptfds = writefds;

            struct timeval timeout;
            timeout.tv_sec = connectTimeout;
            timeout.tv_usec = 0;

            OH_LOG_INFO(LOG_APP, "等待 select (第 %{public}d 次尝试)...", retryCount);
            int select_ret = select(*sock.get() + 1, &readfds, &writefds, &exceptfds, &timeout);

            if (select_ret > 0) {
                // select 返回 > 0 表示套接字状态改变
                int error = 0;
                socklen_t len = sizeof(error);
                // 检查套接字错误状态
                if (getsockopt(*sock.get(), SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
                    if (error == 0) {
                        // 没有错误，连接成功
                        OH_LOG_INFO(LOG_APP, "select 报告连接成功 (SO_ERROR=0)");
                        connected = true;
                        fcntl(*sock.get(), F_SETFL, flags); // 恢复阻塞模式
                        break; // 成功，退出等待循环
                    } else {
                        // 连接过程中发生错误
                        OH_LOG_ERROR(LOG_APP, "select 报告连接失败 (SO_ERROR=%{public}d): %{public}s", error, strerror(error));
                        errno = error; // 设置 errno 以便外部检查
                        // 不再重试，直接跳出循环去处理失败
                        break;
                    }
                } else {
                     OH_LOG_ERROR(LOG_APP, "getsockopt(SO_ERROR) 失败: %{public}s", strerror(errno));
                     // 不再重试，直接跳出循环去处理失败
                     break;
                }
            } else if (select_ret == 0) {
                // select 超时
                OH_LOG_INFO(LOG_APP, "select 等待超时 (第 %{public}d 次尝试)", retryCount);
                // 继续循环，直到达到 maxRetries
            } else { // select_ret < 0
                // select 本身出错
                OH_LOG_ERROR(LOG_APP, "select 失败: %{public}s", strerror(errno));
                // 不再重试，直接跳出循环去处理失败
                break;
            }
        } // while (retryCount < maxRetries)
    } else {
        // 初始 connect 调用直接失败 (非 EINPROGRESS)
        OH_LOG_ERROR(LOG_APP, "初始 connect 调用失败: %{public}s", strerror(errno));
        // connected 保持 false
    }
    // --------------------------

    if (!connected) {
        OH_LOG_INFO(LOG_APP, "Failed to connect after %{public}d attempts", maxRetries);
        lastConnectError = "TCP_CONNECT_FAILED";

        if (sock && *sock != LIBSSH2_INVALID_SOCKET) {
            close(*sock);
            *sock = LIBSSH2_INVALID_SOCKET;
        }

        libssh2_exit();

        return false;
    }
    } else {
    // ============ 代理路径（SOCKS5/HTTP） ============
    OH_LOG_INFO(LOG_APP, "使用代理: %{public}s:%{public}d 目标 %{public}s:%{public}d",
                proxyConfig.host.c_str(), proxyConfig.port, hostname.c_str(), port);
    int proxyFd = connectViaProxy(proxyConfig, hostname, port);
    if (proxyFd < 0) {
        // pass a stable proxy error code so the UI can localize the message
        lastConnectError = "PROXY_ERR:" + std::to_string(-getLastProxyError());
        OH_LOG_ERROR(LOG_APP, "connect via proxy failed, code=%d", getLastProxyError());
        libssh2_exit();
        return false;
    }
    sock = std::make_unique<libssh2_socket_t>(proxyFd);
    OH_LOG_INFO(LOG_APP, "代理隧道建立成功 (fd=%{public}d)", proxyFd);
    }

    session.reset(libssh2_session_init());
    if (!session) {
        OH_LOG_INFO(LOG_APP, "Could not initialize SSH session.");
        lastConnectError = "SESSION_INIT_FAILED";
        return false;
    }

    libssh2_trace(session.get(), 0);

    if (!expectedHostKeyType.empty()) {
        static const char* kHostKeyOrder =
            "ecdsa-sha2-nistp256,ecdsa-sha2-nistp384,ecdsa-sha2-nistp521,"
            "ssh-ed25519,rsa-sha2-512,rsa-sha2-256,ssh-rsa,ssh-dss";
        std::string hostKeyPref = expectedHostKeyType + "," + kHostKeyOrder;
        if (libssh2_session_method_pref(session.get(), LIBSSH2_METHOD_HOSTKEY, hostKeyPref.c_str()) == 0) {
            OH_LOG_INFO(LOG_APP, "host key preference: %{public}s", hostKeyPref.c_str());
        }
    }

    knownHosts.reset(libssh2_knownhost_init(session.get()));
    if (!knownHosts) {
        OH_LOG_INFO(LOG_APP, "Failed to initialize known hosts");
        lastConnectError = "KNOWNHOST_INIT_FAILED";
        return false;
    }

    const int handshakeTimeout = SessionAnalyseHandle->timeout > 0 ? SessionAnalyseHandle->timeout : 10;
    auto handshakeStartTime = std::chrono::steady_clock::now();
    while ((ret = libssh2_session_handshake(session.get(), *sock)) == LIBSSH2_ERROR_EAGAIN) {
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - handshakeStartTime).count();
        if (elapsedTime > handshakeTimeout) {
            OH_LOG_INFO(LOG_APP, "Session handshake failed due to timeout");
            lastConnectError = "HANDSHAKE_TIMEOUT";
            return false;
        }
        if (waitSocket() <= 0) {
            OH_LOG_INFO(LOG_APP, "Session handshake failed due to timeout");
            lastConnectError = "HANDSHAKE_TIMEOUT";
            return false;
        }
    }

    if (ret != 0) {
        int hsErrno = libssh2_session_last_errno(session.get());
        char *hsMsg = nullptr;
        int hsMsgLen = 0;
        libssh2_session_last_error(session.get(), &hsMsg, &hsMsgLen, 0);
        OH_LOG_ERROR(LOG_APP, "SSH handshake failed: rc=%{public}d errno=%{public}d msg=%{public}s",
                     ret, hsErrno, hsMsg ? hsMsg : "N/A");
        lastConnectError = std::string("HANDSHAKE_FAILED:") + std::to_string(hsErrno);
        if (sock && *sock != LIBSSH2_INVALID_SOCKET) {
            close(*sock);
            *sock = LIBSSH2_INVALID_SOCKET;
        }
        return false;
    }

    if (!verifyHostKey()) {
        lastConnectError = "HOSTKEY_ERR:" + hostKeyFingerprint;
        return false;
    }

    const char *authListRaw = libssh2_userauth_list(session.get(), SessionAnalyseHandle->userName, (unsigned int)strlen(SessionAnalyseHandle->userName));
    if (authListRaw != nullptr) {
        userAuthList = authListRaw;
    }
    if (!userAuthList.empty()) {
        // 根据前端配置的认证方式选择认证方法
        if (SessionAnalyseHandle->authType == AUTH_PASSWORD) {
            if (userAuthList.find("password") == std::string::npos) {
                OH_LOG_INFO(LOG_APP, "Password authentication not supported by server");
                lastConnectError = "AUTH_METHOD_UNSUPPORTED";
                return false;
            }

            if (strlen(SessionAnalyseHandle->password) != 0) {
                const int authTimeout = SessionAnalyseHandle->timeout > 0 ? SessionAnalyseHandle->timeout : 10;
                auto authStartTime = std::chrono::steady_clock::now();
                while ((ret = libssh2_userauth_password(session.get(),
                    SessionAnalyseHandle->userName,
                    SessionAnalyseHandle->password)) == LIBSSH2_ERROR_EAGAIN) {
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - authStartTime).count();
                    if (elapsedTime > authTimeout) {
                        OH_LOG_INFO(LOG_APP, "Authentication timeout");
                        lastConnectError = "AUTH_TIMEOUT";
                        return false;
                    }
                    if (waitSocket() <= 0) {
                        OH_LOG_INFO(LOG_APP, "Authentication timeout");
                        lastConnectError = "AUTH_TIMEOUT";
                        return false;
                    }
                }

                if (ret != 0) {
                    OH_LOG_INFO(LOG_APP, "Password authentication failed with error code: %{public}d", ret);
                    lastConnectError = "AUTH_FAILED";
                    return false;
                }
            }
        }
        else if (SessionAnalyseHandle->authType == AUTH_PUBLICKEY) {
            if (userAuthList.find("publickey") == std::string::npos) {
                OH_LOG_INFO(LOG_APP, "Public key authentication not supported by server");
                lastConnectError = "AUTH_METHOD_UNSUPPORTED";
                return false;
            }

            // 使用前端提供的私钥文件描述符进行认证
            if (SessionAnalyseHandle->privateKeyFd < 0) {
                OH_LOG_INFO(LOG_APP, "Invalid private key file descriptor");
                lastConnectError = "KEY_FD_INVALID";
                return false;
            }

            // 从文件描述符读取私钥内容
            std::vector<char> keyDataVec;
            char buffer[READ_MAX_BUFF_SIZE];
            ssize_t bytesRead;
            while ((bytesRead = read(SessionAnalyseHandle->privateKeyFd, buffer, sizeof(buffer))) > 0) {
                keyDataVec.insert(keyDataVec.end(), buffer, buffer + bytesRead);
            }

            if (keyDataVec.empty()) {
                OH_LOG_INFO(LOG_APP, "Failed to read private key data");
                lastConnectError = "KEY_READ_FAILED";
                return false;
            }
            // 将读取到的密钥数据存储到成员变量中
            this->privateKeyData = std::string(keyDataVec.begin(), keyDataVec.end());

            // 使用内存中的私钥数据进行认证
            if (libssh2_userauth_publickey_frommemory(session.get(),
                SessionAnalyseHandle->userName,
                strlen(SessionAnalyseHandle->userName),
                nullptr, 0,  // 公钥数据可以为空，因为服务器会从私钥中提取
                this->privateKeyData.data(), // 使用成员变量
                this->privateKeyData.size(), // 使用成员变量
                SessionAnalyseHandle->password)) {  // 如果私钥有密码保护，使用提供的密码
                OH_LOG_INFO(LOG_APP, "Authentication by public key failed");
                // --- 添加详细错误日志 ---
                int last_errno = libssh2_session_last_errno(session.get());
                char *errmsg = nullptr;
                int errmsg_len = 0;
                libssh2_session_last_error(session.get(), &errmsg, &errmsg_len, 0);
                OH_LOG_ERROR(LOG_APP, "Public key auth failed with libssh2 error code: %{public}d, message: %{public}s", last_errno, errmsg ? errmsg : "N/A");
                // -----------------------
                lastConnectError = "AUTH_FAILED";
                return false;
            }
            OH_LOG_INFO(LOG_APP, "Authentication by public key succeeded");
        }
        else {
            OH_LOG_INFO(LOG_APP, "Unsupported authentication type");
            lastConnectError = "AUTH_METHOD_UNSUPPORTED";
            return false;
        }
    }
    else {
        OH_LOG_INFO(LOG_APP, "Failed to get authentication methods");
        lastConnectError = "AUTH_LIST_FAILED";
        return false;
    }

    do {
        channelShell.reset(libssh2_channel_open_session(session.get()));
        if (channelShell || libssh2_session_last_error(session.get(), NULL, NULL, 0) != LIBSSH2_ERROR_EAGAIN)
            break;
        waitSocket();
    } while (1);

    if (!channelShell) {
        int chErrno = libssh2_session_last_errno(session.get());
        char *chMsg = nullptr;
        int chMsgLen = 0;
        libssh2_session_last_error(session.get(), &chMsg, &chMsgLen, 0);
        OH_LOG_ERROR(LOG_APP, "Failed to open shell channel: errno=%{public}d msg=%{public}s",
                     chErrno, chMsg ? chMsg : "N/A");
        lastConnectError = "CHANNEL_OPEN_FAILED";
        return false;
    }

    while((ret = libssh2_channel_request_pty(channelShell.get(), "xterm")) == LIBSSH2_ERROR_EAGAIN) {
        waitSocket();
    }

    while((ret = libssh2_channel_shell(channelShell.get())) == LIBSSH2_ERROR_EAGAIN) {
        waitSocket();
    }

    string m_termType = "xterm-256color";

    // 终端尺寸常量
    const int TERM_WIDTH = 80;      // 终端宽度（字符数）
    const int TERM_HEIGHT = 24;     // 终端高度（行数）
    const int TERM_WIDTH_PX = 0;    // 终端像素宽度（0表示使用默认值）
    const int TERM_HEIGHT_PX = 0;   // 终端像素高度（0表示使用默认值）

    // 禁用终端回显，避免双重换行问题
    // 注意：SSH终端模式是按照POSIX规范定义的
    // ECHO (53) - 是否回显输入字符
    // ECHOCTL (90) - 是否回显控制字符
    // ECHONL (91) - 是否回显换行符
    typedef struct {
        char opcode;
        int value;
    } LIBSSH2_TERM_MODE;

    LIBSSH2_TERM_MODE modes[4] = {
        {53, 0},  // Disable ECHO
        {90, 0},  // Disable ECHOCTL
        {91, 0},  // Disable ECHONL
        {0, 0}    // Terminal
    };

    // 设置终端模式以禁用回显，确保在所有服务器环境下都能正确控制回显
    while((ret = libssh2_channel_request_pty_ex(channelShell.get(), m_termType.c_str(), m_termType.length(),
                                            (char*)modes, sizeof(modes), // 传递实际的终端模式设置
                                            TERM_WIDTH, TERM_HEIGHT, TERM_WIDTH_PX, TERM_HEIGHT_PX)) == LIBSSH2_ERROR_EAGAIN) {
        waitSocket();
    }

    // 初始化SSH接收数据线程
    stopThread.store(false);
    status = SessionStatus::CONNECTED;
    statusChange.store(true);

    // 启动数据接收线程
    runAsyncInThreadPool([this]() {
        this->receiveData();
    });

    // 启动数据发送回调线程
    runAsyncInThreadPool([this]() {
        this->callBackSendData();
    });

    // --- 新增: 配置 SSH Keepalive (替换原来的线程启动) ---
    if (keepAliveInterval > 0 && session) {
        OH_LOG_INFO(LOG_APP, "为 SSH 会话配置 libssh2 Keepalive (Interval: %{public}d)", keepAliveInterval);
        libssh2_keepalive_config(session.get(), 1, keepAliveInterval); // Re-enable keepalive

        // --- 新增: 启动Keepalive线程 ---
        stopThread.store(false); // 确保线程不会立即退出
        runAsyncInThreadPool([this]() {
            this->keepAliveThread();
        });
        OH_LOG_INFO(LOG_APP, "SSH Keepalive线程已启动");
        // --- 新增完毕 ---

    } else {
        OH_LOG_INFO(LOG_APP, "SSH Keepalive 已禁用 (Interval: %{public}d).", keepAliveInterval);
    }
    // --- 修改：同步初始化 SFTP 会话 ---
    OH_LOG_INFO(LOG_APP, "开始同步初始化 SFTP 会话...");
    if (!this->initSftp()) {
        OH_LOG_ERROR(LOG_APP, "同步 SFTP 初始化失败，将断开连接");
        // 尝试清理已建立的 SSH 连接
        disconnect(); // 调用 disconnect 进行清理
        return false; // 返回 false 表示整体连接失败
    }
    OH_LOG_INFO(LOG_APP, "同步 SFTP 初始化成功");
    // Check the actual result of SFTP initialization
    if (!interactiveSftpSession) {
        OH_LOG_WARN(LOG_APP, "openConnect: Interactive SFTP session failed to initialize.");
        // Consider setting a flag: sftpInteractiveAvailable = false;
    }
    if (!transferSftpSession) {
        OH_LOG_WARN(LOG_APP, "openConnect: Transfer SFTP session failed to initialize.");
        // Consider setting a flag: sftpTransferAvailable = false;
    }
    if (!interactiveSftpSession && !transferSftpSession) {
         OH_LOG_ERROR(LOG_APP, "openConnect: Both SFTP sessions failed to initialize. SFTP functionality will be unavailable.");
         // Consider setting a flag: sftpAvailable = false;
    }
    // --- SFTP Init Check End ---

    OH_LOG_INFO(LOG_APP, "SSH 连接和 SFTP 初始化全部成功");
    OH_LOG_INFO(LOG_APP, "[Session %{public}d] openConnect successful. logManagerInitialized = %{public}d", sessionId, logManagerInitialized.load()); // 添加日志：记录 logManagerInitialized 状态

    OH_LOG_INFO(LOG_APP, "Authentication successful.");

    OH_LOG_INFO(LOG_APP, "Main session connected and authenticated. Initializing Port Forwarding Manager...");

    // Gather parameters for the dedicated port forwarding session
    SshPortForwardConfig pfConfig;
    pfConfig.hostname = this->hostname;
    pfConfig.port = this->port;
    pfConfig.username = this->username;
    pfConfig.password = this->password; // Contains password OR passphrase
    pfConfig.useKeyAuth = this->useKeyAuth;
    pfConfig.privateKeyData = this->privateKeyData; // Pass the key data
    pfConfig.passphrase = this->passphrase; // Pass the passphrase if using key auth
    pfConfig.keepAliveInterval = this->keepAliveInterval; // Copy keepalive interval
    pfConfig.proxy = this->proxyConfig; // PF 独立会话同步走代理

    // Initialize the Port Forwarding Manager with the dedicated config
    try {
        // Pass 'this' for ThreadPool access, and pfConfig for connection details
        portForward = std::make_unique<SshPortForward>(this, pfConfig); // <-- Corrected call here
        OH_LOG_INFO(LOG_APP, "Port Forwarding Manager initialized with dedicated configuration.");

        // --- Establish the dedicated PF session --- 
        // OH_LOG_INFO(LOG_APP, "Attempting to establish dedicated Port Forwarding session...");
        // if (!portForward->connectSession()) { // Call connectSession()
        //     OH_LOG_ERROR(LOG_APP, "Failed to establish dedicated Port Forwarding session.");
        //     // --- Modified: Don't disconnect, just log and continue ---
        //     // disconnect(); // Clean up main connection resources - REMOVED
        //     // return false; // Return failure from openConnect - REMOVED
        //     OH_LOG_WARN(LOG_APP, "Port Forwarding connection failed. Port forwarding functionality will be unavailable.");
        //     // Optionally set a flag: portForwardingAvailable = false;
        //     // ---------------------------------------------------------
        // }
        // OH_LOG_INFO(LOG_APP, "Dedicated Port Forwarding session established successfully.");
        // ----------------------------------------
        // --- REMOVED: connectSession call moved to lazy initialization in start... methods ---

    } catch (const std::bad_alloc& e) {
        OH_LOG_ERROR(LOG_APP, "Failed to allocate memory for Port Forwarding Manager: %{public}s", e.what());
        // Clean up resources before returning false
        disconnect(); // Replace closeConnect with disconnect
        return false; // Indicate connection failure
    } catch (const std::exception& e) {
        OH_LOG_ERROR(LOG_APP, "Exception initializing Port Forwarding Manager: %{public}s", e.what());
        disconnect(); // Replace closeConnect with disconnect
        return false; // Indicate connection failure
    } catch (...) {
        OH_LOG_ERROR(LOG_APP, "Unknown exception initializing Port Forwarding Manager.");
        disconnect(); // Replace closeConnect with disconnect
        return false; // Indicate connection failure
    }

    // 连接状态设置为已连接 - Moved outside PF try-catch
    // connected = true; // This variable is not used later, status is set earlier

    OH_LOG_INFO(LOG_APP, "SshSession::openConnect completed successfully (potentially with partial SFTP/PF functionality).");
    return true; // Return true as long as main shell connection succeeded
}

bool SshSession::disconnect() {
    OH_LOG_INFO(LOG_APP, "SshSession::disconnect called."); // 添加日志以便追踪

    { // 清理缓冲区
        std::lock_guard<std::mutex> lock(bufferMutex);
        initialBuffer.clear();
    }

    if(status != SessionStatus::CONNECTED) {
        OH_LOG_INFO(LOG_APP, "Attempting to disconnect, but session is not in CONNECTED state.");
        return false;
    }

    OH_LOG_INFO(LOG_APP, "Setting stopThread flag to true.");
    stopThread.store(true);
    callBackDataCV.notify_one(); // 唤醒 callBackSendData 线程

    // 标记状态为正在断开连接
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = SessionStatus::DISCONNECTING;
    }

    bool success = true;

    // 使用锁保护资源访问
    {
        std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);

        // --- 新增：在断开 SSH 之前清理 SFTP 会话 ---
        OH_LOG_INFO(LOG_APP, "Disconnect: Resetting SFTP sessions.");
        if (interactiveSftpSession) {
            interactiveSftpSession.reset();
        }
        if (transferSftpSession) {
            transferSftpSession.reset();
        }

        // 关闭channelShell
        if(channelShell) {
            try {
                int attempts = 0;
                int result = LIBSSH2_ERROR_EAGAIN;
                while(attempts < 3 && result == LIBSSH2_ERROR_EAGAIN) {
                    result = libssh2_channel_close(channelShell.get());
                    if(result == LIBSSH2_ERROR_EAGAIN)
                        waitSocket();
                    attempts++;
                }

                if(result != 0 && result != LIBSSH2_ERROR_EAGAIN) {
                    OH_LOG_ERROR(LOG_APP, "关闭Shell通道失败: %{public}d", result);
                }
            } catch(...) {
                OH_LOG_ERROR(LOG_APP, "关闭Shell通道时发生异常");
            }
        }

        // 断开SSH会话
        if(session) {
            try {
                int attempts = 0;
                int result = LIBSSH2_ERROR_EAGAIN;
                while(attempts < 3 && result == LIBSSH2_ERROR_EAGAIN) {
                    result = libssh2_session_disconnect(session.get(), "Normal shutdown");
                    if(result == LIBSSH2_ERROR_EAGAIN)
                        waitSocket();
                    attempts++;
                }

                if(result != 0 && result != LIBSSH2_ERROR_EAGAIN) {
                    OH_LOG_ERROR(LOG_APP, "断开SSH会话失败: %{public}d", result);
                }
            } catch(...) {
                OH_LOG_ERROR(LOG_APP, "断开SSH会话时发生异常");
            }
        }
    }

    // 更新状态为已断开连接
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = SessionStatus::DISCONNECTED;
    }

    // 禁用日志管理器并刷新尾行（如有）
    try {
        logManager.disable();
        logManagerInitialized.store(false);
    } catch (...) {
        OH_LOG_WARN(LOG_APP, "SshSession::disconnect: exception while disabling LogManager");
    }

    // 发送会话关闭通知
    pendingCallData("SSH连接已断开", true);

    OH_LOG_INFO(LOG_APP, "SshSession::disconnect returning: %{public}d", success); // 添加日志：记录最终返回值
    return success;
}

bool SshSession::sendData(const std::string& command) {
    // V5 - Extremely detailed logging

    // 1. Check status
    SessionStatus currentStatus;
    {
        // OH_LOG_INFO(LOG_APP, "sendData: Acquiring statusMutex...");
        std::lock_guard<std::mutex> lock(statusMutex);
        // OH_LOG_INFO(LOG_APP, "sendData: Acquired statusMutex.");
        currentStatus = status;
        if (currentStatus == SessionStatus::CONNECTED) {
            statusChange = true; // Mark activity only if connected
        }
    } // statusMutex released
    if (currentStatus != SessionStatus::CONNECTED) {
        OH_LOG_ERROR(LOG_APP, "sendData: Failed - Session not connected. Returning false.");
        return false;
    }

    // 2. Check channelShell validity
    if (!channelShell) {
        OH_LOG_ERROR(LOG_APP, "sendData: Failed - channelShell is NULL. Returning false.");
        return false;
    }

    // 3. Acquire libssh2Mutex and check EOF inside lock
    std::lock_guard<std::recursive_mutex> sshLock(libssh2Mutex);

    if (session) {
        libssh2_session_set_blocking(session.get(), 0);
    }

    // 4. Check channelShell EOF
    bool is_eof = libssh2_channel_eof(channelShell.get());
    if (is_eof) {
        OH_LOG_ERROR(LOG_APP, "sendData: Failed - channelShell is EOF. Returning false.");
        return false; // Exiting while lock is held
    }

    // 5. Prepare for write
    size_t totalWritten = 0;
    size_t commandLength = command.length();
    if (commandLength == 0) {
        return true; // Exiting while lock is held
    }

    // 6. Write loop
    const long long stallBudgetMs = 60000;
    auto lastProgressAt = std::chrono::steady_clock::now();
    while (totalWritten < commandLength) {

        // --- 恢复为直接阻塞写入 ---
        ssize_t n = libssh2_channel_write(channelShell.get(),
                                      command.c_str() + totalWritten,
                                      commandLength - totalWritten);
        // ------------------------


        if (n > 0) {
            totalWritten += n;
            lastProgressAt = std::chrono::steady_clock::now();
        } else if (n == LIBSSH2_ERROR_EAGAIN) {
            // Check EOF *before* waiting on the socket
            if (libssh2_channel_eof(channelShell.get())) {
                OH_LOG_ERROR(LOG_APP, "sendData: Detected EOF during EAGAIN handling. Aborting write.");
                return false; // Exit loop if EOF detected
            }
            // Keep lock while calling waitSocket as it uses session/sock
            int ws_rc = waitSocket();
            if (ws_rc < 0) {
                // Check EOF again after waitSocket timeout/error
                if (libssh2_channel_eof(channelShell.get())) {
                     OH_LOG_ERROR(LOG_APP, "sendData: Detected EOF after waitSocket timeout/error. Aborting write.");
                     return false; // Exit loop if EOF detected
                }
                OH_LOG_ERROR(LOG_APP, "sendData: waitSocket failed (%d) with %zu/%zu bytes written.", ws_rc, totalWritten, commandLength);
                return false;
            }
            long long stalledMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastProgressAt).count();
            if (stalledMs > stallBudgetMs) {
                OH_LOG_ERROR(LOG_APP, "sendData: write stalled %lld ms. Wrote %zu/%zu bytes. Returning false.",
                             stalledMs, totalWritten, commandLength);
                return false;
            }
        } else {
            // Other write error
            int write_errno = libssh2_session_last_errno(session.get());
            char *errmsg = nullptr; // 添加 errmsg 获取
            int errmsg_len = 0;     // 添加 errmsg 获取
            libssh2_session_last_error(session.get(), &errmsg, &errmsg_len, 0); // 添加 errmsg 获取
            OH_LOG_ERROR(LOG_APP, "sendData: Write error (n=%zd, libssh2_errno=%{public}d, msg: %{public}s). Returning false.", n, write_errno, errmsg ? errmsg : "N/A"); // 添加 errmsg 输出
            return false; // Exiting while lock is held
        }
    }

    return true; // Exiting while lock is held (lock_guard will release)
}

void SshSession::receiveData() {
    auto threadId = std::this_thread::get_id();
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData 开始执行 (v13 - 增加初始化日志)", threadId, sessionId); // 添加 sessionId
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Initial logManagerInitialized = %{public}d", threadId, sessionId, logManagerInitialized.load()); // 添加日志：记录初始 logManagerInitialized

    char buffer[READ_MAX_BUFF_SIZE];
    ssize_t n = 0;
    int read_errno = 0;
    int ws_rc = 0;
    bool should_exit = false;

    // --- Initial non-blocking read (unchanged from previous version) ---
    bool initial_data_processed = false;
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Attempting initial non-blocking read...", threadId);
    {
        std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
        if (session && channelShell) {
            libssh2_channel_set_blocking(channelShell.get(), 0);
            n = libssh2_channel_read(channelShell.get(), buffer, sizeof(buffer) - 1);

            // --- 添加详细的首次读取日志 ---
            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Initial read result n = %zd", threadId, sessionId, n);
            // -----------------------------

            if (n > 0) {
                buffer[n] = '\0';
                std::string initial_received_data(buffer, n);
                OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Initial data received (first 50 chars): %{public}.50s", threadId, sessionId, initial_received_data.c_str()); // 打印部分初始数据

                // --- 新增：检查是否在配置 Shell ---
                if (isConfiguringShell.load()) {
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Discarding initial data received during shell configuration (%zd bytes).", threadId, sessionId, n);
                    initial_data_processed = true; // 标记为已处理，避免后续逻辑误判
                    // 直接跳出 if (n > 0) 的处理，因为首次读取只执行一次
                } else {
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Initial read successful (%zd bytes). Buffering/Processing...", threadId, sessionId, n);
                    if (followTerminalDirectory) {
                        parseOSC7Sequence(initial_received_data); // Use the string version
                    }
                    if (!logManagerInitialized.load()) {
                        OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: logManager not initialized, appending to initialBuffer.", threadId, sessionId); // 记录缓冲行为
                        std::lock_guard<std::mutex> bufferLock(bufferMutex);
                        if (initialBuffer.length() + n < MAX_INITIAL_BUFFER_SIZE) {
                            initialBuffer.append(buffer, n);
                            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Appended initial data to buffer. Buffer size: %zu", threadId, sessionId, initialBuffer.length());
                        } else {
                            OH_LOG_WARN(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Initial buffer full, dropping initial data.", threadId, sessionId);
                        }
                    } else {
                        OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: logManager initialized, writing to logManager.", threadId, sessionId); // 记录写日志行为
                        logManager.writeLog(buffer, n);
                    }
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] receiveData: Calling pendingCallData for initial data.", threadId, sessionId); // 记录调用回调
                    pendingCallData(buffer, false); // Send to UI
                    {
                        std::lock_guard<std::mutex> status_lock(statusMutex);
                        statusChange = true;
                    }
                    initial_data_processed = true;
                    // --- 原始逻辑结束 ---
                } // --- 新增 else 块结束 ---

            } else if (n == 0) {
                 OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Initial read returned 0 (EOF).", threadId);
                 should_exit = true;
            } else {
                 read_errno = libssh2_session_last_errno(session.get());
                 if (read_errno != LIBSSH2_ERROR_EAGAIN) {
                      OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Initial read error (errno=%d).", threadId, read_errno);
                      should_exit = true;
                 } else {
                     OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Initial read got EAGAIN. Proceeding to main loop.", threadId);
                 }
            }
        } else {
            OH_LOG_WARN(LOG_APP, "[Thread %{public}ld] receiveData: Session/Channel invalid before initial read.", threadId);
            should_exit = true;
        }
    }
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Initial read attempt finished.", threadId);


    // --- 主接收循环 ---
    while(!stopThread.load() && !should_exit) {
        n = 0;
        ws_rc = 0;
        read_errno = 0;
        bool channel_valid = true;

        // 1. Check validity (unchanged)
        {
            std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
            if (!session || !channelShell) {
                OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: 会话或通道已无效.", threadId);
                channel_valid = false;
                should_exit = true;
            }
        }
        if (!channel_valid) {
            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Channel invalid, breaking loop.", threadId);
            break;
        }

        // 2. Wait socket (unchanged)
        ws_rc = waitSocket();

        if (ws_rc < 0) {
            read_errno = errno;
            OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: waitSocket error (%d: %s)，标记退出", threadId, read_errno, strerror(read_errno));
            should_exit = true;
        } else if (ws_rc > 0) {
            // 3. Attempt read (unchanged, includes try-catch)
            {
                std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
                try {
                    if (!channelShell) {
                         OH_LOG_WARN(LOG_APP, "[Thread %{public}ld] receiveData: channelShell became null before read attempt.", threadId);
                         n = -1; read_errno = -999;
                    } else {
                       // --- 设置为非阻塞读 ---
                       libssh2_channel_set_blocking(channelShell.get(), 0);
                       n = libssh2_channel_read(channelShell.get(), buffer, sizeof(buffer) - 1);

                       if (n < 0) {
                           read_errno = libssh2_session_last_errno(session.get());
                       }
                    }
                } catch (const std::exception& e) {
                    OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Exception during read: %{public}s", threadId, e.what());
                    n = -1; read_errno = -998;
                } catch (...) {
                    OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Unknown exception during read", threadId);
                    n = -1; read_errno = -997;
                }
            }

            // --- 4. Process read result (Filtering added here) ---
            if (n > 0) {
                buffer[n] = '\0';
                std::string received_data(buffer, n);

                if (isConfiguringShell.load()) {
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Discarding data received during shell configuration (%zd bytes).", threadId, n);
                    continue;
                }

                if (followTerminalDirectory) {
                    parseOSC7Sequence(received_data); // Use the string
                }

                if (!logManagerInitialized.load()) {
                    std::lock_guard<std::mutex> bufferLock(bufferMutex);
                     if (!logManagerInitialized.load()) {
                         if (initialBuffer.length() + n < MAX_INITIAL_BUFFER_SIZE) {
                             initialBuffer.append(buffer, n);
                             // OH_LOG_DEBUG(LOG_APP, "[Thread %{public}ld] receiveData: Appended data to buffer. Buffer size: %zu", threadId, initialBuffer.length());
                         } else {
                              OH_LOG_WARN(LOG_APP, "[Thread %{public}ld] receiveData: Initial buffer full, dropping data.", threadId);
                         }
                     } else {
                        logManager.writeLog(buffer, n);
                     }
                } else {
                    logManager.writeLog(buffer, n);
                }

                pendingCallData(buffer, false); // Send to UI

                { // Update status (unchanged)
                    std::lock_guard<std::mutex> status_lock(statusMutex);
                    statusChange = true;
                }
                continue;
            } else if (n == 0) {
                // EOF (unchanged)
                OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Read 0 bytes -> Channel EOF detected，标记退出", threadId);
                should_exit = true;
            } else { // n < 0
                // Read error (unchanged)
                if (read_errno != LIBSSH2_ERROR_EAGAIN) {
                    OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Non-EAGAIN read error (errno=%{public}d)，标记退出", threadId, read_errno);
                    should_exit = true;
                } else {
                    // EAGAIN (unchanged)
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
        } else { // ws_rc == 0
            // waitSocket timeout (unchanged)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

    } // while loop end

    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData 循环最终结束 (should_exit=%d)", threadId, should_exit);

    // --- 5. Post-loop processing (unchanged) ---
    bool notify_needed = false;
    bool cleanup_needed = false; // <<< Add a flag to trigger cleanup
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        if (status == SessionStatus::CONNECTED) {
            if (should_exit && !stopThread.load()) {
               status = SessionStatus::DISCONNECTED;
               notify_needed = true;
               cleanup_needed = true; // <<< Mark for cleanup
               OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData 设置状态为 DISCONNECTED，标记需要清理", threadId);
            } else if (stopThread.load()) {
               OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData 循环因外部停止而结束.", threadId);
               // External stop via disconnect() or destructor will handle cleanup
            }
        }
    }

    // --- Perform cleanup if marked ---
    if (cleanup_needed) {
        OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Triggering cleanup for SFTP and Port Forward due to unexpected disconnect...", threadId);
        // Use recursive mutex as these might call back into libssh2 functions
        std::lock_guard<std::recursive_mutex> sshLock(libssh2Mutex);
        try {
            // Reset SFTP sessions (calls their destructors)
            if (interactiveSftpSession) {
                OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Resetting interactive SFTP session.", threadId);
                interactiveSftpSession.reset();
            }
            if (transferSftpSession) {
                OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Resetting transfer SFTP session.", threadId);
                transferSftpSession.reset();
            }
            // Disconnect Port Forwarding session
            if (portForward) {
                 OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: Calling portForward->disconnectSession().", threadId);
                 portForward->disconnectSession(); // Call the explicit disconnect
                 // We don't reset portForward here, its destructor handles full cleanup later
            }
            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld] receiveData: SFTP and Port Forward cleanup calls completed.", threadId);
        } catch (const std::exception& e) {
             OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Exception during SFTP/PF cleanup: %{public}s", threadId, e.what());
        } catch (...) {
             OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld] receiveData: Unknown exception during SFTP/PF cleanup", threadId);
        }
    }
    // --------------------------------

    if (notify_needed) {
        pendingCallData("Connection closed or error occurred in receive thread", true);
    }
}

void inline SshSession::pendingCallData(std::string data, bool status) {
    // --- 新增检查：如果正在配置Shell，则阻止回调 ---
    if (isConfiguringShell.load()) {
        OH_LOG_INFO(LOG_APP, "pendingCallData: Suppressing callback during shell configuration. Data length: %zu, Status: %d", data.length(), status);
        return; // 在配置期间阻止数据发送
    }
    // --------------------------------------------

    // 直接调用回调函数，避免使用队列缓冲，减少延迟
    if (dataCallback) {
        try {
            dataCallback(sessionId, data, status); // 添加sessionId参数
            return;
        } catch (const std::exception& e) {
            OH_LOG_ERROR(LOG_APP, "直接调用数据回调异常: %s", e.what());
            // 如果直接调用失败，退回到队列方式
        } catch (...) {
            OH_LOG_ERROR(LOG_APP, "直接调用数据回调未知异常");
            // 如果直接调用失败，退回到队列方式
        }
    }

    // 如果没有回调或回调失败，使用队列
    std::lock_guard<std::mutex> lock(callBackDataMutex);
    dataCallBackList.emplace_back(std::move(data), status);
    callBackDataCV.notify_one();
}


void SshSession::callBackSendData() {

    std::unique_lock<std::mutex> lock(callBackDataMutex);

    while (!stopThread.load()) {

        callBackDataCV.wait(lock, [this] {
            return !dataCallBackList.empty() || stopThread.load();
        });

        if (stopThread.load()) {
            break;
        }

        if (!dataCallBackList.empty() && dataCallback) {
            auto &front = dataCallBackList.front();
            std::string data = std::move(front.first);
            bool status = std::move(front.second);
            dataCallBackList.pop_front();
            lock.unlock();

            dataCallback(sessionId, data, status); // 添加sessionId参数

            lock.lock();
        }
    }
    OH_LOG_INFO(LOG_APP, "callBackSendData线程正常退出");
}

// --- 新增：keepAliveThread方法实现 ---
void SshSession::keepAliveThread() {
    auto threadId = std::this_thread::get_id();
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] keepAliveThread 开始执行", threadId, sessionId);
    
    // 确保间隔大于0
    const bool idleCheckEnabled = keepAliveInterval > 0;
    int idleLimitSec = keepAliveInterval * 10;
    if (idleLimitSec < 600) {
        idleLimitSec = 600;
    }
    lastRxMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());

    int sendFailStreak = 0;
    int eofStreak = 0;

    // 记录开始时间
    auto startTime = std::chrono::steady_clock::now();

    auto markDead = [&](const char* reason) {
        OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld][Session %{public}d] %{public}s", threadId, sessionId, reason);
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            status = SessionStatus::DISCONNECTED;
        }
        pendingCallData("idle timeout", true);
        libssh2_socket_t deadSock = LIBSSH2_INVALID_SOCKET;
        {
            std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
            if (sock && *sock != LIBSSH2_INVALID_SOCKET) {
                deadSock = *sock;
            }
        }
        if (deadSock != LIBSSH2_INVALID_SOCKET) {
            shutdown(deadSock, SHUT_RDWR);
            OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld][Session %{public}d] 已关闭失效连接的 socket", threadId, sessionId);
        }
    };
    
    while (!stopThread.load()) {
        // 检查会话是否连接
        SessionStatus currentStatus;
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            currentStatus = status;
        }
        
        if (currentStatus != SessionStatus::CONNECTED) {
            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] 会话未连接，keepAliveThread 退出", 
                       threadId, sessionId);
            break;
        }

        if (idleCheckEnabled) {
            int pi = probeInbound(0);
            if (pi > 0) {
                lastRxMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            }
            const long long last = lastRxMs.load();
            const long long idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() - last;
            if (last > 0 && idleMs > (long long)idleLimitSec * 1000) {
                char reason[160];
                snprintf(reason, sizeof(reason), "空闲 %d 秒超过阈值 %d 秒且无任何入站字节，判定连接失效",
                         (int)(idleMs / 1000), idleLimitSec);
                markDead(reason);
                break;
            }
        }

        // 定期发送 keepalive 消息
        int next_time = keepAliveInterval > 0 ? keepAliveInterval : 30;
        bool sendDead = false;
        if (keepAliveInterval > 0) {
            std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
            if (session) {
                int rc = libssh2_keepalive_send(session.get(), &next_time);
                
                if (rc == 0) {
                    sendFailStreak = 0;
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] Keepalive 消息已发送，下次发送时间: %{public}d秒后", 
                               threadId, sessionId, next_time);
                } else if (rc == LIBSSH2_ERROR_EAGAIN || rc == LIBSSH2_ERROR_SOCKET_SEND || rc == LIBSSH2_ERROR_SOCKET_TIMEOUT) {
                    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] Keepalive 暂不可发送 (errno=%{public}d)，稍后再试", 
                               threadId, sessionId, rc);
                } else {
                    ++sendFailStreak;
                    OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld][Session %{public}d] Keepalive 发送失败，错误码: %{public}d，连续 %{public}d 次", 
                                threadId, sessionId, rc, sendFailStreak);
                    if (sendFailStreak >= 3) {
                        sendDead = true;
                    }
                }
            } else {
                OH_LOG_ERROR(LOG_APP, "[Thread %{public}ld][Session %{public}d] 会话已失效，keepAliveThread 退出", 
                            threadId, sessionId);
                sendDead = true;
            }
        }
        if (sendDead) {
            markDead("keepalive 连续发送失败，判定连接失效");
            break;
        }
        
        // 更新运行时间统计（可选）
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - startTime).count();
        
        // 每5分钟记录一次日志（避免日志过多）
        if (elapsedSeconds % 300 == 0) {
            OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] keepAliveThread 已运行 %{public}ld 秒", 
                       threadId, sessionId, elapsedSeconds);
        }
        
        // 使用libssh2返回的next_time作为实际休眠时间
        int sleepTime = next_time;
        
        // 安全检查: 确保休眠时间合理
        if (sleepTime <= 0) {
            sleepTime = 1; // 至少休眠1秒
            OH_LOG_WARN(LOG_APP, "[Thread %{public}ld][Session %{public}d] 无效的next_time值(%{public}d)，使用最小值1秒", 
                       threadId, sessionId, next_time);
        } else if (keepAliveInterval > 0 && sleepTime > keepAliveInterval) {
            sleepTime = keepAliveInterval; // 不超过配置的最大间隔
            OH_LOG_WARN(LOG_APP, "[Thread %{public}ld][Session %{public}d] next_time值(%{public}d)超过keepAliveInterval，使用%{public}d秒", 
                       threadId, sessionId, next_time, keepAliveInterval);
        }
        
        OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] keepAliveThread 将休眠 %{public}d 秒", 
                   threadId, sessionId, sleepTime);
        
        // 分段休眠，便于快速响应停止信号
        bool eofDead = false;
        for (int i = 0; i < sleepTime && !stopThread.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!idleCheckEnabled) {
                continue;
            }
            int pi = probeInbound(0);
            if (pi > 0) {
                eofStreak = 0;
                lastRxMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            } else if (pi < 0) {
                if (++eofStreak >= 3) {
                    eofDead = true;
                    break;
                }
            } else {
                eofStreak = 0;
            }
        }
        if (eofDead) {
            markDead("socket 连续返回 EOF/无效，判定连接失效");
            break;
        }
    }
    
    OH_LOG_INFO(LOG_APP, "[Thread %{public}ld][Session %{public}d] keepAliveThread 正常退出", threadId, sessionId);
}
// --- 新增结束 ---

bool SshSession::initSftp() {
    std::lock_guard<std::recursive_mutex> lock(libssh2Mutex); // 确保在访问 session 时加锁
    if (!session) {
        OH_LOG_ERROR(LOG_APP, "SSH 会话未初始化，无法初始化 SFTP");
        return false;
    }

    OH_LOG_INFO(LOG_APP, "开始并行初始化 SFTP 会话...");

    // 销毁旧的SFTP会话（如果存在）
    if (interactiveSftpSession) {
        interactiveSftpSession.reset();
    }
    if (transferSftpSession) {
        transferSftpSession.reset();
    }

    SftpConfig config = getSftpConfig(); // 获取一次配置

    // 用于存储连接结果和临时会话对象
    std::unique_ptr<SftpSession> tempInteractiveSession = nullptr;
    std::unique_ptr<SftpSession> tempTransferSession = nullptr;
    std::atomic<bool> interactiveSuccess{false};
    std::atomic<bool> transferSuccess{false};
    std::string interactiveErrorMsg;
    std::string transferErrorMsg;
    std::mutex errorMutex; // 用于保护错误消息写入

    // --- 启动交互式 SFTP 会话连接线程 ---
    std::thread interactiveThread([&]() {
        OH_LOG_INFO(LOG_APP, "交互式 SFTP 连接线程启动");
        try {
            // Use a temporary unique_ptr within the thread
            auto sftpSession = std::make_unique<SftpSession>(config, true); // true表示非阻塞
            if (sftpSession->connect()) { // Check connection result
                // If connect succeeds, move ownership to the temporary variable captured by the outer scope
                tempInteractiveSession = std::move(sftpSession);
                interactiveSuccess.store(true);
                OH_LOG_INFO(LOG_APP, "交互式 SFTP 会话连接成功 (线程内)");
            } else {
                // Connection failed, log error but don't store the session
                std::lock_guard<std::mutex> lock(errorMutex);
                interactiveErrorMsg = "交互式 SFTP 会话连接失败 (connect 返回 false)";
                OH_LOG_ERROR(LOG_APP, "%s", interactiveErrorMsg.c_str());
                // interactiveSuccess remains false
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex);
            interactiveErrorMsg = std::string("创建/连接交互式 SFTP 会话异常: ") + e.what(); // More specific message
            OH_LOG_ERROR(LOG_APP, "%s", interactiveErrorMsg.c_str());
            // interactiveSuccess remains false
        } catch (...) {
            std::lock_guard<std::mutex> lock(errorMutex);
            interactiveErrorMsg = "创建/连接交互式 SFTP 会话发生未知异常";
            OH_LOG_ERROR(LOG_APP, "%s", interactiveErrorMsg.c_str());
            // interactiveSuccess remains false
        }
         OH_LOG_INFO(LOG_APP, "交互式 SFTP 连接线程结束");
    });

    // --- 启动传输 SFTP 会话连接线程 ---
    std::thread transferThread([&]() {
         OH_LOG_INFO(LOG_APP, "传输 SFTP 连接线程启动");
        try {
            // Use a temporary unique_ptr within the thread
            auto sftpSession = std::make_unique<SftpSession>(config, true); // true表示非阻塞
            if (sftpSession->connect()) { // Check connection result
                // If connect succeeds, move ownership
                tempTransferSession = std::move(sftpSession);
                transferSuccess.store(true);
                OH_LOG_INFO(LOG_APP, "传输 SFTP 会话连接成功 (线程内)");
            } else {
                // Connection failed, log error
                std::lock_guard<std::mutex> lock(errorMutex);
                transferErrorMsg = "传输 SFTP 会话连接失败 (connect 返回 false)";
                 OH_LOG_ERROR(LOG_APP, "%s", transferErrorMsg.c_str());
                 // transferSuccess remains false
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex);
            transferErrorMsg = std::string("创建/连接传输 SFTP 会话异常: ") + e.what(); // More specific message
            OH_LOG_ERROR(LOG_APP, "%s", transferErrorMsg.c_str());
             // transferSuccess remains false
        } catch (...) {
            std::lock_guard<std::mutex> lock(errorMutex);
            transferErrorMsg = "创建/连接传输 SFTP 会话发生未知异常";
            OH_LOG_ERROR(LOG_APP, "%s", transferErrorMsg.c_str());
             // transferSuccess remains false
        }
         OH_LOG_INFO(LOG_APP, "传输 SFTP 连接线程结束");
    });

    // --- 等待两个线程完成 ---
    OH_LOG_INFO(LOG_APP, "等待 SFTP 连接线程完成...");
    interactiveThread.join();
    transferThread.join();
    OH_LOG_INFO(LOG_APP, "SFTP 连接线程已全部完成");

    // --- 处理结果 --- (Modified logic)
    bool finalInteractiveSuccess = interactiveSuccess.load();
    bool finalTransferSuccess = transferSuccess.load();

    // Move successfully connected sessions to member variables
    if (finalInteractiveSuccess) {
        interactiveSftpSession = std::move(tempInteractiveSession);
        OH_LOG_INFO(LOG_APP, "交互式 SFTP 会话已成功设置.");
    } else {
        interactiveSftpSession.reset(); // Ensure it's null if failed
        std::lock_guard<std::mutex> lock(errorMutex); // Lock needed to read error message safely
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话初始化失败: %s", interactiveErrorMsg.empty() ? "未知错误" : interactiveErrorMsg.c_str());
    }

    if (finalTransferSuccess) {
        transferSftpSession = std::move(tempTransferSession);
        OH_LOG_INFO(LOG_APP, "传输 SFTP 会话已成功设置.");
    } else {
        transferSftpSession.reset(); // Ensure it's null if failed
        std::lock_guard<std::mutex> lock(errorMutex); // Lock needed to read error message safely
        OH_LOG_ERROR(LOG_APP, "传输 SFTP 会话初始化失败: %s", transferErrorMsg.empty() ? "未知错误" : transferErrorMsg.c_str());
    }

    // Always return true to indicate the initialization *attempt* finished, even if partially failed.
    // The caller (openConnect) should check if the pointers are valid.
    OH_LOG_INFO(LOG_APP, "initSftp 尝试完成 (Interactive: %{public}d, Transfer: %{public}d). 将返回 true.", finalInteractiveSuccess, finalTransferSuccess);
    return true;
    // ---------------------

    // Original logic commented out:
    /*
    if (finalInteractiveSuccess && finalTransferSuccess) {
        // 两者都成功，将临时指针移动到成员变量
        OH_LOG_INFO(LOG_APP, "两个 SFTP 会话均已成功初始化");
        interactiveSftpSession = std::move(tempInteractiveSession);
        transferSftpSession = std::move(tempTransferSession);
        return true;
    } else {
        // ... (original error logging) ...
        return false; // 整体初始化失败
    }
    */
}

bool SshSession::listDirectory(const std::string& path, std::function<void(const FileInfo&)> callback) {
    OH_LOG_INFO(LOG_APP, "SshSession::listDirectory 开始执行，路径: %{public}s", path.c_str());
    // 会话未就绪时先尝试重建
    if (!interactiveSftpSession || !interactiveSftpSession->isValid()) {
        OH_LOG_WARN(LOG_APP, "交互式 SFTP 会话未就绪，尝试重新初始化");
        if (!initSftp() || !interactiveSftpSession || !interactiveSftpSession->isValid()) {
            OH_LOG_ERROR(LOG_APP, "重新初始化交互式 SFTP 会话失败");
            return false;
        }
    }

    // 首次调用，捕获异常消息（包含 SFTP Code）
    std::string firstError;
    try {
        bool ok = interactiveSftpSession->listDirectory(path, callback);
        if (ok) return true;
    } catch (const std::exception& e) {
        firstError = e.what();
        OH_LOG_WARN(LOG_APP, "listDirectory 首次异常: %s，尝试重建并重试", firstError.c_str());
    } catch (...) {
        firstError = "Unknown error";
        OH_LOG_WARN(LOG_APP, "listDirectory 首次异常(未知)，尝试重建并重试");
    }

    // 重建并重试一次
    if (!initSftp() || !interactiveSftpSession || !interactiveSftpSession->isValid()) {
        OH_LOG_ERROR(LOG_APP, "listDirectory 重建 SFTP 失败");
        if (!firstError.empty()) throw std::runtime_error(firstError);
        return false;
    }
    try {
        bool ok2 = interactiveSftpSession->listDirectory(path, callback);
        if (!ok2 && !firstError.empty()) throw std::runtime_error(firstError);
        return ok2;
    } catch (...) {
        // 透传异常，便于 N-API 提取 SFTP Code（如权限不足=3）
        throw;
    }
}

bool SshSession::uploadFile(int localFd, const std::string& remotePath, std::function<void(int64_t)> progressCallback) {
    if (!transferSftpSession) {
        OH_LOG_ERROR(LOG_APP, "传输 SFTP 会话未初始化 (Transfer SFTP session not initialized)");
        return false;
    }

    bool result = transferSftpSession->uploadFile(localFd, remotePath, progressCallback);

    return result;
}

void SshSession::setUploadThrottle(int64_t bps) {
    if (!transferSftpSession) return;
    transferSftpSession->setThrottleBps(bps);
}

std::string SshSession::getLastUploadError() const {
    if (!transferSftpSession) {
        return std::string();
    }
    return transferSftpSession->getLastUploadError();
}

bool SshSession::downloadFile(const std::string& remotePath, int localFd, std::function<void(int64_t)> progressCallback) {
    OH_LOG_INFO(LOG_APP, "SshSession::downloadFile called for %s to fd %d", remotePath.c_str(), localFd);
    if (!transferSftpSession) {
        OH_LOG_ERROR(LOG_APP, "传输 SFTP 会话未初始化 (Transfer SFTP session not initialized)");
        return false;
    }

    // 直接将字节回调传递给 SftpSession
    bool result = transferSftpSession->downloadFile(remotePath, localFd, progressCallback);
    OH_LOG_INFO(LOG_APP, "SshSession::downloadFile forwarding result: %d", result);

    return result;
}

bool SshSession::deleteFile(const std::string& path) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }

    bool result = interactiveSftpSession->deleteFile(path);

    return result;
}

bool SshSession::realPath(const std::string& path, std::string& out) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }
    return interactiveSftpSession->realPath(path, out);
}

bool SshSession::createDirectory(const std::string& path) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }

    bool result = interactiveSftpSession->createDirectory(path);

    return result;
}

bool SshSession::deleteDirectory(const std::string& path) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }

    bool result = interactiveSftpSession->deleteDirectory(path);

    return result;
}

bool SshSession::rename(const std::string& oldPath, const std::string& newPath) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }

    bool result = interactiveSftpSession->rename(oldPath, newPath);

    return result;
}

// 设置文件时间
bool SshSession::setFileTime(const std::string& path, uint64_t mtime, uint64_t atime) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }
    return interactiveSftpSession->setFileTime(path, mtime, atime);
}

// 获取文件信息
bool SshSession::getFileInfo(const std::string& path, FileInfo& info) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }
    return interactiveSftpSession->getFileInfo(path, info);
}

// 设置文件权限
bool SshSession::setPermissions(const std::string& path, uint32_t permissions) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return false;
    }
    return interactiveSftpSession->setPermissions(path, permissions);
}

// 获取权限字符串
std::string SshSession::getPermissionString(uint32_t permissions) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return "";
    }
    return interactiveSftpSession->getPermissionString(permissions);
}

// 获取时间字符串
std::string SshSession::getTimeString(uint64_t timestamp) {
    if (!interactiveSftpSession) {
        OH_LOG_ERROR(LOG_APP, "交互式 SFTP 会话未初始化 (Interactive SFTP session not initialized)");
        return "";
    }
    return interactiveSftpSession->getTimeString(timestamp);
}

// 端口转发相关方法实现 - 转发给portForward对象
bool SshSession::startLocalPortForwarding(int localPort, const std::string& targetHost, int targetPort, bool anyInterface) {
    // --- Lazy Initialization Check ---
    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "Cannot start local forward: Port Forwarding Manager not initialized.");
        return false;
    }
    if (!portForward->isPfConnected()) {
        OH_LOG_INFO(LOG_APP, "startLocalPortForwarding: PF session not connected, attempting to connect...");
        if (!portForward->connectSession()) {
            OH_LOG_ERROR(LOG_APP, "Cannot start local forward: Failed to establish dedicated PF session.");
            return false;
        }
        OH_LOG_INFO(LOG_APP, "startLocalPortForwarding: Dedicated PF session established.");
    }
    // --- End Lazy Check ---

    // Original logic:
    // if (!portForward) {
    //     OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
    //     return false;
    // }
    return portForward->startLocalPortForwarding(localPort, targetHost, targetPort, anyInterface);
}

bool SshSession::startRemotePortForwarding(int remotePort, const std::string& targetHost, int targetPort) {
    // --- Lazy Initialization Check ---
    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "Cannot start remote forward: Port Forwarding Manager not initialized.");
        return false;
    }
    if (!portForward->isPfConnected()) {
        OH_LOG_INFO(LOG_APP, "startRemotePortForwarding: PF session not connected, attempting to connect...");
        if (!portForward->connectSession()) {
            OH_LOG_ERROR(LOG_APP, "Cannot start remote forward: Failed to establish dedicated PF session.");
            return false;
        }
        OH_LOG_INFO(LOG_APP, "startRemotePortForwarding: Dedicated PF session established.");
    }
    // --- End Lazy Check ---

    // Original logic:
    // if (!portForward) {
    //     OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
    //     return false;
    // }
    return portForward->startRemotePortForwarding(remotePort, targetHost, targetPort);
}

bool SshSession::startDynamicPortForwarding(int localPort, bool anyInterface) {
    // --- Lazy Initialization Check ---
    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "Cannot start dynamic forward: Port Forwarding Manager not initialized.");
        return false;
    }
    if (!portForward->isPfConnected()) {
        OH_LOG_INFO(LOG_APP, "startDynamicPortForwarding: PF session not connected, attempting to connect...");
        if (!portForward->connectSession()) {
            OH_LOG_ERROR(LOG_APP, "Cannot start dynamic forward: Failed to establish dedicated PF session.");
            return false;
        }
        OH_LOG_INFO(LOG_APP, "startDynamicPortForwarding: Dedicated PF session established.");
    }
    // --- End Lazy Check ---

    // Original logic:
    // if (!portForward) {
    //     OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
    //     return false;
    // }
    return portForward->startDynamicPortForwarding(localPort, anyInterface);
}

bool SshSession::stopPortForwarding(int port, bool isRemote) {
    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
        return false;
    }
    return portForward->stopPortForwarding(port, isRemote);
}

bool SshSession::isPortForwardingActive(int port, bool isRemote) {
    // --- Add check for session status --- 
    SessionStatus currentStatus;
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        currentStatus = status;
    }
    if (currentStatus != SessionStatus::CONNECTED) {
        OH_LOG_WARN(LOG_APP, "isPortForwardingActive called on non-connected session (status=%{public}d). Returning false.", static_cast<int>(currentStatus));
        return false; // Return false if not connected
    }
    // ------------------------------------

    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
        return false;
    }
    return portForward->isPortForwardingActive(port, isRemote);
}

std::vector<std::tuple<int, std::string, int, bool, bool>> SshSession::listActivePortForwardings() {
    // --- Add check for session status --- 
    SessionStatus currentStatus;
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        currentStatus = status;
    }
    if (currentStatus != SessionStatus::CONNECTED) {
        OH_LOG_WARN(LOG_APP, "listActivePortForwardings called on non-connected session (status=%{public}d). Returning empty list.", static_cast<int>(currentStatus));
        return {}; // Return empty vector if not connected
    }
    // ------------------------------------

    if (!portForward) {
        OH_LOG_ERROR(LOG_APP, "端口转发未初始化");
        return {};
    }
    return portForward->listActivePortForwardings();
}

int SshSession::getForwardFailures() {
    if (!portForward) {
        return 0;
    }
    return portForward->forwardFailures();
}

int SshSession::getForwardAccepts() {
    if (!portForward) {
        return 0;
    }
    return portForward->forwardAccepts();
}

int SshSession::getForwardFailuresOf(int port) {
    if (!portForward) {
        return 0;
    }
    return portForward->forwardFailuresOf(port);
}

int SshSession::getForwardAcceptsOf(int port) {
    if (!portForward) {
        return 0;
    }
    return portForward->forwardAcceptsOf(port);
}

// --- 恢复 setLogging 实现 ---
bool SshSession::setLogging(int fd, int logType, int existOperation, bool includeHeader, bool omitKnownPassword, bool omitSessionData) {
    OH_LOG_INFO(LOG_APP, "SshSession::setLogging called with fd=%{public}d, logType=%{public}d, existOperation=%{public}d, includeHeader=%{public}d, omitKnownPassword=%{public}d, omitSessionData=%{public}d", 
        fd, logType, existOperation, includeHeader, omitKnownPassword, omitSessionData);
    bool success = false;
    bool loggingEnabled = false;

    try {
        // 根据参数直接初始化或禁用 LogManager
        if (logType == 0 || fd < 0) {
            OH_LOG_INFO(LOG_APP, "setLogging: 参数指示禁用日志，调用 logManager.disable()");
            logManager.disable();
            loggingEnabled = false; // 标记日志为禁用
        } else {
            OH_LOG_INFO(LOG_APP, "setLogging: 参数指示启用日志，调用 logManager.init()");
            logManager.init(fd, logType, existOperation, includeHeader, omitKnownPassword, omitSessionData);
            loggingEnabled = true; // 标记日志为启用
        }
        OH_LOG_INFO(LOG_APP, "LogManager 配置完成 (通过 setLogging)。");
        success = true;

        // --- 处理初始缓冲区 ---
        std::string bufferedData;
        {
            std::lock_guard<std::mutex> bufferLock(bufferMutex);
            // 只有在日志实际启用时才写入缓冲区
            if (loggingEnabled && !initialBuffer.empty()) {
                OH_LOG_INFO(LOG_APP, "setLogging: LogManager 初始化完成，开始写入 %zu 字节的初始缓冲区数据...", initialBuffer.length());
                bufferedData = std::move(initialBuffer); // 移动数据以减少拷贝
                initialBuffer.clear(); // 确保清空
            } else if (!initialBuffer.empty()) {
                 OH_LOG_INFO(LOG_APP, "setLogging: LogManager 已禁用或缓冲区为空，丢弃初始缓冲区数据。");
                 initialBuffer.clear(); // 如果日志被禁用，也清空缓冲区
            }
            // --- 设置初始化标志 ---
            logManagerInitialized.store(true);
            OH_LOG_INFO(LOG_APP, "setLogging: logManagerInitialized 标志设置为 true.");
        } // 释放 bufferMutex

        // 在锁外部写入数据，避免长时间持有锁
        if (loggingEnabled && !bufferedData.empty()) {
            try {
                logManager.writeLog(bufferedData.c_str(), bufferedData.length());
                OH_LOG_INFO(LOG_APP, "setLogging: 初始缓冲区数据写入完成。");
            } catch(const std::exception& writeEx) {
                 OH_LOG_ERROR(LOG_APP, "setLogging: 写入初始缓冲区数据时发生异常: %{public}s", writeEx.what());
                 // success 已经为 true，这里只记录错误
            } catch(...) {
                 OH_LOG_ERROR(LOG_APP, "setLogging: 写入初始缓冲区数据时发生未知异常。");
            }
        }
        // --------------------

        return success; // 返回 setLogging 操作本身的成功状态

    } catch (const std::exception& e) {
        OH_LOG_ERROR(LOG_APP, "配置 LogManager 失败: %{public}s", e.what());
        // 尝试在错误时禁用
        try {
            logManager.disable();
        } catch(...) { /*忽略禁用时的错误*/ }
         // 即使配置失败，也要标记初始化完成（虽然是失败的初始化），避免 receiveData 永远缓冲
         logManagerInitialized.store(true);
         OH_LOG_WARN(LOG_APP, "setLogging: 因异常设置 logManagerInitialized 标志为 true，但日志可能未启用。");
        return false; // 返回 setLogging 失败
    }
}
// ----------------------------

bool SshSession::resizePty(int width, int height) {
    // 使用带标签的日志记录宽度和高度
    OH_LOG_INFO(LOG_APP, "尝试将 PTY 大小调整为 宽度=%{public}d, 高度=%{public}d", width, height);

    // 检查宽度和高度是否有效
    if (width <= 0 || height <= 0) {
        OH_LOG_ERROR(LOG_APP, "调整 PTY 大小失败：无效的尺寸（宽度=%{public}d, 高度=%{public}d）", width, height);
        return false;
    }

    // 使用递归互斥锁保护对 libssh2 资源的访问
    std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
    OH_LOG_INFO(LOG_APP, "调整 PTY 大小：已获取 libssh2Mutex");

    // 检查会话和 Shell 通道是否有效
    if (!session || !channelShell) {
        OH_LOG_ERROR(LOG_APP, "调整 PTY 大小失败：会话或 Shell 通道无效。");
        OH_LOG_INFO(LOG_APP, "调整 PTY 大小：释放 libssh2Mutex"); // 在返回前记录锁的释放
        return false;
    }
    OH_LOG_INFO(LOG_APP, "调整 PTY 大小：会话和 Shell 通道有效");

    int rc;
    int attempts = 0;
    const int maxAttempts = 20; // 限制重试次数，防止无限循环

    // 循环调用 libssh2_channel_request_pty_size，处理 EAGAIN 错误
    OH_LOG_INFO(LOG_APP, "调整 PTY 大小：进入 libssh2_channel_request_pty_size 循环");
    while (attempts < maxAttempts) {
        rc = libssh2_channel_request_pty_size(channelShell.get(), width, height);
        OH_LOG_INFO(LOG_APP, "调整 PTY 大小：libssh2_channel_request_pty_size 返回 %{public}d (尝试次数 %{public}d/%{public}d)", rc, attempts + 1, maxAttempts);

        if (rc == LIBSSH2_ERROR_EAGAIN) {
            OH_LOG_INFO(LOG_APP, "调整 PTY 大小：收到 EAGAIN，等待 Socket...");
            int ws_rc = waitSocket(); // 等待 Socket 状态变化
            OH_LOG_INFO(LOG_APP, "调整 PTY 大小：waitSocket 返回 %{public}d", ws_rc);
            if (ws_rc <= 0) {
                // 如果 waitSocket 超时或出错，增加尝试次数并短暂休眠
                attempts++;
                OH_LOG_ERROR(LOG_APP, "调整 PTY 大小失败：waitSocket 在 EAGAIN 期间超时或出错（尝试次数 %{public}d/%{public}d）。将休眠 50ms 后重试。", attempts, maxAttempts);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                 // 可以考虑在这里直接返回 false，避免长时间阻塞
                 // return false;
            } else {
                 OH_LOG_INFO(LOG_APP, "调整 PTY 大小：waitSocket 就绪，立即重试请求");
                 // 不需要增加 attempts 或休眠，立即重试
            }
        } else {
            // 如果不是 EAGAIN，则跳出循环
            break;
        }
    } // while 循环结束

    OH_LOG_INFO(LOG_APP, "调整 PTY 大小：退出 libssh2_channel_request_pty_size 循环");

    // 检查最终结果
    if (rc != 0) {
        int last_errno = libssh2_session_last_errno(session.get());
        char *errmsg = nullptr;
        int errmsg_len = 0;
        libssh2_session_last_error(session.get(), &errmsg, &errmsg_len, 0);
        OH_LOG_ERROR(LOG_APP, "调整 PTY 大小最终失败，rc=%{public}d, libssh2_errno=%{public}d, 消息: %{public}s", rc, last_errno, errmsg ? errmsg : "N/A");
        OH_LOG_INFO(LOG_APP, "调整 PTY 大小：释放 libssh2Mutex"); // 在返回前记录锁的释放
        return false;
    }

    OH_LOG_INFO(LOG_APP, "PTY 大小调整成功。");
    OH_LOG_INFO(LOG_APP, "调整 PTY 大小：释放 libssh2Mutex"); // 在返回前记录锁的释放
    return true;
}

// --- 新增: 取消传输实现 ---
bool SshSession::cancelTransfer() {
    OH_LOG_INFO(LOG_APP, "SshSession::cancelTransfer called.");
    bool requestSent = false;

    // 主要取消传输会话上的操作
    // 使用锁确保线程安全地访问和操作 transferSftpSession
    std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
    if (transferSftpSession) {
        OH_LOG_INFO(LOG_APP, "Requesting cancel on transfer SFTP session.");
        transferSftpSession->requestCancel(); // 调用 SftpSession 的取消方法
        requestSent = true; // 标记请求已发送
    } else {
        OH_LOG_WARN(LOG_APP, "Cannot cancel transfer: transfer SFTP session is not initialized.");
    }

    // 可选：如果需要，也可以对 interactiveSftpSession 发送取消请求
    // if (interactiveSftpSession) {
    //     OH_LOG_INFO(LOG_APP, "Requesting cancel on interactive SFTP session.");
    //     interactiveSftpSession->requestCancel();
    //     requestSent = true; // 如果任一请求发送成功，则标记为 true
    // }

    return requestSent; // 返回是否至少向一个 SFTP 会话发送了取消请求
}
// -------------------------

bool SshSession::pauseTransfer(bool on) {
    OH_LOG_INFO(LOG_APP, "SshSession::pauseTransfer called: %{public}d", on ? 1 : 0);
    bool sent = false;
    std::lock_guard<std::recursive_mutex> lock(libssh2Mutex);
    if (transferSftpSession) {
        transferSftpSession->pauseTransfer(on);
        sent = true;
    }
    if (interactiveSftpSession) {
        interactiveSftpSession->pauseTransfer(on);
        sent = true;
    }
    return sent;
}

// 实现parseOSC7Sequence方法 (手动解析版本)
void SshSession::parseOSC7Sequence(const std::string& data) {
    size_t startPos = 0;
    const std::string prefix = OSC7_PREFIX;
    const std::string suffix = OSC7_SUFFIX;
    while ((startPos = data.find(prefix, startPos)) != std::string::npos) {
        // 找到 OSC7 前缀
        startPos += prefix.length(); // 移动到前缀之后

        // 查找结束符 BEL ()
        size_t endPos = data.find(suffix, startPos);
        if (endPos == std::string::npos) {
            // 未找到结束符，可能序列不完整，停止解析
            OH_LOG_WARN(LOG_APP, "parseOSC7Sequence: 未找到结束符，序列可能不完整");
            break;
        }

        // 提取 file://hostname/path 部分
        std::string uri = data.substr(startPos, endPos - startPos);

        // 跳过 hostname，提取路径
        // 查找第一个 '/' 作为路径的开始
        size_t pathStartPos = uri.find('/');
        if (pathStartPos != std::string::npos) {
            std::string newPath = uri.substr(pathStartPos);

            // (可选) URL 解码 - 简单的 %XX 解码
            std::string decodedPath;
            decodedPath.reserve(newPath.length()); // 预分配空间
            for (size_t i = 0; i < newPath.length(); ++i) {
                if (newPath[i] == '%' && i + 2 < newPath.length()) {
                    try {
                        std::string hex = newPath.substr(i + 1, 2);
                        char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
                        decodedPath += decodedChar;
                        i += 2;
                    } catch (const std::invalid_argument& e) {
                        // 无效的 % 序列，按原样添加
                        decodedPath += newPath[i];
                        OH_LOG_WARN(LOG_APP, "parseOSC7Sequence: 无效的URL编码序列 %s", newPath.substr(i, 3).c_str());
                    } catch (const std::out_of_range& e) {
                         // 转换超出范围
                        decodedPath += newPath[i];
                        OH_LOG_WARN(LOG_APP, "parseOSC7Sequence: URL编码转换超出范围 %s", newPath.substr(i, 3).c_str());
                    }
                } else {
                    decodedPath += newPath[i];
                }
            }


            // 如果目录有变化，更新当前目录并通知回调
            if (currentDirectory != decodedPath) {
                OH_LOG_INFO(LOG_APP, "检测到目录变更: %s", decodedPath.c_str());
                currentDirectory = decodedPath;

                // 如果设置了回调，通知目录变更
                if (directoryChangeCallback) {
                    try {
                       directoryChangeCallback(currentDirectory);
                    } catch (const std::exception& e) {
                       OH_LOG_ERROR(LOG_APP, "执行目录变更回调时发生异常: %s", e.what());
                    } catch (...) {
                       OH_LOG_ERROR(LOG_APP, "执行目录变更回调时发生未知异常");
                    }
                }
            }
        } else {
             OH_LOG_WARN(LOG_APP, "parseOSC7Sequence: 在URI中未找到路径分隔符 '/' (%s)", uri.c_str());
        }

        // 从结束符之后继续搜索
        startPos = endPos + suffix.length();
    }
}

// 实现enableDirectoryTracking方法
bool SshSession::enableDirectoryTracking(bool enable) {
    OH_LOG_INFO(LOG_APP, "enableDirectoryTracking: %d", enable);
    
    // 如果之前没有启用但现在要启用，发送配置命令
    if (!followTerminalDirectory && enable) {
        bool result = configureRemoteShell();
        if (!result) {
            OH_LOG_ERROR(LOG_APP, "Failed to configure remote shell for directory tracking");
            return false;
        }
    }
    
    followTerminalDirectory = enable;
    return true;
}

// 实现setDirectoryChangeCallback方法
void SshSession::setDirectoryChangeCallback(std::function<void(const std::string&)> callback) {
    directoryChangeCallback = std::move(callback);
}

// 实现configureRemoteShell方法
bool SshSession::configureRemoteShell() {
    // --- 使用 RAII 控制配置标志 ---
    ShellConfigGuard configGuard(isConfiguringShell);
    // ---------------------------
    OH_LOG_INFO(LOG_APP, "configureRemoteShell: Configuring remote shell for OSC7 directory tracking");

    if (!channelShell || status != SessionStatus::CONNECTED) {
        OH_LOG_ERROR(LOG_APP, "configureRemoteShell: Not connected or no shell channel");
        return false;
    }

    // 更加简洁的方法：使用bash -c执行未记录命令
    const char* bash_command = 
        "{\n"
        "HISTFILE_TMP=$HISTFILE; HISTSIZE_TMP=$HISTSIZE; HISTCONTROL_TMP=$HISTCONTROL;\n"
        "unset HISTFILE; HISTSIZE=0; HISTCONTROL=ignoreboth; stty -echo 2>/dev/null;\n"
        "cat > /tmp/osc7_config.$$ << 'EOFMARKER'\n"
        "#!/bin/bash\n"
        "# 配置OSC7目录跟踪\n"
        "if [ -n \"$BASH_VERSION\" ]; then\n"
        "  # Bash配置\n"
        "  PROMPT_COMMAND=\"${PROMPT_COMMAND:+$PROMPT_COMMAND; }printf '\\033]7;file://%s%s\\007' \\\"\\${HOSTNAME:-$(hostname)}\\\" \\\"\\${PWD/#$HOME/~}\\\"\"\n"
        "elif [ -n \"$ZSH_VERSION\" ]; then\n"
        "  # Zsh配置\n"
        "  precmd() { printf '\\033]7;file://%s%s\\007' \\\"\\${HOST}\\\" \\\"\\${PWD}\\\" ; }\n"
        "fi\n"
        "# 发送初始目录\n"
        "printf '\\033]7;file://%s%s\\007' \"${HOSTNAME:-$(hostname)}\" \"${PWD}\"\n"
        "EOFMARKER\n"
        "chmod +x /tmp/osc7_config.$$;\n"
        "source /tmp/osc7_config.$$ >/dev/null 2>&1;\n"
        "rm -f /tmp/osc7_config.$$;\n"
        "stty echo 2>/dev/null;\n"
        "export HISTFILE=$HISTFILE_TMP; export HISTSIZE=$HISTSIZE_TMP; export HISTCONTROL=$HISTCONTROL_TMP;\n"
        "unset HISTFILE_TMP HISTSIZE_TMP HISTCONTROL_TMP;\n"
        "} 2>/dev/null\n";

    // 使用一行命令发送所有内容
    OH_LOG_INFO(LOG_APP, "configureRemoteShell: Sending one-line configuration script...");
    if (!sendData(bash_command)) {
        OH_LOG_ERROR(LOG_APP, "configureRemoteShell: Failed to send configuration");
        return false;
    }

    // 等待脚本执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    OH_LOG_INFO(LOG_APP, "configureRemoteShell: Configuration completed via special script.");
    
    return true;
}

// --- 新增：实现 resizeTerminal override ---
bool SshSession::resizeTerminal(int width, int height) {
    // 直接调用内部的 pty 调整方法
    return this->resizePty(width, height);
}

std::string SshSession::getCipherName() {
    std::lock_guard<std::recursive_mutex> lk(libssh2Mutex);
    if (!session) return std::string();
    const char* m = libssh2_session_methods(session.get(), LIBSSH2_METHOD_CRYPT_CS);
    return m ? std::string(m) : std::string();
}

std::string SshSession::getMacName() {
    std::lock_guard<std::recursive_mutex> lk(libssh2Mutex);
    if (!session) return std::string();
    const char* m = libssh2_session_methods(session.get(), LIBSSH2_METHOD_MAC_CS);
    return m ? std::string(m) : std::string();
}
