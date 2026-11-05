#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "app/App.h"
#include "app/Layout.h"
#include "app/Config.h"
#include "ui/Render.h"
#include "ui/Chrome.h"
#include "ui/Terminal.h"
#include "ui/Sessions.h"
#include "core/SshService.h"
#include "compat/Log.h"

extern "C" void skPlatformInit(void);
extern "C" void skPlatformShutdown(void);

namespace sk {
void LogWrite(const char* level, const char* fmt, ...) {
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
const wchar_t* kWindowTitle = L"轻舟 Skiff";
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

std::wstring ExeDir() {
  wchar_t buf[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p(buf);
  size_t slash = p.find_last_of(L'\\');
  return slash == std::wstring::npos ? std::wstring() : p.substr(0, slash);
}

std::wstring FindKeyFor(const std::string& hostName) {
  std::wstring dir = ExeDir();
  if (dir.empty()) return std::wstring();
  std::wstring base = dir + L"\\keys\\" + U82W(hostName);
  const wchar_t* exts[] = { L".pem", L".key", L"_rsa", L"" };
  for (int i = 0; i < 4; ++i) {
    std::wstring p = base + exts[i];
    DWORD attr = GetFileAttributesW(p.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) return p;
  }
  return std::wstring();
}

std::wstring ResolveKeyPath(const std::wstring& raw) {
  if (raw.empty()) return raw;
  bool abs = raw.find(L'\\') != std::wstring::npos || raw.find(L'/') != std::wstring::npos ||
             (raw.size() > 1 && raw[1] == L':');
  if (abs) return raw;
  std::wstring dir = sk::KeyDirectory();
  return dir.empty() ? raw : dir + L"\\" + raw;
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
  p.expectedHostKey = W2U8(h.hostKey);
  p.keepAlive = 60;
  h.state = sk::HostState::Busy;
  sk::SshService::Instance().Connect(sid, p);
}

void RequestConnect(int idx) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  int sid = g_state.OpenHostSession(idx);
  if (sid < 0) return;
  sk::Terminal* term = TermOf(sid);
  if (!term) return;

  sk::SshService& svc = sk::SshService::Instance();
  sk::SshState st = svc.State(sid);
  if (st == sk::SshState::Connected || st == sk::SshState::Connecting) return;

  sk::Host& h = g_state.hosts[(size_t)idx];
  std::string user = W2U8(h.user);
  std::string host = W2U8(h.host);
  int port = h.port;
  if (host.empty()) {
    term->Feed("\r\n\x1b[31m主机地址为空\x1b[0m\r\n");
    return;
  }

  g_pwd.erase(sid);
  term->Feed("\r\n\x1b[90m连接 " + user + "@" + host + ":" + std::to_string(port) + "…\x1b[0m\r\n");

  std::wstring key = h.keyPath.empty() ? FindKeyFor(host) : h.keyPath;
  if (!key.empty()) {
    StartConnect(idx, sid, std::string(), key);
    return;
  }
  PwdState& ps = g_pwd[sid];
  ps.active = true;
  ps.buf.clear();
  std::string prompt = user + "@" + host + "'s password: ";
  term->Feed("\r\n" + prompt);
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
  int tabIdx = g_state.FindTabByHost(idx);
  if (tabIdx < 0) return;
  int sid = g_state.tabs[(size_t)tabIdx].sessionId;
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

void PatrolTunnels() {
  ULONGLONG now = GetTickCount64();
  if (now < (ULONGLONG)g_state.tunNextAt) return;
  g_state.tunNextAt = (long long)(now + 10000);
  sk::SshService& svc = sk::SshService::Instance();
  bool changed = false;
  for (size_t hi = 0; hi < g_state.hosts.size(); ++hi) {
    sk::Host& h = g_state.hosts[hi];
    int tabIdx = g_state.FindTabByHost((int)hi);
    int sid = tabIdx >= 0 ? g_state.tabs[(size_t)tabIdx].sessionId : -1;
    bool live = sid >= 0 && svc.IsConnected(sid);
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      sk::Tunnel& tn = h.tunnels[k];
      int port = _wtoi(tn.port.c_str());
      if (port <= 0 || port > 65535) continue;
      if (!tn.on) {
        if (tn.state != sk::TunState::Down) {
          svc.ForwardStopLocal(sid, port);
          tn.state = sk::TunState::Down;
          tn.retryIn = 0;
          changed = true;
        }
        continue;
      }
      if (!live) {
        if (tn.state != sk::TunState::Retry || tn.retryIn != -1) {
          tn.state = sk::TunState::Retry;
          tn.retryIn = -1;
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
      if ((long long)now < tn.nextAt) continue;
      bool active = svc.ForwardActive(sid, port);
      if (!active) {
        std::string th;
        int tp = 0;
        SplitHostPort(tn.dest, th, tp);
        if (tp > 0 && !th.empty() && svc.ForwardStartLocal(sid, port, th, tp)) {
          tn.state = sk::TunState::Listen;
          tn.fails = 0;
          tn.retryIn = 0;
          tn.nextAt = (long long)(now + 2000);
          changed = true;
        } else {
          ++tn.fails;
          int wait = tn.fails <= 1 ? 10 : (tn.fails == 2 ? 20 : (tn.fails == 3 ? 40 : 60));
          tn.state = sk::TunState::Retry;
          tn.retryIn = wait;
          tn.nextAt = (long long)(now + (ULONGLONG)wait * 1000ULL);
          changed = true;
        }
        continue;
      }
      int acc = svc.ForwardAccepts(sid);
      sk::TunState ns = acc > 0 ? sk::TunState::Up : sk::TunState::Listen;
      if (tn.state != ns) {
        tn.state = ns;
        tn.retryIn = 0;
        changed = true;
      }
    }
  }
  if (changed && g_hwnd != nullptr) InvalidateRect(g_hwnd, nullptr, FALSE);
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
    err = L"无法打开本地目录";
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

void SftpLoadRemote() {
  int sid = g_state.CurrentSessionId();
  if (sid < 0) {
    g_state.sftpStatus = L"未连接";
    g_state.sftpRemote.clear();
    return;
  }
  sk::SshService& svc = sk::SshService::Instance();
  if (!svc.IsConnected(sid)) {
    g_state.sftpStatus = L"会话未连接";
    g_state.sftpRemote.clear();
    return;
  }
  std::vector<sk::RemoteEntry> list;
  std::string path = W2U8(g_state.sftpPath);
  if (!svc.SftpList(sid, path, list)) {
    g_state.sftpStatus = L"远程目录读取失败";
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
  g_state.sftpStatus = L"已读取 " + std::to_wstring(list.size()) + L" 项";
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
      g_state.sftpStatus = L"先选中一个本地文件";
      return;
    }
    const sk::SftpEntry& e = g_state.sftpLocal[(size_t)g_state.sftpSelLocal];
    if (e.dir) {
      g_state.sftpStatus = L"目录暂不支持上传";
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = L"会话未连接";
      return;
    }
    std::wstring local = g_state.sftpLocalPath;
    if (!local.empty() && local.back() != L'\\' && local.back() != L'/') local += L"\\";
    std::string err;
    std::wstring remote = JoinRemote(g_state.sftpPath, e.name);
    g_state.sftpStatus = L"上传中 " + e.name;
    if (svc.SftpUpload(sid, local + e.name, W2U8(remote), err)) {
      g_state.sftpStatus = L"已上传 " + e.name;
    } else {
      g_state.sftpStatus = L"上传失败 " + U82W(err);
    }
    SftpLoadRemote();
    return;
  }
  if (cmd == 4) {
    if (g_state.sftpSelRemote < 0 || g_state.sftpSelRemote >= (int)g_state.sftpRemote.size()) {
      g_state.sftpStatus = L"先选中一个远程文件";
      return;
    }
    const sk::SftpEntry& e = g_state.sftpRemote[(size_t)g_state.sftpSelRemote];
    if (e.dir) {
      g_state.sftpStatus = L"目录暂不支持下载";
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = L"会话未连接";
      return;
    }
    std::wstring local = g_state.sftpLocalPath;
    if (!local.empty() && local.back() != L'\\' && local.back() != L'/') local += L"\\";
    std::string err;
    std::wstring remote = JoinRemote(g_state.sftpPath, e.name);
    g_state.sftpStatus = L"下载中 " + e.name;
    if (svc.SftpDownload(sid, W2U8(remote), local + e.name, err)) {
      g_state.sftpStatus = L"已下载 " + e.name;
    } else {
      g_state.sftpStatus = L"下载失败 " + U82W(err);
    }
    SftpLoadLocal();
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
      g_state.sftpStatus = L"名称不能为空";
      return;
    }
    if (sid < 0 || !svc.IsConnected(sid)) {
      g_state.sftpStatus = L"会话未连接";
      return;
    }
    std::wstring remote = JoinRemote(g_state.sftpPath, name);
    if (svc.SftpMkdir(sid, W2U8(remote))) {
      g_state.sftpStatus = L"已创建 " + name;
      SftpLoadRemote();
    } else {
      g_state.sftpStatus = L"创建失败 " + name;
    }
    return;
  }
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
    case WM_CREATE:
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
      g_state.statusHost = g_state.hosts.empty()
                             ? std::wstring()
                             : g_state.hosts[g_state.activeHost].name;
      g_state.statusAddr = g_state.hosts.empty()
                             ? std::wstring()
                             : g_state.hosts[(size_t)g_state.activeHost].Addr();
      SetTimer(hwnd, 1, 900, nullptr);
      sk::SshService::Instance().SetNotify(&OnSshNotify, hwnd);
      return 0;
    case WM_SIZE:
      Relayout(hwnd);
      g_render.Resize(LOWORD(lParam), HIWORD(lParam), (float)g_state.dpi);
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
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER: {
      if (wParam != 1) return 0;
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
      if (g_state.pendingDisconnect >= 0) {
        int idx = g_state.pendingDisconnect;
        g_state.pendingDisconnect = -1;
        int tabIdx = g_state.FindTabByHost(idx);
        if (tabIdx >= 0) {
          int sid = g_state.tabs[(size_t)tabIdx].sessionId;
          if (sid >= 0) sk::SshService::Instance().Disconnect(sid);
        }
      }
      PatrolTunnels();
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
      int id = sk::HitTest(g_state, g_client, x, y);
      sk::OnHit(g_state, id, hwnd);
      if (g_state.pendingConnect >= 0) {
        int idx = g_state.pendingConnect;
        g_state.pendingConnect = -1;
        if (g_state.modal == sk::ModalKind::None) RequestConnect(idx);
      }
      if (g_state.modal == sk::ModalKind::None) SyncTheme();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      int hover = -1;
      if (!g_state.fullscreen) {
        sk::Rect sidebar, tabs, content, status;
        sk::LayoutMetrics(g_state, g_client, sidebar, tabs, content, status);
        hover = sk::SidebarHostAt(g_state, sidebar, x, y);
      }
      if (hover != g_state.hoverHost) {
        g_state.hoverHost = hover;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
      TrackMouseEvent(&tme);
      return 0;
    }
    case WM_MOUSELEAVE: {
      if (g_state.hoverHost != -1) {
        g_state.hoverHost = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_RBUTTONUP: {
      float x = (float)GET_X_LPARAM(lParam);
      float y = (float)GET_Y_LPARAM(lParam);
      int id = sk::HitTest(g_state, g_client, x, y);
      int hostIdx = -1;
      if (id >= sk::HitHostBase && id < sk::HitDashCardBase) {
        hostIdx = id - sk::HitHostBase;
      } else if (id >= sk::HitDashCardBase && id < sk::HitDashNewHost) {
        hostIdx = id - sk::HitDashCardBase;
      }
      if (hostIdx < 0 || hostIdx >= (int)g_state.hosts.size()) return 0;

      POINT pt{ (LONG)x, (LONG)y };
      ClientToScreen(hwnd, &pt);
      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu, MF_STRING, 1, L"连接");
      AppendMenuW(menu, MF_STRING, 2, L"编辑");
      AppendMenuW(menu, MF_STRING, 3, L"复制地址");
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      AppendMenuW(menu, MF_STRING, 4, L"删除");
      SetForegroundWindow(hwnd);
      int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd,
                                    nullptr);
      DestroyMenu(menu);
      if (cmd == 1) {
        RequestConnect(hostIdx);
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
        g_state.modalTitle = L"删除主机";
        g_state.modalNote = g_state.hosts[(size_t)hostIdx].Addr();
        g_state.confirmNote = L"将断开该主机的全部会话并删除其转发规则";
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
          InvalidateRect(hwnd, nullptr, FALSE);
        }
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

      if (st == sk::SshState::Connected) {
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Live;
          g_state.hosts[(size_t)hid].hostKey = U82W(svc.HostKey(sid));
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = true;
        g_state.statsNextAt = 0;
        g_state.tunNextAt = 0;
      } else if (st == sk::SshState::Failed) {
        std::string err = svc.LastError(sid);
        if (term != nullptr) term->Feed("\r\n\x1b[31m连接失败: " + err + "\x1b[0m\r\n");
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Off;
          g_state.hosts[(size_t)hid].latency = -1;
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = false;
        g_pwd.erase(sid);
      } else if (st == sk::SshState::Disconnected) {
        if (term != nullptr) term->Feed("\r\n\x1b[90m会话已断开\x1b[0m\r\n");
        if (hid >= 0 && hid < (int)g_state.hosts.size()) {
          g_state.hosts[(size_t)hid].state = sk::HostState::Off;
          g_state.hosts[(size_t)hid].latency = -1;
        }
        if (tabIdx >= 0) g_state.tabs[(size_t)tabIdx].live = false;
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
      if (g_state.modal != sk::ModalKind::None) {
        if ((sk::ModalIsForm(g_state) || g_state.modal == sk::ModalKind::Prompt) && ch != L'\r' &&
            ch != L'\t') {
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
      if (g_state.modal != sk::ModalKind::None) {
        if (wParam == VK_RETURN) g_suppressChar = true;
        sk::ModalKey(g_state, (int)wParam);
        SyncTheme();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      if (ctrl && wParam == '1') { g_suppressChar = true; g_state.OpenPage(sk::View::Term); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '2') { g_suppressChar = true; g_state.OpenPage(sk::View::Sftp); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '3') { g_suppressChar = true; g_state.OpenPage(sk::View::Tun); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '4') { g_suppressChar = true; g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '0') { g_suppressChar = true; g_state.OpenPage(sk::View::Dash); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == 'K') { g_suppressChar = true; g_state.searchFocus = true; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == 'W') {
        g_suppressChar = true;
        int sid = g_state.CurrentSessionId();
        if (sid >= 0) g_state.CloseTab(g_state.activeTab);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrl && wParam == VK_OEM_COMMA) { g_suppressChar = true; g_state.OpenPage(sk::View::Set); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (wParam == VK_F11) {
        ToggleFullscreen(hwnd, !g_state.fullscreen);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_ESCAPE && g_state.fullscreen) {
        ToggleFullscreen(hwnd, false);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_ESCAPE && g_state.searchFocus) {
        g_state.searchFocus = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      break;
    }
    case WM_DESTROY:
      sk::SaveStore(g_state);
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
