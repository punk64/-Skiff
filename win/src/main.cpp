#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

#include "app/App.h"
#include "app/Layout.h"
#include "ui/Render.h"
#include "ui/Chrome.h"
#include "ui/Terminal.h"
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

sk::Render g_render;
sk::AppState g_state;
sk::Rect g_client;
using sk::g_term;

const UINT kMsgSsh = WM_APP + 1;

bool g_pwdMode = false;
int g_pwdHost = -1;
std::string g_pwdBuf;

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

bool ParseSpec(const std::wstring& spec, std::string& user, std::string& host, int& port) {
  std::string s = W2U8(spec);
  size_t at = s.rfind('@');
  if (at != std::string::npos) {
    user = s.substr(0, at);
    host = s.substr(at + 1);
  } else {
    user = "root";
    host = s;
  }
  size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    port = atoi(host.c_str() + colon + 1);
    host = host.substr(0, colon);
  } else {
    port = 22;
  }
  if (user.empty()) user = "root";
  return !host.empty();
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

void StartConnect(int idx, const std::string& password, const std::wstring& keyPath) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  sk::Host& h = g_state.hosts[idx];
  sk::SshParams p;
  if (!ParseSpec(h.addr, p.user, p.host, p.port)) {
    g_term.Feed("\r\n\x1b[31m地址格式无效\x1b[0m\r\n");
    return;
  }
  p.password = password;
  p.useKey = !keyPath.empty();
  p.keyPath = W2U8(keyPath);
  p.expectedHostKey = W2U8(h.hostKey);
  p.keepAlive = 60;
  h.state = sk::HostState::Busy;
  sk::SshService::Instance().Connect(p);
}

void RequestConnect(int idx) {
  if (idx < 0 || idx >= (int)g_state.hosts.size()) return;
  sk::Host& h = g_state.hosts[idx];
  g_state.view = sk::View::Term;
  g_state.activeHost = idx;
  std::string user;
  std::string host;
  int port = 22;
  if (!ParseSpec(h.addr, user, host, port)) return;
  std::wstring key = h.keyPath.empty() ? FindKeyFor(host) : h.keyPath;
  if (!key.empty()) {
    StartConnect(idx, std::string(), key);
    return;
  }
  g_pwdMode = true;
  g_pwdHost = idx;
  g_pwdBuf.clear();
  std::string prompt = user + "@" + host + "'s password: ";
  g_term.Feed("\r\n" + prompt);
}

void OnSshNotify(void* ctx) {
  HWND hwnd = reinterpret_cast<HWND>(ctx);
  if (hwnd != nullptr) PostMessageW(hwnd, kMsgSsh, 0, 0);
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
  if (pt.y < titleH) {
    if (pt.x > wr.right - wr.left - g_state.S(132)) return HTCLIENT;
    return HTCAPTION;
  }
  return HTCLIENT;
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
        return 0;
      }
      return DefWindowProc(hwnd, msg, wParam, lParam);
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
      Relayout(hwnd);
      skPlatformInit();
      if (!g_render.Init(hwnd)) sk::LogWrite("E", "render init failed");
      g_render.Resize((UINT)g_client.W(), (UINT)g_client.H(), (float)g_state.dpi);
      g_state.Seed();
      g_state.statusHost = g_state.hosts.empty() ? L"" : g_state.hosts[0].name;
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
      sk::OnHit(g_state, id);
      int hostIdx = -1;
      if (id >= sk::HitHostBase && id < sk::HitDashCardBase) {
        hostIdx = id - sk::HitHostBase;
      } else if (id >= sk::HitDashCardBase && id < sk::HitDashNewHost) {
        hostIdx = id - sk::HitDashCardBase;
      }
      if (hostIdx >= 0 && hostIdx < (int)g_state.hosts.size()) {
        RequestConnect(hostIdx);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      if (g_state.view == sk::View::Term) {
        g_term.ScrollToBottom();
        InvalidateRect(hwnd, nullptr, FALSE);
      } else if ((GetKeyState(VK_CONTROL) & 0x8000) && g_state.view == sk::View::Term) {
        g_state.termFontSize += delta > 0 ? 1.0f : -1.0f;
        if (g_state.termFontSize < 10) g_state.termFontSize = 10;
        if (g_state.termFontSize > 20) g_state.termFontSize = 20;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case kMsgSsh: {
      std::string data = sk::SshService::Instance().PumpOutput();
      if (!data.empty()) {
        g_term.Feed(data);
      }
      sk::SshState st = sk::SshService::Instance().State();
      if (st == sk::SshState::Connected) {
        for (size_t i = 0; i < g_state.hosts.size(); ++i) {
          if (W2U8(g_state.hosts[i].addr).find(sk::SshService::Instance().ActiveHost()) != std::string::npos) {
            g_state.hosts[i].state = sk::HostState::Live;
            g_state.hosts[i].latency = 0;
            g_state.hosts[i].hostKey = U82W(sk::SshService::Instance().HostKey());
            break;
          }
        }
      } else if (st == sk::SshState::Failed) {
        std::string err = sk::SshService::Instance().LastError();
        g_term.Feed("\r\n\x1b[31m连接失败: " + err + "\x1b[0m\r\n");
        for (size_t i = 0; i < g_state.hosts.size(); ++i) {
          if (g_state.hosts[i].state == sk::HostState::Busy) g_state.hosts[i].state = sk::HostState::Off;
        }
      } else if (st == sk::SshState::Disconnected) {
        for (size_t i = 0; i < g_state.hosts.size(); ++i) {
          if (g_state.hosts[i].state == sk::HostState::Live || g_state.hosts[i].state == sk::HostState::Busy) {
            g_state.hosts[i].state = sk::HostState::Off;
            g_state.hosts[i].latency = -1;
          }
        }
      }
      if (!data.empty() || st != sk::SshState::Connected) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_CHAR: {
      if (g_state.view == sk::View::Term) {
        wchar_t ch = (wchar_t)wParam;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (g_pwdMode) {
          if (ch == L'\r') {
            g_term.Feed("\r\n");
            g_pwdMode = false;
            int idx = g_pwdHost;
            std::string pwd = g_pwdBuf;
            g_pwdHost = -1;
            g_pwdBuf.clear();
            StartConnect(idx, pwd, std::wstring());
          } else if (ch == L'\b') {
            if (!g_pwdBuf.empty()) {
              g_pwdBuf.pop_back();
              g_term.Feed("\b \b");
            }
          } else if (ch >= 32) {
            g_pwdBuf += (char)ch;
            g_term.Feed("*");
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (sk::SshService::Instance().IsConnected()) {
          std::string bytes = KeyToBytes(ch, ctrl);
          if (!bytes.empty()) sk::SshService::Instance().Send(bytes);
        } else if (ch >= 32 || ch == L'\r' || ch == 8) {
          g_term.OnKey(ch, ctrl, false, false);
          InvalidateRect(hwnd, nullptr, FALSE);
        }
      }
      return 0;
    }
    case WM_KEYDOWN: {
      bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      if (ctrl && wParam == '1') { g_state.view = sk::View::Term; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '2') { g_state.view = sk::View::Sftp; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '3') { g_state.view = sk::View::Tun; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '4') { g_state.view = sk::View::Set; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (ctrl && wParam == '0') { g_state.view = sk::View::Dash; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
      if (wParam == VK_F11) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        bool isPopup = (style & WS_POPUP) != 0;
        MONITORINFO mi{ sizeof(MONITORINFO) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        if (!isPopup) {
          SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
          SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                       mi.rcMonitor.right - mi.rcMonitor.left,
                       mi.rcMonitor.bottom - mi.rcMonitor.top,
                       SWP_FRAMECHANGED);
        } else {
          SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
          SetWindowPos(hwnd, nullptr, 80, 60, 1280, 800, SWP_FRAMECHANGED);
        }
        return 0;
      }
      break;
    }
    case WM_DESTROY:
      sk::SshService::Instance().Disconnect();
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
  wc.style = CS_HREDRAW | CS_VREDRAW;
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
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hwnd = CreateWindowExW(
    0, kClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
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
