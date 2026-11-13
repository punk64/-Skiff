#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <exception>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/App.h"
#include "app/Backup.h"
#include "app/Layout.h"
#include "app/Config.h"
#include "app/Lang.h"
#include "ui/Render.h"
#include "ui/Chrome.h"
#include "ui/Terminal.h"
#include "ui/Sessions.h"
#include "core/SshService.h"
#include "compat/Log.h"

extern "C" void skPlatformInit(void);
extern "C" void skPlatformShutdown(void);

namespace sk {

static volatile int g_logLevel = 2;

void SetLogLevel(int lv) { g_logLevel = lv < 0 ? 0 : (lv > 2 ? 2 : lv); }

void LogWrite(const char* level, const char* fmt, ...) {
  int lv = g_logLevel;
  if (lv <= 0) return;
  if (lv == 1) {
    char c = (level != nullptr && level[0] != 0) ? level[0] : 'I';
    if (c != 'W' && c != 'E' && c != 'F') return;
  }
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  char line[2100];
  snprintf(line, sizeof(line), "[%s] %s\n", level, buf);
  OutputDebugStringA(line);
  static FILE* fp = nullptr;
  static long long bytes = 0;
  static int seg = 1;
  static int segDay = -1;
  SYSTEMTIME st;
  GetLocalTime(&st);
  if (fp != nullptr && (st.wDay != segDay || bytes > 32LL * 1024 * 1024)) {
    fclose(fp);
    fp = nullptr;
    if (st.wDay == segDay) ++seg;
  }
  if (fp == nullptr) {
    segDay = st.wDay;
    char dir[MAX_PATH] = { 0 };
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) slash[1] = 0;
    std::string logs = std::string(dir) + "logs";
    CreateDirectoryA(logs.c_str(), nullptr);
    char path[MAX_PATH] = { 0 };
    if (seg <= 1) {
      snprintf(path, sizeof(path), "%s\\app-%04d%02d%02d.log", logs.c_str(),
               st.wYear, st.wMonth, st.wDay);
    } else {
      snprintf(path, sizeof(path), "%s\\app-%04d%02d%02d-%d.log", logs.c_str(),
               st.wYear, st.wMonth, st.wDay, seg);
    }
    fp = fopen(path, "ab");
    bytes = 0;
    if (fp != nullptr) {
      _fseeki64(fp, 0, SEEK_END);
      bytes = (long long)_ftelli64(fp);
    }
  }
  if (fp) {
    fputs(line, fp);
    fflush(fp);
    bytes += (long long)strlen(line);
  }
}
}

namespace {

const wchar_t* kClassName = L"SkiffWinMain";
const wchar_t* kWindowTitle = sk::Tr(L"轻舟 Skiff");
const DWORD kAppStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;

sk::Render g_render;
sk::AppState g_state;
sk::Rect g_client;

const UINT kMsgSsh = WM_APP + 1;

struct PwdState {
  bool active = false;
  std::wstring buf;
};

std::map<int, PwdState> g_pwd;
bool g_suppressChar = false;

sk::Terminal* TermOf(int sid) {
  if (sid < 0) return nullptr;
  return &sk::SessionHub::Instance().Ensure(sid);
}

std::string W2U8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
  return s;
}

std::wstring U82W(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

std::string LogDirA() {
  char dir[MAX_PATH] = { 0 };
  GetModuleFileNameA(nullptr, dir, MAX_PATH);
  char* slash = strrchr(dir, '\\');
  if (slash != nullptr) slash[1] = 0;
  std::string logs = std::string(dir) + "logs";
  CreateDirectoryA(logs.c_str(), nullptr);
  return logs;
}

void CrashWrite(const char* what, EXCEPTION_POINTERS* ep) {
  std::string logs = LogDirA();
  SYSTEMTIME st;
  GetLocalTime(&st);
  char path[MAX_PATH] = { 0 };
  snprintf(path, sizeof(path), "%s\\crash-%04d%02d%02d-%02d%02d%02d.log", logs.c_str(),
           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  FILE* fp = fopen(path, "wb");
  if (fp == nullptr) return;
  fprintf(fp, "what=%s\n", what);
  if (ep != nullptr) {
    fprintf(fp, "code=0x%08lX\n", (unsigned long)ep->ExceptionRecord->ExceptionCode);
    fprintf(fp, "address=%p\n", ep->ExceptionRecord->ExceptionAddress);
    fprintf(fp, "module_base=%p\n", (void*)GetModuleHandleW(nullptr));
    DWORD n = ep->ExceptionRecord->NumberParameters;
    for (DWORD i = 0; i < n && i < 15; ++i) {
      fprintf(fp, "param%lu=0x%p\n", (unsigned long)i,
              (void*)ep->ExceptionRecord->ExceptionInformation[i]);
    }
  }
  fclose(fp);
  std::wstring msg = L"轻舟遇到错误需要关闭。\n\n崩溃日志：\n";
  msg += U82W(path);
  MessageBoxW(nullptr, msg.c_str(), L"轻舟 Skiff", MB_ICONERROR | MB_OK);
}

LONG WINAPI SkiffCrashFilter(EXCEPTION_POINTERS* ep) {
  CrashWrite("seh", ep);
  return EXCEPTION_EXECUTE_HANDLER;
}

std::wstring ErrText(const std::string& code) {
  if (code.empty()) return sk::Tr(L"无法建立连接，详见 logs 目录");
  if (code == "TCP_CONNECT_FAILED") return sk::Tr(L"无法连接目标端口，请检查地址、端口与网络");
  if (code == "HANDSHAKE_TIMEOUT") return sk::Tr(L"SSH 握手超时，对方可能不是 SSH 服务");
  if (code.compare(0, 16, "HANDSHAKE_FAILED") == 0) return sk::Tr(L"SSH 算法协商失败，服务器不支持本客户端可用的加密算法");
  if (code == "CHANNEL_OPEN_FAILED") return sk::Tr(L"服务器拒绝打开终端会话");
  if (code == "AUTH_FAILED") return sk::Tr(L"认证被拒绝，请检查用户名、密码或私钥");
  if (code == "AUTH_TIMEOUT") return sk::Tr(L"认证超时");
  if (code == "AUTH_METHOD_UNSUPPORTED") return sk::Tr(L"服务器不支持所选认证方式");
  if (code == "AUTH_LIST_FAILED") return sk::Tr(L"无法获取服务器支持的认证方式");
  if (code == "KEY_FD_INVALID") return sk::Tr(L"私钥文件无法打开");
  if (code == "KEY_READ_FAILED") return sk::Tr(L"私钥内容为空或不可读");
  if (code == "SESSION_INIT_FAILED") return sk::Tr(L"SSH 会话初始化失败");
  if (code == "KNOWNHOST_INIT_FAILED") return sk::Tr(L"主机指纹库初始化失败");
  if (code == "JSON_PARSE_FAILED") return sk::Tr(L"连接参数格式错误");
  if (code.compare(0, 12, "HOSTKEY_ERR:") == 0) return sk::Tr(L"主机指纹与记录不符，请在编辑主机里重置指纹");
  if (code.compare(0, 10, "PROXY_ERR:") == 0) return sk::Tr(L"代理连接失败");
  return U82W(code);
}

std::wstring ExeDir() {
  wchar_t buf[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p(buf);
  size_t slash = p.find_last_of(L'\\');
  return slash == std::wstring::npos ? std::wstring() : p.substr(0, slash);
}

std::wstring ResolveKeyPath(const std::wstring& raw) {
  if (raw.empty()) return raw;
  bool abs = raw.find(L'\\') != std::wstring::npos || raw.find(L'/') != std::wstring::npos ||
             (raw.size() > 1 && raw[1] == L':');
  if (abs) return raw;
  std::wstring dir = sk::KeyDirectory();
  return dir.empty() ? raw : dir + L"\\" + raw;
}

void CleanOldLogs(int keepDays) {
  if (keepDays <= 0) return;
  SYSTEMTIME now{};
  GetLocalTime(&now);
  FILETIME ftNow{};
  SystemTimeToFileTime(&now, &ftNow);
  ULARGE_INTEGER un{};
  un.LowPart = ftNow.dwLowDateTime;
  un.HighPart = ftNow.dwHighDateTime;
  std::wstring dir = ExeDir() + L"\\logs";
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileW((dir + L"\\app-*.log").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
    ULARGE_INTEGER uf{};
    uf.LowPart = fd.ftLastWriteTime.dwLowDateTime;
    uf.HighPart = fd.ftLastWriteTime.dwHighDateTime;
    unsigned long long days = (un.QuadPart - uf.QuadPart) / 864000000000ULL;
    if (days > (unsigned long long)keepDays) {
      DeleteFileW((dir + L"\\" + fd.cFileName).c_str());
    }
  } while (FindNextFileW(h, &fd) != 0);
  FindClose(h);
}

void StartConnect(int idx, int sid, const std::string& password, const std::wstring& keyPath) {
  if (idx < 0 || idx >= (int)g_state.hosts.size() || sid < 0) return;
  sk::Host& h = g_state.hosts[(size_t)idx];
  sk::SshParams p;
  p.user = W2U8(h.user);
  p.host = W2U8(h.host);
  p.port = h.port;
  if (p.host.empty()) {
    sk::Terminal* term = TermOf(sid);
    if (term) term->Feed("\r\n\x1b[31m主机地址为空\x1b[0m\r\n");
    return;
  }
  p.password = password;
  p.useKey = !keyPath.empty();
  p.keyPath = W2U8(ResolveKeyPath(keyPath));
  p.keyPathW = ResolveKeyPath(keyPath);
  p.keyPass = W2U8(h.keyPass);
  p.expectedHostKey = g_state.fpCheck ? W2U8(h.hostKey) : std::string();
  p.expectedHostKeyType = g_state.fpCheck ? W2U8(h.hostKeyType) : std::string();
  if (g_state.fpCheck && p.expectedHostKeyType.empty() && !p.expectedHostKey.empty()) {
    p.expectedHostKeyType = "ssh-rsa";
  }
  static const int kKeepAlive[4] = { 30, 60, 120, -1 };
  int ki = g_state.keepAliveIdx < 0 ? 1 : (g_state.keepAliveIdx > 3 ? 3 : g_state.keepAliveIdx);
  p.keepAlive = kKeepAlive[ki];
  static const int kTimeout[3] = { 5, 10, 20 };
  int ti = g_state.timeoutIdx < 0 ? 1 : (g_state.timeoutIdx > 2 ? 2 : g_state.timeoutIdx);
  p.timeout = kTimeout[ti];
  h.manualStop = false;
  h.retryN = 0;
  h.retryAt = 0;
  h.state = sk::HostState::Busy;
  sk::SshService::Instance().Connect(sid, p);
}

void RequestConnect(int idx, bool openTerm) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  int sid = openTerm ? g_state.OpenHostSession(idx) : g_state.SessionForHost(idx);
  if (sid < 0) return;

  sk::SshService& svc = sk::SshService::Instance();
  sk::SshState st = svc.State(sid);
  if (st == sk::SshState::Connected) return;
  if (st == sk::SshState::Connecting) {
    g_state.toast = sk::Tr(L"正在连接中，请稍候…");
    g_state.toastAt = (long long)GetTickCount64();
    return;
  }

  sk::Host& h = g_state.hosts[(size_t)idx];
  std::string host = W2U8(h.host);
  if (host.empty()) return;

  sk::Terminal* term = TermOf(sid);
  if (term != nullptr) {
    g_pwd.erase(sid);
    term->Feed("\r\n\x1b[90m连接 " + W2U8(h.user) + "@" + host + ":" + std::to_string(h.port) +
               "…\x1b[0m\r\n");
  }

  if (!sk::HostHasCredential(h)) {
    g_state.OpenHostForm(idx);
    g_state.formError = sk::Tr(L"请先设置密码或私钥文件，再连接");
    return;
  }
  if (h.IsKeyAuth()) {
    std::wstring key = h.keyPath.empty() ? sk::FindKeyFor(host) : h.keyPath;
    if (!key.empty()) {
      StartConnect(idx, sid, std::string(), key);
      return;
    }
  }
  if (!h.password.empty()) {
    StartConnect(idx, sid, W2U8(h.password), std::wstring());
  }
}

void ScheduleReconnect(int idx) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  if (!g_state.autoReconnect) return;
  sk::Host& h = g_state.hosts[(size_t)idx];
  if (h.manualStop) return;
  if (h.state == sk::HostState::Busy) return;
  if (h.sessionId >= 0) {
    sk::SshState s = sk::SshService::Instance().State(h.sessionId);
    if (s == sk::SshState::Connected || s == sk::SshState::Connecting) return;
  }
  int n = h.retryN < 0 ? 0 : h.retryN;
  int wait = n == 0 ? 5 : (n == 1 ? 10 : (n == 2 ? 20 : (n == 3 ? 40 : 60)));
  h.retryN = n + 1;
  h.retryAt = (long long)GetTickCount64() + (long long)wait * 1000;
}

void PumpReconnect() {
  long long now = (long long)GetTickCount64();
  for (size_t i = 0; i < g_state.hosts.size(); ++i) {
    sk::Host& h = g_state.hosts[i];
    if (h.retryAt <= 0 || now < h.retryAt) continue;
    h.retryAt = 0;
    if (h.state == sk::HostState::Live) continue;
    RequestConnect((int)i, false);
  }
}

void BuildBootQueue() {
  g_state.bootQueue.clear();
  for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
    const sk::Host& h = g_state.hosts[(size_t)i];
    if (g_state.autoConnectAll || h.autoConnect) g_state.bootQueue.push_back(i);
  }
  g_state.bootNextAt = 0;
}

void PumpBootQueue() {
  if (g_state.bootQueue.empty()) return;
  long long now = (long long)GetTickCount64();
  if (now < g_state.bootNextAt) return;
  int idx = g_state.bootQueue.front();
  g_state.bootQueue.erase(g_state.bootQueue.begin());
  g_state.bootNextAt = now + 500;
  RequestConnect(idx, false);
}

void RestoreLastView() {
  if (!g_state.restoreLast) return;
  if (g_state.lastView != (int)sk::View::Tun) return;
  g_state.OpenPage(sk::View::Tun);
}

void OnSshNotify(void* ctx, int id) {
  HWND hwnd = reinterpret_cast<HWND>(ctx);
  if (hwnd != nullptr) PostMessageW(hwnd, kMsgSsh, (WPARAM)id, 0);
}

std::string KeyToBytes(wchar_t ch, bool ctrl) {
  if (ctrl) {
    if (ch >= L'a' && ch <= L'z') return std::string(1, (char)(ch - L'a' + 1));
    if (ch >= L'A' && ch <= L'Z') return std::string(1, (char)(ch - L'A' + 1));
    if (ch == L' ') return std::string(1, '\0');
    return std::string();
  }
  if (ch == L'\r') return std::string("\r");
  if (ch == L'\b') return std::string("\x7f", 1);
  if (ch == L'\t') return std::string("\t");
  if (ch < 32) return std::string();
  return W2U8(std::wstring(1, ch));
}

void Relayout(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  g_client = sk::Rect{ 0, 0, (float)rc.right, (float)rc.bottom };
}

void SyncTheme() {
  if (g_state.themeMode == 1) {
    g_state.dark = true;
  } else if (g_state.themeMode == 2) {
    g_state.dark = false;
  } else {
    g_state.dark = !sk::SystemPrefersLight();
  }
}

void SyncRender() {
  g_render.SetUiScale(g_state.UiScale());
  g_render.SetMonoFamily(g_state.termFontIdx);
}

void ToggleFullscreen(HWND hwnd, bool on) {
  g_state.fullscreen = on;
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  bool isPopup = (style & WS_POPUP) != 0;
  MONITORINFO mi{};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
  if (on) {
    if (!isPopup) SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
  } else {
    SetWindowLong(hwnd, GWL_STYLE, kAppStyle | WS_VISIBLE);
    SetWindowPos(hwnd, nullptr, 80, 60, (int)g_state.S(sk::L::WinDefaultW),
                 (int)g_state.S(sk::L::WinDefaultH), SWP_FRAMECHANGED);
  }
  Relayout(hwnd);
  g_render.Resize((UINT)g_client.W(), (UINT)g_client.H(), (float)g_state.dpi);
}

void Paint(HWND hwnd) {
  if (!g_render.Begin()) return;
  static int lastLang = -1;
  if (lastLang != g_state.langMode) {
    lastLang = g_state.langMode;
    sk::SetLang(lastLang);
  }
  sk::DrawAll(g_render, g_state, g_client);
  g_render.End();
}

LRESULT HandleHitTest(HWND hwnd, int x, int y) {
  RECT wr;
  GetWindowRect(hwnd, &wr);
  const int kEdge = 6;
  bool left = x < wr.left + kEdge;
  bool right = x > wr.right - kEdge;
  bool top = y < wr.top + kEdge;
  bool bottom = y > wr.bottom - kEdge;
  if (!IsZoomed(hwnd)) {
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
  }
  POINT pt{ x, y };
  ScreenToClient(hwnd, &pt);
  float titleH = g_state.S(sk::L::TitleH);
  if (!g_state.fullscreen && pt.y < titleH) {
    if (pt.x > wr.right - wr.left - g_state.S(132)) return HTCLIENT;
    return HTCAPTION;
  }
  return HTCLIENT;
}

HWND g_hwnd = nullptr;
bool g_inSizeMove = false;

struct Sample {
  bool cpuValid = false;
  long long cpuTotal = 0;
  long long cpuIdle = 0;
  long long rx = 0;
  long long tx = 0;
  long long at = 0;
  int fails = 0;
  long long nextAt = 0;
};

std::map<int, Sample> g_samples;

struct StatsJob {
  int idx = -1;
  int sid = -1;
};

struct StatsPacket {
  int idx = -1;
  bool ok = false;
  int latency = 0;
  int cpu = 0;
  int ram = 0;
  std::wstring net;
  int netp = 0;
};

std::deque<StatsJob> g_statsJobs;
std::vector<StatsPacket> g_statsDone;
std::mutex g_statsMtx;
std::condition_variable g_statsCv;
std::atomic<bool> g_statsStop{false};
std::thread g_statsWorker;

std::string TrimAscii(const std::string& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
  return s.substr(a, b - a);
}

std::wstring FmtRate(long long bytesPerSec) {
  wchar_t buf[32];
  double v = (double)bytesPerSec;
  if (v >= 1048576.0) {
    swprintf_s(buf, L"%.1f MB/s", v / 1048576.0);
  } else if (v >= 1024.0) {
    swprintf_s(buf, L"%.1f KB/s", v / 1024.0);
  } else {
    swprintf_s(buf, L"%d B/s", (int)v);
  }
  return std::wstring(buf);
}

std::wstring FmtBytes(unsigned long long n) {
  wchar_t buf[32];
  if (n >= 1073741824ULL) {
    swprintf_s(buf, L"%.1f GB", (double)n / 1073741824.0);
  } else if (n >= 1048576ULL) {
    swprintf_s(buf, L"%.1f MB", (double)n / 1048576.0);
  } else if (n >= 1024ULL) {
    swprintf_s(buf, L"%.1f KB", (double)n / 1024.0);
  } else {
    swprintf_s(buf, L"%I64u B", n);
  }
  return std::wstring(buf);
}

std::wstring FmtFileTime(unsigned long long mtimeSec) {
  if (mtimeSec == 0) return std::wstring(L"—");
  ULARGE_INTEGER u{};
  u.QuadPart = (mtimeSec * 10000000ULL) + 116444736000000000ULL;
  FILETIME ft{};
  ft.dwLowDateTime = u.LowPart;
  ft.dwHighDateTime = u.HighPart;
  FILETIME local{};
  if (!FileTimeToLocalFileTime(&ft, &local)) return std::wstring(L"—");
  SYSTEMTIME st{};
  if (!FileTimeToSystemTime(&local, &st)) return std::wstring(L"—");
  wchar_t buf[32];
  swprintf_s(buf, L"%02d-%02d %02d:%02d", st.wMonth, st.wDay, st.wHour, st.wMinute);
  return std::wstring(buf);
}

void SyncHostStates() {
  sk::SshService& svc = sk::SshService::Instance();
  bool ch = false;
  for (size_t i = 0; i < g_state.hosts.size(); ++i) {
    sk::Host& h = g_state.hosts[i];
    if (h.state != sk::HostState::Live) continue;
    if (h.sessionId >= 0 && svc.IsConnected(h.sessionId)) continue;
    h.state = sk::HostState::Off;
    h.latency = -1;
    h.cpu = 0;
    h.ram = 0;
    h.netp = 0;
    h.net.clear();
    int t = g_state.FindTabByHost((int)i);
    if (t >= 0) g_state.tabs[(size_t)t].live = false;
    ch = true;
  }
  if (ch) {
    g_state.dirty = true;
    if (g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
  }
}

void CollectStats(int sid, StatsPacket& p) {
  if (sid < 0) return;
  sk::SshService& svc = sk::SshService::Instance();
  if (!svc.IsConnected(sid)) return;

  const char* cmd =
      "head -1 /proc/stat; echo '###'; grep -E '^(MemTotal|MemAvailable):' /proc/meminfo; "
      "echo '###'; cat /proc/net/dev";
  std::string raw;
  ULONGLONG t0 = GetTickCount64();
  if (!svc.Exec(sid, cmd, raw) || raw.empty()) {
    Sample& sp = g_samples[sid];
    ++sp.fails;
    if (sp.fails == 1 || sp.fails == 3) {
      sk::LogWrite("W", "stats exec failed sid=%d fails=%d bytes=%zu", sid, sp.fails, raw.size());
    }
    if (sp.fails >= 3) sp.nextAt = (long long)(GetTickCount64() + 60000);
    return;
  }
  p.latency = (int)(GetTickCount64() - t0);

  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    size_t pos = raw.find("###", start);
    if (pos == std::string::npos) {
      parts.push_back(raw.substr(start));
      break;
    }
    parts.push_back(raw.substr(start, pos - start));
    start = pos + 3;
  }
  if (parts.size() < 3) return;

  Sample& sp = g_samples[sid];
  sp.fails = 0;

  long long v[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int n = sscanf(parts[0].c_str(), "%*s %lld %lld %lld %lld %lld %lld %lld %lld", &v[0], &v[1],
                 &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
  if (n >= 4) {
    long long total = 0;
    for (int i = 0; i < n && i < 8; ++i) total += v[i];
    long long idle = v[3] + (n > 4 ? v[4] : 0);
    if (sp.cpuValid) {
      long long dt = total - sp.cpuTotal;
      long long di = idle - sp.cpuIdle;
      if (dt > 0) {
        int pct = (int)((dt - di) * 100 / dt);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        p.cpu = pct;
      }
    }
    sp.cpuTotal = total;
    sp.cpuIdle = idle;
    sp.cpuValid = true;
  }

  long long memTotal = 0;
  long long memAvail = 0;
  size_t p1 = parts[1].find("MemTotal:");
  if (p1 != std::string::npos) memTotal = _atoi64(parts[1].c_str() + p1 + 9);
  size_t p2 = parts[1].find("MemAvailable:");
  if (p2 != std::string::npos) memAvail = _atoi64(parts[1].c_str() + p2 + 13);
  if (memTotal > 0) {
    int pct = (int)((memTotal - memAvail) * 100 / memTotal);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    p.ram = pct;
  }

  long long rx = 0;
  long long tx = 0;
  std::istringstream iss(parts[2]);
  std::string line;
  while (std::getline(iss, line)) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = TrimAscii(line.substr(0, colon));
    if (name.empty() || name == "lo") continue;
    long long a = 0;
    long long b = 0;
    if (sscanf(line.c_str() + colon + 1, "%lld %*d %*d %*d %*d %*d %*d %*d %lld", &a, &b) != 2) {
      continue;
    }
    rx += a;
    tx += b;
  }
  long long nowMs = (long long)GetTickCount64();
  if (sp.at > 0 && nowMs > sp.at) {
    long long drx = rx - sp.rx;
    long long dtx = tx - sp.tx;
    if (drx < 0) drx = 0;
    if (dtx < 0) dtx = 0;
    double secs = (double)(nowMs - sp.at) / 1000.0;
    if (secs < 0.2) secs = 1.0;
    long long rate = (long long)((double)(drx + dtx) / secs);
    p.net = FmtRate(rate);
    int pct = (int)((double)rate / (10.0 * 1024.0 * 1024.0) * 100.0);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    p.netp = pct;
  }
  sp.rx = rx;
  sp.tx = tx;
  sp.at = nowMs;
  p.ok = true;
}

void StatsWorker() {
  for (;;) {
    StatsJob job;
    {
      std::unique_lock<std::mutex> lk(g_statsMtx);
      g_statsCv.wait(lk, [] { return g_statsStop.load() || !g_statsJobs.empty(); });
      if (g_statsStop.load()) return;
      job = g_statsJobs.front();
      g_statsJobs.pop_front();
    }
    StatsPacket p;
    p.idx = job.idx;
    CollectStats(job.sid, p);
    std::lock_guard<std::mutex> lk(g_statsMtx);
    g_statsDone.push_back(p);
  }
}

void StatsStart() {
  if (!g_statsWorker.joinable()) g_statsWorker = std::thread(StatsWorker);
}

void StatsStop() {
  {
    std::lock_guard<std::mutex> lk(g_statsMtx);
    g_statsStop.store(true);
    g_statsJobs.clear();
  }
  g_statsCv.notify_all();
  if (!g_statsWorker.joinable()) return;
  HANDLE h = (HANDLE)g_statsWorker.native_handle();
  if (WaitForSingleObject(h, 1500) == WAIT_OBJECT_0) {
    g_statsWorker.join();
  } else {
    g_statsWorker.detach();
  }
}

void PumpStats() {
  std::vector<StatsPacket> done;
  {
    std::lock_guard<std::mutex> lk(g_statsMtx);
    if (g_statsDone.empty()) return;
    done.swap(g_statsDone);
  }
  bool ch = false;
  for (size_t i = 0; i < done.size(); ++i) {
    const StatsPacket& p = done[i];
    if (p.idx < 0 || p.idx >= (int)g_state.hosts.size()) continue;
    sk::Host& h = g_state.hosts[(size_t)p.idx];
    if (h.state != sk::HostState::Live) continue;
    if (!p.ok) continue;
    h.latency = p.latency;
    h.cpu = p.cpu;
    h.ram = p.ram;
    ch = true;
  }
  if (ch && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
}

struct NetSmp {
  unsigned long long rx = 0;
  unsigned long long tx = 0;
  unsigned long long at = 0;
  double rate = 0.0;
};

std::map<int, NetSmp> g_netSmp;
long long g_netNextAt = 0;

void PumpNetRate() {
  ULONGLONG now = GetTickCount64();
  sk::SshService& svc = sk::SshService::Instance();
  bool ch = false;
  for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
    sk::Host& h = g_state.hosts[(size_t)i];
    if (h.state != sk::HostState::Live || h.sessionId < 0) {
      g_netSmp.erase(i);
      continue;
    }
    unsigned long long rx = 0;
    unsigned long long tx = 0;
    svc.Traffic(h.sessionId, rx, tx);
    int xid = svc.TransferSessionId(h.sessionId);
    if (xid != -1) {
      unsigned long long xr = 0;
      unsigned long long xt = 0;
      svc.Traffic(xid, xr, xt);
      rx += xr;
      tx += xt;
    }
    NetSmp& s = g_netSmp[i];
    if (s.at > 0 && now > s.at && rx >= s.rx && tx >= s.tx) {
      double secs = (double)(now - s.at) / 1000.0;
      if (secs < 0.2) secs = 1.0;
      double inst = (double)((rx - s.rx) + (tx - s.tx)) / secs;
      s.rate = s.rate > 0.0 ? s.rate * 0.45 + inst * 0.55 : inst;
      long long rate = (long long)s.rate;
      h.net = FmtRate(rate);
      int pct = (int)((double)rate / (10.0 * 1024.0 * 1024.0) * 100.0);
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      h.netp = pct;
      ch = true;
    }
    s.rx = rx;
    s.tx = tx;
    s.at = now;
  }
  if (ch && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
}

void SplitHostPort(const std::wstring& dest, std::string& host, int& port) {
  std::string s = W2U8(dest);
  size_t c = s.rfind(':');
  if (c == std::string::npos) {
    host = TrimAscii(s);
    port = 0;
    return;
  }
  host = TrimAscii(s.substr(0, c));
  port = atoi(s.c_str() + c + 1);
}

bool ProbeLocal(int port, int timeoutMs = 350);

struct ProbeJob {
  int hostId = 0;
  int tunId = 0;
  int sid = -1;
  int port = 0;
};

struct ProbeResult {
  int hostId = 0;
  int tunId = 0;
  int ok = 0;
  int sid = -1;
  int port = 0;
};

std::mutex g_probeMtx;
std::condition_variable g_probeCv;
std::deque<ProbeJob> g_probeJobs;
std::thread g_probeWorker;
bool g_probeStop = false;

std::mutex g_probeDoneMtx;
std::deque<ProbeResult> g_probeDone;

void ProbeWorker() {
  sk::SshService& svc = sk::SshService::Instance();
  for (;;) {
    ProbeJob job;
    {
      std::unique_lock<std::mutex> lk(g_probeMtx);
      g_probeCv.wait(lk, [] { return g_probeStop || !g_probeJobs.empty(); });
      if (g_probeStop) return;
      job = g_probeJobs.front();
      g_probeJobs.pop_front();
    }
    int baseAcc = svc.ForwardAcceptsOf(job.sid, job.port);
    int baseFail = svc.ForwardFailuresOf(job.sid, job.port);
    int ok = 0;
    if (ProbeLocal(job.port, 160)) {
      ok = 1;
      for (int i = 0; i < 10; ++i) {
        Sleep(30);
        if (svc.ForwardFailuresOf(job.sid, job.port) > baseFail) {
          ok = 0;
          break;
        }
        if (svc.ForwardAcceptsOf(job.sid, job.port) > baseAcc) {
          ok = 1;
          break;
        }
      }
    }
    {
      std::lock_guard<std::mutex> lk(g_probeDoneMtx);
      g_probeDone.push_back(ProbeResult{ job.hostId, job.tunId, ok, job.sid, job.port });
    }
    if (g_hwnd != nullptr) PostMessageW(g_hwnd, WM_APP + 42, 0, 0);
  }
}

void ProbeStop() {
  {
    std::lock_guard<std::mutex> lk(g_probeMtx);
    g_probeStop = true;
    g_probeJobs.clear();
  }
  g_probeCv.notify_all();
  if (!g_probeWorker.joinable()) return;
  HANDLE h = (HANDLE)g_probeWorker.native_handle();
  if (WaitForSingleObject(h, 1500) == WAIT_OBJECT_0) {
    g_probeWorker.join();
  } else {
    g_probeWorker.detach();
  }
}

void QueueProbe(int hostId, int tunId, int sid, int port) {
  {
    std::lock_guard<std::mutex> lk(g_probeMtx);
    for (size_t i = 0; i < g_probeJobs.size(); ++i) {
      if (g_probeJobs[i].tunId == tunId) return;
    }
    g_probeJobs.push_back(ProbeJob{ hostId, tunId, sid, port });
  }
  if (!g_probeWorker.joinable()) g_probeWorker = std::thread(ProbeWorker);
  g_probeCv.notify_one();
}

void PumpProbe() {
  std::deque<ProbeResult> done;
  {
    std::lock_guard<std::mutex> lk(g_probeDoneMtx);
    if (g_probeDone.empty()) return;
    done.swap(g_probeDone);
  }
  bool changed = false;
  long long now = (long long)GetTickCount64();
  for (size_t i = 0; i < done.size(); ++i) {
    const ProbeResult& d = done[i];
    for (size_t hi = 0; hi < g_state.hosts.size(); ++hi) {
      if (g_state.hosts[hi].id != d.hostId) continue;
      std::vector<sk::Tunnel>& list = g_state.hosts[hi].tunnels;
      for (size_t k = 0; k < list.size(); ++k) {
        sk::Tunnel& tn = list[k];
        if (tn.id != d.tunId) continue;
        if (d.ok) {
          tn.state = sk::TunState::Up;
          tn.probed = 2;
          tn.fails = 0;
          tn.retryIn = 0;
          tn.nextAt = now + 3000;
        } else {
          ++tn.fails;
          tn.probed = 0;
          if (tn.fails < 3) {
            tn.state = sk::TunState::Retry;
            tn.retryIn = 3;
            tn.nextAt = now + 2000;
          } else {
            tn.fails = 0;
            tn.state = sk::TunState::Retry;
            tn.retryIn = 15;
            tn.nextAt = now + 15000;
            if (d.sid >= 0 && d.port > 0) sk::SshService::Instance().ForwardStopLocal(d.sid, d.port);
          }
        }
        changed = true;
        break;
      }
      break;
    }
  }
  if (changed && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
}

void PatrolTunnels() {
  ULONGLONG now = GetTickCount64();
  if (now < (ULONGLONG)g_state.tunNextAt) return;
  g_state.tunNextAt = (long long)(now + 3000);
  sk::SshService& svc = sk::SshService::Instance();
  bool changed = false;
  for (size_t hi = 0; hi < g_state.hosts.size(); ++hi) {
    sk::Host& h = g_state.hosts[hi];
    int sid = h.sessionId;
    if (sid < 0) {
      int tabIdx = g_state.FindTabByHost((int)hi);
      if (tabIdx >= 0) sid = g_state.tabs[(size_t)tabIdx].sessionId;
    }
    bool live = sid >= 0 && svc.IsConnected(sid);
    bool wantOn = false;
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      if (h.tunnels[k].on) {
        wantOn = true;
        break;
      }
    }
    if (wantOn && !live && h.retryAt <= 0 && !h.manualStop && h.state != sk::HostState::Busy) {
      ScheduleReconnect((int)hi);
    }
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      sk::Tunnel& tn = h.tunnels[k];
      int port = _wtoi(tn.port.c_str());
      if (port <= 0 || port > 65535) continue;
      if (!tn.on) {
        if (tn.state != sk::TunState::Down) {
          svc.ForwardStopLocal(sid, port);
          tn.state = sk::TunState::Down;
          tn.retryIn = 0;
          tn.probed = 0;
          tn.fails = 0;
          tn.lastAcc = 0;
          tn.lastFails = 0;
          changed = true;
        }
        continue;
      }
      if (!live) {
        if (tn.state != sk::TunState::Retry || tn.retryIn != -1) {
          tn.state = sk::TunState::Retry;
          tn.retryIn = -1;
          tn.probed = 0;
          tn.lastAcc = 0;
          tn.lastFails = 0;
          changed = true;
        }
        continue;
      }
      if (tn.IsRemote()) {
        if (tn.state != sk::TunState::Listen) {
          tn.state = sk::TunState::Listen;
          changed = true;
        }
        continue;
      }
      if (!svc.ForwardActive(sid, port)) {
        if ((long long)now < tn.nextAt) continue;
        std::string th;
        int tp = 0;
        SplitHostPort(tn.dest, th, tp);
        if (tp > 0 && !th.empty() && svc.ForwardStartLocal(sid, port, th, tp)) {
          tn.state = sk::TunState::Listen;
          tn.fails = 0;
          tn.retryIn = 0;
          tn.probed = 0;
          tn.nextAt = (long long)(now + 600);
          tn.lastAcc = svc.ForwardAcceptsOf(sid, port);
          tn.lastFails = svc.ForwardFailuresOf(sid, port);
          changed = true;
        } else {
          ++tn.fails;
          int wait = tn.fails <= 1 ? 5 : (tn.fails == 2 ? 10 : 15);
          tn.state = sk::TunState::Retry;
          tn.retryIn = wait;
          tn.probed = 0;
          tn.nextAt = (long long)(now + (ULONGLONG)wait * 1000ULL);
          changed = true;
        }
        continue;
      }
      int acc = svc.ForwardAcceptsOf(sid, port);
      int fl = svc.ForwardFailuresOf(sid, port);
      if (fl > tn.lastFails) {
        tn.lastFails = fl;
        tn.lastAcc = acc;
        ++tn.fails;
        if (tn.fails < 3) {
          continue;
        }
        tn.fails = 0;
        tn.state = sk::TunState::Retry;
        tn.retryIn = 5;
        tn.probed = 0;
        tn.nextAt = (long long)(now + 5000);
        svc.ForwardStopLocal(sid, port);
        changed = true;
        continue;
      }
      if (acc > tn.lastAcc) {
        tn.lastAcc = acc;
        tn.probed = 2;
        tn.fails = 0;
        tn.retryIn = 0;
        tn.nextAt = (long long)(now + 3000);
        if (tn.state != sk::TunState::Up) {
          tn.state = sk::TunState::Up;
          changed = true;
        }
        continue;
      }
      if (tn.state == sk::TunState::Up) continue;
      tn.probed = 1;
      tn.nextAt = (long long)now + 4000;
      QueueProbe(h.id, tn.id, sid, port);
      changed = true;
    }
  }
  if (changed && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
}

bool ClipboardSet(const std::wstring& text);
bool ClipboardCopy(const std::wstring& text);

bool ProbeLocal(int port, int timeoutMs) {
  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((u_short)port);
  a.sin_addr.s_addr = inet_addr("127.0.0.1");
  u_long nb = 1;
  ::ioctlsocket(s, FIONBIO, &nb);
  bool ok = false;
  if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) {
    ok = true;
  } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
    fd_set wf;
    fd_set ef;
    FD_ZERO(&wf);
    FD_ZERO(&ef);
    FD_SET(s, &wf);
    FD_SET(s, &ef);
  timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = (long)timeoutMs * 1000;
    int r = ::select(0, nullptr, &wf, &ef, &tv);
    if (r > 0 && FD_ISSET(s, &wf) && !FD_ISSET(s, &ef)) {
      int err = 0;
      int len = (int)sizeof(err);
      ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
      ok = err == 0;
    }
  }
  ::closesocket(s);
  return ok;
}

void BrowseTunnel(int index) {
  long long now = (long long)GetTickCount64();
  int hi = g_state.CurrentHostId() >= 0 ? g_state.CurrentHostId() : g_state.ActiveHostIndex();
  if (hi < 0 || hi >= (int)g_state.hosts.size()) return;
  sk::Host& h = g_state.hosts[(size_t)hi];
  if (index < 0 || index >= (int)h.tunnels.size()) return;
  sk::Tunnel& tn = h.tunnels[(size_t)index];
  int port = _wtoi(tn.port.c_str());
  if (port <= 0 || port > 65535) {
    g_state.toast = sk::Tr(L"转发端口无效");
    g_state.toastAt = now;
    g_state.toastKind = 1;
    return;
  }
  int sid = g_state.CurrentSessionId();
  sk::SshService& svc = sk::SshService::Instance();
  if (sid < 0 || !svc.IsConnected(sid)) {
    ScheduleReconnect(hi);
    g_state.toast = sk::Tr(L"会话未连接，通道不可用");
    g_state.toastAt = now;
    g_state.toastKind = 1;
    return;
  }
  if (tn.IsRemote()) {
    g_state.toast = sk::Tr(L"远程转发不支持本地浏览");
    g_state.toastAt = now;
    g_state.toastKind = 1;
    return;
  }
  if (!svc.ForwardActive(sid, port)) {
    std::string th;
    int tp = 0;
    SplitHostPort(tn.dest, th, tp);
    if (tp > 0 && !th.empty()) {
      svc.ForwardStartLocal(sid, port, th, tp);
      tn.state = sk::TunState::Listen;
      tn.nextAt = now + 300;
      tn.retryIn = 0;
      tn.probed = 0;
      tn.lastAcc = svc.ForwardAcceptsOf(sid, port);
      tn.lastFails = svc.ForwardFailuresOf(sid, port);
      if (g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
      Sleep(300);
    }
  }
  bool up = ProbeLocal(port);
  tn.state = up ? sk::TunState::Up : sk::TunState::Retry;
  tn.retryIn = 0;
  tn.fails = 0;
  tn.probed = up ? 2 : 0;
  tn.lastAcc = svc.ForwardAcceptsOf(sid, port);
  tn.lastFails = svc.ForwardFailuresOf(sid, port);
  tn.nextAt = now + 3000;
  g_state.refreshAt = now;
  g_state.dirty = true;
  if (!up) {
    g_state.toast = sk::Tr(L"通道未连通：本地 ") + std::to_wstring(port) +
                    sk::Tr(L" 无响应，目标服务可能未启动");
    g_state.toastAt = now;
    g_state.toastKind = 1;
    return;
  }
  g_state.chanPort = port;
  g_state.chanAddr = L"127.0.0.1:" + std::to_wstring(port);
  g_state.chanNote = tn.dest + L" · " + sk::Tr(L"通道已连通");
  g_state.modal = sk::ModalKind::Channel;
  g_state.modalTitle = sk::Tr(L"本地通道");
  g_state.modalNote = sk::Tr(L"转发已建立，可直接访问本地端口");
}

void OpenChannelUrl(int port) {
  if (port <= 0) return;
  std::wstring url = L"http://127.0.0.1:" + std::to_wstring(port);
  HINSTANCE r = ShellExecuteW(g_hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  g_state.toast = (INT_PTR)r > 32 ? (sk::Tr(L"已在浏览器打开 ") + url)
                                  : (sk::Tr(L"无法打开浏览器，地址已复制"));
  if ((INT_PTR)r <= 32) ClipboardSet(url);
  g_state.toastAt = (long long)GetTickCount64();
  g_state.toastKind = (INT_PTR)r > 32 ? 0 : 1;
  g_state.modal = sk::ModalKind::None;
}

void RunBackup() {
  int kind = g_state.pendingBackup;
  g_state.pendingBackup = 0;
  if (kind == 0) return;
  bool encrypt = !g_state.backupPlain;
  std::wstring pwd = g_state.backupPass;
  g_state.backupPass.clear();

  wchar_t file[MAX_PATH * 2] = { 0 };
  if (kind == 1) {
    std::wstring def = sk::BackupDefaultName();
    wcsncpy_s(file, MAX_PATH * 2, def.c_str(), _TRUNCATE);
  }
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = g_hwnd;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH * 2;
  ofn.lpstrFilter = L"轻舟备份\0*.skbk\0所有文件\0*.*\0";
  ofn.lpstrDefExt = L"skbk";
  ofn.lpstrTitle = kind == 1 ? sk::Tr(L"导出备份") : sk::Tr(L"导入备份");
  ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
              (kind == 1 ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  BOOL picked = kind == 1 ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
  if (!picked) return;
  std::wstring path(file);

  g_state.toastKind = 0;
  if (kind == 1) {
    std::string payload = sk::BuildBackupPayload(g_state);
    std::string text = sk::SealBackup(payload, pwd, encrypt);
    if (text.empty() || !sk::WriteTextFile(path, text)) {
      g_state.toast = sk::Tr(L"导出失败，请检查目标目录是否可写");
      g_state.toastKind = 1;
    } else {
      wchar_t msg[128];
      swprintf_s(msg, sk::Tr(L"已导出 %d 台主机"), (int)g_state.hosts.size());
      g_state.toast = msg;
    }
  } else {
    std::string text;
    if (!sk::ReadTextFile(path, text)) {
      g_state.toast = sk::Tr(L"无法读取备份文件");
      g_state.toastKind = 1;
    } else {
      std::string payload;
      if (!sk::OpenBackup(text, pwd, payload)) {
        g_state.toast = sk::Tr(L"密码错误或备份文件已损坏");
        g_state.toastKind = 1;
      } else {
        sk::MergeStat stat;
        std::wstring err;
        if (!sk::MergeBackupPayload(payload, g_state, stat, err)) {
          g_state.toast = err;
          g_state.toastKind = 1;
        } else {
          sk::SaveStore(g_state);
          g_state.dirty = false;
          wchar_t msg[160];
          swprintf_s(msg, sk::Tr(L"新增 %d 台，跳过 %d 台，密钥 %d 个"), stat.added,
                     stat.skipped, stat.keys);
          g_state.toast = msg;
        }
      }
    }
  }
  g_state.toastAt = (long long)GetTickCount64();
}

bool LoadLocalDir(const std::wstring& dir, std::vector<sk::SftpEntry>& out, std::wstring& err) {
  out.clear();
  std::wstring base = dir;
  if (base.size() > 3 && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
  if (base.size() > 2 && base[1] == L':' && base.size() == 2) base += L"\\";
  std::wstring pattern = base + L"\\*";
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    err = sk::Tr(L"无法打开本地目录");
    return false;
  }
  std::wstring parent = base;
  size_t sep = parent.find_last_of(L"\\/");
  if (sep != std::wstring::npos) {
    if (sep <= 2) parent = parent.substr(0, 3);
    else parent = parent.substr(0, sep);
  }
  out.push_back({ L"..", L"—", L"—", true });
  std::vector<sk::SftpEntry> dirs;
  std::vector<sk::SftpEntry> files;
  do {
    if (fd.cFileName[0] == L'.' && fd.cFileName[1] == 0) continue;
    if (fd.cFileName[0] == L'.' && fd.cFileName[1] == L'.' && fd.cFileName[2] == 0) continue;
    bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    sk::SftpEntry e;
    e.name = fd.cFileName;
    e.dir = isDir;
    e.size = isDir ? std::wstring(L"—") : FmtBytes(((unsigned long long)fd.nFileSizeHigh << 32) |
                                                   fd.nFileSizeLow);
    ULARGE_INTEGER u{};
    u.LowPart = fd.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
    long long sec = (long long)(u.QuadPart / 10000000ULL) - 11644473600LL;
    e.time = FmtFileTime(sec < 0 ? 0 : (unsigned long long)sec);
    if (isDir) dirs.push_back(e);
    else files.push_back(e);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  for (size_t i = 0; i < dirs.size(); ++i) out.push_back(dirs[i]);
  for (size_t i = 0; i < files.size(); ++i) out.push_back(files[i]);
  return true;
}

std::wstring JoinRemote(const std::wstring& base, const std::wstring& name) {
  if (base.empty() || base == L"/") return std::wstring(L"/") + name;
  if (base.back() == L'/') return base + name;
  return base + L"/" + name;
}

std::wstring RemoteParent(const std::wstring& path) {
  if (path.empty() || path == L"/") return L"/";
  std::wstring p = path;
  if (p.size() > 1 && p.back() == L'/') p.pop_back();
  size_t pos = p.find_last_of(L'/');
  if (pos == std::wstring::npos || pos == 0) return L"/";
  return p.substr(0, pos);
}

void SftpLoadLocal() {
  std::wstring err;
  if (!LoadLocalDir(g_state.sftpLocalPath, g_state.sftpLocal, err)) {
    g_state.sftpStatus = err;
    return;
  }
  g_state.sftpSelLocal = -1;
}

void SftpNotify(const std::wstring& msg, bool err) {
  g_state.sftpStatus = msg;
  g_state.toast = msg;
  g_state.toastAt = (long long)GetTickCount64();
  g_state.toastKind = err ? 1 : 0;
}

std::wstring WithReason(const std::wstring& head, const std::string& err) {
  if (err.empty()) return head;
  return head + L" · " + U82W(err);
}

bool XferBusy();

struct ListJob {
  int gen = 0;
  int sid = -1;
  std::wstring path;
};

struct ListResult {
  int gen = 0;
  int sid = -1;
  std::wstring path;
  bool ok = false;
  std::string err;
  std::vector<sk::RemoteEntry> list;
};

std::mutex g_lsMtx;
std::condition_variable g_lsCv;
std::deque<ListJob> g_lsJobs;
std::thread g_lsWorker;
bool g_lsStop = false;
std::mutex g_lsDoneMtx;
std::deque<ListResult> g_lsDone;
int g_lsGen = 0;

void ListWorker() {
  sk::SshService& svc = sk::SshService::Instance();
  for (;;) {
    ListJob job;
    {
      std::unique_lock<std::mutex> lk(g_lsMtx);
      g_lsCv.wait(lk, [] { return g_lsStop || !g_lsJobs.empty(); });
      if (g_lsStop) return;
      job = g_lsJobs.front();
      g_lsJobs.pop_front();
    }
    std::string home;
    std::wstring path = job.path;
    if (path.empty()) {
      if (!svc.SftpHome(job.sid, home) || home.empty() || home == "/") {
        std::string out;
        if (svc.Exec(job.sid, "cd ~ && pwd", out)) {
          std::string t = TrimAscii(out);
          if (!t.empty()) home = t;
        }
      }
      path = home.empty() ? std::wstring(L"/") : U82W(home);
    }
    ListResult res;
    res.gen = job.gen;
    res.sid = job.sid;
    res.path = path;
    std::string err;
    res.ok = svc.SftpList(job.sid, W2U8(path), res.list, &err);
    res.err = err;
    {
      std::lock_guard<std::mutex> lk(g_lsDoneMtx);
      while (g_lsDone.size() >= 4) g_lsDone.pop_front();
      g_lsDone.push_back(std::move(res));
    }
    if (g_hwnd != nullptr) PostMessageW(g_hwnd, WM_APP + 44, 0, 0);
    if (g_lsStop) return;
  }
}

void ListStop() {
  {
    std::lock_guard<std::mutex> lk(g_lsMtx);
    g_lsStop = true;
    g_lsJobs.clear();
  }
  g_lsCv.notify_all();
  if (!g_lsWorker.joinable()) return;
  HANDLE h = (HANDLE)g_lsWorker.native_handle();
  if (WaitForSingleObject(h, 1500) == WAIT_OBJECT_0) {
    g_lsWorker.join();
  } else {
    g_lsWorker.detach();
  }
}

void SftpLoadRemote() {
  int sid = g_state.CurrentSessionId();
  int curHost = g_state.CurrentHostId();
  if (curHost != g_state.sftpPathHost) {
    g_state.sftpPathHost = curHost;
    g_state.sftpPath.clear();
    g_state.sftpScrollL = 0.0f;
    g_state.sftpScrollR = 0.0f;
  }
  if (sid < 0) {
    g_state.sftpStatus = sk::Tr(L"未连接");
    g_state.sftpRemote.clear();
    return;
  }
  sk::SshService& svc = sk::SshService::Instance();
  if (!svc.IsConnected(sid)) {
    g_state.sftpStatus = sk::Tr(L"会话未连接");
    g_state.sftpRemote.clear();
    return;
  }
  if (!g_lsWorker.joinable()) g_lsWorker = std::thread(ListWorker);
  ListJob job;
  job.gen = ++g_lsGen;
  job.sid = sid;
  job.path = g_state.sftpPath;
  {
    std::lock_guard<std::mutex> lk(g_lsMtx);
    g_lsJobs.clear();
    g_lsJobs.push_back(job);
  }
  g_lsCv.notify_all();
  g_state.sftpStatus = sk::Tr(L"读取中…");
  g_state.refreshAt = (long long)GetTickCount64();
}

void PumpSftpList() {
  std::deque<ListResult> done;
  {
    std::lock_guard<std::mutex> lk(g_lsDoneMtx);
    done.swap(g_lsDone);
  }
  bool ch = false;
  for (size_t k = 0; k < done.size(); ++k) {
    ListResult& res = done[k];
    if (res.gen != g_lsGen) continue;
    if (g_state.CurrentSessionId() != res.sid) continue;
    ch = true;
    bool moved = (g_state.sftpPath != res.path);
    g_state.sftpPath = res.path;
    if (moved) {
      g_state.sftpScrollR = 0.0f;
      g_state.sftpSelRemote = -1;
    }
    if (!res.ok) {
      g_state.sftpStatus = res.err.empty() ? std::wstring(sk::Tr(L"远程目录读取失败"))
                                           : std::wstring(sk::Tr(L"读取失败 ")) + U82W(res.err);
      g_state.sftpRemote.clear();
      continue;
    }
    g_state.sftpRemote.clear();
    g_state.sftpRemote.push_back({ L"..", L"—", L"—", true, 0, 0 });
    std::vector<sk::SftpEntry> dirs;
    std::vector<sk::SftpEntry> files;
    for (size_t i = 0; i < res.list.size(); ++i) {
      sk::SftpEntry e;
      e.name = U82W(res.list[i].name);
      e.dir = res.list[i].dir;
      e.size = res.list[i].dir ? std::wstring(L"—") : FmtBytes(res.list[i].size);
      e.time = FmtFileTime(res.list[i].mtime);
      e.mode = res.list[i].mode;
      e.rawSize = res.list[i].size;
      if (res.list[i].dir) {
        dirs.push_back(e);
      } else {
        files.push_back(e);
      }
    }
    for (size_t i = 0; i < dirs.size(); ++i) g_state.sftpRemote.push_back(dirs[i]);
    for (size_t i = 0; i < files.size(); ++i) g_state.sftpRemote.push_back(files[i]);
    g_state.sftpSelRemote = -1;
    g_state.sftpStatus = sk::Tr(L"已读取 ") + std::to_wstring(res.list.size()) + sk::Tr(L" 项");
  }
  if (ch && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
}

struct XferRow {
  int kind = 0;
  int sid = -1;
  int host = -1;
  std::wstring name;
  std::wstring localPath;
  std::string remotePath;
  int state = 0;
  int pct = 0;
  unsigned long long done = 0;
  unsigned long long total = 0;
  bool paused = false;
  bool cancelReq = false;
  std::wstring err;
};

unsigned long long g_xfSpeed = 0;
unsigned long long g_xfEta = 0;

struct FlowState {
  std::atomic<long long> linkBps{0};
  std::atomic<long long> lastAct{0};
  std::atomic<long long> keyAt{0};
  std::atomic<int> rttMs{0};
  double factor = 1.0;
  long long lastLimit = 0;
};

std::mutex g_flowMtx;
std::map<int, std::shared_ptr<FlowState>> g_flows;

std::shared_ptr<FlowState> FlowOf(int host) {
  std::lock_guard<std::mutex> lk(g_flowMtx);
  std::shared_ptr<FlowState>& f = g_flows[host];
  if (!f) f = std::make_shared<FlowState>();
  return f;
}

void FlowTouch() {
  FlowOf(g_state.CurrentHostId())->lastAct.store((long long)GetTickCount64());
}

void FlowKey() {
  std::shared_ptr<FlowState> f = FlowOf(g_state.CurrentHostId());
  long long now = (long long)GetTickCount64();
  f->lastAct.store(now);
  f->keyAt.store(now);
}

void FlowRtt() {
  std::shared_ptr<FlowState> f = FlowOf(g_state.CurrentHostId());
  long long now = (long long)GetTickCount64();
  long long k = f->keyAt.exchange(0);
  if (k <= 0 || now <= k) return;
  long long ms = now - k;
  if (ms > 5000) return;
  int prev = f->rttMs.load();
  int v = prev > 0 ? (int)((prev * 3 + ms) / 4) : (int)ms;
  f->rttMs.store(v);
}

void FlowTick(int host, int sid) {
  if (sid < 0) return;
  std::shared_ptr<FlowState> f = FlowOf(host);
  long long link = f->linkBps.load();
  if (link <= 0) return;
  long long now = (long long)GetTickCount64();
  bool adaptive = g_state.xferAdaptive;
  bool active = (now - f->lastAct.load()) < 2000;
  int rtt = f->rttMs.load();

  double target = 1.0;
  if (adaptive) {
    if (active) target = 0.35;
    if (rtt > 300) target *= 0.6;
    else if (rtt > 150) target *= 0.85;
  }
  if (target < f->factor) {
    f->factor = target;
  } else if (target > f->factor) {
    f->factor += 0.12;
    if (f->factor > target) f->factor = target;
  }

  long long lim = adaptive ? (long long)((double)link * f->factor) : 0;
  if (lim > 0 && lim < 32768) lim = 32768;
  if (lim == f->lastLimit) return;
  f->lastLimit = lim;
  sk::SshService::Instance().SetUploadLimit(sid, lim);
}

std::wstring FmtDur(unsigned long long sec) {
  if (sec >= 3600) {
    return std::to_wstring((int)(sec / 3600)) + L"h" + std::to_wstring((int)((sec % 3600) / 60)) + L"m";
  }
  if (sec >= 60) return std::to_wstring((int)(sec / 60)) + L"m" + std::to_wstring((int)(sec % 60)) + L"s";
  return std::to_wstring((int)sec) + L"s";
}

struct XferJob {
  int idx = -1;
  int sid = -1;
  int host = -1;
  int kind = 0;
  std::wstring localPath;
  std::string remotePath;
};

struct XferLane {
  std::mutex mtx;
  std::condition_variable cv;
  std::deque<XferJob> jobs;
  std::thread th;
  bool stop = false;

  std::mutex runMtx;
  int runIdx = -1;
  int pct = 0;
  unsigned long long done = 0;
  unsigned long long total = 0;

  unsigned long long speed = 0;
  std::deque<std::pair<ULONGLONG, unsigned long long>> hist;
  int prevIdx = -1;
};

std::mutex g_xfLaneMtx;
std::map<int, std::shared_ptr<XferLane>> g_xfLanes;

std::shared_ptr<XferLane> XferLaneOf(int host) {
  std::lock_guard<std::mutex> lk(g_xfLaneMtx);
  std::shared_ptr<XferLane>& l = g_xfLanes[host];
  if (!l) l = std::make_shared<XferLane>();
  return l;
}

std::vector<std::pair<int, std::shared_ptr<XferLane>>> XferLaneList() {
  std::vector<std::pair<int, std::shared_ptr<XferLane>>> ls;
  std::lock_guard<std::mutex> lk(g_xfLaneMtx);
  for (std::map<int, std::shared_ptr<XferLane>>::iterator it = g_xfLanes.begin(); it != g_xfLanes.end(); ++it) {
    ls.push_back(*it);
  }
  return ls;
}

std::mutex g_xfDoneMtx;
std::deque<std::pair<int, std::wstring>> g_xfDone;

std::vector<XferRow> g_xfRows;
bool g_xfNeedRemote = false;
bool g_xfNeedLocal = false;

void XferWorker(std::shared_ptr<XferLane> lane) {
  sk::SshService& svc = sk::SshService::Instance();
  for (;;) {
    XferJob job;
    {
      std::unique_lock<std::mutex> lk(lane->mtx);
      lane->cv.wait(lk, [&lane] { return lane->stop || !lane->jobs.empty(); });
      if (lane->stop) return;
      job = lane->jobs.front();
      lane->jobs.pop_front();
    }
    {
      std::lock_guard<std::mutex> lk(lane->runMtx);
      lane->runIdx = job.idx;
      lane->pct = 0;
      lane->done = 0;
      lane->total = 0;
    }
    sk::SshService::ProgressFn prog = [lane](unsigned long long d, unsigned long long t) {
      std::lock_guard<std::mutex> lk(lane->runMtx);
      lane->done = d;
      lane->total = t;
      int pct = t > 0 ? (int)((d * 100ULL) / t) : 0;
      lane->pct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    };
    std::string err;
    bool ok = false;
    if (job.kind == 0) {
      ok = svc.SftpUpload(job.sid, job.localPath, job.remotePath, err, prog);
    } else {
      ok = svc.SftpDownload(job.sid, job.remotePath, job.localPath, err, prog);
    }
    svc.ReapTransferSession();
    {
      std::lock_guard<std::mutex> lk(lane->runMtx);
      lane->runIdx = -1;
    }
    {
      std::lock_guard<std::mutex> lk(g_xfDoneMtx);
      g_xfDone.push_back({ job.idx, ok ? std::wstring() : U82W(err) });
    }
    if (g_hwnd != nullptr) PostMessageW(g_hwnd, WM_APP + 41, 0, 0);
  }
}

void XferSubmit(const XferJob& job) {
  std::shared_ptr<XferLane> lane = XferLaneOf(job.host);
  bool needStart = false;
  {
    std::lock_guard<std::mutex> lk(lane->mtx);
    lane->jobs.push_back(job);
    if (!lane->th.joinable()) {
      lane->stop = false;
      needStart = true;
    }
  }
  if (needStart) {
    lane->th = std::thread(XferWorker, lane);
    SK_LOGI("xfer lane start host=%d", job.host);
  }
  SK_LOGI("xfer submit host=%d idx=%d kind=%d", job.host, job.idx, job.kind);
  lane->cv.notify_one();
}

void XferRequestStop() {
  std::vector<std::pair<int, std::shared_ptr<XferLane>>> ls = XferLaneList();
  for (size_t i = 0; i < ls.size(); ++i) {
    {
      std::lock_guard<std::mutex> lk(ls[i].second->mtx);
      ls[i].second->stop = true;
      ls[i].second->jobs.clear();
    }
    ls[i].second->cv.notify_all();
  }
}

void XferStop() {
  XferRequestStop();
  std::vector<std::pair<int, std::shared_ptr<XferLane>>> ls = XferLaneList();
  for (size_t i = 0; i < ls.size(); ++i) {
    std::shared_ptr<XferLane> lane = ls[i].second;
    if (!lane->th.joinable()) continue;
    HANDLE h = (HANDLE)lane->th.native_handle();
    if (WaitForSingleObject(h, 1500) == WAIT_OBJECT_0) {
      lane->th.join();
    } else {
      lane->th.detach();
    }
  }
}

bool XferBusy() {
  for (size_t i = 0; i < g_xfRows.size(); ++i) {
    if (g_xfRows[i].state < 2) return true;
  }
  return false;
}

void XferRebuild() {
  g_state.sftpQueue.clear();
  g_state.sftpQueueIdx.clear();
  g_state.sftpQueueState.clear();
  size_t n = g_xfRows.size();
  size_t start = n > 10 ? n - 10 : 0;
  int curHost = g_state.CurrentHostId();
  for (size_t i = start; i < n; ++i) {
    const XferRow& r = g_xfRows[i];
    if (r.host != curHost) continue;
    std::wstring line = r.kind == 0 ? L"↑ " : L"↓ ";
    line += r.name;
    line += L" · ";
    if (r.state == 0) {
      line += sk::Tr(L"排队中");
    } else if (r.state == 1) {
      if (r.paused) {
        line += sk::Tr(L"已暂停");
        if (r.total > 0) line += L"  " + FmtBytes(r.done) + L" / " + FmtBytes(r.total);
      } else {
        line += std::to_wstring(r.pct) + L"%";
        if (r.total > 0) line += L"  " + FmtBytes(r.done) + L" / " + FmtBytes(r.total);
        if (g_xfSpeed > 0) line += L"  " + FmtBytes(g_xfSpeed) + L"/s";
        if (g_xfEta > 0) line += L"  " + std::wstring(sk::Tr(L"剩")) + L" " + FmtDur(g_xfEta);
      }
    } else if (r.state == 2) {
      line += sk::Tr(L"完成");
    } else {
      line += sk::Tr(L"失败");
      if (!r.err.empty()) line += L" " + r.err;
      line += L" · ";
      line += sk::Tr(L"点击重试");
    }
    g_state.sftpQueue.push_back(line);
    g_state.sftpQueueIdx.push_back((int)i);
    g_state.sftpQueueState.push_back(r.state);
  }
}

void UpdateXferBar() {
  g_state.sftpBar.clear();
  if (g_xfRows.empty()) return;
  int curHost = g_state.CurrentHostId();
  int running = -1;
  int wait = 0;
  for (size_t i = 0; i < g_xfRows.size(); ++i) {
    const XferRow& r = g_xfRows[i];
    if (r.host != curHost) continue;
    if (r.state == 1) running = (int)i;
    else if (r.state == 0) ++wait;
  }
  std::wstring s;
  if (running >= 0) {
    const XferRow& r = g_xfRows[(size_t)running];
    s = r.kind == 0 ? L"↑ " : L"↓ ";
    s += r.name;
    s += L" " + std::to_wstring(r.pct) + L"%";
    if (r.paused) {
      s += L" · " + std::wstring(sk::Tr(L"已暂停"));
      g_xfEta = 0;
    } else if (g_xfSpeed > 0) {
      s += L" " + FmtBytes(g_xfSpeed) + L"/s";
      g_xfEta = r.total > r.done ? (r.total - r.done) / g_xfSpeed : 0;
    } else {
      g_xfEta = 0;
    }
    if (g_xfEta > 0) s += L" " + std::wstring(sk::Tr(L"剩")) + L" " + FmtDur(g_xfEta);
    long long lim = FlowOf(curHost)->lastLimit;
    if (lim > 0) {
      s += L" · ";
      s += sk::Tr(L"限");
      s += L" ";
      s += FmtBytes((unsigned long long)lim);
      s += L"/s";
    }
    if (wait > 0) s += L" · " + std::to_wstring(wait) + sk::Tr(L" 排队");
  } else if (wait > 0) {
    s = std::to_wstring(wait) + sk::Tr(L" 项排队");
  }
  int others = 0;
  for (size_t i = 0; i < g_xfRows.size(); ++i) {
    if (g_xfRows[i].host != curHost && g_xfRows[i].state < 2) ++others;
  }
  if (others > 0) {
    if (!s.empty()) s += L" · ";
    s += std::wstring(sk::Tr(L"另有 ")) + std::to_wstring(others) + sk::Tr(L" 项在其他主机");
  }
  if (s.empty()) return;
  g_state.sftpBar = s;
}

void XferRetryJob(int idx) {
  if (idx < 0 || idx >= (int)g_xfRows.size()) return;
  XferRow& r = g_xfRows[(size_t)idx];
  if (r.state != 3) return;
  XferJob job;
  job.idx = idx;
  job.sid = r.sid;
  job.host = r.host;
  job.kind = r.kind;
  job.localPath = r.localPath;
  job.remotePath = r.remotePath;
  r.state = 0;
  r.pct = 0;
  r.done = 0;
  r.err.clear();
  XferSubmit(job);
  g_state.sftpStatus = std::wstring(sk::Tr(L"重试 ")) + r.name;
  XferRebuild();
}

void XferCancelJob(int idx) {
  if (idx < 0 || idx >= (int)g_xfRows.size()) return;
  XferRow& r = g_xfRows[(size_t)idx];
  if (r.state == 3) {
    XferRetryJob(idx);
    return;
  }
  if (r.state == 1) {
    r.cancelReq = true;
    r.paused = false;
    sk::SshService::Instance().CancelTransfer(r.sid);
    g_state.sftpStatus = std::wstring(sk::Tr(L"正在取消 ")) + r.name;
    XferRebuild();
    return;
  }
  bool removed = false;
  {
    std::shared_ptr<XferLane> lane = XferLaneOf(r.host);
    std::lock_guard<std::mutex> lk(lane->mtx);
    for (std::deque<XferJob>::iterator it = lane->jobs.begin(); it != lane->jobs.end(); ++it) {
      if (it->idx != idx) continue;
      lane->jobs.erase(it);
      removed = true;
      break;
    }
  }
  if (r.state == 0) {
    r.state = 3;
    r.err = sk::Tr(L"已取消");
    g_state.sftpStatus = sk::Tr(L"已取消 ") + r.name;
    removed = true;
  }
  if (removed) XferRebuild();
}

void XferPauseJob(int idx) {
  if (idx < 0 || idx >= (int)g_xfRows.size()) return;
  XferRow& r = g_xfRows[(size_t)idx];
  if (r.state != 1) return;
  r.paused = !r.paused;
  sk::SshService::Instance().PauseTransfer(r.sid, r.paused);
  g_state.sftpStatus = (r.paused ? std::wstring(sk::Tr(L"已暂停 ")) : std::wstring(sk::Tr(L"继续 "))) + r.name;
  XferRebuild();
  UpdateXferBar();
}

void XferRowClick(int idx) {
  if (idx < 0 || idx >= (int)g_xfRows.size()) return;
  if (g_xfRows[(size_t)idx].state == 1) {
    XferPauseJob(idx);
    return;
  }
  XferCancelJob(idx);
}

void XferQueue(int kind, int sid, const std::wstring& name, const std::wstring& localPath,
               const std::string& remotePath) {
  if (!XferBusy() && g_xfRows.size() >= 12) {
    g_xfRows.clear();
    g_state.sftpQueue.clear();
  }
  XferRow row;
  row.kind = kind;
  row.sid = sid;
  row.host = g_state.CurrentHostId();
  row.name = name;
  row.localPath = localPath;
  row.remotePath = remotePath;
  g_xfRows.push_back(row);
  XferJob job;
  job.idx = (int)g_xfRows.size() - 1;
  job.sid = sid;
  job.host = row.host;
  job.kind = kind;
  job.localPath = localPath;
  job.remotePath = remotePath;
  XferSubmit(job);
  XferRebuild();
}

void XferPoll() {
  bool changed = false;
  std::deque<std::pair<int, std::wstring>> done;
  {
    std::lock_guard<std::mutex> lk(g_xfDoneMtx);
    done.swap(g_xfDone);
  }
  for (size_t i = 0; i < done.size(); ++i) {
    int idx = done[i].first;
    if (idx < 0 || idx >= (int)g_xfRows.size()) continue;
    XferRow& r = g_xfRows[(size_t)idx];
    bool cancelled = r.cancelReq;
    r.cancelReq = false;
    r.paused = false;
    if (done[i].second.empty()) {
      r.state = 2;
      r.pct = 100;
      g_state.sftpStatus = (r.kind == 0 ? std::wstring(sk::Tr(L"已上传 ")) : std::wstring(sk::Tr(L"已下载 "))) + r.name;
    } else if (cancelled) {
      r.state = 3;
      r.err = sk::Tr(L"已取消");
      g_state.sftpStatus = sk::Tr(L"已取消 ") + r.name;
    } else {
      r.state = 3;
      r.err = done[i].second;
      g_state.sftpStatus = (r.kind == 0 ? std::wstring(sk::Tr(L"上传失败 ")) : std::wstring(sk::Tr(L"下载失败 "))) + r.name;
    }
    if (r.kind == 0) {
      g_xfNeedRemote = true;
    } else {
      g_xfNeedLocal = true;
    }
    changed = true;
  }
  struct LaneSnap {
    int runIdx = -1;
    int pct = 0;
    unsigned long long done = 0;
    unsigned long long total = 0;
    unsigned long long speed = 0;
  };
  std::map<int, LaneSnap> snaps;
  {
    std::vector<std::pair<int, std::shared_ptr<XferLane>>> ls = XferLaneList();
    ULONGLONG nowTick = GetTickCount64();
    for (size_t i = 0; i < ls.size(); ++i) {
      std::shared_ptr<XferLane> lane = ls[i].second;
      LaneSnap sn;
      {
        std::lock_guard<std::mutex> lk(lane->runMtx);
        sn.runIdx = lane->runIdx;
        sn.pct = lane->pct;
        sn.done = lane->done;
        sn.total = lane->total;
      }
      if (sn.runIdx >= 0) {
        if (lane->prevIdx != sn.runIdx) {
          lane->prevIdx = sn.runIdx;
          lane->hist.clear();
          lane->hist.push_back(std::make_pair(nowTick, sn.done));
          lane->speed = 0;
        } else {
          lane->hist.push_back(std::make_pair(nowTick, sn.done));
          while (lane->hist.size() > 2 && nowTick - lane->hist.front().first > 3000) lane->hist.pop_front();
          ULONGLONG t0 = lane->hist.front().first;
          unsigned long long d0 = lane->hist.front().second;
          if (nowTick - t0 >= 800) {
            lane->speed = (sn.done > d0 ? sn.done - d0 : 0) * 1000ULL / (nowTick - t0);
          }
        }
      } else {
        lane->prevIdx = -1;
        lane->hist.clear();
        lane->speed = 0;
      }
      if (lane->speed > 0) {
        std::shared_ptr<FlowState> f = FlowOf(ls[i].first);
        long long prev = f->linkBps.load();
        long long v = prev > 0 ? (prev * 3 + (long long)lane->speed * 7) / 10 : (long long)lane->speed;
        if (v < 16384) v = 16384;
        f->linkBps.store(v);
      }
      sn.speed = lane->speed;
      snaps[ls[i].first] = sn;
    }
  }
  int curHost = g_state.CurrentHostId();
  int curRun = -1;
  g_xfSpeed = 0;
  {
    std::map<int, LaneSnap>::iterator it = snaps.find(curHost);
    if (it != snaps.end()) {
      curRun = it->second.runIdx;
      if (curRun >= 0) g_xfSpeed = it->second.speed;
    }
  }
  for (size_t i = 0; i < g_xfRows.size(); ++i) {
    XferRow& r = g_xfRows[i];
    if (r.state >= 2) continue;
    std::map<int, LaneSnap>::iterator it = snaps.find(r.host);
    bool running = (it != snaps.end() && it->second.runIdx == (int)i);
    if (running) {
      const LaneSnap& sn = it->second;
      if (r.state != 1 || r.pct != sn.pct || r.done != sn.done || r.total != sn.total) {
        r.state = 1;
        r.pct = sn.pct;
        r.done = sn.done;
        r.total = sn.total;
        changed = true;
        if (r.host == curHost) {
          g_state.sftpStatus = (r.kind == 0 ? std::wstring(sk::Tr(L"上传中 ")) : std::wstring(sk::Tr(L"下载中 "))) +
                               r.name + L" " + std::to_wstring(sn.pct) + L"%";
          if (sn.speed > 0) g_state.sftpStatus += L"  " + FmtBytes(sn.speed) + L"/s";
        }
      }
    } else if (r.state != 0) {
      r.state = 0;
      r.pct = 0;
      changed = true;
    }
  }
  if (changed) XferRebuild();
  static int qHostSeen = -2;
  if (qHostSeen != curHost) {
    qHostSeen = curHost;
    XferRebuild();
    changed = true;
  }
  std::wstring prevBar = g_state.sftpBar;
  UpdateXferBar();
  if (g_hwnd != nullptr && (changed || prevBar != g_state.sftpBar)) {
    InvalidateRect(g_hwnd, nullptr, FALSE);
  }
  bool idle = true;
  for (std::map<int, LaneSnap>::iterator it = snaps.begin(); it != snaps.end(); ++it) {
    if (it->second.runIdx >= 0) {
      idle = false;
      break;
    }
  }
  if (idle) {
    std::vector<std::pair<int, std::shared_ptr<XferLane>>> ls = XferLaneList();
    for (size_t i = 0; i < ls.size(); ++i) {
      std::lock_guard<std::mutex> lk(ls[i].second->mtx);
      if (!ls[i].second->jobs.empty()) {
        idle = false;
        break;
      }
    }
  }
  if (idle && changed) {
    bool needR = g_xfNeedRemote;
    bool needL = g_xfNeedLocal;
    g_xfNeedRemote = false;
    g_xfNeedLocal = false;
    if (needR || needL) {
      std::wstring keep = g_state.sftpStatus;
      if (needR && g_state.view == sk::View::Sftp) SftpLoadRemote();
      if (needL) SftpLoadLocal();
      g_state.sftpStatus = keep;
    }
  }
  for (std::map<int, LaneSnap>::iterator it = snaps.begin(); it != snaps.end(); ++it) {
    int rowIdx = it->second.runIdx;
    if (rowIdx < 0 || rowIdx >= (int)g_xfRows.size()) continue;
    FlowTick(it->first, g_xfRows[(size_t)rowIdx].sid);
  }
}

void RunSftp(int cmd) {
  int sid = g_state.CurrentSessionId();
  sk::SshService& svc = sk::SshService::Instance();
  if (cmd == 1 || cmd == 2) {
    if (g_state.sftpLocal.empty()) SftpLoadLocal();
    SftpLoadRemote();
    return;
  }
  if (cmd == 3) {
    if (g_state.sftpSelLocal < 0 || g_state.sftpSelLocal >= (int)g_state.sftpLocal.size()) {
      g_state.sftpStatus = sk::Tr(L"先选中一个本地文件");
      return;
    }
    const sk::SftpEntry& e = g_state.sftpLocal[(size_t)g_state.sftpSelLocal];
    if (e.dir) {
      g_state.sftpStatus = sk::Tr(L"目录暂不支持上传");
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    std::wstring local = g_state.sftpLocalPath;
    if (!local.empty() && local.back() != L'\\' && local.back() != L'/') local += L"\\";
    std::wstring remote = JoinRemote(g_state.sftpPath, e.name);
    XferQueue(0, sid, e.name, local + e.name, W2U8(remote));
    g_state.sftpStatus = sk::Tr(L"已加入队列 ") + e.name;
    return;
  }
  if (cmd == 4) {
    if (g_state.sftpSelRemote < 0 || g_state.sftpSelRemote >= (int)g_state.sftpRemote.size()) {
      g_state.sftpStatus = sk::Tr(L"先选中一个远程文件");
      return;
    }
    const sk::SftpEntry& e = g_state.sftpRemote[(size_t)g_state.sftpSelRemote];
    if (e.dir) {
      g_state.sftpStatus = sk::Tr(L"目录暂不支持下载");
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    std::wstring local = g_state.sftpLocalPath;
    if (!local.empty() && local.back() != L'\\' && local.back() != L'/') local += L"\\";
    std::wstring remote = JoinRemote(g_state.sftpPath, e.name);
    XferQueue(1, sid, e.name, local + e.name, W2U8(remote));
    g_state.sftpStatus = sk::Tr(L"已加入队列 ") + e.name;
    return;
  }
  if (cmd == 5) {
    if (g_state.sftpSelLocal < 0 || g_state.sftpSelLocal >= (int)g_state.sftpLocal.size()) return;
    const sk::SftpEntry& e = g_state.sftpLocal[(size_t)g_state.sftpSelLocal];
    if (!e.dir) return;
    if (e.name == L"..") {
      std::wstring base = g_state.sftpLocalPath;
      if (base.size() <= 3) return;
      while (base.size() > 3 && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
      size_t sep = base.find_last_of(L"\\/");
      if (sep == std::wstring::npos) return;
      g_state.sftpLocalPath = sep <= 2 ? base.substr(0, 3) : base.substr(0, sep);
    } else {
      std::wstring base = g_state.sftpLocalPath;
      if (!base.empty() && base.back() != L'\\' && base.back() != L'/') base += L"\\";
      g_state.sftpLocalPath = base + e.name;
    }
    SftpLoadLocal();
    return;
  }
  if (cmd == 6) {
    if (g_state.sftpSelRemote < 0 || g_state.sftpSelRemote >= (int)g_state.sftpRemote.size()) return;
    const sk::SftpEntry& e = g_state.sftpRemote[(size_t)g_state.sftpSelRemote];
    if (!e.dir) return;
    if (e.name == L"..") {
      g_state.sftpPath = RemoteParent(g_state.sftpPath);
    } else {
      g_state.sftpPath = JoinRemote(g_state.sftpPath, e.name);
    }
    g_state.sftpScrollR = 0.0f;
    SftpLoadRemote();
    return;
  }
  if (cmd == 7) {
    std::wstring name = g_state.promptValue;
    while (!name.empty() && (name.front() == L' ' || name.front() == L'/')) name.erase(name.begin());
    while (!name.empty() && name.back() == L' ') name.pop_back();
    if (name.empty()) {
      SftpNotify(sk::Tr(L"名称不能为空"), true);
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      SftpNotify(sk::Tr(L"会话未连接"), true);
      return;
    }
    std::wstring remote = JoinRemote(g_state.sftpPath, name);
    std::string err;
    if (svc.SftpMkdir(sid, W2U8(remote), &err)) {
      SftpNotify(sk::Tr(L"已创建 ") + name, false);
      SftpLoadRemote();
    } else {
      sk::LogWrite("E", "mkdir %s failed: %s", W2U8(remote).c_str(), err.c_str());
      if (err.find("SFTP Code: 3") != std::string::npos) {
        SftpNotify(sk::Tr(L"当前目录不可写：") + g_state.sftpPath +
                       sk::Tr(L" · 点工具栏「主目录」回到登录目录后重试"),
                   true);
      } else {
        SftpNotify(WithReason(sk::Tr(L"创建失败 ") + remote, err), true);
      }
    }
    return;
  }
  if (cmd == 8) {
    std::wstring target = g_state.promptTarget;
    g_state.promptTarget.clear();
    g_state.promptKind = 0;
    if (target.empty()) return;
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    bool dir = false;
    for (size_t i = 0; i < g_state.sftpRemote.size(); ++i) {
      if (g_state.sftpRemote[i].name != target) continue;
      dir = g_state.sftpRemote[i].dir;
      break;
    }
    std::wstring remote = JoinRemote(g_state.sftpPath, target);
    std::string err;
    if (svc.SftpDelete(sid, W2U8(remote), dir, &err)) {
      SftpNotify(sk::Tr(L"已删除 ") + target, false);
      for (size_t i = 0; i < g_state.sftpRemote.size(); ++i) {
        if (g_state.sftpRemote[i].name != target) continue;
        g_state.sftpRemote.erase(g_state.sftpRemote.begin() + (long)i);
        break;
      }
      g_state.sftpSelRemote = -1;
      SftpLoadRemote();
    } else {
      SftpNotify(WithReason(sk::Tr(L"删除失败 ") + target, err), true);
    }
    return;
  }
  if (cmd == 9) {
    std::wstring target = g_state.promptTarget;
    g_state.promptTarget.clear();
    g_state.promptKind = 0;
    std::wstring name = g_state.promptValue;
    while (!name.empty() && (name.front() == L' ' || name.front() == L'/')) name.erase(name.begin());
    while (!name.empty() && name.back() == L' ') name.pop_back();
    if (target.empty() || name.empty() || name == target) return;
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    std::wstring from = JoinRemote(g_state.sftpPath, target);
    std::wstring to = JoinRemote(g_state.sftpPath, name);
    std::string err;
    if (svc.SftpRename(sid, W2U8(from), W2U8(to), &err)) {
      SftpNotify(sk::Tr(L"已重命名为 ") + name, false);
      SftpLoadRemote();
    } else {
      SftpNotify(WithReason(sk::Tr(L"重命名失败 ") + target, err), true);
    }
    return;
  }
  if (cmd == 10) {
    std::wstring target = g_state.promptTarget;
    g_state.promptTarget.clear();
    g_state.promptKind = 0;
    if (target.empty()) return;
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    unsigned int mode = 0;
    int digits = 0;
    bool bad = false;
    for (size_t i = 0; i < g_state.promptValue.size(); ++i) {
      wchar_t c = g_state.promptValue[i];
      if (c == L' ') continue;
      if (c < L'0' || c > L'7' || digits >= 4) {
        bad = true;
        break;
      }
      mode = mode * 8u + (unsigned int)(c - L'0');
      ++digits;
    }
    if (bad || digits == 0) {
      SftpNotify(sk::Tr(L"权限需为 3 到 4 位八进制，如 644 或 0755"), true);
      return;
    }
    std::wstring remote = JoinRemote(g_state.sftpPath, target);
    std::string err;
    if (svc.SftpChmod(sid, W2U8(remote), mode, &err)) {
      SftpNotify(sk::Tr(L"权限已修改 ") + target, false);
      SftpLoadRemote();
    } else {
      SftpNotify(WithReason(sk::Tr(L"权限修改失败 ") + target, err), true);
    }
    return;
  }
  if (cmd == 12) {
    g_state.sftpPath = RemoteParent(g_state.sftpPath);
    g_state.sftpScrollR = 0.0f;
    SftpLoadRemote();
    return;
  }
  if (cmd == 13) {
    std::vector<std::wstring> files = g_state.pendingFiles;
    g_state.pendingFiles.clear();
    if (files.empty()) return;
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    int n = 0;
    for (size_t i = 0; i < files.size(); ++i) {
      const std::wstring& f = files[i];
      size_t sep = f.find_last_of(L"\\/");
      std::wstring name = sep == std::wstring::npos ? f : f.substr(sep + 1);
      if (name.empty()) continue;
      XferQueue(0, sid, name, f, W2U8(JoinRemote(g_state.sftpPath, name)));
      ++n;
    }
    g_state.sftpStatus = sk::Tr(L"已加入队列 ") + std::to_wstring(n) + sk::Tr(L" 个文件");
    return;
  }
}

void PumpPendingSftp() {
  if (g_state.pendingSftp == 0) return;
  int cmd = g_state.pendingSftp;
  g_state.pendingSftp = 0;
  RunSftp(cmd);
}

bool ClipboardOpen(bool write) {
  HWND owner = write ? g_hwnd : nullptr;
  for (int i = 0; i < 20; ++i) {
    if (OpenClipboard(owner)) return true;
    Sleep(30);
  }
  return false;
}

std::wstring ClipboardGet() {
  if (!ClipboardOpen(false)) return std::wstring();
  std::wstring out;
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (h != nullptr) {
    const wchar_t* src = static_cast<const wchar_t*>(GlobalLock(h));
    if (src != nullptr) {
      out = src;
      GlobalUnlock(h);
    }
  }
  CloseClipboard();
  return out;
}

bool ClipboardSet(const std::wstring& text) {
  if (text.empty()) return false;
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (mem == nullptr) return false;
  void* dst = GlobalLock(mem);
  if (dst == nullptr) {
    GlobalFree(mem);
    return false;
  }
  memcpy(dst, text.c_str(), bytes);
  GlobalUnlock(mem);
  if (!ClipboardOpen(true)) {
    GlobalFree(mem);
    return false;
  }
  bool ok = false;
  if (EmptyClipboard()) ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
  CloseClipboard();
  if (!ok) GlobalFree(mem);
  return ok;
}

bool ClipboardCopy(const std::wstring& text) {
  if (!ClipboardSet(text)) {
    SK_LOGW("clip set fail chars=%d", (int)text.size());
    return false;
  }
  std::wstring back = ClipboardGet();
  if (back != text) {
    SK_LOGW("clip verify mismatch wrote=%d read=%d", (int)text.size(), (int)back.size());
    return false;
  }
  SK_LOGI("clip copy ok chars=%d", (int)text.size());
  return true;
}

sk::FormField* EditField() {
  if (!sk::ModalIsForm(g_state) || g_state.formFocus < 0 ||
      g_state.formFocus >= (int)g_state.form.size()) return nullptr;
  sk::FormField& f = g_state.form[(size_t)g_state.formFocus];
  if (f.kind == sk::FieldKind::Seg || f.kind == sk::FieldKind::Group ||
      f.kind == sk::FieldKind::Info) return nullptr;
  return &f;
}

std::wstring* EditBuffer() {
  if (g_state.modal == sk::ModalKind::Prompt) return &g_state.promptValue;
  sk::FormField* f = EditField();
  if (f != nullptr) return &f->value;
  if (g_state.searchFocus) return &g_state.query;
  return nullptr;
}

void EditReset(bool replace) {
  if (replace) {
    std::wstring* buf = EditBuffer();
    if (buf != nullptr) buf->clear();
  }
  sk::FormField* f = EditField();
  if (f != nullptr) f->fresh = false;
  g_state.selectAll = false;
  g_state.dirty = true;
}

void EditCopy(bool cut) {
  std::wstring* buf = EditBuffer();
  if (buf == nullptr) return;
  if (!buf->empty()) ClipboardSet(*buf);
  if (cut) EditReset(true);
}

void EditPaste() {
  std::wstring* buf = EditBuffer();
  if (buf == nullptr) return;
  std::wstring clean;
  for (wchar_t c : ClipboardGet()) {
    if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    if (c < 32) continue;
    clean.push_back(c);
  }
  if (g_state.selectAll) *buf = clean;
  else buf->append(clean);
  EditReset(false);
}

struct TermGeom {
  sk::Rect area;
  float cw = 1.0f;
  float lh = 1.0f;
};

bool TermGeomOf(TermGeom& g) {
  if (g_state.view != sk::View::Term || g_state.modal != sk::ModalKind::None) return false;
  bool session = g_state.CurrentHostId() >= 0;
  if (session && g_state.sessTab != 0) return false;
  sk::Rect sidebar, tabs, content, status;
  sk::LayoutMetrics(g_state, g_client, sidebar, tabs, content, status);
  sk::Rect inner = session ? sk::SessInnerRect(g_state, content) : content;
  float px = g_state.S(sk::L::TermPadX);
  float py = g_state.S(sk::L::TermPadY);
  g.area = sk::Rect{ inner.l + px, inner.t + py, inner.r - px, inner.b - py };
  if (sk::SessionHub::Instance().Find(g_state.CurrentSessionId()) == nullptr) return false;
  float scale = g_render.Dpi() > 0.0f ? g_render.Dpi() / 96.0f : 1.0f;
  float fs = (g_state.termFontSize > 0.0f ? g_state.termFontSize : sk::L::TermFontSize) * scale;
  g.cw = g_render.CharW(sk::FontId::Mono) * fs / 13.0f;
  if (!(g.cw > 0.0f)) g.cw = fs * 0.6f;
  if (!(g.cw > 0.0f)) g.cw = 7.0f;
  g.lh = fs * sk::L::TermLineGap;
  if (!(g.lh > 0.0f)) g.lh = fs * 1.4f;
  return true;
}

void TermClamp(float x, float y, const TermGeom& g, sk::Terminal* term, int& row, int& col) {
  row = (int)((y - g.area.t) / g.lh);
  col = (int)((x - g.area.l) / g.cw);
  if (row < 0) row = 0;
  if (col < 0) col = 0;
  if (row >= term->Rows()) row = term->Rows() - 1;
  if (col >= term->Cols()) col = term->Cols() - 1;
}

bool TermPoint(float x, float y, sk::Terminal*& term, int& row, int& col) {
  TermGeom g;
  if (!TermGeomOf(g) || !g.area.Hit(x, y)) return false;
  term = sk::SessionHub::Instance().Find(g_state.CurrentSessionId());
  if (term == nullptr) return false;
  TermClamp(x, y, g, term, row, col);
  return true;
}

std::map<int, int> g_ptySize;

void ApplyTermGrid() {
  TermGeom g;
  if (!TermGeomOf(g)) return;
  if (!(g_client.W() > 120.0f) || !(g_client.H() > 120.0f)) return;
  int sid = g_state.CurrentSessionId();
  sk::Terminal* term = sk::SessionHub::Instance().Find(sid);
  if (term == nullptr) return;
  int cols = (int)(g.area.W() / g.cw);
  int rows = (int)(g.area.H() / g.lh);
  if (cols < 20 || rows < 5) return;
  if (cols > 500) cols = 500;
  if (rows > 200) rows = 200;
  if (term->Cols() != cols || term->Rows() != rows) term->Resize(cols, rows);
  int sig = cols * 1000 + rows;
  std::map<int, int>::iterator it = g_ptySize.find(sid);
  if (it != g_ptySize.end() && it->second == sig) return;
  if (!sk::SshService::Instance().IsConnected(sid)) return;
  bool firstSync = (it == g_ptySize.end());
  static int pendSid = -1;
  static int pendSig = 0;
  static int pendCount = 0;
  static unsigned long long pendAt = 0;
  unsigned long long now = GetTickCount64();
  if (pendSid == sid && pendSig == sig) {
    ++pendCount;
  } else {
    pendSid = sid;
    pendSig = sig;
    pendCount = 1;
  }
  if (!firstSync) {
    if (pendCount < 2) return;
    if (now - pendAt < 300) return;
  }
  pendAt = now;
  if (sk::SshService::Instance().ResizePty(sid, cols, rows)) g_ptySize[sid] = sig;
}

struct TermOut {
  int sid = -1;
  std::string bytes;
};

std::deque<TermOut> g_termOut;
std::set<int> g_termWarned;
std::mutex g_termOutMtx;
std::condition_variable g_termOutCv;
std::atomic<bool> g_termOutStop{false};
std::thread g_termOutWorker;

void TermOutWorker() {
  for (;;) {
    TermOut job;
    {
      std::unique_lock<std::mutex> lk(g_termOutMtx);
      g_termOutCv.wait(lk, [] { return g_termOutStop.load() || !g_termOut.empty(); });
      if (g_termOut.empty()) {
        if (g_termOutStop.load()) return;
        continue;
      }
      job = g_termOut.front();
      g_termOut.pop_front();
    }
    sk::SshService::Instance().Send(job.sid, job.bytes);
  }
}

void TermOutStart() {
  if (!g_termOutWorker.joinable()) g_termOutWorker = std::thread(TermOutWorker);
}

void TermOutStop() {
  {
    std::lock_guard<std::mutex> lk(g_termOutMtx);
    g_termOutStop.store(true);
    g_termOut.clear();
  }
  g_termOutCv.notify_all();
  if (!g_termOutWorker.joinable()) return;
  HANDLE h = (HANDLE)g_termOutWorker.native_handle();
  if (WaitForSingleObject(h, 800) == WAIT_OBJECT_0) {
    g_termOutWorker.join();
  } else {
    g_termOutWorker.detach();
  }
}

void TermQueue(int sid, const std::string& bytes) {
  if (sid < 0 || bytes.empty()) return;
  FlowKey();
  {
    std::lock_guard<std::mutex> lk(g_termOutMtx);
    if (g_termOut.size() > 4096) g_termOut.pop_front();
    TermOut job;
    job.sid = sid;
    job.bytes = bytes;
    g_termOut.push_back(job);
  }
  TermOutStart();
  g_termOutCv.notify_one();
}

void TermSend(const std::string& bytes) {
  int sid = g_state.CurrentSessionId();
  if (sid < 0) return;
  sk::Terminal* cur = sk::SessionHub::Instance().Find(sid);
  if (cur != nullptr && cur->ScrollBack() > 0) cur->ScrollToBottom();
  if (sk::SshService::Instance().IsConnected(sid)) {
    TermQueue(sid, bytes);
  }
}

void TermPaste() {
  std::wstring norm;
  for (wchar_t c : ClipboardGet()) {
    if (c == L'\r') continue;
    if (c == L'\n') c = L'\r';
    norm.push_back(c);
  }
  if (!norm.empty()) TermSend(W2U8(norm));
}

struct SelDrag {
  bool active = false;
  int sid = -1;
  int row = -1;
  int col = -1;
};

SelDrag g_drag;


std::string TermKeySeq(UINT vk, bool ctrl, bool shift, bool appCursor) {
  char finalCh = 0;
  switch (vk) {
    case VK_UP: finalCh = 'A'; break;
    case VK_DOWN: finalCh = 'B'; break;
    case VK_RIGHT: finalCh = 'C'; break;
    case VK_LEFT: finalCh = 'D'; break;
    case VK_HOME: finalCh = 'H'; break;
    case VK_END: finalCh = 'F'; break;
    default: break;
  }
  int mod = (shift ? 1 : 0) + (ctrl ? 4 : 0);
  if (finalCh != 0) {
    std::string out = "\x1b";
    if (mod != 0) {
      out += "[1;";
      out += std::to_string(mod + 1);
      out += finalCh;
    } else if (appCursor) {
      out += "O";
      out += finalCh;
    } else {
      out += "[";
      out += finalCh;
    }
    return out;
  }
  if (vk == VK_INSERT) return "\x1b[2~";
  if (vk == VK_DELETE) return "\x1b[3~";
  if (vk == VK_PRIOR) return "\x1b[5~";
  if (vk == VK_NEXT) return "\x1b[6~";
  return std::string();
}

UINT KeyVk(UINT wp, LPARAM lp) {
  if (wp != VK_PROCESSKEY) return wp;
  UINT scan = (UINT)((lp >> 16) & 0xFF);
  UINT real = MapVirtualKeyW(scan, 3);
  if ((real >= 'A' && real <= 'Z') || (real >= '0' && real <= '9') ||
      real == VK_OEM_COMMA) return real;
  for (UINT k = 'A'; k <= 'Z'; ++k) {
    if (GetAsyncKeyState((int)k) & 0x8000) return k;
  }
  for (UINT k = '0'; k <= '9'; ++k) {
    if (GetAsyncKeyState((int)k) & 0x8000) return k;
  }
  if (GetAsyncKeyState(VK_OEM_COMMA) & 0x8000) return VK_OEM_COMMA;
  return wp;
}

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_NCCALCSIZE: {
      if (wParam == TRUE) {
        NCCALCSIZE_PARAMS& p = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        if (IsZoomed(hwnd)) {
          MONITORINFO mi{ sizeof(MONITORINFO) };
          if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            p.rgrc[0] = mi.rcWork;
          }
        }
      }
      return 0;
    }
    case WM_NCHITTEST:
      return HandleHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    case WM_GETMINMAXINFO: {
      MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = (LONG)g_state.S(sk::L::WinMinW);
      mmi->ptMinTrackSize.y = (LONG)g_state.S(sk::L::WinMinH);
      return 0;
    }
    case WM_CREATE: {
      g_hwnd = hwnd;
      Relayout(hwnd);
      skPlatformInit();
      if (!g_render.Init(hwnd)) sk::LogWrite("E", "render init failed");
      g_render.Resize((UINT)g_client.W(), (UINT)g_client.H(), (float)g_state.dpi);
      g_state.Seed();
      if (!sk::LoadStore(g_state)) {
        sk::SaveStore(g_state);
      }
      if (g_state.sftpLocalPath.empty()) g_state.sftpLocalPath = sk::ExeDirectory();
      g_state.RefreshKeyStore();
      SyncTheme();
      SyncRender();
      RestoreLastView();
      BuildBootQueue();
      static const int kKeep[3] = { 7, 30, 90 };
      int li = g_state.logKeepIdx < 0 ? 0 : (g_state.logKeepIdx > 2 ? 2 : g_state.logKeepIdx);
      sk::SetLogLevel(g_state.logLevelIdx);
      CleanOldLogs(kKeep[li]);
      g_state.statusHost = g_state.hosts.empty()
                             ? std::wstring()
                             : g_state.hosts[g_state.activeHost].name;
      g_state.statusAddr = g_state.hosts.empty()
                             ? std::wstring()
                             : g_state.hosts[(size_t)g_state.activeHost].Addr();
      SetTimer(hwnd, 1, 900, nullptr);
      SetTimer(hwnd, 2, 500, nullptr);
      ApplyTermGrid();
      sk::SshService::Instance().SetNotify(&OnSshNotify, hwnd);
      return 0;
    }
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED || wParam == SIZE_MAXHIDE) return 0;
      Relayout(hwnd);
      g_render.Resize(LOWORD(lParam), HIWORD(lParam), (float)g_state.dpi);
      if (!g_inSizeMove) ApplyTermGrid();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_ENTERSIZEMOVE:
      g_inSizeMove = true;
      return 0;
    case WM_EXITSIZEMOVE:
      g_inSizeMove = false;
      ApplyTermGrid();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_DPICHANGED: {
      g_state.dpi = HIWORD(wParam);
      RECT* prc = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                   prc->right - prc->left, prc->bottom - prc->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_SETFOCUS: {
      sk::Terminal* t = sk::SessionHub::Instance().Find(g_state.CurrentSessionId());
      if (t != nullptr) t->SetFocused(true);
      InvalidateRect(hwnd, nullptr, FALSE);
      break;
    }
    case WM_KILLFOCUS: {
      sk::Terminal* t = sk::SessionHub::Instance().Find(g_state.CurrentSessionId());
      if (t != nullptr) t->SetFocused(false);
      InvalidateRect(hwnd, nullptr, FALSE);
      break;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER: {
      if (wParam == 7) {
        KillTimer(hwnd, 7);
        DestroyWindow(hwnd);
        return 0;
      }
      if (wParam == 2) {
        bool need = g_state.view == sk::View::Term ||
                    g_state.modal == sk::ModalKind::Prompt;
        if (need) ApplyTermGrid();
        if (!need && g_state.refreshAt > 0 &&
            (long long)GetTickCount64() - g_state.refreshAt < 1400) {
          need = true;
        }
        if (need) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam != 1) return 0;
      SyncHostStates();
      PumpSftpList();
      sk::SshService::Instance().ReapTransferSession();
      if (!g_state.toast.empty() &&
          (long long)GetTickCount64() > g_state.toastAt + 2600) {
        g_state.toast.clear();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      if (g_state.dirty) {
        sk::SaveStore(g_state);
        g_state.dirty = false;
      }
      if (g_state.pendingSftp != 0) {
        PumpPendingSftp();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      if (g_state.pendingBackup != 0) {
        RunBackup();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      if (g_state.pendingDisconnect >= 0) {
        int idx = g_state.pendingDisconnect;
        g_state.pendingDisconnect = -1;
        if (idx >= 0 && idx < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)idx].manualStop = true;
          g_state.hosts[(size_t)idx].retryAt = 0;
        }
        int tabIdx = g_state.FindTabByHost(idx);
        int sid = -1;
        if (tabIdx >= 0) sid = g_state.tabs[(size_t)tabIdx].sessionId;
        if (sid < 0 && idx >= 0 && idx < (int)g_state.hosts.size()) {
          sid = g_state.hosts[(size_t)idx].sessionId;
        }
        if (sid >= 0) sk::SshService::Instance().Disconnect(sid);
      }
      ApplyTermGrid();
      PatrolTunnels();
      PumpProbe();
      PumpBootQueue();
      PumpReconnect();
      XferPoll();
      ULONGLONG now = GetTickCount64();
      if (now >= (ULONGLONG)g_state.statsNextAt) {
        g_state.statsNextAt = (long long)(now + 5000);
        {
          std::lock_guard<std::mutex> lk(g_statsMtx);
          for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
            sk::Host& h = g_state.hosts[(size_t)i];
            if (h.state != sk::HostState::Live) continue;
            StatsJob job;
            job.idx = i;
            job.sid = h.sessionId;
            g_statsJobs.push_back(job);
          }
        }
        if (!g_statsWorker.joinable()) StatsStart();
        g_statsCv.notify_all();
      }
      if (now >= (ULONGLONG)g_netNextAt) {
        g_netNextAt = (long long)(now + 1000);
        PumpNetRate();
      }
      PumpStats();
      return 0;
    }
    case WM_APP + 41:
      XferPoll();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_APP + 42:
      PumpProbe();
      return 0;
    case WM_APP + 43:
      XferRowClick((int)wParam);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_APP + 44:
      PumpSftpList();
      return 0;
    case WM_DROPFILES: {
      HDROP drop = (HDROP)wParam;
      UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
      std::vector<std::wstring> files;
      for (UINT i = 0; i < n; ++i) {
        wchar_t f[MAX_PATH * 2] = { 0 };
        if (DragQueryFileW(drop, i, f, MAX_PATH * 2) == 0) continue;
        DWORD attr = GetFileAttributesW(f);
        if (attr == INVALID_FILE_ATTRIBUTES) continue;
        if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        files.push_back(f);
      }
      DragFinish(drop);
      if (!files.empty()) {
        g_state.pendingFiles = files;
        g_state.pendingSftp = 13;
        g_state.view = sk::View::Sftp;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      Paint(hwnd);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      FlowTouch();
      sk::Terminal* dragTerm = nullptr;
      int row = 0, col = 0;
      if (TermPoint(x, y, dragTerm, row, col)) {
        SetCapture(hwnd);
        dragTerm->ClearSelection();
        g_state.searchFocus = false;
        g_state.selectAll = false;
        g_drag.active = true;
        g_drag.sid = g_state.CurrentSessionId();
        g_drag.row = row;
        g_drag.col = col;
        SK_LOGI("term sel down %d,%d", row, col);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      sk::Terminal* clearTerm = sk::SessionHub::Instance().Find(g_state.CurrentSessionId());
      if (clearTerm != nullptr && clearTerm->HasSelection()) {
        clearTerm->ClearSelection();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      int id = sk::HitTest(g_state, g_client, x, y);
      if (g_state.modal == sk::ModalKind::Channel) {
        if (id == sk::HitModalOk) {
          OpenChannelUrl(g_state.chanPort);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (id == sk::HitModalCancel) {
          ClipboardSet(g_state.chanAddr);
          g_state.toast = sk::Tr(L"已复制通道地址");
          g_state.toastAt = (long long)GetTickCount64();
          g_state.toastKind = 0;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
      }
      bool tunRefresh = id >= sk::HitTunRefreshBase && id < sk::HitTunEditBase;
      sk::OnHit(g_state, id, hwnd);
      if (tunRefresh) {
        g_state.tunNextAt = 0;
        PatrolTunnels();
      }
      if (g_state.pendingTunBrowse >= 0) {
        int index = g_state.pendingTunBrowse;
        g_state.pendingTunBrowse = -1;
        BrowseTunnel(index);
      }
      if (g_state.pendingConnect >= 0) {
        int idx = g_state.pendingConnect;
        bool asTerm = g_state.pendingConnectTerm;
        g_state.pendingConnect = -1;
        g_state.pendingConnectTerm = true;
        if (g_state.modal == sk::ModalKind::None) RequestConnect(idx, asTerm);
      }
      if (g_state.modal == sk::ModalKind::None) SyncTheme();
      PumpPendingSftp();
      SyncRender();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_LBUTTONUP: {
      bool dragging = g_drag.active;
      int dragSid = g_drag.sid;
      g_drag.active = false;
      if (GetCapture() == hwnd) ReleaseCapture();
      if (!dragging) break;
      sk::Terminal* term = sk::SessionHub::Instance().Find(dragSid);
      if (term != nullptr && term->HasSelection()) {
        std::wstring picked = term->SelectedText();
        if (!picked.empty()) {
          bool ok = ClipboardCopy(picked);
          g_state.toast =
              ok ? sk::Tr(L"已复制 ") + std::to_wstring((int)picked.size()) + sk::Tr(L" 字符")
                 : sk::Tr(L"复制失败：剪贴板被其他程序占用");
          g_state.toastAt = (long long)GetTickCount64();
          g_state.toastKind = ok ? 0 : 1;
          SK_LOGI("term sel copy %d ok=%d", (int)picked.size(), ok ? 1 : 0);
        }
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      if (g_drag.active) {
        sk::Terminal* term = sk::SessionHub::Instance().Find(g_drag.sid);
        TermGeom g;
        if (term != nullptr && TermGeomOf(g)) {
          int row = 0, col = 0;
          TermClamp(x, y, g, term, row, col);
          term->SetSelection(g_drag.row, g_drag.col, row, col);
          static int lastDragRow = -1;
          static int lastDragCol = -1;
          if (row != lastDragRow || col != lastDragCol) {
            lastDragRow = row;
            lastDragCol = col;
            SK_LOGI("term sel drag %d,%d", row, col);
          }
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      int hover = -1;
      int hoverId = -1;
      if (!g_state.fullscreen) {
        int id = sk::HitTest(g_state, g_client, x, y);
        if (id == sk::HitSideCollapse || id == sk::HitSideExpand ||
            id == sk::HitSideSettings || id == sk::HitTabsRightBase ||
            id == sk::HitFtpRefresh || id == sk::HitFtpNew ||
            id == sk::HitKeyGen ||
            (id >= sk::HitFtpUp && id <= sk::HitFtpParent) ||
            (id >= sk::HitQueueRowBase && id < sk::HitQueueRowBase + 12) ||
            (id >= sk::HitTunRefreshBase && id < sk::HitSetNavBase) ||
            (id >= sk::HitFormBrowseBase && id < sk::HitModalKeyStore) ||
            (id >= sk::HitTunBrowseBase && id < sk::HitTunBrowseBase + 20)) {
          hoverId = id;
        }
        if (id >= sk::HitSideCardBase && id < sk::HitRecentBase) {
          int pos = (id - sk::HitSideCardBase) / sk::kSideCardSlots;
          int vis[sk::kSideCardCap];
          int n = sk::SidebarVisibleHosts(g_state, vis, sk::kSideCardCap);
          if (pos < n) hover = vis[pos];
        }
      }
      if (hover != g_state.hoverHost || hoverId != g_state.hoverId) {
        g_state.hoverHost = hover;
        g_state.hoverId = hoverId;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
      TrackMouseEvent(&tme);
      return 0;
    }
    case WM_MOUSELEAVE: {
      if (g_state.hoverHost != -1 || g_state.hoverId != -1) {
        g_state.hoverHost = -1;
        g_state.hoverId = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_RBUTTONUP: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      sk::Terminal* pasteTerm = nullptr;
      int pasteRow = 0, pasteCol = 0;
      if (TermPoint(x, y, pasteTerm, pasteRow, pasteCol)) {
        g_state.searchFocus = false;
        g_state.selectAll = false;
        TermPaste();
        return 0;
      }
      int id = sk::HitTest(g_state, g_client, x, y);
      if (id >= sk::HitQueueRowBase && id < sk::HitQueueRowBase + 12) {
        int qi = id - sk::HitQueueRowBase;
        if (qi < 0 || qi >= (int)g_state.sftpQueueIdx.size()) return 0;
        int idx = g_state.sftpQueueIdx[(size_t)qi];
        if (idx < 0 || idx >= (int)g_xfRows.size()) return 0;
        const XferRow& row = g_xfRows[(size_t)idx];
        POINT qpt{ (LONG)x, (LONG)y };
        ClientToScreen(hwnd, &qpt);
        HMENU qm = CreatePopupMenu();
        if (row.state == 1) {
          AppendMenuW(qm, MF_STRING, 1, row.paused ? sk::Tr(L"继续") : sk::Tr(L"暂停"));
          AppendMenuW(qm, MF_STRING, 2, sk::Tr(L"取消本次"));
        } else if (row.state == 0) {
          AppendMenuW(qm, MF_STRING, 2, sk::Tr(L"取消本次"));
        } else if (row.state == 3) {
          AppendMenuW(qm, MF_STRING, 3, sk::Tr(L"重试"));
        }
        if (GetMenuItemCount(qm) == 0) {
          DestroyMenu(qm);
          return 0;
        }
        SetForegroundWindow(hwnd);
        int qc = (int)TrackPopupMenu(qm, TPM_RETURNCMD | TPM_RIGHTBUTTON, qpt.x, qpt.y, 0, hwnd,
                                     nullptr);
        DestroyMenu(qm);
        if (qc == 1) XferPauseJob(idx);
        else if (qc == 2) XferCancelJob(idx);
        else if (qc == 3) XferCancelJob(idx);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      int hostIdx = -1;
      if (id >= sk::HitHostBase && id < sk::HitDashCardBase) {
        hostIdx = id - sk::HitHostBase;
      } else if (id >= sk::HitDashCardBase && id < sk::HitDashNewHost) {
        hostIdx = id - sk::HitDashCardBase;
      } else if (id >= sk::HitSideCardBase && id < sk::HitSortKey) {
        int base = id >= sk::HitRecentBase ? sk::HitRecentBase : sk::HitSideCardBase;
        int pos = (id - base) / sk::kSideCardSlots;
        if (base == sk::HitRecentBase) {
          int rec[sk::kRecentCap];
          int n = sk::RecentHosts(g_state, rec, sk::kRecentCap);
          if (pos < n) hostIdx = rec[pos];
        } else {
          int vis[sk::kSideCardCap];
          int n = sk::SidebarVisibleHosts(g_state, vis, sk::kSideCardCap);
          if (pos < n) hostIdx = vis[pos];
        }
      }
      if (hostIdx < 0 || hostIdx >= (int)g_state.hosts.size()) return 0;

      POINT pt{ (LONG)x, (LONG)y };
      ClientToScreen(hwnd, &pt);
      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu, MF_STRING, 1, sk::Tr(L"连接"));
      AppendMenuW(menu, MF_STRING, 2, sk::Tr(L"编辑"));
      AppendMenuW(menu, MF_STRING, 3, sk::Tr(L"复制地址"));
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      AppendMenuW(menu, MF_STRING, 4, sk::Tr(L"删除"));
      SetForegroundWindow(hwnd);
      int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd,
                                    nullptr);
      DestroyMenu(menu);
      if (cmd == 1) {
        RequestConnect(hostIdx, false);
      } else if (cmd == 2) {
        g_state.OpenHostForm(hostIdx);
      } else if (cmd == 3) {
        std::wstring addr = g_state.hosts[(size_t)hostIdx].Addr();
        if (OpenClipboard(hwnd)) {
          EmptyClipboard();
          size_t bytes = (addr.size() + 1) * sizeof(wchar_t);
          HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
          if (mem != nullptr) {
            void* dst = GlobalLock(mem);
            if (dst != nullptr) {
              memcpy(dst, addr.c_str(), bytes);
              GlobalUnlock(mem);
              SetClipboardData(CF_UNICODETEXT, mem);
            }
          }
          CloseClipboard();
        }
      } else if (cmd == 4) {
        g_state.confirmHost = hostIdx;
        g_state.confirmTunnel = -1;
        g_state.modal = sk::ModalKind::Confirm;
        g_state.modalTitle = sk::Tr(L"删除主机");
        g_state.modalNote = g_state.hosts[(size_t)hostIdx].Addr();
        g_state.confirmNote = sk::Tr(L"将断开该主机的全部会话并删除其转发规则");
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MBUTTONUP: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      int id = sk::HitTest(g_state, g_client, x, y);
      if (id >= sk::HitTabBase && id < sk::HitTabAdd) {
        g_state.CloseTab(id - sk::HitTabBase);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_CAPTURECHANGED:
      g_drag.active = false;
      return 0;
    case WM_LBUTTONDBLCLK: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      sk::Terminal* dblTerm = nullptr;
      int dblRow = 0, dblCol = 0;
      if (TermPoint(x, y, dblTerm, dblRow, dblCol)) {
        static unsigned long long lastDblAt = 0;
        static int lastDblRow = -1;
        static int lastDblCol = -1;
        unsigned long long nowDbl = GetTickCount64();
        bool triple = (nowDbl - lastDblAt < 600 && dblRow == lastDblRow && dblCol == lastDblCol);
        lastDblAt = nowDbl;
        lastDblRow = dblRow;
        lastDblCol = dblCol;
        if (triple) {
          dblTerm->SelectLineAt(dblRow);
        } else {
          dblTerm->SelectWordAt(dblRow, dblCol);
        }
        g_drag.active = false;
        if (dblTerm->HasSelection()) {
          std::wstring picked = dblTerm->SelectedText();
          if (!picked.empty()) {
            bool ok = ClipboardCopy(picked);
            g_state.toast =
                ok ? sk::Tr(L"已复制 ") + std::to_wstring((int)picked.size()) + sk::Tr(L" 字符")
                   : sk::Tr(L"复制失败：剪贴板被其他程序占用");
            g_state.toastAt = (long long)GetTickCount64();
            g_state.toastKind = ok ? 0 : 1;
            SK_LOGI("term sel dbl %d,%d triple=%d chars=%d ok=%d", dblRow, dblCol, triple ? 1 : 0,
                    (int)picked.size(), ok ? 1 : 0);
          }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      int id = sk::HitTest(g_state, g_client, x, y);
      if (id >= sk::HitSideCardBase && id < sk::HitSortKey) {
        int base = id >= sk::HitRecentBase ? sk::HitRecentBase : sk::HitSideCardBase;
        int rel = id - base;
        int pos = rel / sk::kSideCardSlots;
        int slot = rel % sk::kSideCardSlots;
        if (slot == 0) {
          int idx = -1;
          if (base == sk::HitRecentBase) {
            int rec[sk::kRecentCap];
            int n = sk::RecentHosts(g_state, rec, sk::kRecentCap);
            if (pos < n) idx = rec[pos];
          } else {
            int vis[sk::kSideCardCap];
            int n = sk::SidebarVisibleHosts(g_state, vis, sk::kSideCardCap);
            if (pos < n) idx = vis[pos];
          }
          if (idx >= 0 && idx < (int)g_state.hosts.size()) {
            if (!sk::HostHasCredential(g_state.hosts[(size_t)idx])) {
              g_state.OpenHostForm(idx);
              g_state.formError = sk::Tr(L"请先设置密码或私钥文件，再连接");
            } else {
              g_state.OpenHostView(idx, 0);
              RequestConnect(idx, true);
            }
          }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (id >= sk::HitFtpRowBase && id < sk::HitFtpRowBase + sk::kFtpRowCap) {
        g_state.sftpSide = 0;
        g_state.sftpSelLocal = id - sk::HitFtpRowBase;
        g_state.pendingSftp = 5;
      } else if (id >= sk::HitFtpRowBase + sk::kFtpRowCap &&
                 id < sk::HitFtpRowBase + 2 * sk::kFtpRowCap) {
        g_state.sftpSide = 1;
        g_state.sftpSelRemote = id - sk::HitFtpRowBase - sk::kFtpRowCap;
        g_state.pendingSftp = 6;
      } else {
        sk::OnHit(g_state, id, hwnd);
      }
      PumpPendingSftp();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        if (g_state.view == sk::View::Term) {
          g_state.termFontSize += delta > 0 ? 1.0f : -1.0f;
          if (g_state.termFontSize < 10) g_state.termFontSize = 10;
          if (g_state.termFontSize > 28) g_state.termFontSize = 28;
          g_state.termSizeIdx = g_state.termFontSize <= 12.0f
                                    ? 0
                                    : (g_state.termFontSize <= 13.0f
                                           ? 1
                                           : (g_state.termFontSize <= 14.0f ? 2 : 3));
          ApplyTermGrid();
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      ScreenToClient(hwnd, &pt);
      sk::Rect sb, tabsRc, contentRc, statusRc;
      sk::LayoutMetrics(g_state, g_client, sb, tabsRc, contentRc, statusRc);
      if (!g_state.fullscreen && !g_state.sideCollapsed && pt.x >= sb.l && pt.x < sb.r) {
        float maxScroll = sk::SideScrollMax(g_state, sb);
        g_state.sideScroll += (delta > 0 ? -1.0f : 1.0f) * g_state.S(60.0f);
        if (g_state.sideScroll < 0.0f) g_state.sideScroll = 0.0f;
        if (g_state.sideScroll > maxScroll) g_state.sideScroll = maxScroll;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      bool sftpPage = g_state.view == sk::View::Sftp ||
                      (g_state.view == sk::View::Term && g_state.sessTab == 2 &&
                       g_state.CurrentHostId() >= 0);
      if (sftpPage) {
        sk::Rect inner = g_state.view == sk::View::Term ? sk::SessInnerRect(g_state, contentRc)
                                                        : contentRc;
        for (int side = 0; side < 2; ++side) {
          sk::Rect panel = sk::SftpPanelRect(g_state, inner, side);
          if (!panel.Hit((float)pt.x, (float)pt.y)) continue;
          float maxScroll = sk::SftpScrollMax(g_state, inner, side);
          float& cur = side == 0 ? g_state.sftpScrollL : g_state.sftpScrollR;
          cur += (-g_state.S(108.0f)) * ((float)delta / 120.0f);
          if (cur < 0.0f) cur = 0.0f;
          if (cur > maxScroll) cur = maxScroll;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
      }
      if (g_state.view == sk::View::Term) {
        sk::Terminal* term = TermOf(g_state.CurrentSessionId());
        if (term) term->ScrollLines(delta > 0 ? 3 : -3);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case kMsgSsh: {
      int sid = (int)wParam;
      if (sid < 0) return 0;
      sk::SshService& svc = sk::SshService::Instance();
      std::string data = svc.PumpOutput(sid);
      sk::Terminal* term = sk::SessionHub::Instance().Find(sid);
      if (term != nullptr && !data.empty()) term->Feed(data);
      if (!data.empty()) FlowRtt();
      FlowTouch();

      sk::SshState st = svc.State(sid);
      int tabIdx = g_state.FindTabBySession(sid);
      int hid = tabIdx >= 0 ? g_state.tabs[(size_t)tabIdx].hostId : -1;
      if (hid < 0) {
        for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
          if (g_state.hosts[(size_t)i].sessionId == sid) {
            hid = i;
            break;
          }
        }
      }

      if (st == sk::SshState::Connected) {
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Live;
          g_state.hosts[(size_t)hid].hostKey = U82W(svc.HostKey(sid));
          g_state.hosts[(size_t)hid].hostKeyType = U82W(svc.HostKeyType(sid));
          g_state.hosts[(size_t)hid].lastConnected = sk::NowStamp();
          g_state.hosts[(size_t)hid].retryN = 0;
          g_state.hosts[(size_t)hid].retryAt = 0;
          g_state.hosts[(size_t)hid].manualStop = false;
          g_state.dirty = true;
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = true;
        g_state.statsNextAt = 0;
        g_state.tunNextAt = 0;
        if (g_state.view == sk::View::Term && g_state.sessTab == 2 &&
            g_state.CurrentHostId() == hid) {
          g_state.pendingSftp = 1;
        }
        if (term != nullptr) term->SetFocused(true);
        ApplyTermGrid();
      } else if (st == sk::SshState::Failed) {
        std::string err = svc.LastError(sid);
        if (err == "连接失败") err.clear();
        std::wstring emsg = ErrText(err);
        if (term != nullptr) term->Feed("\r\n\x1b[31m连接失败：" + W2U8(emsg) + "\x1b[0m\r\n");
        g_state.toast = sk::Tr(L"连接失败：") + emsg;
        g_state.toastAt = (long long)GetTickCount64();
        g_state.toastKind = 1;
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Off;
          g_state.hosts[(size_t)hid].latency = -1;
          if (g_state.hosts[(size_t)hid].retryN < 3) ScheduleReconnect(hid);
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = false;
        g_pwd.erase(sid);
        g_ptySize.erase(sid);
      } else if (st == sk::SshState::Disconnected) {
        if (term != nullptr) term->Feed("\r\n\x1b[90m会话已断开\x1b[0m\r\n");
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Off;
          g_state.hosts[(size_t)hid].latency = -1;
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = false;
        g_ptySize.erase(sid);
        ScheduleReconnect(hid);
      }
      if (!data.empty() || st != sk::SshState::Connected) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_CHAR: {
      if (g_suppressChar) {
        g_suppressChar = false;
        return 0;
      }
      wchar_t ch = (wchar_t)wParam;
      if (g_state.selectAll && (ch >= 32 || ch == L'\b')) {
        std::wstring* buf = EditBuffer();
        if (buf != nullptr) buf->clear();
        sk::FormField* f = EditField();
        if (f != nullptr) f->fresh = false;
        g_state.selectAll = false;
      }
      if (g_state.modal != sk::ModalKind::None) {
        if ((sk::ModalIsForm(g_state) || g_state.modal == sk::ModalKind::Prompt ||
             g_state.modal == sk::ModalKind::Backup) && ch != L'\r' &&
            ch != L'\t' && (ch >= 32 || ch == L'\b')) {
          sk::ModalChar(g_state, ch);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (g_state.searchFocus) {
        if (ch == L'\r' || ch == 27) {
          g_state.searchFocus = false;
        } else if (ch == L'\b') {
          if (!g_state.query.empty()) g_state.query.pop_back();
        } else if (ch >= 32) {
          g_state.query.push_back(ch);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (g_state.view == sk::View::Term) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        int sid = g_state.CurrentSessionId();
        sk::Terminal* term = TermOf(sid);
        auto pit = g_pwd.find(sid);
        bool pwdOn = pit != g_pwd.end() && pit->second.active;
        if (pwdOn) {
          PwdState& ps = pit->second;
          if (ch == L'\r') {
            term->Feed("\r\n");
            ps.active = false;
            std::string pwd = W2U8(ps.buf);
            ps.buf.clear();
            int hid = g_state.CurrentHostId();
            StartConnect(hid, sid, pwd, std::wstring());
          } else if (ch == L'\b') {
            if (!ps.buf.empty()) {
              ps.buf.pop_back();
              term->Feed("\b \b");
            }
          } else if (ch >= 32) {
            ps.buf.push_back(ch);
            term->Feed("*");
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (sid >= 0 && sk::SshService::Instance().IsConnected(sid)) {
          g_termWarned.erase(sid);
          std::string bytes = KeyToBytes(ch, ctrl);
          if (!bytes.empty()) TermQueue(sid, bytes);
        } else if (term != nullptr && (ch >= 32 || ch == L'\r' || ch == 8)) {
          if (g_termWarned.insert(sid).second) {
            term->Feed("\r\n\x1b[90m" + W2U8(sk::Tr(L"连接已断开，按键不会发送，请重新连接")) + "\x1b[0m\r\n");
          }
          InvalidateRect(hwnd, nullptr, FALSE);
        }
      }
      return 0;
    }
    case WM_KEYDOWN: {
      UINT vk = KeyVk((UINT)wParam, lParam);
      bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      if (ctrl && (vk == 'C' || vk == 'X' || vk == 'V' || vk == 'A')) {
        std::wstring* buf = EditBuffer();
        if (buf != nullptr) {
          if (vk == 'A') g_state.selectAll = true;
          else if (vk == 'V') EditPaste();
          else EditCopy(vk == 'X');
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (g_state.view == sk::View::Term && g_state.modal == sk::ModalKind::None) {
          if (vk == 'C') {
            sk::Terminal* tc = TermOf(g_state.CurrentSessionId());
            if (tc != nullptr && tc->HasSelection()) {
              std::wstring picked = tc->SelectedText();
              bool ok = ClipboardCopy(picked);
              g_state.toast = ok ? sk::Tr(L"已复制选中的终端内容") : sk::Tr(L"复制失败：剪贴板被其他程序占用");
              g_state.toastAt = (long long)GetTickCount64();
              g_state.toastKind = ok ? 0 : 1;
              SK_LOGI("term sel keycopy %d ok=%d", (int)picked.size(), ok ? 1 : 0);
              InvalidateRect(hwnd, nullptr, FALSE);
              return 0;
            }
          }
          TermSend(std::string(1, (char)(vk - 'A' + 1)));
          return 0;
        }
      }
      if (g_state.modal != sk::ModalKind::None) {
        if (vk == VK_RETURN) g_suppressChar = true;
        sk::ModalKey(g_state, (int)vk);
        SyncTheme();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if ((vk == VK_PRIOR || vk == VK_NEXT) && g_state.view == sk::View::Term &&
          g_state.modal == sk::ModalKind::None) {
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
          sk::Terminal* term = TermOf(g_state.CurrentSessionId());
          if (term != nullptr) {
            int step = term->Rows() - 1;
            if (step < 1) step = 1;
            term->ScrollLines(vk == VK_PRIOR ? step : -step);
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
      }
      if (g_state.view == sk::View::Term && g_state.modal == sk::ModalKind::None &&
          (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
           vk == VK_HOME || vk == VK_END || vk == VK_INSERT || vk == VK_DELETE ||
           vk == VK_PRIOR || vk == VK_NEXT || (vk == VK_TAB && shift))) {
        sk::Terminal* tk = TermOf(g_state.CurrentSessionId());
        std::string seqOut;
        if (vk == VK_TAB) {
          seqOut = "\x1b[Z";
        } else {
          seqOut = TermKeySeq(vk, ctrl, shift, tk != nullptr && tk->AppCursor());
        }
        if (!seqOut.empty()) TermSend(seqOut);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrl && vk == '1') { g_state.OpenPage(sk::View::Term); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '2') { g_state.OpenPage(sk::View::Sftp); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '3') { g_state.OpenPage(sk::View::Tun); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '4') { g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '0') { g_state.OpenPage(sk::View::Dash); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == 'K') { g_state.searchFocus = true; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == 'W') {
        int sid = g_state.CurrentSessionId();
        if (sid >= 0) g_state.CloseTab(g_state.activeTab);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrl && vk == VK_OEM_COMMA) { g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (vk == VK_F11) {
        ToggleFullscreen(hwnd, !g_state.fullscreen);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (vk == VK_ESCAPE && g_state.fullscreen) {
        ToggleFullscreen(hwnd, false);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (vk == VK_ESCAPE && g_state.searchFocus) {
        g_state.searchFocus = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrl && vk >= 'A' && vk <= 'Z' && g_state.view == sk::View::Term) {
        TermSend(std::string(1, (char)(vk - 'A' + 1)));
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      sk::SshService::Instance().CancelAllTransfers();
      XferRequestStop();
      if (XferBusy()) {
        g_state.sftpBar = sk::Tr(L"正在停止传输…");
        InvalidateRect(hwnd, nullptr, FALSE);
        SetTimer(hwnd, 7, 400, nullptr);
        return 0;
      }
      return DefWindowProc(hwnd, WM_CLOSE, wParam, lParam);
    case WM_DESTROY:
      sk::SaveStore(g_state);
      sk::SshService::Instance().CancelAllTransfers();
      XferStop();
      ListStop();
      ProbeStop();
      StatsStop();
      TermOutStop();
      sk::SshService::Instance().DisconnectAll();
      sk::SessionHub::Instance().Clear();
      g_render.Shutdown();
      skPlatformShutdown();
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\Skiff-Single-8f2b4c1e");
  if (single != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND prev = FindWindowW(kClassName, nullptr);
    if (prev != nullptr) {
      ShowWindow(prev, SW_RESTORE);
      SetForegroundWindow(prev);
    } else {
      MessageBoxW(nullptr,
                  L"已有一个轻舟进程在后台运行，但它的主窗口已不存在（通常发生在传输文件时点了关闭）。\n\n"
                  L"请打开「任务管理器 → 详细信息」，结束名为 Skiff-*.exe 的进程后再启动。",
                  L"轻舟 Skiff", MB_ICONWARNING | MB_OK);
    }
    return 0;
  }
  SetUnhandledExceptionFilter(SkiffCrashFilter);
  std::set_terminate([]() {
    CrashWrite("terminate", nullptr);
    ExitProcess(3);
  });
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
  wc.hIconSm = wc.hIcon;
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kClassName;
  if (!RegisterClassExW(&wc)) return 1;

  HDC screen = GetDC(nullptr);
  int dpi = GetDeviceCaps(screen, LOGPIXELSX);
  ReleaseDC(nullptr, screen);
  g_state.dpi = (UINT)dpi;
  float scale = dpi / 96.0f;

  RECT wr{ 0, 0, (LONG)(sk::L::WinDefaultW * scale), (LONG)(sk::L::WinDefaultH * scale) };
  AdjustWindowRect(&wr, kAppStyle, FALSE);

  HWND hwnd = CreateWindowExW(
    WS_EX_APPWINDOW, kClassName, kWindowTitle, kAppStyle,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right - wr.left, wr.bottom - wr.top,
    nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) return 2;

  DragAcceptFiles(hwnd, TRUE);
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}
