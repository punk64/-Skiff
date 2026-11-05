#define SK_NO_MACROS 1
#include "Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
#include <mutex>
#include <ctime>

namespace {

std::mutex g_nonblockMutex;
std::map<int64_t, bool> g_nonblockMap;

std::mutex g_logMutex;
FILE* g_logFile = nullptr;
int g_logDay = -1;
char g_logDir[MAX_PATH] = {0};
thread_local int g_lastErrno = 0;

void resolveLogDir() {
    char path[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0) return;
    char* slash = strrchr(path, '\\');
    if (slash) *slash = '\0';
    snprintf(g_logDir, MAX_PATH, "%s\\logs", path);
    CreateDirectoryA(g_logDir, nullptr);
}

void openLogForToday() {
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &now);
    int day = (tmv.tm_year + 1900) * 1000 + tmv.tm_mon * 32 + tmv.tm_mday;
    if (day == g_logDay && g_logFile != nullptr) return;
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    g_logDay = day;
    if (g_logDir[0] == 0) return;
    char file[MAX_PATH + 32];
    snprintf(file, sizeof(file), "%s\\app-%04d%02d%02d.log", g_logDir,
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    g_logFile = fopen(file, "a");
}

void sanitizeFormat(const char* fmt, std::string& out) {
    out.clear();
    size_t i = 0;
    size_t len = strlen(fmt);
    while (i < len) {
        if (fmt[i] == '%' && i + 1 < len && fmt[i + 1] == '{') {
            const char* close = strchr(fmt + i + 2, '}');
            if (close) {
                out.push_back('%');
                i = static_cast<size_t>(close - fmt) + 1;
                continue;
            }
        }
        out.push_back(fmt[i]);
        ++i;
    }
}

}

extern "C" {

void skPlatformInit(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    resolveLogDir();
}

void skPlatformShutdown(void) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    WSACleanup();
}

int skMapWsaError(int wsaErr) {
    int mapped = EINVAL;
    switch (wsaErr) {
        case WSAEWOULDBLOCK: mapped = EAGAIN; break;
        case WSAEINPROGRESS: mapped = EINPROGRESS; break;
        case WSAEINTR: mapped = EINTR; break;
        case WSAECONNREFUSED: mapped = ECONNREFUSED; break;
        case WSAECONNRESET: mapped = ECONNRESET; break;
        case WSAECONNABORTED: mapped = ECONNABORTED; break;
        case WSAETIMEDOUT: mapped = ETIMEDOUT; break;
        case WSAENOTCONN: mapped = ENOTCONN; break;
        case WSAEISCONN: mapped = EISCONN; break;
        case WSAENOTSOCK: mapped = ENOTSOCK; break;
        case WSAEBADF: mapped = EBADF; break;
        case WSAEACCES: mapped = EACCES; break;
        case WSAENETUNREACH: mapped = ENETUNREACH; break;
        case WSAEHOSTUNREACH: mapped = EHOSTUNREACH; break;
        case WSAEADDRINUSE: mapped = EADDRINUSE; break;
        case WSAEADDRNOTAVAIL: mapped = EADDRNOTAVAIL; break;
        case WSAEMSGSIZE: mapped = EMSGSIZE; break;
        case WSAENOBUFS: mapped = ENOBUFS; break;
        default: mapped = EINVAL; break;
    }
    g_lastErrno = mapped;
    errno = mapped;
    return mapped;
}

int skLastErrno(void) {
    return g_lastErrno;
}

int64_t skSocket(int domain, int type, int protocol) {
    SOCKET s = ::socket(domain, type, protocol);
    if (s == INVALID_SOCKET) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return static_cast<int64_t>(s);
}

int64_t skAccept(int64_t s, void* addr, int* addrlen) {
    SOCKET r = ::accept(static_cast<SOCKET>(s), reinterpret_cast<struct sockaddr*>(addr), addrlen);
    if (r == INVALID_SOCKET) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return static_cast<int64_t>(r);
}

int skConnect(int64_t s, const void* name, int namelen) {
    int r = ::connect(static_cast<SOCKET>(s), reinterpret_cast<const struct sockaddr*>(name), namelen);
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return 0;
}

int skSend(int64_t s, const void* buf, int len, int flags) {
    int r = ::send(static_cast<SOCKET>(s), reinterpret_cast<const char*>(buf), len, flags);
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return r;
}

int skRecv(int64_t s, void* buf, int len, int flags) {
    int r = ::recv(static_cast<SOCKET>(s), reinterpret_cast<char*>(buf), len, flags);
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return r;
}

int skSelect(int nfds, void* rd, void* wr, void* ex, void* tv) {
    int r = ::select(nfds, reinterpret_cast<fd_set*>(rd), reinterpret_cast<fd_set*>(wr),
                     reinterpret_cast<fd_set*>(ex), reinterpret_cast<struct timeval*>(tv));
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return r;
}

int skGetsockopt(int64_t s, int level, int optname, void* optval, int* optlen) {
    int r = ::getsockopt(static_cast<SOCKET>(s), level, optname,
                         reinterpret_cast<char*>(optval), optlen);
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return 0;
}

int skSetsockopt(int64_t s, int level, int optname, const void* optval, int optlen) {
    int r = ::setsockopt(static_cast<SOCKET>(s), level, optname,
                         reinterpret_cast<const char*>(optval), optlen);
    if (r == SOCKET_ERROR) {
        skMapWsaError(WSAGetLastError());
        return -1;
    }
    return 0;
}

int skClose(int64_t fd) {
    if (fd < 0) return -1;
    int type = 0;
    int len = sizeof(type);
    if (::getsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_TYPE,
                     reinterpret_cast<char*>(&type), &len) == 0) {
        std::lock_guard<std::mutex> lk(g_nonblockMutex);
        g_nonblockMap.erase(fd);
        return ::closesocket(static_cast<SOCKET>(fd));
    }
    return _close(static_cast<int>(fd));
}

int skFcntl(int64_t fd, int cmd, int64_t arg) {
    if (fd < 0) return -1;
    if (cmd == F_GETFL) {
        std::lock_guard<std::mutex> lk(g_nonblockMutex);
        auto it = g_nonblockMap.find(fd);
        return (it != g_nonblockMap.end() && it->second) ? O_NONBLOCK : 0;
    }
    if (cmd == F_SETFL) {
        bool wantNonBlock = (arg & O_NONBLOCK) != 0;
        u_long mode = wantNonBlock ? 1 : 0;
        if (::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode) != 0) {
            skMapWsaError(WSAGetLastError());
            return -1;
        }
        std::lock_guard<std::mutex> lk(g_nonblockMutex);
        g_nonblockMap[fd] = wantNonBlock;
        return 0;
    }
    return -1;
}

void skLogRaw(int level, const char* fmt, ...) {
    static const char* names[] = {"D", "I", "W", "E", "F"};
    const char* tag = (level >= 0 && level <= 4) ? names[level] : "I";

    std::string clean;
    sanitizeFormat(fmt != nullptr ? fmt : "", clean);

    char msg[4096];
    msg[0] = '\0';
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), clean.c_str(), ap);
    va_end(ap);

    time_t now = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &now);

    char line[4300];
    snprintf(line, sizeof(line), "%02d:%02d:%02d [%s] %s\n", tmv.tm_hour, tmv.tm_min,
             tmv.tm_sec, tag, msg);
    OutputDebugStringA(line);

    std::lock_guard<std::mutex> lk(g_logMutex);
    openLogForToday();
    if (g_logFile) {
        fputs(line, g_logFile);
        fflush(g_logFile);
    }
}

}
