#pragma once
#include <string>
#include <vector>
#include "app/Theme.h"
#include "app/Layout.h"

namespace sk {

enum class View { Dash = 0, Term = 1, Sftp = 2, Tun = 3, Set = 4 };
enum class HostState { Off, Busy, Live };

struct Host {
  std::wstring name;
  std::wstring addr;
  std::wstring auth;
  HostState state = HostState::Off;
  int daysLeft = -1;
  bool autoConnect = false;
  int latency = -1;
  std::wstring keyPath;
  std::wstring hostKey;
};

struct Tab {
  int view = 0;
  std::wstring title;
  int icon = 0;
  bool live = false;
};

struct AppState {
  View view = View::Dash;
  bool dark = true;
  float termFontSize = 13.0f;
  float sidebarW = 240.0f;
  int activeHost = 0;
  int activeTab = 0;
  std::vector<Host> hosts;
  std::vector<Tab> tabs;
  std::wstring query;
  std::wstring statusHost;
  int statusLatency = 12;
  UINT dpi = 96;

  float S(float v) const { return v * dpi / 96.0f; }
  void Seed();
};

const wchar_t* ViewTitle(View v);

}
