#include "socks5Client.h"
#include "proxyClient.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>

// SOCKS5 protocol constants
#define SOCKS5_VERSION    0x05
#define SOCKS5_NO_AUTH    0x00
#define SOCKS5_USERPASS   0x02
#define SOCKS5_ATYP_IPV4  0x01
#define SOCKS5_ATYP_DOMAIN 0x03
#define SOCKS5_ATYP_IPV6  0x04

int socks5Handshake(int sockFd, const std::string& username, const std::string& password,
                    const std::string& targetHost, int targetPort, int timeoutSec) {
    // RFC1929 ULEN/PLEN are single-byte fields (max 255);
    // reject overlong credentials instead of emitting a malformed request.
    if (username.size() > 255 || password.size() > 255) {
        setLastProxyError(PROXY_ERR_GENERAL);
        return PROXY_ERR_GENERAL;
    }

    bool needAuth = !username.empty();

    // 1. Greeting: offer supported methods
    std::vector<unsigned char> greeting;
    greeting.push_back(SOCKS5_VERSION);
    if (needAuth) {
        greeting.push_back(0x02);            // NMETHODS
        greeting.push_back(SOCKS5_NO_AUTH);  // 方法1：无鉴权
        greeting.push_back(SOCKS5_USERPASS); // 方法2：用户名密码
    } else {
        greeting.push_back(0x01);            // NMETHODS
        greeting.push_back(SOCKS5_NO_AUTH);  // 方法1：无鉴权
    }
    if (!proxySendAll(sockFd, greeting.data(), greeting.size(), timeoutSec)) {
        setLastProxyError(PROXY_ERR_TIMEOUT);
        return PROXY_ERR_TIMEOUT;
    }

    // 2. Server method selection
    unsigned char methodResp[2] = {0, 0};
    if (!proxyRecvExact(sockFd, methodResp, 2, timeoutSec)) {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }
    if (methodResp[0] != SOCKS5_VERSION) {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }
    if (methodResp[1] == 0xFF) { // 无可用方法
        setLastProxyError(PROXY_ERR_AUTH);
        return PROXY_ERR_AUTH;
    }
    if (methodResp[1] == SOCKS5_USERPASS) {
        // 3. Username/password subnegotiation (RFC1929)
        if (!needAuth) {
            setLastProxyError(PROXY_ERR_AUTH);
            return PROXY_ERR_AUTH;
        }
        std::vector<unsigned char> up;
        up.push_back(0x01);                                   // VER
        up.push_back((unsigned char)username.size());         // ULEN
        up.insert(up.end(), username.begin(), username.end());
        up.push_back((unsigned char)password.size());         // PLEN
        up.insert(up.end(), password.begin(), password.end());
        if (!proxySendAll(sockFd, up.data(), up.size(), timeoutSec)) {
            setLastProxyError(PROXY_ERR_TIMEOUT);
            return PROXY_ERR_TIMEOUT;
        }
        unsigned char authResp[2] = {0, 0};
        if (!proxyRecvExact(sockFd, authResp, 2, timeoutSec)) {
            setLastProxyError(PROXY_ERR_HANDSHAKE);
            return PROXY_ERR_HANDSHAKE;
        }
        if (authResp[1] != 0x00) { // auth rejected
            setLastProxyError(PROXY_ERR_AUTH);
            return PROXY_ERR_AUTH;
        }
    }

    // 4. CONNECT request: IPv4 / IPv6 / domain (remote DNS)
    std::vector<unsigned char> req;
    req.push_back(SOCKS5_VERSION);
    req.push_back(0x01);   // CMD: CONNECT
    req.push_back(0x00);   // RSV
    struct in_addr addr4;
    struct in6_addr addr6;
    if (inet_pton(AF_INET, targetHost.c_str(), &addr4) == 1) {
        req.push_back(SOCKS5_ATYP_IPV4);
        req.insert(req.end(), (unsigned char*)&addr4, (unsigned char*)&addr4 + 4);
    } else if (inet_pton(AF_INET6, targetHost.c_str(), &addr6) == 1) {
        req.push_back(SOCKS5_ATYP_IPV6);
        req.insert(req.end(), (unsigned char*)&addr6, (unsigned char*)&addr6 + 16);
    } else {
        req.push_back(SOCKS5_ATYP_DOMAIN);
        req.push_back((unsigned char)targetHost.size());
        req.insert(req.end(), targetHost.begin(), targetHost.end());
    }
    req.push_back((unsigned char)((targetPort >> 8) & 0xFF));
    req.push_back((unsigned char)(targetPort & 0xFF));
    if (!proxySendAll(sockFd, req.data(), req.size(), timeoutSec)) {
        setLastProxyError(PROXY_ERR_TIMEOUT);
        return PROXY_ERR_TIMEOUT;
    }

    // 5. Reply: VER REP RSV ATYP [BND.ADDR] BND.PORT
    unsigned char hdr[4] = {0, 0, 0, 0};
    if (!proxyRecvExact(sockFd, hdr, 4, timeoutSec)) {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }
    if (hdr[1] != 0x00) { // REP != 0：代理拒绝
        setLastProxyError(PROXY_ERR_REJECTED);
        return PROXY_ERR_REJECTED;
    }
    // Skip BND.ADDR
    unsigned char atyp = hdr[3];
    int skipLen = 0;
    if (atyp == SOCKS5_ATYP_IPV4) {
        skipLen = 4;
    } else if (atyp == SOCKS5_ATYP_IPV6) {
        skipLen = 16;
    } else if (atyp == SOCKS5_ATYP_DOMAIN) {
        unsigned char domainLen = 0;
        if (!proxyRecvExact(sockFd, &domainLen, 1, timeoutSec)) {
            setLastProxyError(PROXY_ERR_HANDSHAKE);
            return PROXY_ERR_HANDSHAKE;
        }
        skipLen = domainLen;
    } else {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }
    std::vector<unsigned char> skipBuf(skipLen + 2); // BND.ADDR + BND.PORT
    if (!proxyRecvExact(sockFd, skipBuf.data(), skipBuf.size(), timeoutSec)) {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }

    setLastProxyError(PROXY_OK);
    return PROXY_OK;
}
