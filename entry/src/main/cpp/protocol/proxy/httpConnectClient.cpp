#include "httpConnectClient.h"
#include "proxyClient.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>

// Base64 encoding for the Proxy-Authorization: Basic header
static std::string base64Encode(const std::string& in) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
        out.push_back('=');
    }
    return out;
}

int httpConnectHandshake(int sockFd, const std::string& username, const std::string& password,
                         const std::string& targetHost, int targetPort, int timeoutSec) {
    // 1. Build the CONNECT request
    std::string hostPort = targetHost + ":" + std::to_string(targetPort);
    std::string req = "CONNECT " + hostPort + " HTTP/1.1\r\n";
    req += "Host: " + hostPort + "\r\n";
    req += "Proxy-Connection: Keep-Alive\r\n";
    if (!username.empty()) {
        req += "Proxy-Authorization: Basic " + base64Encode(username + ":" + password) + "\r\n";
    }
    req += "\r\n";

    if (!proxySendAll(sockFd, req.data(), req.size(), timeoutSec)) {
        setLastProxyError(PROXY_ERR_TIMEOUT);
        return PROXY_ERR_TIMEOUT;
    }

    // 2. Read the header up to \r\n\r\n byte by byte, so any tunnel data
    // following the header (e.g. a server banner in the same segment)
    // stays in the kernel buffer for libssh2/Telnet to read normally.
    // timeoutSec is the total deadline for this stage.
    std::string resp;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec < 0 ? 0 : timeoutSec);
    while (true) {
        if (resp.size() >= 4 && resp.compare(resp.size() - 4, 4, "\r\n\r\n") == 0) {
            break; // 头部结束
        }
        auto remainMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now()).count();
        if (remainMs <= 0) {
            setLastProxyError(PROXY_ERR_TIMEOUT);
            return PROXY_ERR_TIMEOUT;
        }
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sockFd, &rfds);
        struct timeval tv;
        tv.tv_sec = static_cast<long>(remainMs / 1000);
        tv.tv_usec = static_cast<long>((remainMs % 1000) * 1000);
        int rc = select(sockFd + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) {
            setLastProxyError(PROXY_ERR_TIMEOUT);
            return PROXY_ERR_TIMEOUT;
        }
        char c = 0;
        ssize_t n = recv(sockFd, &c, 1, 0);
        if (n > 0) {
            resp.push_back(c);
        } else if (n == 0) {
            setLastProxyError(PROXY_ERR_HANDSHAKE);
            return PROXY_ERR_HANDSHAKE;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            continue;
        } else {
            setLastProxyError(PROXY_ERR_HANDSHAKE);
            return PROXY_ERR_HANDSHAKE;
        }
        if (resp.size() > 65536) { // header too large
            setLastProxyError(PROXY_ERR_HANDSHAKE);
            return PROXY_ERR_HANDSHAKE;
        }
    }

    // 3. Parse the status line: HTTP/1.1 <CODE> ...
    size_t eol = resp.find("\r\n");
    std::string statusLine = resp.substr(0, eol);
    size_t sp1 = statusLine.find(' ');
    if (sp1 == std::string::npos) {
        setLastProxyError(PROXY_ERR_HANDSHAKE);
        return PROXY_ERR_HANDSHAKE;
    }
    size_t sp2 = statusLine.find(' ', sp1 + 1);
    std::string codeStr = (sp2 == std::string::npos)
                              ? statusLine.substr(sp1 + 1)
                              : statusLine.substr(sp1 + 1, sp2 - sp1 - 1);
    int code = atoi(codeStr.c_str());

    if (code == 200) {
        setLastProxyError(PROXY_OK);
        return PROXY_OK;
    }
    if (code == 407) { // proxy authentication required
        setLastProxyError(PROXY_ERR_AUTH);
        return PROXY_ERR_AUTH;
    }
    setLastProxyError(PROXY_ERR_REJECTED);
    return PROXY_ERR_REJECTED;
}
