#ifndef PROXY_CLIENT_H
#define PROXY_CLIENT_H

#include <string>
#include "proxyConfig.h"

// 代理错误码（与 Layer3 错误提示映射）
enum ProxyError {
    PROXY_OK = 0,
    PROXY_ERR_GENERAL = -1,     // 通用错误（未启用/配置非法）
    PROXY_ERR_SOCKET = -2,      // 创建 Socket 失败
    PROXY_ERR_CONNECT = -3,     // 连接代理服务器失败
    PROXY_ERR_HANDSHAKE = -4,   // 代理协议握手失败
    PROXY_ERR_AUTH = -5,        // 代理鉴权失败（SOCKS 0x02 被拒 / HTTP 407）
    PROXY_ERR_TIMEOUT = -6,     // 连接/握手超时
    PROXY_ERR_REJECTED = -7,    // 代理拒绝连接目标（SOCKS REP!=0 / HTTP 非 200）
    PROXY_ERR_CONNREFUSED = -8, // 连接代理被拒绝（ECONNREFUSED，端口无服务监听）
};

// 统一入口：经代理建立到 targetHost:targetPort 的隧道。
// 成功返回已连通（阻塞模式）fd，供 libssh2/Telnet 等协议层直接使用；
// 失败返回 -1（可通过 getLastProxyError() 查看原因）。
int connectViaProxy(const ProxyConfig& cfg, const std::string& targetHost, int targetPort);

// 解析顶层 JSON（openProtocol 参数）中的 "Proxy" 对象到 ProxyConfig。
// cjsonRoot 为 cJSON* 根对象指针；成功返回 true（cfg.enabled=true）。
bool parseProxyConfigFromJson(void* cjsonRoot, ProxyConfig& cfg);

// 最近一次代理错误
void setLastProxyError(int err);
int getLastProxyError();
const char* proxyErrorString(int err);

// ================================================================
// 内部辅助（仅供 proxy 模块内部使用）
// ================================================================
// 非阻塞 fd 上发送全部数据 / 精确接收 n 字节（select + 超时）
bool proxySendAll(int fd, const void* data, size_t len, int timeoutSec);
bool proxyRecvExact(int fd, void* buf, size_t n, int timeoutSec);

#endif // PROXY_CLIENT_H
