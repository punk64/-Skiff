#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include <string>

// Proxy connection configuration
struct ProxyConfig {
    bool enabled = false;        // whether the proxy is enabled
    std::string type;            // "SOCKS5" | "HTTP" | "None"
    std::string host;            // proxy server address
    int port = 0;                // proxy server port (0 = unset; connectViaProxy rejects it)
    std::string username;        // proxy username (optional)
    std::string password;        // proxy password (optional)
    int timeoutSec = 10;         // connect/handshake timeout in seconds
};

#endif // PROXY_CONFIG_H
