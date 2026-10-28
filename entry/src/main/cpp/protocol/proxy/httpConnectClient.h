#ifndef HTTP_CONNECT_CLIENT_H
#define HTTP_CONNECT_CLIENT_H

#include <string>

// 对已连接（非阻塞）的代理 fd 执行 HTTP CONNECT 握手（支持 Basic 鉴权）。
// 成功返回 0（PROXY_OK）；失败返回负错误码（见 proxyClient.h ProxyError）。
int httpConnectHandshake(int sockFd, const std::string& username, const std::string& password,
                         const std::string& targetHost, int targetPort, int timeoutSec);

#endif // HTTP_CONNECT_CLIENT_H
