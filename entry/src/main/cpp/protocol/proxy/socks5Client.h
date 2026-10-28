#ifndef SOCKS5_CLIENT_H
#define SOCKS5_CLIENT_H

#include <string>

// 对已连接（非阻塞）的代理 fd 执行 SOCKS5 握手（RFC1928 精简：NoAuth + UserPass）。
// 成功返回 0（PROXY_OK）；失败返回负错误码（见 proxyClient.h ProxyError）。
int socks5Handshake(int sockFd, const std::string& username, const std::string& password,
                    const std::string& targetHost, int targetPort, int timeoutSec);

#endif // SOCKS5_CLIENT_H
