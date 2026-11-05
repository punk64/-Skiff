#pragma once
#include <windows.h>
#include <string>
#include "ui/Render.h"
#include "app/App.h"

namespace sk {

class Terminal;
extern Terminal g_term;

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
  HitTabBase = 100, HitTabAdd = 130,
  HitHostBase = 200,
  HitDashCardBase = 300, HitDashNewHost = 320,
  HitFtpUp = 400, HitFtpDown, HitFtpRefresh, HitFtpQueue,
  HitTunNew = 500, HitTunSwitchBase = 520,
  HitSetNavBase = 600,
  HitStatusConn = 700
};

void DrawAll(Render& r, AppState& st, const Rect& client);
void DrawDash(Render& r, const AppState& st, const Rect& rc);
void DrawTermView(Render& r, const AppState& st, const Rect& rc);
void DrawSftp(Render& r, const AppState& st, const Rect& rc);
void DrawTun(Render& r, const AppState& st, const Rect& rc);
void DrawSet(Render& r, const AppState& st, const Rect& rc);
int HitTest(const AppState& st, const Rect& client, float x, float y);
void OnHit(AppState& st, int id);
void LayoutMetrics(const AppState& st, const Rect& client, Rect& sidebar, Rect& tabs, Rect& content, Rect& status);

}
