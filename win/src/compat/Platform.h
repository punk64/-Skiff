#pragma once

#ifdef _WIN32

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>
#include <chrono>
#include <deque>
#include <list>
#include <set>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <future>
#include <limits>
#include <random>
#include <regex>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <numeric>
#include <tuple>
#include <system_error>
#include <stdexcept>
#include <iomanip>
#include <ctime>
#include <cctype>
#include <cmath>
#include <utility>
#include <array>
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#ifdef ERROR
#undef ERROR
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#ifdef small
#undef small
#endif
#ifdef interface
#undef interface
#endif
#ifdef PARITY_NONE
#undef PARITY_NONE
#endif
#ifdef PARITY_ODD
#undef PARITY_ODD
#endif
#ifdef PARITY_EVEN
#undef PARITY_EVEN
#endif
#ifdef PARITY_MARK
#undef PARITY_MARK
#endif
#ifdef PARITY_SPACE
#undef PARITY_SPACE
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

#ifndef _SK_SSIZE_T_DEFINED
#define _SK_SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif

#ifndef _SK_PID_T_DEFINED
#define _SK_PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef _SK_MODE_T_DEFINED
#define _SK_MODE_T_DEFINED
typedef unsigned short mode_t;
#endif

#ifndef _SK_UID_T_DEFINED
#define _SK_UID_T_DEFINED
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#endif

typedef int socklen_t;

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#endif

#ifdef __cplusplus
extern "C" {
#endif

void skPlatformInit(void);
void skPlatformShutdown(void);

int skClose(int64_t fd);
int skFcntl(int64_t fd, int cmd, int64_t arg);
int64_t skSocket(int domain, int type, int protocol);
int64_t skAccept(int64_t s, void* addr, int* addrlen);
int skConnect(int64_t s, const void* name, int namelen);
int skSend(int64_t s, const void* buf, int len, int flags);
int skRecv(int64_t s, void* buf, int len, int flags);
int skSelect(int nfds, void* rd, void* wr, void* ex, void* tv);
int skGetsockopt(int64_t s, int level, int optname, void* optval, int* optlen);
int skSetsockopt(int64_t s, int level, int optname, const void* optval, int optlen);
int skMapWsaError(int wsaErr);
int skLastErrno(void);
void skLogRaw(int level, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#ifndef SK_NO_MACROS

#define close(...) skClose(__VA_ARGS__)
#define fcntl(...) skFcntl(__VA_ARGS__)
#define socket(...) skSocket(__VA_ARGS__)
#define accept(...) skAccept(__VA_ARGS__)
#define connect(...) skConnect(__VA_ARGS__)
#define send(...) skSend(__VA_ARGS__)
#define recv(...) skRecv(__VA_ARGS__)
#define select(...) skSelect(__VA_ARGS__)
#define getsockopt(...) skGetsockopt(__VA_ARGS__)
#define setsockopt(...) skSetsockopt(__VA_ARGS__)

#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define localtime_r(t, r) (localtime_s((r), (t)) == 0 ? (r) : nullptr)
#define gmtime_r(t, r) (gmtime_s((r), (t)) == 0 ? (r) : nullptr)
#define usleep(us) Sleep((DWORD)((us) / 1000))
#define sleep(s) Sleep((DWORD)((s) * 1000))

#define ftruncate(fd, len) _chsize_s((fd), (__int64)(len))
#define lseek(fd, off, wh) _lseeki64((fd), (__int64)(off), (wh))
#define fsync(fd) _commit((fd))

#define inet_pton(af, src, dst) InetPtonA((af), (src), (dst))
#define inet_ntop(af, src, dst, sz) InetNtopA((af), (src), (dst), (sz))

#endif

#endif
