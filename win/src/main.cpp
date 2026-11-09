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
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
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
  static bool opened = false;
  if (!opened) {
    opened = true;
    char dir[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) slash[1] = 0;
    std::string logs = std::string(dir) + "logs";
    CreateDirectoryA(logs.c_str(), nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\app-%04d%02d%02d.log", logs.c_str(),
             st.wYear, st.wMonth, st.wDay);
    fp = fopen(path, "ab");
  }
  if (fp) {
    fputs(line, fp);
    fflush(fp);
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

std::wstring ErrText(const std::string& code) {
  if (code.empty()) return sk::Tr(L"无法建立连接，详见 logs 目录");
  if (code == "TCP_CONNECT_FAILED") return sk::Tr(L"无法连接目标端口，请检查地址、端口与网络");
  if (code == "HANDSHAKE_TIMEOUT") return sk::Tr(L"SSH 握手超时，对方可能不是 SSH 服务");
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
  p.keyPass = W2U8(h.keyPass);
  p.expectedHostKey = g_state.fpCheck ? W2U8(h.hostKey) : std::string();
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
  int v = g_state.lastView;
  if (v <= 0 || v > 4) return;
  if (v == (int)sk::View::Term) {
    int idx = -1;
    for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
      if (g_state.hosts[(size_t)i].id == g_state.lastHost) {
        idx = i;
        break;
      }
    }
    if (idx >= 0) g_state.OpenHostView(idx, 0);
    return;
  }
  g_state.OpenPage((sk::View)v);
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

void CollectStats(int idx) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  sk::Host& h = g_state.hosts[(size_t)idx];
  if (h.state != sk::HostState::Live) {
    h.cpu = 0;
    h.ram = 0;
    h.netp = 0;
    h.net.clear();
    return;
  }
  int sid = h.sessionId;
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
    if (sp.fails >= 3) sp.nextAt = (long long)(GetTickCount64() + 60000);
    return;
  }
  h.latency = (int)(GetTickCount64() - t0);

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
        h.cpu = pct;
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
    h.ram = pct;
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
    h.net = FmtRate(rate);
    int pct = (int)((double)rate / (10.0 * 1024.0 * 1024.0) * 100.0);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    h.netp = pct;
  }
  sp.rx = rx;
  sp.tx = tx;
  sp.at = nowMs;
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
    int baseAcc = svc.ForwardAccepts(job.sid);
    int baseFail = svc.ForwardFailures(job.sid);
    int ok = 0;
    if (ProbeLocal(job.port, 160)) {
      ok = 1;
      for (int i = 0; i < 10; ++i) {
        Sleep(30);
        if (svc.ForwardFailures(job.sid) > baseFail) {
          ok = 0;
          break;
        }
        if (svc.ForwardAccepts(job.sid) > baseAcc) {
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
          tn.state = sk::TunState::Retry;
          tn.probed = 0;
          ++tn.fails;
          int wait = tn.fails <= 1 ? 5 : (tn.fails == 2 ? 10 : 15);
          tn.retryIn = wait;
          tn.nextAt = now + (long long)wait * 1000;
          if (d.sid >= 0 && d.port > 0) sk::SshService::Instance().ForwardStopLocal(d.sid, d.port);
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
          tn.lastAcc = svc.ForwardAccepts(sid);
          tn.lastFails = svc.ForwardFailures(sid);
          tn.nextAt = (long long)(now + 600);
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
      int acc = svc.ForwardAccepts(sid);
      int fl = svc.ForwardFailures(sid);
      if (fl > tn.lastFails) {
        tn.lastFails = fl;
        tn.lastAcc = acc;
        ++tn.fails;
        int wait = tn.fails <= 1 ? 5 : (tn.fails == 2 ? 10 : 15);
        tn.state = sk::TunState::Retry;
        tn.retryIn = wait;
        tn.probed = 0;
        tn.nextAt = (long long)(now + (ULONGLONG)wait * 1000ULL);
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

void ClipboardSet(const std::wstring& text);

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
      tn.lastAcc = svc.ForwardAccepts(sid);
      tn.lastFails = svc.ForwardFailures(sid);
      if (g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
      Sleep(300);
    }
  }
  bool up = ProbeLocal(port);
  tn.state = up ? sk::TunState::Up : sk::TunState::Retry;
  tn.retryIn = 0;
  tn.fails = 0;
  tn.probed = up ? 2 : 0;
  tn.lastAcc = svc.ForwardAccepts(sid);
  tn.lastFails = svc.ForwardFailures(sid);
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

bool XferBusy();

void SftpLoadRemote() {
  int sid = g_state.CurrentSessionId();
  if (XferBusy()) {
    g_state.sftpStatus = sk::Tr(L"传输中，稍后刷新");
    return;
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
  std::vector<sk::RemoteEntry> list;
  std::string path = W2U8(g_state.sftpPath);
  if (!svc.SftpList(sid, path, list)) {
    g_state.sftpStatus = sk::Tr(L"远程目录读取失败");
    g_state.sftpRemote.clear();
    return;
  }
  g_state.sftpRemote.clear();
  g_state.sftpRemote.push_back({ L"..", L"—", L"—", true });
  std::vector<sk::SftpEntry> dirs;
  std::vector<sk::SftpEntry> files;
  for (size_t i = 0; i < list.size(); ++i) {
    sk::SftpEntry e;
    e.name = U82W(list[i].name);
    e.dir = list[i].dir;
    e.size = list[i].dir ? std::wstring(L"—") : FmtBytes(list[i].size);
    e.time = FmtFileTime(list[i].mtime);
    if (list[i].dir) dirs.push_back(e);
    else files.push_back(e);
  }
  for (size_t i = 0; i < dirs.size(); ++i) g_state.sftpRemote.push_back(dirs[i]);
  for (size_t i = 0; i < files.size(); ++i) g_state.sftpRemote.push_back(files[i]);
  g_state.sftpSelRemote = -1;
  g_state.sftpStatus = sk::Tr(L"已读取 ") + std::to_wstring(list.size()) + sk::Tr(L" 项");
}

struct XferRow {
  int kind = 0;
  std::wstring name;
  int state = 0;
  int pct = 0;
  unsigned long long done = 0;
  unsigned long long total = 0;
  std::wstring err;
};

struct XferJob {
  int idx = -1;
  int sid = -1;
  int kind = 0;
  std::wstring localPath;
  std::string remotePath;
};

std::mutex g_xfMtx;
std::condition_variable g_xfCv;
std::deque<XferJob> g_xfJobs;
std::thread g_xfWorker;
bool g_xfStop = false;

std::mutex g_xfRunMtx;
int g_xfRunIdx = -1;
int g_xfRunPct = 0;
unsigned long long g_xfRunDone = 0;
unsigned long long g_xfRunTotal = 0;

std::mutex g_xfDoneMtx;
std::deque<std::pair<int, std::wstring>> g_xfDone;

std::vector<XferRow> g_xfRows;
bool g_xfNeedRemote = false;
bool g_xfNeedLocal = false;

void XferWorker() {
  sk::SshService& svc = sk::SshService::Instance();
  for (;;) {
    XferJob job;
    {
      std::unique_lock<std::mutex> lk(g_xfMtx);
      g_xfCv.wait(lk, [] { return g_xfStop || !g_xfJobs.empty(); });
      if (g_xfStop) return;
      job = g_xfJobs.front();
      g_xfJobs.pop_front();
    }
    {
      std::lock_guard<std::mutex> lk(g_xfRunMtx);
      g_xfRunIdx = job.idx;
      g_xfRunPct = 0;
      g_xfRunDone = 0;
      g_xfRunTotal = 0;
    }
    sk::SshService::ProgressFn prog = [](unsigned long long d, unsigned long long t) {
      std::lock_guard<std::mutex> lk(g_xfRunMtx);
      g_xfRunDone = d;
      g_xfRunTotal = t;
      int pct = t > 0 ? (int)((d * 100ULL) / t) : 0;
      g_xfRunPct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    };
    std::string err;
    bool ok = false;
    if (job.kind == 0) {
      ok = svc.SftpUpload(job.sid, job.localPath, job.remotePath, err, prog);
    } else {
      ok = svc.SftpDownload(job.sid, job.remotePath, job.localPath, err, prog);
    }
    {
      std::lock_guard<std::mutex> lk(g_xfRunMtx);
      g_xfRunIdx = -1;
    }
    {
      std::lock_guard<std::mutex> lk(g_xfDoneMtx);
      g_xfDone.push_back({ job.idx, ok ? std::wstring() : U82W(err) });
    }
    if (g_hwnd != nullptr) PostMessageW(g_hwnd, WM_APP + 41, 0, 0);
  }
}

void XferStop() {
  {
    std::lock_guard<std::mutex> lk(g_xfMtx);
    g_xfStop = true;
    g_xfJobs.clear();
  }
  g_xfCv.notify_all();
  if (!g_xfWorker.joinable()) return;
  HANDLE h = (HANDLE)g_xfWorker.native_handle();
  if (WaitForSingleObject(h, 1500) == WAIT_OBJECT_0) {
    g_xfWorker.join();
  } else {
    g_xfWorker.detach();
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
  size_t n = g_xfRows.size();
  size_t start = n > 8 ? n - 8 : 0;
  for (size_t i = start; i < n; ++i) {
    const XferRow& r = g_xfRows[i];
    std::wstring line = r.kind == 0 ? L"↑ " : L"↓ ";
    line += r.name;
    line += L" · ";
    if (r.state == 0) {
      line += sk::Tr(L"排队中");
    } else if (r.state == 1) {
      line += std::to_wstring(r.pct) + L"%";
      if (r.total > 0) line += L"  " + FmtBytes(r.done) + L" / " + FmtBytes(r.total);
    } else if (r.state == 2) {
      line += sk::Tr(L"完成");
    } else {
      line += sk::Tr(L"失败");
      if (!r.err.empty()) line += L" " + r.err;
    }
    g_state.sftpQueue.push_back(line);
  }
}

void XferQueue(int kind, int sid, const std::wstring& name, const std::wstring& localPath,
               const std::string& remotePath) {
  if (!XferBusy() && !g_xfRows.empty()) {
    g_xfRows.clear();
    g_state.sftpQueue.clear();
  }
  XferRow row;
  row.kind = kind;
  row.name = name;
  g_xfRows.push_back(row);
  XferJob job;
  job.idx = (int)g_xfRows.size() - 1;
  job.sid = sid;
  job.kind = kind;
  job.localPath = localPath;
  job.remotePath = remotePath;
  {
    std::lock_guard<std::mutex> lk(g_xfMtx);
    g_xfJobs.push_back(job);
  }
  if (!g_xfWorker.joinable()) g_xfWorker = std::thread(XferWorker);
  g_xfCv.notify_one();
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
    if (done[i].second.empty()) {
      r.state = 2;
      r.pct = 100;
      g_state.sftpStatus = (r.kind == 0 ? std::wstring(sk::Tr(L"已上传 ")) : std::wstring(sk::Tr(L"已下载 "))) + r.name;
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
  int runIdx = -1;
  int pct = 0;
  unsigned long long dn = 0;
  unsigned long long tt = 0;
  {
    std::lock_guard<std::mutex> lk(g_xfRunMtx);
    runIdx = g_xfRunIdx;
    pct = g_xfRunPct;
    dn = g_xfRunDone;
    tt = g_xfRunTotal;
  }
  for (size_t i = 0; i < g_xfRows.size(); ++i) {
    XferRow& r = g_xfRows[i];
    if (r.state >= 2) continue;
    if ((int)i == runIdx) {
      if (r.state != 1 || r.pct != pct || r.done != dn || r.total != tt) {
        r.state = 1;
        r.pct = pct;
        r.done = dn;
        r.total = tt;
        changed = true;
        g_state.sftpStatus = (r.kind == 0 ? std::wstring(sk::Tr(L"上传中 ")) : std::wstring(sk::Tr(L"下载中 "))) +
                             r.name + L" " + std::to_wstring(pct) + L"%";
      }
    } else if (r.state != 0) {
      r.state = 0;
      r.pct = 0;
      changed = true;
    }
  }
  if (changed) XferRebuild();
  bool idle = runIdx < 0;
  if (idle) {
    std::lock_guard<std::mutex> lk(g_xfMtx);
    if (!g_xfJobs.empty()) idle = false;
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
    SftpLoadRemote();
    return;
  }
  if (cmd == 7) {
    std::wstring name = g_state.promptValue;
    while (!name.empty() && (name.front() == L' ' || name.front() == L'/')) name.erase(name.begin());
    while (!name.empty() && name.back() == L' ') name.pop_back();
    if (name.empty()) {
      g_state.sftpStatus = sk::Tr(L"名称不能为空");
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = sk::Tr(L"会话未连接");
      return;
    }
    std::wstring remote = JoinRemote(g_state.sftpPath, name);
    if (svc.SftpMkdir(sid, W2U8(remote))) {
      g_state.sftpStatus = sk::Tr(L"已创建 ") + name;
      SftpLoadRemote();
    } else {
      g_state.sftpStatus = sk::Tr(L"创建失败 ") + name;
    }
    return;
  }
}

bool ClipboardOpen(bool write) {
  HWND owner = write ? g_hwnd : nullptr;
  for (int i = 0; i < 5; ++i) {
    if (OpenClipboard(owner)) return true;
    Sleep(20);
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

void ClipboardSet(const std::wstring& text) {
  if (text.empty() || !ClipboardOpen(true)) return;
  EmptyClipboard();
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (mem != nullptr) {
    void* dst = GlobalLock(mem);
    if (dst != nullptr) {
      memcpy(dst, text.c_str(), bytes);
      GlobalUnlock(mem);
      SetClipboardData(CF_UNICODETEXT, mem);
    }
  }
  CloseClipboard();
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
  int sid = g_state.CurrentSessionId();
  sk::Terminal* term = sk::SessionHub::Instance().Find(sid);
  if (term == nullptr) return;
  int cols = (int)(g.area.W() / g.cw);
  int rows = (int)(g.area.H() / g.lh);
  if (cols < 20) cols = 20;
  if (rows < 6) rows = 6;
  if (cols > 500) cols = 500;
  if (rows > 200) rows = 200;
  if (term->Cols() != cols || term->Rows() != rows) term->Resize(cols, rows);
  int sig = cols * 1000 + rows;
  std::map<int, int>::iterator it = g_ptySize.find(sid);
  if (it != g_ptySize.end() && it->second == sig) return;
  if (!sk::SshService::Instance().IsConnected(sid)) return;
  if (sk::SshService::Instance().ResizePty(sid, cols, rows)) g_ptySize[sid] = sig;
}

void TermSend(const std::string& bytes) {
  int sid = g_state.CurrentSessionId();
  if (sid < 0) return;
  if (sk::SshService::Instance().IsConnected(sid)) {
    sk::SshService::Instance().Send(sid, bytes);
    return;
  }
  sk::Terminal* term = sk::SessionHub::Instance().Find(sid);
  if (term == nullptr) return;
  for (wchar_t ch : U82W(bytes)) term->OnKey(ch, false, false, false);
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
      if (wParam == 2) {
        bool need = g_state.view == sk::View::Term;
        if (need) ApplyTermGrid();
        if (!need && g_state.refreshAt > 0 &&
            (long long)GetTickCount64() - g_state.refreshAt < 1400) {
          need = true;
        }
        if (need) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam != 1) return 0;
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
        int cmd = g_state.pendingSftp;
        g_state.pendingSftp = 0;
        RunSftp(cmd);
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
        for (int i = 0; i < (int)g_state.hosts.size(); ++i) {
          if (g_state.hosts[(size_t)i].state == sk::HostState::Live) CollectStats(i);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_APP + 41:
      XferPoll();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_APP + 42:
      PumpProbe();
      return 0;
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
      sk::Terminal* dragTerm = nullptr;
      int row = 0, col = 0;
      if (TermPoint(x, y, dragTerm, row, col)) {
        dragTerm->ClearSelection();
        g_state.searchFocus = false;
        g_state.selectAll = false;
        g_drag.active = true;
        g_drag.sid = g_state.CurrentSessionId();
        g_drag.row = row;
        g_drag.col = col;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
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
      SyncRender();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_LBUTTONUP: {
      if (!g_drag.active) break;
      g_drag.active = false;
      sk::Terminal* term = sk::SessionHub::Instance().Find(g_drag.sid);
      if (term != nullptr && term->HasSelection()) ClipboardSet(term->SelectedText());
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
            id == sk::HitFtpRefresh ||
            (id >= sk::HitTunRefreshBase && id < sk::HitSetNavBase) ||
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
    case WM_LBUTTONDBLCLK: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
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
      if (id >= sk::HitFtpRowBase && id < sk::HitFtpRowBase + 40) {
        g_state.sftpSide = 0;
        g_state.sftpSelLocal = id - sk::HitFtpRowBase;
        g_state.pendingSftp = 5;
      } else if (id >= sk::HitFtpRowBase + 40 && id < sk::HitTunNew) {
        g_state.sftpSide = 1;
        g_state.sftpSelRemote = id - sk::HitFtpRowBase - 40;
        g_state.pendingSftp = 6;
      } else {
        sk::OnHit(g_state, id, hwnd);
      }
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
      if (g_state.view == sk::View::Term) {
        sk::Terminal* term = TermOf(g_state.CurrentSessionId());
        if (term) term->ScrollToBottom();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case kMsgSsh: {
      int sid = (int)wParam;
      sk::SshService& svc = sk::SshService::Instance();
      std::string data = svc.PumpOutput(sid);
      sk::Terminal* term = sk::SessionHub::Instance().Find(sid);
      if (term != nullptr && !data.empty()) term->Feed(data);

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
          g_state.hosts[(size_t)hid].lastConnected = sk::NowStamp();
          g_state.hosts[(size_t)hid].retryN = 0;
          g_state.hosts[(size_t)hid].retryAt = 0;
          g_state.hosts[(size_t)hid].manualStop = false;
          g_state.dirty = true;
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = true;
        g_state.statsNextAt = 0;
        g_state.tunNextAt = 0;
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
          std::string bytes = KeyToBytes(ch, ctrl);
          if (!bytes.empty()) sk::SshService::Instance().Send(sid, bytes);
        } else if (term != nullptr && (ch >= 32 || ch == L'\r' || ch == 8)) {
          term->OnKey(ch, ctrl, false, false);
          InvalidateRect(hwnd, nullptr, FALSE);
        }
      }
      return 0;
    }
    case WM_KEYDOWN: {
      UINT vk = KeyVk((UINT)wParam, lParam);
      bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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
      if (ctrl && vk == '1') { g_suppressChar = true; g_state.OpenPage(sk::View::Term); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '2') { g_suppressChar = true; g_state.OpenPage(sk::View::Sftp); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '3') { g_suppressChar = true; g_state.OpenPage(sk::View::Tun); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '4') { g_suppressChar = true; g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == '0') { g_suppressChar = true; g_state.OpenPage(sk::View::Dash); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == 'K') { g_suppressChar = true; g_state.searchFocus = true; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && vk == 'W') {
        g_suppressChar = true;
        int sid = g_state.CurrentSessionId();
        if (sid >= 0) g_state.CloseTab(g_state.activeTab);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrl && vk == VK_OEM_COMMA) { g_suppressChar = true; g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
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
    case WM_DESTROY:
      sk::SaveStore(g_state);
      XferStop();
      ProbeStop();
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

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}
