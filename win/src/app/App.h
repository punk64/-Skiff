#pragma once
#include <string>
#include <vector>
#include <climits>
#include "app/Theme.h"
#include "app/Layout.h"
#include "app/DateTime.h"

namespace sk {

enum class View { Dash = 0, Term = 1, Sftp = 2, Tun = 3, Set = 4 };
enum class HostState { Off, Busy, Live };
enum class TunState { Up = 0, Listen = 1, Retry = 2, Down = 3 };
enum class ModalKind {
  None = 0, HostForm = 1, TunForm = 2, StatusPanel = 3, Queue = 4, Help = 5,
  Confirm = 6, KeyStore = 7, Prompt = 8, DatePicker = 9, Channel = 10, Backup = 11
};

enum class FieldKind { Text = 0, Seg = 1, Pass = 2, Info = 3, Group = 4, File = 5, Date = 6 };

struct FormField {
  std::wstring key;
  std::wstring label;
  std::wstring value;
  std::wstring hint;
  FieldKind kind = FieldKind::Text;
  std::vector<std::wstring> options;
  int opt = 0;
  bool fresh = false;
  std::wstring depKey;
  int depOpt = -1;
};

struct SftpEntry {
  std::wstring name;
  std::wstring size;
  std::wstring time;
  bool dir = false;
};

struct Tunnel {
  int id = 0;
  int hostId = 0;
  std::wstring type;
  std::wstring port;
  std::wstring dest;
  bool on = true;
  TunState state = TunState::Down;
  int retryIn = 0;
  int fails = 0;
  long long nextAt = 0;
  bool IsRemote() const { return !type.empty() && (type[0] == L'R' || type[0] == L'r'); }
};

struct Host {
  int id = 0;
  int sessionId = -1;
  std::wstring name;
  std::wstring user;
  std::wstring host;
  int port = 22;
  std::wstring auth;
  std::wstring password;
  std::wstring keyPath;
  std::wstring keyPass;
  std::wstring hostKey;
  std::wstring hostKeyType;
  std::wstring registeredAt;
  std::wstring expireAt;
  std::wstring lastConnected;
  bool autoConnect = false;
  HostState state = HostState::Off;
  int latency = -1;
  int cpu = 0;
  int ram = 0;
  std::wstring net;
  int netp = 0;
  long long retryAt = 0;
  int retryN = 0;
  bool manualStop = false;
  std::vector<Tunnel> tunnels;

  std::wstring Addr() const {
    std::wstring s = user.empty() ? std::wstring() : user + L"@";
    s += host;
    if (port > 0) s += L":" + std::to_wstring(port);
    return s;
  }
  bool IsKeyAuth() const { return auth == L"密钥" || auth.compare(0, 2, L"私钥") == 0; }
  int DaysLeft() const {
    if (StampMillis(expireAt) < 0) return INT_MIN;
    return DaysLeftOf(expireAt);
  }
  bool HasExpire() const { return StampMillis(expireAt) >= 0; }
};

struct Tab {
  int view = 0;
  int hostId = -1;
  int sessionId = -1;
  std::wstring title;
  int icon = 0;
  bool live = false;
};

struct AppState {
  View view = View::Dash;
  bool dark = true;
  bool fullscreen = false;
  bool searchFocus = false;
  bool selectAll = false;
  float termFontSize = 13.0f;
  float sidebarW = 360.0f;
  bool sideCollapsed = false;
  int sortKey = 0;
  bool sortAsc = true;
  float sideScroll = 0.0f;
  int activeHost = 0;
  int activeTab = 0;
  int sessTab = 0;
  int setNav = 0;
  int themeMode = 0;
  int langMode = 0;
  int accentIdx = 0;
  int uiScaleIdx = 1;
  int termFontIdx = 0;
  int termSizeIdx = 1;
  int cursorIdx = 0;
  bool autoReconnect = true;
  bool autoConnectAll = false;
  bool restoreLast = true;
  bool fpCheck = true;
  int keepAliveIdx = 1;
  int timeoutIdx = 1;
  int logLevelIdx = 1;
  int logKeepIdx = 0;
  std::vector<Host> hosts;
  std::vector<Tab> tabs;
  std::vector<SftpEntry> sftpRemote;
  std::vector<SftpEntry> sftpLocal;
  std::vector<std::wstring> sftpQueue;
  std::wstring sftpPath = L"/";
  std::wstring sftpLocalPath;
  std::wstring sftpStatus;
  int sftpSelRemote = -1;
  int sftpSelLocal = -1;
  int sftpSide = 1;
  int sftpView = 0;
  bool sftpBusy = false;
  std::wstring query;
  std::wstring statusHost;
  std::wstring statusAddr;
  std::wstring statusKey;
  int statusLatency = 12;
  ModalKind modal = ModalKind::None;
  ModalKind keyReturn = ModalKind::None;
  std::wstring modalTitle;
  std::wstring modalNote;
  std::vector<FormField> form;
  int formFocus = 0;
  int formTarget = -1;
  int formHost = -1;
  int formTunnel = -1;
  std::wstring formError;
  float formScroll = 0.0f;
  float formScrollMax = 0.0f;
  std::vector<std::wstring> keyStore;
  std::wstring confirmTitle;
  std::wstring confirmNote;
  int confirmHost = -1;
  int confirmTunnel = -1;
  std::wstring promptValue;
  int dateField = -1;
  ModalKind dateReturn = ModalKind::HostForm;
  int dpYear = 0;
  int dpMonth = 1;
  int dpDay = 1;
  int dpHour = 0;
  int dpMinute = 0;
  int dpSecond = 0;
  int dpDrop = 0;
  int pendingConnect = -1;
  bool pendingConnectTerm = true;
  int pendingDisconnect = -1;
  int pendingSftp = 0;
  int pendingBackup = 0;
  int backupKind = 0;
  int backupFocus = 0;
  bool backupPlain = false;
  std::wstring backupPass;
  int pendingTunBrowse = -1;
  long long refreshAt = 0;
  int toastKind = 0;
  int chanPort = 0;
  std::wstring chanAddr;
  std::wstring chanNote;
  int nextSessionId = 1;
  int nextHostId = 1;
  int nextTunnelId = 1;
  int hoverHost = -1;
  int hoverId = -1;
  int menuHost = -1;
  long long statsNextAt = 0;
  int statsChunk = 0;
  long long tunNextAt = 0;
  bool tunPatrolling = false;
  int lastView = 0;
  int lastHost = -1;
  std::vector<int> bootQueue;
  long long bootNextAt = 0;
  std::wstring toast;
  long long toastAt = 0;
  bool dirty = false;
  UINT dpi = 96;

  float UiScale() const {
    static const float kScale[4] = { 0.9f, 1.0f, 1.1f, 1.25f };
    int i = uiScaleIdx < 0 ? 1 : (uiScaleIdx > 3 ? 3 : uiScaleIdx);
    return kScale[i];
  }
  float S(float v) const { return v * dpi / 96.0f * UiScale(); }
  int CurrentSessionId() const {
    if (activeTab < 0 || activeTab >= (int)tabs.size()) return -1;
    return tabs[(size_t)activeTab].sessionId;
  }
  int CurrentHostId() const {
    if (activeTab < 0 || activeTab >= (int)tabs.size()) return -1;
    return tabs[(size_t)activeTab].hostId;
  }
  int FindTabByHost(int hostId) const {
    for (size_t i = 0; i < tabs.size(); ++i) {
      if (tabs[i].hostId == hostId && tabs[i].view == (int)View::Term) return (int)i;
    }
    return -1;
  }
  int FindTabBySession(int sessionId) const {
    for (size_t i = 0; i < tabs.size(); ++i) {
      if (tabs[i].sessionId == sessionId) return (int)i;
    }
    return -1;
  }
  int HostIndexById(int id) const {
    for (size_t i = 0; i < hosts.size(); ++i) {
      if (hosts[i].id == id) return (int)i;
    }
    return -1;
  }
  const Host* HostById(int id) const {
    int idx = HostIndexById(id);
    return idx < 0 ? nullptr : &hosts[(size_t)idx];
  }
  int ActiveHostIndex() const {
    if (activeHost < 0 || activeHost >= (int)hosts.size()) return -1;
    return activeHost;
  }
  const Host* ActiveHost() const {
    int idx = ActiveHostIndex();
    return idx < 0 ? nullptr : &hosts[(size_t)idx];
  }
  Host* ActiveHost() {
    int idx = ActiveHostIndex();
    return idx < 0 ? nullptr : &hosts[(size_t)idx];
  }
  Tunnel* TunnelById(int hostIndex, int tunnelId) {
    if (hostIndex < 0 || hostIndex >= (int)hosts.size()) return nullptr;
    std::vector<Tunnel>& list = hosts[(size_t)hostIndex].tunnels;
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].id == tunnelId) return &list[i];
    }
    return nullptr;
  }
  int OpenHostSession(int hostId);
  int SessionForHost(int hostId);
  int OpenHostView(int hostId, int subView);
  void OpenPage(View v);
  void ActivateTab(int index);
  void CloseTab(int index);
  void CloseTabAndSession(int index);
  void RemoveHost(int hostId);
  void RecalcDaysLeft();
  void Seed();
  void OpenHostForm(int index);
  void OpenTunForm(int hostIndex, int tunnelId);
  void OpenKeyStore();
  void RefreshKeyStore();
  void ResetFingerprint(int hostIndex);
  void OpenDatePicker(int fieldIndex);
  void CommitDatePicker();
  void ClearDateField();
  bool CommitForm();
  void MarkDirty() { dirty = true; }
  void MarkLastView() {
    lastView = (int)view;
    int hid = CurrentHostId();
    lastHost = (hid >= 0 && hid < (int)hosts.size()) ? hosts[(size_t)hid].id : -1;
  }
};

const wchar_t* ViewTitle(View v);

}
