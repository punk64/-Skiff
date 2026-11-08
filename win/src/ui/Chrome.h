#pragma once
#include <windows.h>
#include <string>
#include "ui/Render.h"
#include "app/App.h"

namespace sk {

class Terminal;

struct Rect {
  float l = 0, t = 0, r = 0, b = 0;
  float W() const { return r - l; }
  float H() const { return b - t; }
  bool Hit(float x, float y) const { return x >= l && x < r && y >= t && y < b; }
  D2D1_RECT_F D2D() const { return D2D1::RectF(l, t, r, b); }
  Rect Inset(float v) const { return Rect{ l + v, t + v, r - v, b - v }; }
};

enum HitId {
  HitNone = 0,
  HitTitleMin = 1, HitTitleMax, HitTitleClose,
  HitSearch = 10, HitNewHost, HitSettings, HitHelp,
  HitTabBase = 100, HitTabAdd = 130, HitTabsRightBase = 132,
  HitHostBase = 200,
  HitDashCardBase = 300, HitDashNewHost = 320, HitDashEditBase = 330,
  HitFtpNew = 400, HitFtpRefresh, HitFtpUp, HitFtpDown, HitFtpQueue,
  HitFtpRowBase = 420,
  HitTunNew = 500, HitTunSwitchBase = 520,
  HitTunRefreshBase = 540, HitTunEditBase = 560, HitTunDelBase = 580,
  HitSetNavBase = 600,
  HitSetCtrlBase = 640,
  HitStatusConn = 700,
  HitModalBackdrop = 800, HitModalClose, HitModalOk, HitModalCancel,
  HitTabCloseBase = 900, HitHostEditBase = 940,
  HitFormFieldBase = 1000, HitFormSegBase = 1020, HitFormBrowseBase = 1100,
  HitModalKeyStore = 1120, HitKeyRowBase = 1130,
  HitBackupPlain = 1150,
  HitSessSegBase = 1160,
  HitDashToggleBase = 1170, HitDashQuickBase = 1190, HitDashStatBase = 1300,
  HitTunRowBase = 1400,
  HitNavBase = 1500, HitDashSearch = 1510,
  HitDpPrev = 1520, HitDpNext, HitDpToday, HitDpClear, HitDpHour, HitDpMinute, HitDpSecond,
  HitDpDayBase = 1540, HitDpCellBase = 1600,
  HitTunBrowseBase = 1670,
  HitSideCardBase = 2000, HitRecentBase = 2400,
  HitSortKey = 2500, HitSortDir = 2501, HitSideSearch = 2510,
  HitSideSettings = 2511, HitSideCollapse = 2512, HitSideExpand = 2513
};

enum SetKindId {
  SetSeg = 0, SetSwatch = 1, SetDrop = 2, SetSwitch = 3, SetPlain = 4, SetLink = 5,
  SetAction = 6
};

constexpr float kSideCollapsedW = 52.0f;
constexpr int kSideCardSlots = 8;
constexpr int kSideCardCap = 50;
constexpr int kRecentCap = 3;

void DrawAll(Render& r, AppState& st, const Rect& client);
void DrawDash(Render& r, const AppState& st, const Rect& rc);
void DrawTermView(Render& r, const AppState& st, const Rect& rc);
void DrawSftp(Render& r, const AppState& st, const Rect& rc);
void DrawTun(Render& r, const AppState& st, const Rect& rc);
void DrawSet(Render& r, const AppState& st, const Rect& rc);
int HitTest(const AppState& st, const Rect& client, float x, float y);
void OnHit(AppState& st, int id, HWND hwnd = nullptr);
Rect SetNavRect(const AppState& st, const Rect& c, int index);
Rect SetRowRect(const AppState& st, const Rect& c, int index);
int SetRowCount(int nav);
int SetKindOf(int nav, int row);
int* SetIntSlot(AppState& st, int nav, int row);
bool* SetBoolSlot(AppState& st, int nav, int row);
int SetIntValue(const AppState& st, int nav, int row);
bool SetBoolValue(const AppState& st, int nav, int row);
int SetOptionCount(int nav, int row);
int SidebarVisibleHosts(const AppState& st, int* out, int cap);
int RecentHosts(const AppState& st, int* out, int cap);
float SideScrollMax(const AppState& st, const Rect& sidebar);
int SidebarHostAt(const AppState& st, const Rect& sidebar, float x, float y);
Rect DashSearchRect(const AppState& st, const Rect& c);
Rect DashStatCardRect(const AppState& st, const Rect& c, int index);
Rect DashRecentCardRect(const AppState& st, const Rect& c, int index);
Rect DashRecentBtnRect(const AppState& st, const Rect& c, int index);
void LayoutMetrics(const AppState& st, const Rect& client, Rect& sidebar, Rect& tabs, Rect& content, Rect& status);
Rect TunOpRectOf(const AppState& st, const Rect& c, int index, int slot);
Rect SftpPanelRect(const AppState& st, const Rect& c, int side);
Rect SftpRowRectOf(const AppState& st, const Rect& c, int side, int row);
Rect FtpActionRectOf(const AppState& st, const Rect& c, int slot);
Rect DashEditRectOf(const AppState& st, const Rect& c, int index);
Rect DashToggleRectOf(const AppState& st, const Rect& c, int index);
Rect DashQuickRectOf(const AppState& st, const Rect& c, int index, int slot);
Rect DashStatRectOf(const AppState& st, const Rect& c, int index, int slot);
Rect SessSegRectOf(const AppState& st, const Rect& c, int slot);
int SessSegAt(const AppState& st, const Rect& c, float x, float y);
Rect SessInnerRect(const AppState& st, const Rect& c);
Rect TunRowRectOf(const AppState& st, const Rect& c, int index);
int TunIndexOfId(const Host& h, int tunnelId);
void DrawModal(Render& r, AppState& st, const Rect& client);
bool ModalIsForm(const AppState& st);
void ModalChar(AppState& st, wchar_t ch);
void ModalKey(AppState& st, int vk);

}
