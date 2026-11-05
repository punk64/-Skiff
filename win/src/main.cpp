#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdarg>
#include <string>

#include "app/App.h"
#include "app/Layout.h"
#include "ui/Render.h"
#include "ui/Chrome.h"
#include "ui/Terminal.h"
#include "compat/Log.h"

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
}
}

namespace {

const wchar_t* kClassName = L"SkiffWinMain";
const wchar_t* kWindowTitle = L"轻舟 Skiff";

sk::Render g_render;
sk::AppState g_state;
sk::Rect g_client;
using sk::g_term;

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
      g_render.Init(hwnd);
      g_render.Resize((UINT)g_client.W(), (UINT)g_client.H(), (float)g_state.dpi);
      g_state.Seed();
      g_state.statusHost = g_state.hosts.empty() ? L"" : g_state.hosts[0].name;
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
    case WM_CHAR: {
      if (g_state.view == sk::View::Term) {
        wchar_t ch = (wchar_t)wParam;
        if (ch >= 32 || ch == L'\r' || ch == 8) {
          bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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
      g_render.Shutdown();
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
