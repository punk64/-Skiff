#include "ui/Chrome.h"

#include <commdlg.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/SshService.h"
#include "ui/Sessions.h"
#include "app/Config.h"
#include "app/Lang.h"
#include "app/Version.h"
#include "compat/Log.h"

namespace sk {
namespace {

constexpr float kHalf = 0.5f;

float MidY(const Rect& rc) { return rc.t + rc.H() * kHalf; }

std::wstring Num(int v) { return std::to_wstring(v); }

std::wstring U8ToW(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

const FormField* FieldOf(const std::vector<FormField>& form, const std::wstring& key) {
  for (size_t i = 0; i < form.size(); ++i) {
    if (form[i].key == key) return &form[i];
  }
  return nullptr;
}

std::wstring FormValue(const std::vector<FormField>& form, const std::wstring& key) {
  const FormField* f = FieldOf(form, key);
  return f == nullptr ? std::wstring() : f->value;
}

int FormOpt(const std::vector<FormField>& form, const std::wstring& key) {
  const FormField* f = FieldOf(form, key);
  return f == nullptr ? 0 : f->opt;
}

std::wstring PickKeyFile(HWND owner) {
  wchar_t file[MAX_PATH * 2] = { 0 };
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH * 2;
  ofn.lpstrTitle = Tr(L"选择私钥文件");
  ofn.lpstrFilter = L"私钥文件\0*.pem;*.key;*_rsa;*_ed25519;*\0所有文件\0*.*\0";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn)) return std::wstring();
  return std::wstring(file);
}

float EstTextW(const AppState& st, const std::wstring& s, float fs) {
  float units = 0.0f;
  for (size_t i = 0; i < s.size(); ++i) units += s[i] < 0x2E80 ? 0.56f : 1.0f;
  return st.S(units * fs);
}

Palette BuildPalette(bool dark, int accentIdx) {
  static const unsigned kAccentHex[5] = { 0x3DDC97, 0x60A5FA, 0xA78BFA, 0xFBBF24, 0xF87171 };
  Palette p{};
  p.dark = dark;
  int ai = accentIdx < 0 ? 0 : (accentIdx > 4 ? 4 : accentIdx);
  D2D1_COLOR_F base = Rgb(kAccentHex[ai]);
  if (dark) {
    p.bg = Rgb(0x08090C);
    p.surface = Rgb(0x14171D);
    p.surface2 = Rgb(0x1C2028);
    p.line = Rgb(0x262C37);
    p.lineStrong = Rgb(0x2E3644);
    p.text = Rgb(0xE8EDF5);
    p.text2 = Rgb(0x98A4B6);
    p.text3 = Rgb(0x78849B);
    p.accent = base;
    p.accentSoft = Alpha(base, 0.13f);
    p.danger = Rgb(0xF87171);
    p.warn = Rgb(0xFBBF24);
    p.info = Rgb(0x60A5FA);
    p.violet = Rgb(0xA78BFA);
    p.blue = Rgb(0xE8A44B);
    p.termBg = Rgb(0x08090C);
    p.termText = Rgb(0xD7DEE9);
    p.termDim = Rgb(0x6D7889);
  } else {
    p.bg = Rgb(0xF4F6F9);
    p.surface = Rgb(0xFFFFFF);
    p.surface2 = Rgb(0xEDF0F5);
    p.line = Rgb(0xE2E7EF);
    p.lineStrong = Rgb(0xD6DCE6);
    p.text = Rgb(0x0F141B);
    p.text2 = Rgb(0x5A6474);
    p.text3 = Rgb(0x646F80);
    p.accent = Mix(base, Rgb(0x000000), 0.22f);
    p.accentSoft = Alpha(p.accent, 0.10f);
    p.danger = Rgb(0xDC2626);
    p.warn = Rgb(0xB45309);
    p.info = Rgb(0x2563EB);
    p.violet = Rgb(0x7C3AED);
    p.blue = Rgb(0xC2711D);
    p.termBg = Rgb(0x0B0D11);
    p.termText = Rgb(0xD7DEE9);
    p.termDim = Rgb(0x6D7889);
  }
  float lum = 0.299f * p.accent.r + 0.587f * p.accent.g + 0.114f * p.accent.b;
  p.accentFg = lum > 0.55f ? Rgb(0x0B1014) : Rgb(0xFFFFFF);
  return p;
}

Rect TitleBarRect(const AppState& st, const Rect& client) {
  return Rect{ client.l, client.t, client.r, client.t + st.S(L::TitleH) };
}

Rect TitleButtonRect(const AppState& st, const Rect& client, int index) {
  float w = st.S(44);
  float right = client.r - w * (float)(2 - index);
  return Rect{ right - w, client.t, right, client.t + st.S(L::TitleH) };
}

float SidebarSearchTop(const AppState& st, const Rect& s) {
  return s.t + st.S(16) + st.S(30) + st.S(10) + st.S(2);
}

Rect SidebarSearchRect(const AppState& st, const Rect& s) {
  float top = SidebarSearchTop(st, s);
  return Rect{ s.l + st.S(12), top, s.r - st.S(12), top + st.S(34) };
}

Rect SidebarSectionRect(const AppState& st, const Rect& s) {
  float top = SidebarSearchRect(st, s).b + st.S(6);
  return Rect{ s.l + st.S(16), top, s.r - st.S(16), top + st.S(16) };
}

float SidebarHostListTop(const AppState& st, const Rect& s) {
  return SidebarSectionRect(st, s).b + st.S(4);
}

Rect SidebarHostRect(const AppState& st, const Rect& s, int index) {
  float top = SidebarHostListTop(st, s) + st.S(L::HostRowH) * (float)index;
  return Rect{ s.l + st.S(8), top, s.r - st.S(8), top + st.S(L::HostRowH) };
}

float SidebarFootTop(const AppState& st, const Rect& s) {
  return s.b - st.S(8) - st.S(34) - st.S(2) - st.S(34) - st.S(8);
}

Rect SidebarFootItemRect(const AppState& st, const Rect& s, int index) {
  float top = SidebarFootTop(st, s) + st.S(8) + st.S(36) * (float)index;
  return Rect{ s.l + st.S(8), top, s.r - st.S(8), top + st.S(34) };
}

Rect SideExpandRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(56.0f);
  return Rect{ s.l + st.S(8.0f), top, s.r - st.S(8.0f), top + st.S(36.0f) };
}

Rect SideCollapsedSettingsRect(const AppState& st, const Rect& s) {
  float size = st.S(36.0f);
  float top = s.b - st.S(12.0f) - size;
  float left = s.l + (s.W() - size) * kHalf;
  return Rect{ left, top, left + size, top + size };
}

Rect SideBrandFoldRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(19.0f);
  return Rect{ s.r - st.S(38.0f), top, s.r - st.S(14.0f), top + st.S(24.0f) };
}

Rect SideSettingsRect(const AppState& st, const Rect& s) {
  return SidebarFootItemRect(st, s, 0);
}

Rect SideCollapseRect(const AppState& st, const Rect& s) {
  Rect row = SidebarFootItemRect(st, s, 1);
  float w = st.S(76.0f);
  return Rect{ row.r - w, row.t, row.r, row.b };
}

Rect SideSummaryRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(52.0f);
  return Rect{ s.l + st.S(12.0f), top, s.r - st.S(12.0f), top + st.S(24.0f) };
}

Rect SideSearchRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(82.0f);
  return Rect{ s.l + st.S(12.0f), top, s.r - st.S(12.0f), top + st.S(32.0f) };
}

Rect SideSortKeyRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(120.0f);
  return Rect{ s.l + st.S(12.0f), top, s.r - st.S(12.0f) - st.S(38.0f), top + st.S(26.0f) };
}

Rect SideSortDirRect(const AppState& st, const Rect& s) {
  float top = s.t + st.S(120.0f);
  return Rect{ s.r - st.S(12.0f) - st.S(32.0f), top, s.r - st.S(12.0f), top + st.S(26.0f) };
}

float SideListTop(const AppState& st, const Rect& s) {
  return s.t + st.S(154.0f);
}

float SideListBottom(const AppState& st, const Rect& s) {
  return SidebarFootTop(st, s) - st.S(6.0f);
}

Rect SideCardRect(const AppState& st, const Rect& s, int index) {
  float h = st.S(148.0f);
  float gap = st.S(8.0f);
  float top = SideListTop(st, s) + (h + gap) * (float)index - st.sideScroll;
  return Rect{ s.l + st.S(10.0f), top, s.r - st.S(10.0f), top + h };
}

Rect SideEditRectOf(const AppState& st, const Rect& s, int index) {
  Rect card = SideCardRect(st, s, index);
  float w = st.S(38.0f);
  return Rect{ card.r - st.S(12.0f) - w, card.t + st.S(9.0f), card.r - st.S(12.0f),
               card.t + st.S(9.0f) + st.S(20.0f) };
}

Rect SideBtnRectOf(const AppState& st, const Rect& s, int index, int slot) {
  Rect card = SideCardRect(st, s, index);
  float h = st.S(26.0f);
  float top = card.b - st.S(12.0f) - h;
  float l = card.l + st.S(12.0f);
  float r = card.r - st.S(12.0f);
  float conn = st.S(76.0f);
  float gap = st.S(6.0f);
  float rest = (r - l - conn - gap * 3.0f) / 3.0f;
  if (slot == 2) return Rect{ l, top, l + conn, top + h };
  int k = slot - 3;
  float left = l + conn + gap + (rest + gap) * (float)k;
  return Rect{ left, top, left + rest, top + h };
}

void SideBtn(Render& r, const AppState& st, const Rect& rc, const Palette& p,
             const wchar_t* label, int mode) {
  if (mode == 1) {
    r.FillRound(rc.D2D(), st.S(L::RCtrl), p.accent);
    r.TextVertCenter(label, rc.D2D(), p.accentFg, FontId::Small, Align::Center);
  } else if (mode == 2) {
    r.StrokeRound(rc.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(label, rc.D2D(), p.danger, FontId::Small, Align::Center);
  } else {
    r.StrokeRound(rc.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(label, rc.D2D(), p.text2, FontId::Small, Align::Center);
  }
}

void DrawSideHostCard(Render& r, const AppState& st, const Rect& s, int index,
                      int hostIndex, const Palette& p) {
  Rect card = SideCardRect(st, s, index);
  r.FillRound(card.D2D(), st.S(L::RCard), p.surface2);
  if (st.activeHost == hostIndex) {
    r.StrokeRound(card.D2D(), st.S(L::RCard), p.accent, st.S(1.2f));
  }
  const Host& h = st.hosts[(size_t)hostIndex];

  D2D1_COLOR_F dc = p.text3;
  if (h.state == HostState::Live) dc = p.accent;
  if (h.state == HostState::Busy) dc = p.warn;
  float dotX = card.l + st.S(14.0f);
  float dotY = card.t + st.S(20.0f);
  r.Circle(dotX, dotY, st.S(4.0f), dc);

  Rect edit = SideEditRectOf(st, s, index);
  float nameRight = edit.l - st.S(6.0f);
  float nameLeft = dotX + st.S(10.0f);
  if (nameRight > nameLeft) {
    r.PushClip(D2D1::RectF(nameLeft, card.t, nameRight, card.t + st.S(34.0f)));
    r.TextVertCenter(h.name.c_str(), D2D1::RectF(nameLeft, card.t + st.S(6.0f), nameRight,
                                                 card.t + st.S(32.0f)),
                     p.text, FontId::Title);
    r.PopClip();
  }
  r.TextVertCenter(Tr(L"编辑"), edit.D2D(), p.text3, FontId::Micro, Align::Center);

  r.PushClip(D2D1::RectF(card.l + st.S(12.0f), card.t + st.S(34.0f),
                         card.r - st.S(12.0f), card.t + st.S(54.0f)));
  r.TextVertCenter(h.Addr().c_str(),
                   D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(34.0f),
                               card.r - st.S(12.0f), card.t + st.S(54.0f)),
                   p.text3, FontId::MonoSm);
  r.PopClip();

  std::wstring meta;
  D2D1_COLOR_F mc = p.text3;
  if (h.HasExpire()) {
    int dl = h.DaysLeft();
    meta = dl < 0 ? std::wstring(Tr(L"已过期 ")) + Num(-dl) + Tr(L" 天")
                  : std::wstring(Tr(L"剩 ")) + Num(dl) + Tr(L" 天");
    if (dl < 0) mc = p.danger;
    else if (dl <= 7) mc = p.warn;
  } else {
    meta = Tr(L"未设到期");
  }
  r.TextVertCenter(meta.c_str(),
                   D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(56.0f),
                               card.r - st.S(14.0f), card.t + st.S(76.0f)),
                   mc, FontId::Micro);
  std::wstring reg = h.registeredAt.empty() ? std::wstring(Tr(L"未填注册时间"))
                                            : h.registeredAt.substr(0, 10);
  r.TextVertCenter(reg.c_str(),
                   D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(56.0f),
                               card.r - st.S(14.0f), card.t + st.S(76.0f)),
                   p.text3, FontId::Micro, Align::Right);

  float perfTop = card.t + st.S(80.0f);
  float perfH = st.S(22.0f);
  float perfGap = st.S(6.0f);
  float perfW = (card.W() - st.S(24.0f) - perfGap * 2.0f) / 3.0f;
  std::wstring perfText[3];
  int perfPct[3] = { 0, 0, 0 };
  if (h.state == HostState::Live) {
    perfText[0] = std::wstring(L"CPU ") + (h.cpu > 0 ? Num(h.cpu) + L"%" : std::wstring(L"--"));
    perfText[1] = std::wstring(L"RAM ") + (h.ram > 0 ? Num(h.ram) + L"%" : std::wstring(L"--"));
    perfText[2] = h.net.empty() ? std::wstring(L"--") : h.net;
    perfPct[0] = h.cpu;
    perfPct[1] = h.ram;
    perfPct[2] = h.netp;
  } else {
    perfText[0] = L"CPU --";
    perfText[1] = L"RAM --";
    perfText[2] = L"--";
  }
  for (int k = 0; k < 3; ++k) {
    float lx = card.l + st.S(12.0f) + (perfW + perfGap) * (float)k;
    Rect cell{ lx, perfTop, lx + perfW, perfTop + perfH };
    r.FillRound(cell.D2D(), st.S(5.0f), p.bg);
    if (perfPct[k] > 0) {
      float pw = cell.W() * (float)perfPct[k] / 100.0f;
      if (pw > 0.0f) {
        D2D1_COLOR_F bc = perfPct[k] >= 85 ? p.danger : (perfPct[k] >= 60 ? p.warn : p.accent);
        r.FillRound(D2D1::RectF(cell.l, cell.t, cell.l + pw, cell.b), st.S(5.0f),
                    Alpha(bc, 0.20f));
      }
    }
    r.TextVertCenter(perfText[k].c_str(), cell.D2D(), p.text2, FontId::Micro, Align::Center);
  }

  bool live = h.state == HostState::Live;
  const wchar_t* connLabel = live ? Tr(L"断开") : (h.state == HostState::Busy ? Tr(L"连接中") : Tr(L"连接"));
  int connMode = live ? 2 : (h.state == HostState::Busy ? 0 : 1);
  SideBtn(r, st, SideBtnRectOf(st, s, index, 2), p, connLabel, connMode);
  const wchar_t* names[3] = { Tr(L"终端"), Tr(L"隧道"), Tr(L"文件") };
  for (int k = 0; k < 3; ++k) {
    SideBtn(r, st, SideBtnRectOf(st, s, index, 3 + k), p, names[k], 0);
  }
}

float TabWidthOf(const AppState& st, const Tab& tab) {
  float w = st.S(12) + st.S(13) + st.S(8) + EstTextW(st, tab.title, L::FsSmall) +
            st.S(8) + st.S(17) + st.S(10);
  float mn = st.S(L::TabMinW);
  float mx = st.S(L::TabMaxW);
  if (w < mn) w = mn;
  if (w > mx) w = mx;
  return w;
}

Rect TabRectOf(const AppState& st, const Rect& tabs, int index) {
  float x = tabs.l + st.S(6);
  for (int i = 0; i < index && i < (int)st.tabs.size(); ++i) {
    x += TabWidthOf(st, st.tabs[i]) + st.S(2);
  }
  float w = TabWidthOf(st, st.tabs[index]);
  return Rect{ x, tabs.t, x + w, tabs.b };
}

Rect TabCloseRect(const AppState& st, const Rect& tabs, int index) {
  Rect tab = TabRectOf(st, tabs, index);
  float left = tab.r - st.S(10) - st.S(17);
  return Rect{ left, MidY(tab) - st.S(8.5f), left + st.S(17), MidY(tab) + st.S(8.5f) };
}

Rect HostEditRect(const AppState& st, const Rect& sidebar, int index) {
  Rect row = SidebarHostRect(st, sidebar, index);
  float w = st.S(38);
  float h = st.S(18);
  float right = row.r - st.S(9);
  return Rect{ right - w, MidY(row) - h * kHalf, right, MidY(row) + h * kHalf };
}

Rect TabsRightButtonRect(const AppState& st, const Rect& tabs, int index) {
  float size = st.S(28);
  float right = tabs.r - st.S(6) - size * (float)(1 - index);
  float top = tabs.t + (tabs.H() - size) * kHalf;
  return Rect{ right - size, top, right, top + size };
}

float DashCardsTop(const AppState& st, const Rect& c) {
  return c.t + st.S(26) + st.S(76) + st.S(96) + st.S(22);
}

int DashCols(const AppState& st, const Rect& c) {
  float avail = c.W() - st.S(56);
  int n = (int)((avail + st.S(10)) / (st.S(300) + st.S(10)));
  if (n < 1) n = 1;
  if (n > 4) n = 4;
  return n;
}

Rect DashCardRect(const AppState& st, const Rect& c, int index) {
  int cols = DashCols(st, c);
  float gap = st.S(10);
  float avail = c.W() - st.S(56);
  float w = (avail - gap * (float)(cols - 1)) / (float)cols;
  float top = DashCardsTop(st, c) + (float)(index / cols) * (st.S(168) + gap);
  float left = c.l + st.S(28) + (float)(index % cols) * (w + gap);
  return Rect{ left, top, left + w, top + st.S(168) };
}

Rect DashNewHostRect(const AppState& st, const Rect& c) {
  float w = st.S(112);
  float h = st.S(34);
  float right = c.r - st.S(28);
  float top = c.t + st.S(34);
  return Rect{ right - w, top, right, top + h };
}

Rect FtpToolRect(const AppState& st, const Rect& c, int slot) {
  float w = st.S(74);
  float h = st.S(28);
  float right = c.r - st.S(14) - (float)slot * (w + st.S(8));
  float top = c.t + st.S(21) - h * kHalf;
  return Rect{ right - w, top, right, top + h };
}

Rect FtpQueueRect(const AppState& st, const Rect& c) {
  float w = st.S(58);
  float h = st.S(28);
  float cy = c.b - st.S(24);
  return Rect{ c.r - st.S(14) - w, cy - h * kHalf, c.r - st.S(14), cy + h * kHalf };
}

Rect FtpActionRect(const AppState& st, const Rect& c, int slot) {
  Rect q = FtpQueueRect(st, c);
  float w = st.S(30);
  float h = st.S(28);
  float right = q.l - st.S(10) - (float)slot * (w + st.S(6));
  return Rect{ right - w, q.t, right, q.b };
}

Rect TunNewRect(const AppState& st, const Rect& c) {
  float w = st.S(98);
  float h = st.S(32);
  float right = c.r - st.S(24);
  float top = c.t + st.S(20);
  return Rect{ right - w, top, right, top + h };
}

float TunRowsTop(const AppState& st, const Rect& c) {
  return c.t + st.S(20) + st.S(52) + st.S(16) + st.S(36);
}

Rect TunRowRect(const AppState& st, const Rect& c, int index) {
  float top = TunRowsTop(st, c) + st.S(52) * (float)index;
  return Rect{ c.l + st.S(24), top, c.r - st.S(24), top + st.S(52) };
}

void DrawSail(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float cx = rc.l + rc.W() * kHalf;
  float top = rc.t + rc.H() * 0.08f;
  float bottom = rc.b - rc.H() * 0.04f;
  r.Line(cx, top, cx, bottom, c, st.S(1.4f));
  r.Line(cx, top, rc.l + rc.W() * 0.14f, bottom - rc.H() * 0.14f, c, st.S(1.4f));
}

void DrawSearchIcon(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float rad = rc.W() * 0.32f;
  float cx = rc.l + rc.W() * 0.40f;
  float cy = rc.t + rc.H() * 0.40f;
  Rect bb{ cx - rad, cy - rad, cx + rad, cy + rad };
  r.StrokeRound(bb.D2D(), rad, c, st.S(1.2f));
  r.Line(cx + rad * 0.72f, cy + rad * 0.72f, rc.r - st.S(1), rc.b - st.S(1), c, st.S(1.2f));
}

void DrawTabIcon(Render& r, const AppState& st, int view, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  if (view == (int)View::Term) {
    r.StrokeRound(rc.D2D(), st.S(2.5f), c, w);
    float cx = rc.l + rc.W() * 0.28f;
    float cy = rc.t + rc.H() * 0.34f;
    r.Line(cx, cy, cx + rc.W() * 0.16f, cy + rc.H() * 0.16f, c, w);
    r.Line(cx + rc.W() * 0.16f, cy + rc.H() * 0.16f, cx, cy + rc.H() * 0.32f, c, w);
    r.Line(rc.l + rc.W() * 0.52f, rc.b - rc.H() * 0.26f, rc.r - rc.W() * 0.18f,
           rc.b - rc.H() * 0.26f, c, w);
  } else if (view == (int)View::Sftp) {
    Rect body{ rc.l, rc.t + rc.H() * 0.24f, rc.r, rc.b - rc.H() * 0.04f };
    r.StrokeRound(body.D2D(), st.S(2), c, w);
    r.Line(rc.l + rc.W() * 0.10f, body.t, rc.l + rc.W() * 0.30f, rc.t, c, w);
    r.Line(rc.l + rc.W() * 0.30f, rc.t, rc.l + rc.W() * 0.54f, rc.t, c, w);
    r.Line(rc.l + rc.W() * 0.54f, rc.t, rc.l + rc.W() * 0.68f, body.t, c, w);
  } else if (view == (int)View::Tun) {
    float y = rc.t + rc.H() * 0.26f;
    float foldX = rc.l + rc.W() * 0.46f;
    r.Line(rc.l, y, foldX, y, c, w);
    r.Line(foldX, y, foldX, rc.b - rc.H() * 0.22f, c, w);
    r.Line(foldX, rc.b - rc.H() * 0.22f, rc.r - rc.W() * 0.18f, rc.b - rc.H() * 0.22f, c, w);
    r.Line(rc.r - rc.W() * 0.18f, rc.b - rc.H() * 0.22f, rc.r - rc.W() * 0.44f,
           rc.b - rc.H() * 0.46f, c, w);
    r.Line(rc.r - rc.W() * 0.18f, rc.b - rc.H() * 0.22f, rc.r - rc.W() * 0.44f,
           rc.b - rc.H() * 0.02f, c, w);
  } else {
    float apex = rc.t + rc.H() * 0.40f;
    r.Line(rc.l, apex, rc.l + rc.W() * kHalf, rc.t, c, w);
    r.Line(rc.l + rc.W() * kHalf, rc.t, rc.r, apex, c, w);
    r.Line(rc.l + rc.W() * 0.16f, apex, rc.l + rc.W() * 0.16f, rc.b, c, w);
    r.Line(rc.r - rc.W() * 0.16f, apex, rc.r - rc.W() * 0.16f, rc.b, c, w);
    r.Line(rc.l + rc.W() * 0.16f, rc.b, rc.r - rc.W() * 0.16f, rc.b, c, w);
  }
}

void DrawRefreshIcon(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float rad = rc.W() * 0.30f;
  float cx = rc.l + rc.W() * kHalf;
  float cy = MidY(rc);
  Rect bb{ cx - rad, cy - rad, cx + rad, cy + rad };
  r.StrokeRound(bb.D2D(), rad, c, st.S(1.2f));
  r.Line(cx + rad * 0.15f, cy - rad, cx + rad * 1.05f, cy - rad * 0.55f, c, st.S(1.2f));
  r.Line(cx + rad * 1.05f, cy - rad * 0.55f, cx + rad * 0.25f, cy - rad * 0.70f, c, st.S(1.2f));
}

void DrawChevronPair(Render& r, const AppState& st, const Rect& rc, bool left, D2D1_COLOR_F c) {
  float cx = rc.l + rc.W() * kHalf;
  float cy = MidY(rc);
  float h = st.S(3.8f);
  float w = st.S(2.4f);
  float gap = st.S(3.6f);
  float dir = left ? -1.0f : 1.0f;
  for (int i = 0; i < 2; ++i) {
    float x = cx + dir * (i == 0 ? -gap * kHalf : gap * kHalf);
    r.Line(x - dir * w * kHalf, cy - h, x + dir * w * kHalf, cy, c, st.S(1.3f));
    r.Line(x + dir * w * kHalf, cy, x - dir * w * kHalf, cy + h, c, st.S(1.3f));
  }
}

void DrawSettingsIcon(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float lw = st.S(1.2f);
  float knob = st.S(2.4f);
  float y1 = rc.t + rc.H() * 0.30f;
  float y2 = rc.t + rc.H() * 0.70f;
  r.Line(rc.l, y1, rc.r, y1, c, lw);
  r.Line(rc.l, y2, rc.r, y2, c, lw);
  r.Circle(rc.l + rc.W() * 0.64f, y1, knob, c);
  r.Circle(rc.l + rc.W() * 0.34f, y2, knob, c);
}

void DrawChip(Render& r, const AppState& st, const std::wstring& text, const Rect& rc,
              D2D1_COLOR_F fg, D2D1_COLOR_F bg) {
  r.FillRound(rc.D2D(), st.S(L::RChip), bg);
  r.TextVertCenter(text.c_str(), rc.D2D(), fg, FontId::Micro, Align::Center);
}

Rect ChipRectOf(Render& r, const AppState& st, const std::wstring& text, const Rect& row) {
  float tw = r.Measure(text.c_str(), FontId::Micro);
  float h = st.S(18);
  float w = tw + st.S(12);
  float right = row.r - st.S(9);
  return Rect{ right - w, MidY(row) - h * kHalf, right, MidY(row) + h * kHalf };
}

void DrawTitleBar(Render& r, const AppState& st, const Rect& client, const Palette& p) {
  Rect bar = TitleBarRect(st, client);
  r.Fill(bar.D2D(), p.surface);
  r.Fill(Rect{ bar.l, bar.b - st.S(1), bar.r, bar.b }.D2D(), p.line);

  float mark = st.S(9);
  float cy = MidY(bar);
  Rect mk{ bar.l + st.S(8), cy - mark * kHalf, bar.l + st.S(8) + mark, cy + mark * kHalf };
  r.FillRound(mk.D2D(), st.S(3), p.accent);

  float tx = mk.r + st.S(6);
  float limit = bar.r - st.S(44) * 3 - st.S(10);
  if (limit > tx) {
    r.PushClip(D2D1::RectF(tx, bar.t, limit, bar.b));
    const wchar_t* brand = Tr(L"轻舟 Skiff");
    float bw = r.Measure(brand, FontId::Small);
    r.TextVertCenter(brand, D2D1::RectF(tx, bar.t, tx + bw + st.S(1), bar.b), p.text,
                     FontId::Small);
    std::wstring tail = std::wstring(L" — ") + ViewTitle(st.view);
    r.TextVertCenter(tail.c_str(), D2D1::RectF(tx + bw, bar.t, limit, bar.b), p.text2,
                     FontId::Small);
    r.PopClip();
  }

  for (int i = 0; i < 3; ++i) {
    Rect b = TitleButtonRect(st, client, i);
    float cx = b.l + b.W() * kHalf;
    float by = MidY(b);
    float s = st.S(11);
    float lw = st.S(1);
    if (i == 0) {
      r.Line(cx - s * kHalf, by, cx + s * kHalf, by, p.text3, lw);
    } else if (i == 1) {
      Rect box{ cx - s * kHalf, by - s * kHalf, cx + s * kHalf, by + s * kHalf };
      r.StrokeRound(box.D2D(), st.S(1), p.text3, lw);
    } else {
      r.Line(cx - s * kHalf, by - s * kHalf, cx + s * kHalf, by + s * kHalf, p.text3, lw);
      r.Line(cx + s * kHalf, by - s * kHalf, cx - s * kHalf, by + s * kHalf, p.text3, lw);
    }
  }
}

void DrawSidebar(Render& r, const AppState& st, const Rect& s, const Palette& p) {
  r.Fill(s.D2D(), p.surface);
  r.Fill(Rect{ s.r - st.S(1), s.t, s.r, s.b }.D2D(), p.line);

  float logo = st.S(30);
  float logoX = st.sideCollapsed ? s.l + (s.W() - logo) * kHalf : s.l + st.S(14.0f);
  Rect lg{ logoX, s.t + st.S(16), logoX + logo, s.t + st.S(16) + logo };
  r.FillRound(lg.D2D(), st.S(9), p.accentSoft);
  DrawSail(r, st, lg.Inset(st.S(9)), p.accent);

  if (st.sideCollapsed) {
    Rect ex = SideExpandRect(st, s);
    r.FillRound(ex.D2D(), st.S(8.0f),
                st.hoverId == HitSideExpand ? p.lineStrong : p.surface2);
    r.StrokeRound(ex.D2D(), st.S(8.0f), p.line, st.S(1.0f));
    DrawChevronPair(r, st, ex, false, p.accent);

    Rect stg = SideCollapsedSettingsRect(st, s);
    r.FillRound(stg.D2D(), st.S(8.0f),
                st.hoverId == HitSideSettings ? p.lineStrong : p.surface2);
    DrawSettingsIcon(r, st, stg.Inset(st.S(11.0f)), p.text2);
    return;
  }

  float tx = lg.r + st.S(9);
  Rect bf = SideBrandFoldRect(st, s);
  float textRight = bf.l - st.S(8);
  r.TextVertCenter(Tr(L"轻舟"), D2D1::RectF(tx, lg.t, textRight, lg.t + lg.H() * 0.60f), p.text,
                   FontId::Body);
  r.TextVertCenter(L"SKIFF · PORTABLE",
                   D2D1::RectF(tx, lg.t + lg.H() * 0.52f, textRight, lg.b), p.text3,
                   FontId::Micro);

  r.FillRound(bf.D2D(), st.S(6.0f),
              st.hoverId == HitSideCollapse ? p.lineStrong : p.surface2);
  DrawChevronPair(r, st, bf, true, p.text2);

  int liveN = 0;
  int tunUp = 0;
  int tunTotal = 0;
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    const Host& h = st.hosts[i];
    if (h.state == HostState::Live) ++liveN;
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      if (!h.tunnels[k].on) continue;
      ++tunTotal;
      if (h.tunnels[k].state == TunState::Up) ++tunUp;
    }
  }
  Rect sum = SideSummaryRect(st, s);
  r.FillRound(sum.D2D(), st.S(L::RCtrl), p.surface2);
  std::wstring sumText = std::wstring(Tr(L"在线设备 ")) + Num(liveN) + L"/" +
                         Num((int)st.hosts.size()) + Tr(L"  ·  连通隧道 ") + Num(tunUp) + L"/" +
                         Num(tunTotal);
  r.TextVertCenter(sumText.c_str(),
                   D2D1::RectF(sum.l + st.S(10.0f), sum.t, sum.r - st.S(10.0f), sum.b),
                   p.text2, FontId::Micro);

  Rect sq = SideSearchRect(st, s);
  r.FillRound(sq.D2D(), st.S(L::RCtrl), p.surface2);
  if (st.searchFocus) r.StrokeRound(sq.D2D(), st.S(L::RCtrl), p.accent, st.S(1.2f));
  float scy = sq.t + sq.H() * kHalf;
  float icx = sq.l + st.S(15.0f);
  r.Circle(icx, scy, st.S(4.0f), Alpha(p.text3, 0.55f));
  r.Line(icx + st.S(3.0f), scy + st.S(3.0f), icx + st.S(7.0f), scy + st.S(7.0f), p.text3,
         st.S(1.5f));
  if (st.selectAll && st.searchFocus && !st.query.empty()) {
    float vw = r.Measure(st.query.c_str(), FontId::Small);
    r.Fill(D2D1::RectF(icx + st.S(10.0f), sq.t + st.S(4.0f),
                       icx + st.S(14.0f) + vw, sq.b - st.S(4.0f)),
           Alpha(p.accent, 0.25f));
  }
  std::wstring shint = st.query.empty() ? std::wstring(Tr(L"搜索主机…")) : st.query;
  r.PushClip(D2D1::RectF(icx + st.S(12.0f), sq.t, sq.r - st.S(8.0f), sq.b));
  r.TextVertCenter(shint.c_str(),
                   D2D1::RectF(icx + st.S(12.0f), sq.t, sq.r - st.S(8.0f), sq.b),
                   st.query.empty() ? p.text3 : p.text, FontId::Small);
  r.PopClip();

  Rect sk = SideSortKeyRect(st, s);
  Rect sd = SideSortDirRect(st, s);
  r.FillRound(sk.D2D(), st.S(L::RCtrl), p.surface2);
  std::wstring sortText = std::wstring(Tr(L"排序：")) +
                          (st.sortKey == 0 ? Tr(L"到期时间") : Tr(L"注册时间"));
  r.TextVertCenter(sortText.c_str(),
                   D2D1::RectF(sk.l + st.S(10.0f), sk.t, sk.r - st.S(6.0f), sk.b), p.text2,
                   FontId::Micro);
  r.FillRound(sd.D2D(), st.S(L::RCtrl), p.surface2);
  r.TextVertCenter(st.sortAsc ? L"↑" : L"↓", sd.D2D(), p.accent, FontId::Body, Align::Center);

  int vis[kSideCardCap];
  int visN = SidebarVisibleHosts(st, vis, kSideCardCap);
  float listTop = SideListTop(st, s);
  float listBottom = SideListBottom(st, s);
  r.PushClip(D2D1::RectF(s.l, listTop, s.r, listBottom));
  for (int i = 0; i < visN; ++i) {
    Rect card = SideCardRect(st, s, i);
    if (card.b <= listTop || card.t >= listBottom) continue;
    DrawSideHostCard(r, st, s, i, vis[i], p);
  }
  if (visN == 0) {
    const wchar_t* tip = st.hosts.empty() ? Tr(L"还没有主机，点首页右上角新建")
                                          : Tr(L"没有匹配的主机");
    r.TextVertCenter(tip,
                     D2D1::RectF(s.l + st.S(16.0f), listTop + st.S(24.0f),
                                 s.r - st.S(16.0f), listTop + st.S(58.0f)),
                     p.text3, FontId::Micro, Align::Center);
  }
  r.PopClip();

  float totalH = (st.S(148.0f) + st.S(8.0f)) * (float)visN - st.S(8.0f);
  float viewH = listBottom - listTop;
  if (totalH > viewH && viewH > 0) {
    float maxScroll = totalH - viewH;
    float sc = st.sideScroll;
    if (sc < 0) sc = 0;
    if (sc > maxScroll) sc = maxScroll;
    float th = viewH * viewH / totalH;
    if (th < st.S(24.0f)) th = st.S(24.0f);
    float ty = listTop + (viewH - th) * sc / maxScroll;
    r.FillRound(D2D1::RectF(s.r - st.S(6.0f), ty, s.r - st.S(2.0f), ty + th), st.S(2.0f),
                p.lineStrong);
  }

  float foot = SidebarFootTop(st, s);
  r.Fill(Rect{ s.l, foot, s.r, foot + st.S(1) }.D2D(), p.line);

  Rect setRc = SideSettingsRect(st, s);
  r.FillRound(setRc.D2D(), st.S(L::RCtrl),
              st.hoverId == HitSideSettings ? p.lineStrong : p.surface2);
  float setTw = r.Measure(Tr(L"设置"), FontId::Body);
  float setIco = st.S(14.0f);
  float setGap = st.S(7.0f);
  float setX = setRc.l + (setRc.W() - (setIco + setGap + setTw)) * kHalf;
  Rect setIc{ setX, MidY(setRc) - setIco * kHalf, setX + setIco, MidY(setRc) + setIco * kHalf };
  DrawSettingsIcon(r, st, setIc, p.text2);
  r.TextVertCenter(Tr(L"设置"),
                   D2D1::RectF(setX + setIco + setGap, setRc.t, setRc.r, setRc.b), p.text2,
                   FontId::Body);

  Rect vrow = SidebarFootItemRect(st, s, 1);
  Rect fold = SideCollapseRect(st, s);
  std::wstring ver = std::wstring(L"v") + SKIFF_WIN_VERSION;
  r.TextVertCenter(ver.c_str(),
                   D2D1::RectF(vrow.l + st.S(10), vrow.t, fold.l - st.S(6), vrow.b), p.text3,
                   FontId::Micro);

  r.FillRound(fold.D2D(), st.S(6.0f),
              st.hoverId == HitSideCollapse ? p.lineStrong : p.surface2);
  float foldTw = r.Measure(Tr(L"收起"), FontId::Micro);
  float foldIco = st.S(12.0f);
  float foldGap = st.S(6.0f);
  float foldX = fold.l + (fold.W() - (foldIco + foldGap + foldTw)) * kHalf;
  Rect foldIc{ foldX, MidY(fold) - foldIco * kHalf, foldX + foldIco, MidY(fold) + foldIco * kHalf };
  DrawChevronPair(r, st, foldIc, true, p.text2);
  r.TextVertCenter(Tr(L"收起"),
                   D2D1::RectF(foldX + foldIco + foldGap, fold.t, fold.r, fold.b), p.text2,
                   FontId::Micro);
}

void DrawTabs(Render& r, const AppState& st, const Rect& tabs, const Palette& p) {
  r.Fill(tabs.D2D(), p.surface);

  for (int i = 0; i < (int)st.tabs.size(); ++i) {
    Rect tab = TabRectOf(st, tabs, i);
    if (tab.l >= tabs.r - st.S(70)) break;
    bool on = i == st.activeTab;
    if (on) {
      r.Fill(tab.D2D(), p.bg);
      Rect barRc{ tab.l + st.S(10), tab.t, tab.r - st.S(10), tab.t + st.S(2) };
      if (barRc.r > barRc.l) r.FillRound(barRc.D2D(), st.S(1), p.accent);
    }
    D2D1_COLOR_F fg = on ? p.text : p.text2;
    float iconSize = st.S(13);
    Rect ic{ tab.l + st.S(12), MidY(tab) - iconSize * kHalf, tab.l + st.S(12) + iconSize,
             MidY(tab) + iconSize * kHalf };
    DrawTabIcon(r, st, st.tabs[i].view, ic, fg);

    float titleLeft = ic.r + st.S(8);
    Rect xr = TabCloseRect(st, tabs, i);
    float closeLeft = xr.l;
    if (closeLeft - st.S(6) > titleLeft) {
      r.PushClip(D2D1::RectF(titleLeft, tab.t, closeLeft - st.S(4), tab.b));
      r.TextVertCenter(st.tabs[i].title.c_str(),
                       D2D1::RectF(titleLeft, tab.t, closeLeft, tab.b), fg, FontId::Small);
      r.PopClip();
    }

    float xs = st.S(3.4f);
    float xc = xr.l + xr.W() * kHalf;
    float yc = MidY(xr);
    r.Line(xc - xs, yc - xs, xc + xs, yc + xs, p.text3, st.S(1.1f));
    r.Line(xc + xs, yc - xs, xc - xs, yc + xs, p.text3, st.S(1.1f));
  }

  Rect refresh = TabsRightButtonRect(st, tabs, 0);
  bool spinning = st.refreshAt > 0 && (long long)GetTickCount64() - st.refreshAt < 1200;
  bool rHot = st.hoverId == HitTabsRightBase;
  if (spinning || rHot) {
    Rect bgc = refresh.Inset(st.S(1.0f));
    r.FillRound(bgc.D2D(), st.S(7.0f), Alpha(p.accent, spinning ? 0.18f : 0.12f));
    r.StrokeRound(bgc.D2D(), st.S(7.0f), Alpha(p.accent, 0.55f), st.S(1.0f));
  }
  Rect ri = refresh.Inset(st.S(7));
  DrawRefreshIcon(r, st, ri, (spinning || rHot) ? p.accent : p.text3);
  if (spinning) {
    float cx = ri.l + ri.W() * kHalf;
    float cy = MidY(ri);
    float rad = ri.W() * 0.48f;
    double ang = (double)((GetTickCount64() - (ULONGLONG)st.refreshAt) % 1200ULL) / 1200.0 * 6.283185307179586;
    for (int k = 0; k < 3; ++k) {
      double a = ang + (double)k * 2.0943951023931953;
      float x0 = cx + (float)std::cos(a) * rad * 0.55f;
      float y0 = cy + (float)std::sin(a) * rad * 0.55f;
      float x1 = cx + (float)std::cos(a) * rad;
      float y1 = cy + (float)std::sin(a) * rad;
      r.Line(x0, y0, x1, y1, p.accent, st.S(1.3f));
    }
  }
  r.Fill(Rect{ tabs.l, tabs.b - st.S(1), tabs.r, tabs.b }.D2D(), p.line);
}

void DrawStatus(Render& r, const AppState& st, const Rect& status, const Palette& p) {
  r.Fill(status.D2D(), p.surface);
  r.Fill(Rect{ status.l, status.t, status.r, status.t + st.S(1) }.D2D(), p.line);

  int sid = st.CurrentSessionId();
  int hid = st.CurrentHostId();
  SshState ss = sid >= 0 ? SshService::Instance().State(sid) : SshState::Disconnected;

  std::wstring hostName;
  if (hid >= 0 && hid < (int)st.hosts.size()) hostName = st.hosts[(size_t)hid].name;

  D2D1_COLOR_F dotColor = p.text3;
  D2D1_COLOR_F textColor = p.text3;
  std::wstring conn;
  if (ss == SshState::Connected) {
    dotColor = p.accent;
    textColor = p.accent;
    conn = Tr(L"已连接");
  } else if (ss == SshState::Connecting) {
    dotColor = p.warn;
    textColor = p.warn;
    conn = Tr(L"连接中");
  } else if (ss == SshState::Failed) {
    dotColor = p.danger;
    textColor = p.danger;
    conn = Tr(L"连接失败");
  } else {
    conn = Tr(L"未连接");
  }
  if (!hostName.empty()) conn += std::wstring(L" · ") + hostName;

  float cy = MidY(status);
  float x = status.l + st.S(12);
  float dot = st.S(6);
  r.Circle(x + dot * kHalf, cy, dot * kHalf, dotColor);
  x += dot + st.S(6);

  float connW = r.Measure(conn.c_str(), FontId::MonoSm);
  r.TextVertCenter(conn.c_str(), D2D1::RectF(x, status.t, x + connW + st.S(2), status.b), textColor,
                   FontId::MonoSm);
  x += connW;

  int latency = (hid >= 0 && hid < (int)st.hosts.size()) ? st.hosts[(size_t)hid].latency : -1;
  std::wstring left[3];
  left[0] = latency >= 0 ? Num(latency) + L" ms" : std::wstring(L"—");
  std::wstring cipher = ss == SshState::Connected ? U8ToW(SshService::Instance().CipherName(sid))
                                                  : std::wstring();
  std::wstring mac = ss == SshState::Connected ? U8ToW(SshService::Instance().MacName(sid))
                                               : std::wstring();
  left[1] = cipher.empty() ? std::wstring(L"—") : cipher;
  left[2] = mac.empty() ? std::wstring(L"—") : mac;
  for (int i = 0; i < 3; ++i) {
    x += st.S(14);
    r.Fill(Rect{ x, status.t + st.S(6), x + st.S(1), status.b - st.S(6) }.D2D(), p.line);
    x += st.S(14);
    float w = r.Measure(left[i].c_str(), FontId::MonoSm);
    r.TextVertCenter(left[i].c_str(), D2D1::RectF(x, status.t, x + w + st.S(2), status.b), p.text3,
                     FontId::MonoSm);
    x += w;
  }

  Terminal* term = sid >= 0 ? SessionHub::Instance().Find(sid) : nullptr;
  std::wstring dim = term ? Num(term->Cols()) + L"×" + Num(term->Rows()) : std::wstring(L"—");
  std::wstring sha = L"—";
  if (ss == SshState::Connected) {
    std::string fp = SshService::Instance().HostKey(sid);
    if (!fp.empty()) {
      std::wstring w(fp.begin(), fp.end());
      if (w.size() > 18) w = w.substr(0, 14) + L"…" + w.substr(w.size() - 4);
      sha = w;
    }
  }
  std::wstring right[4];
  right[0] = std::wstring(L"v") + SKIFF_WIN_VERSION;
  right[1] = L"UTF-8";
  right[2] = dim;
  right[3] = sha;
  float rx = status.r - st.S(12);
  for (int i = 0; i < 4; ++i) {
    float w = r.Measure(right[i].c_str(), FontId::MonoSm);
    r.TextVertCenter(right[i].c_str(), D2D1::RectF(status.l, status.t, rx, status.b), p.text3,
                     FontId::MonoSm, Align::Right);
    rx -= w;
    if (i < 3) {
      rx -= st.S(14);
      r.Fill(Rect{ rx, status.t + st.S(6), rx + st.S(1), status.b - st.S(6) }.D2D(), p.line);
      rx -= st.S(14);
    }
  }
}

void TunCols(const AppState& st, const Rect& c, float* xs) {
  float left = c.l + st.S(24.0f);
  float right = c.r - st.S(24.0f);
  float inner = (right - left) - st.S(28.0f);
  float fixedW = st.S(108.0f) + st.S(72.0f) + st.S(84.0f) + st.S(62.0f) + st.S(110.0f);
  float unit = (inner - fixedW) / 2.6f;
  if (unit < st.S(36.0f)) unit = st.S(36.0f);
  float cw[7] = { st.S(108.0f), st.S(72.0f), unit * 1.1f, unit * 1.5f,
                  st.S(84.0f), st.S(62.0f), st.S(110.0f) };
  float totalW = 0.0f;
  for (int i = 0; i < 7; ++i) totalW += cw[i];
  if (totalW > inner) {
    float fit = inner / totalW;
    for (int i = 0; i < 7; ++i) cw[i] *= fit;
  }
  xs[0] = left + st.S(14.0f);
  for (int i = 0; i < 7; ++i) xs[i + 1] = xs[i] + cw[i];
}

Rect TunSwitchRect(const AppState& st, const Rect& c, int index) {
  float xs[8];
  TunCols(st, c, xs);
  Rect row = TunRowRect(st, c, index);
  float w = st.S(34.0f);
  float h = st.S(20.0f);
  float right = xs[6] - st.S(10.0f);
  return Rect{ right - w, MidY(row) - h * kHalf, right, MidY(row) + h * kHalf };
}

Rect TunOpRect(const AppState& st, const Rect& c, int index, int slot) {
  float xs[8];
  TunCols(st, c, xs);
  Rect row = TunRowRect(st, c, index);
  float ob = st.S(24.0f);
  float step = ob + st.S(2.0f);
  float cy = MidY(row);
  Rect cell{ xs[6] + step * (float)slot, cy - ob * kHalf, xs[6] + step * (float)slot + ob,
             cy + ob * kHalf };
  return cell;
}

bool IsFormModal(const AppState& st) {
  return st.modal == ModalKind::HostForm || st.modal == ModalKind::TunForm;
}

bool FieldVisible(const AppState& st, int index) {
  if (index < 0 || index >= (int)st.form.size()) return false;
  const FormField& f = st.form[(size_t)index];
  if (f.depKey.empty() || f.depOpt < 0) return true;
  const FormField* dep = FieldOf(st.form, f.depKey);
  if (dep == nullptr) return true;
  return dep->opt == f.depOpt;
}

int ModalRowCount(const AppState& st) {
  if (IsFormModal(st)) return (int)st.form.size();
  if (st.modal == ModalKind::StatusPanel) return 7;
  if (st.modal == ModalKind::Queue) return st.sftpQueue.empty() ? 1 : (int)st.sftpQueue.size();
  if (st.modal == ModalKind::KeyStore) return st.keyStore.empty() ? 1 : (int)st.keyStore.size();
  if (st.modal == ModalKind::Prompt) return 1;
  if (st.modal == ModalKind::Channel) return 2;
  return 0;
}

float ModalRowHAt(const AppState& st, int i) {
  if (IsFormModal(st)) {
    if (i >= 0 && i < (int)st.form.size() && st.form[i].kind == FieldKind::Group) return st.S(32.0f);
    return st.S(50.0f);
  }
  return st.S(40.0f);
}

float ModalRowH(const AppState& st) { return ModalRowHAt(st, 0); }

float ModalBodyH(const AppState& st) {
  int n = ModalRowCount(st);
  if (n <= 0) return 0.0f;
  if (!IsFormModal(st)) return ModalRowHAt(st, 0) * (float)n;
  float h = 0.0f;
  for (int i = 0; i < n; ++i) {
    if (!FieldVisible(st, i)) continue;
    h += ModalRowHAt(st, i);
  }
  return h;
}

Rect DpGridRect(const AppState& st, const Rect& panel) {
  float top = panel.t + st.S(64.0f) + st.S(38.0f) + st.S(22.0f);
  return Rect{ panel.l + st.S(22.0f), top, panel.r - st.S(22.0f), top + st.S(192.0f) };
}

Rect DpPanelRect(const AppState& st, const Rect& client) {
  float w = st.S(360.0f);
  float h = st.S(426.0f);
  if (h > client.H() - st.S(32.0f)) h = client.H() - st.S(32.0f);
  float cx = client.l + client.W() * kHalf;
  float cy = client.t + client.H() * kHalf;
  return Rect{ cx - w * kHalf, cy - h * kHalf, cx + w * kHalf, cy + h * kHalf };
}

Rect ModalPanel(const AppState& st, const Rect& client) {
  if (st.modal == ModalKind::DatePicker) return DpPanelRect(st, client);
  float w = st.S(520.0f);
  if (w > client.W() - st.S(48.0f)) w = client.W() - st.S(48.0f);
  float maxBody = client.H() - st.S(48.0f) - st.S(64.0f) - st.S(66.0f);
  float bodyH = ModalBodyH(st);
  if (maxBody < st.S(80.0f)) maxBody = st.S(80.0f);
  if (bodyH > maxBody) bodyH = maxBody;
  float h = st.S(64.0f) + bodyH + st.S(66.0f);
  float cx = client.l + client.W() * kHalf;
  float cy = client.t + client.H() * kHalf;
  return Rect{ cx - w * kHalf, cy - h * kHalf, cx + w * kHalf, cy + h * kHalf };
}

Rect ModalFieldRect(const AppState& st, const Rect& panel, int i) {
  if (IsFormModal(st) && !FieldVisible(st, i)) return Rect{};
  float top = panel.t + st.S(64.0f) - st.formScroll;
  for (int k = 0; k < i; ++k) {
    if (!FieldVisible(st, k)) continue;
    top += ModalRowHAt(st, k);
  }
  return Rect{ panel.l + st.S(22.0f), top, panel.r - st.S(22.0f), top + ModalRowHAt(st, i) };
}

Rect ModalBodyRect(const AppState& st, const Rect& panel) {
  return Rect{ panel.l + st.S(12.0f), panel.t + st.S(64.0f), panel.r - st.S(12.0f),
               panel.b - st.S(62.0f) };
}

Rect ModalOkRect(const AppState& st, const Rect& panel) {
  float w = st.S(88.0f);
  float h = st.S(32.0f);
  return Rect{ panel.r - st.S(22.0f) - w, panel.b - st.S(16.0f) - h, panel.r - st.S(22.0f),
               panel.b - st.S(16.0f) };
}

Rect ModalCancelRect(const AppState& st, const Rect& panel) {
  Rect ok = ModalOkRect(st, panel);
  float w = st.S(76.0f);
  return Rect{ ok.l - st.S(10.0f) - w, ok.t, ok.l - st.S(10.0f), ok.b };
}

Rect ModalCloseRect(const AppState& st, const Rect& panel) {
  float s = st.S(22.0f);
  return Rect{ panel.r - st.S(18.0f) - s, panel.t + st.S(18.0f), panel.r - st.S(18.0f),
               panel.t + st.S(18.0f) + s };
}

Rect DpDayRect(const AppState& st, const Rect& panel, int index) {
  Rect g = DpGridRect(st, panel);
  float w = g.W() / 7.0f;
  float h = g.H() / 6.0f;
  int row = index / 7;
  int col = index % 7;
  return Rect{ g.l + w * (float)col, g.t + h * (float)row, g.l + w * (float)(col + 1),
               g.t + h * (float)(row + 1) };
}

Rect DpCellRect(const AppState& st, const Rect& panel, int index) {
  Rect g = DpGridRect(st, panel);
  int cols = st.dpDrop == 1 ? 6 : 10;
  int rows = st.dpDrop == 1 ? 4 : 6;
  float w = g.W() / (float)cols;
  float h = g.H() / (float)rows;
  int row = index / cols;
  int col = index % cols;
  return Rect{ g.l + w * (float)col, g.t + h * (float)row, g.l + w * (float)(col + 1),
               g.t + h * (float)(row + 1) };
}

Rect DpArrowRect(const AppState& st, const Rect& panel, int slot) {
  float top = panel.t + st.S(68.0f);
  float s = st.S(30.0f);
  if (slot == 0) return Rect{ panel.l + st.S(22.0f), top, panel.l + st.S(22.0f) + s, top + s };
  return Rect{ panel.r - st.S(22.0f) - s, top, panel.r - st.S(22.0f), top + s };
}

Rect DpTimeBtnRect(const AppState& st, const Rect& panel, int slot) {
  float top = DpGridRect(st, panel).b + st.S(14.0f);
  float w = st.S(74.0f);
  float gap = st.S(8.0f);
  float left = panel.l + st.S(22.0f) + (w + gap) * (float)slot;
  return Rect{ left, top, left + w, top + st.S(34.0f) };
}

Rect DpAuxRect(const AppState& st, const Rect& panel, int slot) {
  float w = st.S(64.0f);
  float h = st.S(30.0f);
  float left = panel.l + st.S(22.0f) + (w + st.S(8.0f)) * (float)slot;
  float top = panel.b - st.S(16.0f) - h;
  return Rect{ left, top, left + w, top + h };
}

void DrawModalImpl(Render& r, AppState& st, const Rect& client) {
  if (st.modal == ModalKind::None) return;
  const Palette& p = PaletteFor(st.dark, st.accentIdx);
  r.Fill(client.D2D(), Alpha(Rgb(0x000000), st.dark ? 0.58f : 0.34f));

  Rect panel = ModalPanel(st, client);
  r.FillRound(panel.D2D(), st.S(L::RCard), p.surface);
  r.StrokeRound(panel.Inset(st.S(0.5f)).D2D(), st.S(L::RCard), p.lineStrong, st.S(1.0f));

  r.TextVertCenter(st.modalTitle.c_str(),
                   D2D1::RectF(panel.l + st.S(22.0f), panel.t + st.S(16.0f),
                               panel.r - st.S(52.0f), panel.t + st.S(40.0f)),
                   p.text, FontId::Title);
  if (!st.modalNote.empty()) {
    r.PushClip(D2D1::RectF(panel.l + st.S(22.0f), panel.t + st.S(40.0f),
                           panel.r - st.S(22.0f), panel.t + st.S(60.0f)));
    r.TextVertCenter(st.modalNote.c_str(),
                     D2D1::RectF(panel.l + st.S(22.0f), panel.t + st.S(40.0f),
                                 panel.r - st.S(22.0f), panel.t + st.S(60.0f)),
                     p.text3, FontId::Micro);
    r.PopClip();
  }

  Rect cl = ModalCloseRect(st, panel);
  float ccx = cl.l + cl.W() * kHalf;
  float ccy = cl.t + cl.H() * kHalf;
  float cd = st.S(5.0f);
  r.Line(ccx - cd, ccy - cd, ccx + cd, ccy + cd, p.text3, st.S(1.4f));
  r.Line(ccx + cd, ccy - cd, ccx - cd, ccy + cd, p.text3, st.S(1.4f));

  r.Fill(D2D1::RectF(panel.l + st.S(22.0f), panel.t + st.S(62.0f),
                     panel.r - st.S(22.0f), panel.t + st.S(63.0f)), p.line);

  bool isForm = st.modal == ModalKind::HostForm || st.modal == ModalKind::TunForm;
  int rowN = ModalRowCount(st);

  if (isForm) {
    Rect body = ModalBodyRect(st, panel);
    r.PushClip(D2D1::RectF(body.l, body.t, body.r, body.b));
    float labelW = st.S(84.0f);
    float valLeft = panel.l + st.S(22.0f) + labelW;
    float valRight = panel.r - st.S(22.0f);
    for (int i = 0; i < rowN && i < (int)st.form.size(); ++i) {
      const FormField& f = st.form[i];
      if (!FieldVisible(st, i)) continue;
      Rect row = ModalFieldRect(st, panel, i);
      if (row.b <= body.t || row.t >= body.b) continue;
      if (f.kind == FieldKind::Group) {
        r.TextVertCenter(f.label.c_str(), D2D1::RectF(row.l, row.t, row.r, row.b), p.text3,
                         FontId::Micro);
        r.Fill(D2D1::RectF(row.l, row.b - st.S(9.0f), row.r, row.b - st.S(8.0f)), p.line);
        continue;
      }
      r.TextVertCenter(f.label.c_str(), D2D1::RectF(row.l, row.t, valLeft - st.S(10.0f), row.b),
                       p.text2, FontId::Body);
      bool focus = i == st.formFocus;
      if (f.kind == FieldKind::Seg) {
        Rect seg{ valLeft, row.t + st.S(10.0f), valRight, row.b - st.S(10.0f) };
        if (seg.W() > st.S(190.0f)) seg.r = seg.l + st.S(190.0f);
        int n = (int)f.options.size();
        for (int k = 0; k < n; ++k) {
          Rect cell{ seg.l + seg.W() * (float)k / (float)n, seg.t,
                     seg.l + seg.W() * (float)(k + 1) / (float)n, seg.b };
          bool on = k == f.opt;
          r.FillRound(cell.Inset(st.S(1.0f)).D2D(), st.S(L::RBtn), on ? p.accent : p.surface2);
          r.TextVertCenter(f.options[(size_t)k].c_str(), cell.D2D(),
                           on ? p.accentFg : p.text2, FontId::Small, Align::Center);
        }
        continue;
      }
      if (f.kind == FieldKind::Info) {
        r.PushClip(D2D1::RectF(valLeft, row.t, valRight, row.b));
        r.TextVertCenter(f.value.c_str(), D2D1::RectF(valLeft, row.t, valRight, row.b), p.text3,
                         FontId::MonoSm);
        r.PopClip();
        continue;
      }
      bool browse = f.kind == FieldKind::File;
      bool datePick = f.kind == FieldKind::Date;
      float boxRight = (browse || datePick) ? valRight - st.S(64.0f) : valRight;
      Rect box{ valLeft, row.t + st.S(9.0f), boxRight, row.b - st.S(9.0f) };
      r.FillRound(box.D2D(), st.S(L::RCtrl), p.surface2);
      if (focus) r.StrokeRound(box.Inset(st.S(0.5f)).D2D(), st.S(L::RCtrl), p.accent, st.S(1.5f));
      std::wstring shown = f.value;
      if (f.kind == FieldKind::Pass && !shown.empty()) shown.assign(shown.size(), L'\x2022');
      if (focus && st.selectAll && !shown.empty()) {
        float vw = r.Measure(shown.c_str(), FontId::Body);
        r.Fill(D2D1::RectF(box.l + st.S(9.0f), box.t + st.S(3.0f),
                           box.l + st.S(13.0f) + vw, box.b - st.S(3.0f)),
               Alpha(p.accent, 0.25f));
      }
      if (shown.empty() && !focus) {
        r.PushClip(D2D1::RectF(box.l + st.S(10.0f), box.t, box.r - st.S(8.0f), box.b));
        r.TextVertCenter(f.hint.c_str(),
                         D2D1::RectF(box.l + st.S(10.0f), box.t, box.r - st.S(8.0f), box.b),
                         p.text3, FontId::Body);
        r.PopClip();
      } else {
        float vw = r.Measure(shown.c_str(), FontId::Body);
        r.PushClip(D2D1::RectF(box.l + st.S(10.0f), box.t, box.r - st.S(8.0f), box.b));
        r.TextVertCenter(shown.c_str(), D2D1::RectF(box.l + st.S(10.0f), box.t,
                                                   box.l + st.S(12.0f) + vw, box.b),
                         p.text, FontId::Body);
        r.PopClip();
        if (focus) {
          float cx = box.l + st.S(11.0f) + vw;
          float lim = box.r - st.S(8.0f);
          if (cx > lim) cx = lim;
          r.Fill(D2D1::RectF(cx, box.t + st.S(5.0f), cx + st.S(1.5f), box.b - st.S(5.0f)),
                 p.accent);
        }
      }
      if (browse || datePick) {
        Rect br{ boxRight + st.S(8.0f), box.t, valRight, box.b };
        r.StrokeRound(br.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
        r.TextVertCenter(browse ? Tr(L"浏览…") : Tr(L"选择…"), br.D2D(), p.text2, FontId::Small,
                         Align::Center);
      }
    }
    r.PopClip();
    if (!st.formError.empty()) {
      r.TextVertCenter(st.formError.c_str(),
                       D2D1::RectF(panel.l + st.S(22.0f), panel.b - st.S(54.0f),
                                   panel.r - st.S(120.0f), panel.b - st.S(28.0f)),
                       p.danger, FontId::Micro);
    }
  } else if (st.modal == ModalKind::DatePicker) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    float navTop = panel.t + st.S(64.0f) + st.S(4.0f);
    std::wstring head = std::to_wstring(st.dpYear) + Tr(L" 年 ") + std::to_wstring(st.dpMonth) + Tr(L" 月");
    r.TextVertCenter(head.c_str(),
                     D2D1::RectF(panel.l + st.S(60.0f), navTop, panel.r - st.S(60.0f),
                                 navTop + st.S(30.0f)),
                     p.text, FontId::Body, Align::Center);
    for (int k = 0; k < 2; ++k) {
      Rect ar = DpArrowRect(st, panel, k);
      float cx = ar.l + ar.W() * kHalf;
      float cy = MidY(ar);
      float aw = st.S(4.0f);
      float ah = st.S(6.0f);
      if (k == 0) {
        r.Line(cx + aw * kHalf, cy - ah, cx - aw * kHalf, cy, p.text2, st.S(1.4f));
        r.Line(cx - aw * kHalf, cy, cx + aw * kHalf, cy + ah, p.text2, st.S(1.4f));
      } else {
        r.Line(cx - aw * kHalf, cy - ah, cx + aw * kHalf, cy, p.text2, st.S(1.4f));
        r.Line(cx + aw * kHalf, cy, cx - aw * kHalf, cy + ah, p.text2, st.S(1.4f));
      }
    }
    const wchar_t* wd[7] = { Tr(L"日"), Tr(L"一"), Tr(L"二"), Tr(L"三"), Tr(L"四"), Tr(L"五"), Tr(L"六") };
    Rect g = DpGridRect(st, panel);
    float cw = g.W() / 7.0f;
    float weekTop = g.t - st.S(22.0f);
    for (int k = 0; k < 7; ++k) {
      Rect cell{ g.l + cw * (float)k, weekTop, g.l + cw * (float)(k + 1), weekTop + st.S(22.0f) };
      r.TextVertCenter(wd[k], cell.D2D(), p.text3, FontId::Micro, Align::Center);
    }
    if (st.dpDrop == 0) {
      int lead = FirstWeekdayOfMonth(st.dpYear, st.dpMonth);
      int dim = DaysInMonth(st.dpYear, st.dpMonth);
      for (int k = 0; k < 42; ++k) {
        Rect cell = DpDayRect(st, panel, k);
        int day = k - lead + 1;
        if (day < 1 || day > dim) continue;
        bool sel = day == st.dpDay;
        bool today = st.dpYear == now.wYear && st.dpMonth == now.wMonth && day == now.wDay;
        if (sel) {
          r.FillRound(cell.Inset(st.S(6.0f)).D2D(), st.S(L::RBtn), p.accent);
        } else if (today) {
          r.StrokeRound(cell.Inset(st.S(6.0f)).D2D(), st.S(L::RBtn), p.accent, st.S(1.2f));
        }
        std::wstring ds = std::to_wstring(day);
        r.TextVertCenter(ds.c_str(), cell.D2D(), sel ? p.accentFg : p.text, FontId::Small,
                         Align::Center);
      }
    } else {
      int n = st.dpDrop == 1 ? 24 : 60;
      int cur = st.dpDrop == 1 ? st.dpHour : st.dpMinute;
      if (st.dpDrop == 3) cur = st.dpSecond;
      r.FillRound(g.D2D(), st.S(L::RCtrl), p.surface2);
      for (int k = 0; k < n; ++k) {
        Rect cell = DpCellRect(st, panel, k);
        bool on = k == cur;
        if (on) r.FillRound(cell.Inset(st.S(2.0f)).D2D(), st.S(6.0f), p.accent);
        std::wstring vs = std::to_wstring(k);
        r.TextVertCenter(vs.c_str(), cell.D2D(), on ? p.accentFg : p.text2, FontId::Small,
                         Align::Center);
      }
    }
    const wchar_t* tl[3] = { Tr(L"时"), Tr(L"分"), Tr(L"秒") };
    int tv[3] = { st.dpHour, st.dpMinute, st.dpSecond };
    for (int k = 0; k < 3; ++k) {
      Rect b = DpTimeBtnRect(st, panel, k);
      bool open = st.dpDrop == k + 1;
      r.FillRound(b.D2D(), st.S(L::RBtn), open ? p.accentSoft : p.surface2);
      if (open) r.StrokeRound(b.Inset(st.S(0.5f)).D2D(), st.S(L::RBtn), p.accent, st.S(1.2f));
      wchar_t buf[32];
      swprintf_s(buf, L"%ls %02d", tl[k], tv[k]);
      r.TextVertCenter(buf, b.D2D(), open ? p.accent : p.text2, FontId::Small, Align::Center);
    }
    for (int k = 0; k < 2; ++k) {
      Rect b = DpAuxRect(st, panel, k);
      r.StrokeRound(b.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
      r.TextVertCenter(k == 0 ? Tr(L"今天") : Tr(L"清除"), b.D2D(), p.text2, FontId::Small,
                       Align::Center);
    }
    Rect okRc = ModalOkRect(st, panel);
    Rect cancelRc = ModalCancelRect(st, panel);
    r.FillRound(okRc.D2D(), st.S(L::RBtn), p.accent);
    r.TextVertCenter(Tr(L"确定"), okRc.D2D(), p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancelRc.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(Tr(L"取消"), cancelRc.D2D(), p.text2, FontId::Body, Align::Center);
  } else if (st.modal == ModalKind::Confirm) {
    Rect body = ModalBodyRect(st, panel);
    r.TextVertCenter(st.confirmNote.c_str(), body.D2D(), p.text2, FontId::Body, Align::Center);
  } else if (st.modal == ModalKind::KeyStore) {
    Rect body = ModalBodyRect(st, panel);
    r.PushClip(D2D1::RectF(body.l, body.t, body.r, body.b));
    if (st.keyStore.empty()) {
      r.TextVertCenter(Tr(L"keys 目录为空，可放置私钥文件后重开"), body.D2D(), p.text3, FontId::Body,
                       Align::Center);
    } else {
      for (int i = 0; i < rowN && i < (int)st.keyStore.size(); ++i) {
        Rect row = ModalFieldRect(st, panel, i);
        Rect box{ row.l, row.t + st.S(6.0f), row.r, row.b - st.S(6.0f) };
        r.FillRound(box.D2D(), st.S(L::RCtrl), p.surface2);
        r.TextVertCenter(st.keyStore[(size_t)i].c_str(),
                         D2D1::RectF(box.l + st.S(12.0f), box.t, box.r - st.S(12.0f), box.b),
                         p.text2, FontId::MonoSm);
      }
    }
    r.PopClip();
  } else if (st.modal == ModalKind::Prompt) {
    Rect body = ModalBodyRect(st, panel);
    Rect box{ body.l + st.S(10.0f), body.t + st.S(14.0f), body.r - st.S(10.0f),
              body.t + st.S(54.0f) };
    r.FillRound(box.D2D(), st.S(L::RCtrl), p.surface2);
    r.StrokeRound(box.Inset(st.S(0.5f)).D2D(), st.S(L::RCtrl), p.accent, st.S(1.5f));
    std::wstring shown = st.promptValue.empty() ? Tr(L"输入名称") : st.promptValue;
    r.TextVertCenter(shown.c_str(),
                     D2D1::RectF(box.l + st.S(10.0f), box.t, box.r - st.S(8.0f), box.b),
                     st.promptValue.empty() ? p.text3 : p.text, FontId::Body);
  } else if (st.modal == ModalKind::Channel) {
    Rect body = ModalBodyRect(st, panel);
    Rect box{ body.l + st.S(10.0f), body.t + st.S(14.0f), body.r - st.S(10.0f),
              body.t + st.S(58.0f) };
    r.FillRound(box.D2D(), st.S(L::RCtrl), p.surface2);
    r.TextVertCenter(st.chanAddr.c_str(), box.D2D(), p.accent, FontId::Mono, Align::Center);
    if (!st.chanNote.empty()) {
      Rect nrc{ body.l + st.S(10.0f), box.b + st.S(6.0f), body.r - st.S(10.0f),
                box.b + st.S(34.0f) };
      r.PushClip(nrc.D2D());
      r.TextVertCenter(st.chanNote.c_str(), nrc.D2D(), p.text3, FontId::Caption);
      r.PopClip();
    }
  } else if (st.modal == ModalKind::StatusPanel) {
    std::wstring keys[7] = { Tr(L"主机"), Tr(L"地址"), Tr(L"认证"), Tr(L"加密"), Tr(L"密钥交换"), Tr(L"延迟"), Tr(L"主机指纹") };
    std::wstring vals[7];
    const Host* h = nullptr;
    if (st.activeHost >= 0 && st.activeHost < (int)st.hosts.size()) {
      h = &st.hosts[static_cast<size_t>(st.activeHost)];
    }
    vals[0] = h ? h->name : L"—";
    vals[1] = h ? h->Addr() : std::wstring(L"—");
    vals[2] = h ? std::wstring(Tr(h->auth.c_str())) : std::wstring(L"—");
    vals[3] = L"aes256-ctr";
    vals[4] = L"curve25519-sha256";
    vals[5] = st.statusLatency >= 0 ? std::to_wstring(st.statusLatency) + L" ms" : L"—";
    vals[6] = st.statusKey.empty() ? std::wstring(Tr(L"未记录")) : st.statusKey;
    for (int i = 0; i < 7; ++i) {
      Rect row = ModalFieldRect(st, panel, i);
      r.TextVertCenter(keys[i].c_str(),
                       D2D1::RectF(row.l, row.t, row.l + st.S(86.0f), row.b), p.text3,
                       FontId::Micro);
      r.PushClip(D2D1::RectF(row.l + st.S(86.0f), row.t, row.r, row.b));
      r.TextVertCenter(vals[i].c_str(),
                       D2D1::RectF(row.l + st.S(86.0f), row.t, row.r, row.b), p.text,
                       i >= 5 ? FontId::MonoSm : FontId::Body);
      r.PopClip();
    }
  } else if (st.modal == ModalKind::Queue) {
    if (st.sftpQueue.empty()) {
      Rect row = ModalFieldRect(st, panel, 0);
      r.TextVertCenter(Tr(L"传输队列为空"), row.D2D(), p.text3, FontId::Body, Align::Center);
    } else {
      for (int i = 0; i < rowN; ++i) {
        Rect row = ModalFieldRect(st, panel, i);
        float cy = MidY(row);
        Rect ib{ row.l + st.S(2.0f), cy - st.S(5.0f), row.l + st.S(14.0f), cy + st.S(5.0f) };
        r.FillRound(ib.D2D(), st.S(3.0f), p.accentSoft);
        r.Circle(ib.l + st.S(6.0f), cy, st.S(2.0f), p.accent);
        r.PushClip(D2D1::RectF(ib.r + st.S(10.0f), row.t, row.r, row.b));
        r.TextVertCenter(st.sftpQueue[static_cast<size_t>(i)].c_str(),
                         D2D1::RectF(ib.r + st.S(10.0f), row.t, row.r, row.b), p.text2,
                         FontId::MonoSm);
        r.PopClip();
      }
    }
  }

  if (isForm) {
    Rect ok = ModalOkRect(st, panel);
    Rect cancel = ModalCancelRect(st, panel);
    r.FillRound(ok.D2D(), st.S(L::RBtn), p.accent);
    r.TextVertCenter(Tr(L"保存"), ok.D2D(), p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancel.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(Tr(L"取消"), cancel.D2D(), p.text2, FontId::Body, Align::Center);
    if (st.modal == ModalKind::HostForm) {
      Rect ks{ panel.l + st.S(22.0f), ok.t, panel.l + st.S(22.0f) + st.S(84.0f), ok.b };
      r.StrokeRound(ks.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
      r.TextVertCenter(Tr(L"密钥库"), ks.D2D(), p.text2, FontId::Body, Align::Center);
    }
  } else if (st.modal == ModalKind::Confirm) {
    Rect ok = ModalOkRect(st, panel);
    Rect cancel = ModalCancelRect(st, panel);
    bool danger = st.confirmHost >= 0 || st.confirmTunnel >= 0;
    r.FillRound(ok.D2D(), st.S(L::RBtn), danger ? p.danger : p.accent);
    r.TextVertCenter(danger ? Tr(L"删除") : Tr(L"确定"), ok.D2D(),
                     danger ? Rgb(0xFFFFFF) : p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancel.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(Tr(L"取消"), cancel.D2D(), p.text2, FontId::Body, Align::Center);
  } else if (st.modal == ModalKind::Channel) {
    Rect ok = ModalOkRect(st, panel);
    Rect cancel = ModalCancelRect(st, panel);
    r.FillRound(ok.D2D(), st.S(L::RBtn), p.accent);
    r.TextVertCenter(Tr(L"用浏览器打开"), ok.D2D(), p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancel.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(Tr(L"复制地址"), cancel.D2D(), p.text2, FontId::Body, Align::Center);
  } else {
    Rect ok = ModalOkRect(st, panel);
    r.FillRound(ok.D2D(), st.S(L::RBtn), p.surface2);
    r.TextVertCenter(Tr(L"关闭"), ok.D2D(), p.text2, FontId::Body, Align::Center);
  }
}

}

Rect DashSearchRect(const AppState& st, const Rect& c) {
  float right = c.r - st.S(28.0f);
  float nr = right - st.S(112.0f) - st.S(10.0f);
  return Rect{ nr - st.S(220.0f), c.t + st.S(34.0f), nr, c.t + st.S(68.0f) };
}

const Palette& PaletteFor(bool dark, int accentIdx) {
  static Palette cache[2][5];
  static bool ready = false;
  if (!ready) {
    for (int d = 0; d < 2; ++d) {
      for (int a = 0; a < 5; ++a) cache[d][a] = BuildPalette(d == 0, a);
    }
    ready = true;
  }
  int ai = accentIdx < 0 ? 0 : (accentIdx > 4 ? 4 : accentIdx);
  return cache[dark ? 0 : 1][ai];
}

void AppState::Seed() {
  hosts.clear();
  tabs.clear();
  nextHostId = 1;
  nextTunnelId = 1;

  Tab home;
  home.view = (int)View::Dash;
  home.title = Tr(L"首页");
  home.icon = (int)View::Dash;
  tabs.push_back(home);
  activeTab = 0;

  sftpPath = L"/";
  sftpStatus = Tr(L"未连接");
  sftpSelRemote = -1;
  sftpSelLocal = -1;
  sftpQueue.clear();
  sftpLocal.clear();
  sftpRemote.clear();
  sftpLocalPath.clear();
}

static const wchar_t* PageTitle(View v) {
  switch (v) {
    case View::Sftp: return L"SFTP";
    case View::Tun: return Tr(L"端口转发");
    case View::Set: return Tr(L"设置");
    default: return Tr(L"首页");
  }
}

int AppState::SessionForHost(int hostId) {
  if (hostId < 0 || hostId >= (int)hosts.size()) return -1;
  Host& h = hosts[(size_t)hostId];
  if (h.sessionId >= 0) return h.sessionId;
  h.sessionId = nextSessionId++;
  SessionHub::Instance().Ensure(h.sessionId);
  return h.sessionId;
}

int AppState::OpenHostSession(int hostId) {
  if (hostId < 0 || hostId >= (int)hosts.size()) return -1;
  int sid = SessionForHost(hostId);
  if (sid < 0) return -1;
  int found = FindTabByHost(hostId);
  if (found >= 0) {
    ActivateTab(found);
    return sid;
  }
  Tab t;
  t.view = (int)View::Term;
  t.hostId = hostId;
  t.sessionId = sid;
  t.title = hosts[(size_t)hostId].name + Tr(L" · 终端");
  t.icon = (int)View::Term;
  tabs.push_back(t);
  activeHost = hostId;
  activeTab = (int)tabs.size() - 1;
  view = View::Term;
  MarkLastView();
  return sid;
}

void AppState::OpenPage(View v) {
  if (v == View::Term) {
    for (int i = (int)tabs.size() - 1; i >= 0; --i) {
      if (tabs[(size_t)i].hostId >= 0) {
        ActivateTab(i);
        return;
      }
    }
    OpenPage(View::Dash);
    return;
  }
  for (size_t i = 0; i < tabs.size(); ++i) {
    if (tabs[i].view == (int)v && tabs[i].hostId < 0) {
      ActivateTab((int)i);
      return;
    }
  }
  Tab t;
  t.view = (int)v;
  t.title = PageTitle(v);
  t.icon = (int)v;
  tabs.push_back(t);
  activeTab = (int)tabs.size() - 1;
  view = v;
  MarkLastView();
}

void AppState::ActivateTab(int index) {
  if (index < 0 || index >= (int)tabs.size()) return;
  activeTab = index;
  view = (View)tabs[(size_t)index].view;
  if (tabs[(size_t)index].hostId >= 0) activeHost = tabs[(size_t)index].hostId;
  MarkLastView();
}

void AppState::CloseTab(int index) {
  if (index < 0 || index >= (int)tabs.size()) return;
  tabs.erase(tabs.begin() + index);
  if (tabs.empty()) {
    Tab home;
    home.view = (int)View::Dash;
    home.title = Tr(L"首页");
    home.icon = (int)View::Dash;
    tabs.push_back(home);
    activeTab = 0;
    view = View::Dash;
    return;
  }
  if (activeTab > index) --activeTab;
  if (activeTab >= (int)tabs.size()) activeTab = (int)tabs.size() - 1;
  if (activeTab < 0) activeTab = 0;
  ActivateTab(activeTab);
}

void AppState::CloseTabAndSession(int index) {
  if (index < 0 || index >= (int)tabs.size()) return;
  int sid = tabs[(size_t)index].sessionId;
  if (sid >= 0) {
    SshService::Instance().Disconnect(sid);
    SessionHub::Instance().Close(sid);
    int hid = tabs[(size_t)index].hostId;
    if (hid >= 0 && hid < (int)hosts.size()) {
      hosts[(size_t)hid].state = HostState::Off;
      hosts[(size_t)hid].latency = -1;
      hosts[(size_t)hid].sessionId = -1;
      hosts[(size_t)hid].manualStop = true;
      hosts[(size_t)hid].retryAt = 0;
    }
  }
  CloseTab(index);
}

void AppState::RemoveHost(int hostId) {
  if (hostId < 0 || hostId >= (int)hosts.size()) return;
  for (int i = (int)tabs.size() - 1; i >= 0; --i) {
    if (tabs[(size_t)i].hostId == hostId) CloseTabAndSession(i);
  }
  hosts.erase(hosts.begin() + hostId);
  for (size_t i = 0; i < tabs.size(); ++i) {
    if (tabs[i].hostId > hostId) --tabs[i].hostId;
  }
  if (activeHost > hostId) --activeHost;
  if (activeHost >= (int)hosts.size()) activeHost = (int)hosts.size() - 1;
  if (activeHost < 0) activeHost = 0;
  dirty = true;
}

int AppState::OpenHostView(int hostId, int subView) {
  int sid = OpenHostSession(hostId);
  if (sid < 0) return -1;
  sessTab = subView < 0 ? 0 : (subView > 2 ? 2 : subView);
  return sid;
}

void AppState::RecalcDaysLeft() {}

void AppState::RefreshKeyStore() {
  keyStore = ListKeyFiles();
  for (size_t i = 0; i + 1 < keyStore.size(); ++i) {
    for (size_t k = 0; k + 1 < keyStore.size() - i; ++k) {
      if (keyStore[k] > keyStore[k + 1]) std::swap(keyStore[k], keyStore[k + 1]);
    }
  }
}

void AppState::OpenKeyStore() {
  RefreshKeyStore();
  keyReturn = modal;
  modal = ModalKind::KeyStore;
  modalTitle = Tr(L"密钥库");
  modalNote = Tr(L"取自程序目录 keys\\，选中即填入私钥路径");
}

void AppState::ResetFingerprint(int hostIndex) {
  if (hostIndex < 0 || hostIndex >= (int)hosts.size()) return;
  hosts[(size_t)hostIndex].hostKey.clear();
  hosts[(size_t)hostIndex].hostKeyType.clear();
  dirty = true;
}

void AppState::OpenDatePicker(int fieldIndex) {
  if (fieldIndex < 0 || fieldIndex >= (int)form.size()) return;
  dateField = fieldIndex;
  dateReturn = modal;
  SYSTEMTIME now{};
  GetLocalTime(&now);
  SYSTEMTIME cur{};
  if (ParseStamp(TrimWs(form[(size_t)fieldIndex].value), cur)) {
    dpYear = cur.wYear;
    dpMonth = cur.wMonth;
    dpDay = cur.wDay;
    dpHour = cur.wHour;
    dpMinute = cur.wMinute;
    dpSecond = cur.wSecond;
  } else {
    dpYear = now.wYear;
    dpMonth = now.wMonth;
    dpDay = now.wDay;
    dpHour = 0;
    dpMinute = 0;
    dpSecond = 0;
  }
  dpDrop = 0;
  modal = ModalKind::DatePicker;
  modalTitle = form[(size_t)fieldIndex].label;
  modalNote = Tr(L"选日期后调整时 / 分 / 秒，确定写回字段");
}

void AppState::CommitDatePicker() {
  if (dateField < 0 || dateField >= (int)form.size()) return;
  int dim = DaysInMonth(dpYear, dpMonth);
  if (dpDay > dim) dpDay = dim;
  if (dpDay < 1) dpDay = 1;
  SYSTEMTIME out{};
  out.wYear = (WORD)dpYear;
  out.wMonth = (WORD)dpMonth;
  out.wDay = (WORD)dpDay;
  out.wHour = (WORD)dpHour;
  out.wMinute = (WORD)dpMinute;
  out.wSecond = (WORD)dpSecond;
  form[(size_t)dateField].value = FormatStamp(out);
  form[(size_t)dateField].fresh = false;
  modal = dateReturn;
  dateField = -1;
}

void AppState::ClearDateField() {
  if (dateField < 0 || dateField >= (int)form.size()) return;
  form[(size_t)dateField].value.clear();
  form[(size_t)dateField].fresh = false;
  modal = dateReturn;
  dateField = -1;
}

void AppState::OpenHostForm(int index) {
  form.clear();
  formTarget = index;
  formError.clear();
  formScroll = 0.0f;
  bool edit = index >= 0 && index < (int)hosts.size();
  Host blank;
  Host h = edit ? hosts[static_cast<size_t>(index)] : blank;
  int authOpt = edit && h.IsKeyAuth() ? 1 : 0;

  form.push_back({ L"g1", Tr(L"基本信息"), L"", L"", FieldKind::Group });
  form.push_back({ L"name", Tr(L"名称"), h.name, Tr(L"例如 生产机"), FieldKind::Text });
  form.push_back({ L"host", Tr(L"主机"), h.host, Tr(L"域名或 IP"), FieldKind::Text });
  form.push_back({ L"port", Tr(L"端口"), edit ? std::to_wstring(h.port) : std::wstring(L"22"),
                   Tr(L"默认 22"), FieldKind::Text });
  form.push_back({ L"user", Tr(L"用户名"), edit ? h.user : std::wstring(L"root"), Tr(L"登录用户"),
                   FieldKind::Text });

  form.push_back({ L"g2", Tr(L"认证"), L"", L"", FieldKind::Group });
  form.push_back({ L"auth", Tr(L"认证方式"), L"", L"", FieldKind::Seg,
                   { Tr(L"密码"), Tr(L"密钥") }, authOpt });
  form.push_back({ L"password", Tr(L"密码"), h.password, Tr(L"随配置保存在本机"), FieldKind::Pass,
                   {}, 0, false, L"auth", 0 });
  form.push_back({ L"keyPath", Tr(L"私钥文件"), h.keyPath, Tr(L"留空则自动查找 keys 目录"),
                   FieldKind::File, {}, 0, false, L"auth", 1 });
  form.push_back({ L"keyPass", Tr(L"私钥口令"), h.keyPass, Tr(L"私钥无口令可留空"), FieldKind::Pass,
                   {}, 0, false, L"auth", 1 });

  form.push_back({ L"g3", Tr(L"时间与自动化"), L"", L"", FieldKind::Group });
  form.push_back({ L"registeredAt", Tr(L"注册时间"), h.registeredAt, Tr(L"点击选择日期"),
                   FieldKind::Date });
  form.push_back({ L"expireAt", Tr(L"到期时间"), h.expireAt, Tr(L"留空表示不限"), FieldKind::Date });
  form.push_back({ L"autoConnect", Tr(L"启动连接"), L"", L"", FieldKind::Seg,
                   { Tr(L"关闭"), Tr(L"开启") }, h.autoConnect ? 1 : 0 });
  if (edit && !h.hostKey.empty()) {
    form.push_back({ L"hostKey", Tr(L"主机指纹"), h.hostKey, Tr(L"重置后下次连接重新记录"),
                     FieldKind::Info });
  }

  formFocus = 1;
  modal = ModalKind::HostForm;
  modalTitle = edit ? Tr(L"编辑主机") : Tr(L"新建主机");
  modalNote = Tr(L"配置写入程序同目录 skiff_store.json，不写注册表");
}

void AppState::OpenTunForm(int hostIndex, int tunnelId) {
  form.clear();
  formError.clear();
  formScroll = 0.0f;
  formHost = hostIndex;
  formTunnel = tunnelId;
  int hi = hostIndex >= 0 ? hostIndex : ActiveHostIndex();
  Tunnel* t = TunnelById(hi, tunnelId);
  bool edit = t != nullptr;
  Tunnel blank;
  Tunnel tv = edit ? *t : blank;

  form.push_back({ L"g1", Tr(L"转发规则"), L"", L"", FieldKind::Group });
  form.push_back({ L"tunType", Tr(L"类型"), L"", L"", FieldKind::Seg,
                   { Tr(L"本地 L"), Tr(L"远程 R") }, tv.IsRemote() ? 1 : 0 });
  form.push_back({ L"tunPort", Tr(L"本地端口"), tv.port, Tr(L"例如 8080"), FieldKind::Text });
  form.push_back({ L"tunDest", Tr(L"目标地址"), tv.dest, Tr(L"例如 192.0.2.10:80"), FieldKind::Text });
  form.push_back({ L"tunHost", Tr(L"所属主机"), hi >= 0 && hi < (int)hosts.size()
                                               ? hosts[(size_t)hi].name : std::wstring(Tr(L"未选择")),
                   Tr(L"转发经由的主机"), FieldKind::Info });

  formFocus = 1;
  modal = ModalKind::TunForm;
  modalTitle = edit ? Tr(L"编辑转发") : Tr(L"新建转发");
  modalNote = Tr(L"保存后立即进入巡检队列");
}

bool AppState::CommitForm() {
  formError.clear();
  if (modal == ModalKind::HostForm) {
    std::wstring name = TrimWs(FormValue(form, L"name"));
    std::wstring host = TrimWs(FormValue(form, L"host"));
    std::wstring portText = TrimWs(FormValue(form, L"port"));
    std::wstring user = TrimWs(FormValue(form, L"user"));
    std::wstring reg = TrimWs(FormValue(form, L"registeredAt"));
    std::wstring exp = TrimWs(FormValue(form, L"expireAt"));
    if (name.empty()) {
      formError = Tr(L"请填写名称");
      return false;
    }
    if (host.empty()) {
      formError = Tr(L"请填写主机地址");
      return false;
    }
    for (size_t i = 0; i < host.size(); ++i) {
      if (host[i] == L' ' || host[i] == L'@' || host[i] == L':') {
        formError = Tr(L"主机地址不能包含空格、@ 或冒号，端口请填在端口栏");
        return false;
      }
    }
    int port = 22;
    if (!portText.empty()) {
      port = _wtoi(portText.c_str());
      if (port < 1 || port > 65535) {
        formError = Tr(L"端口需在 1 ~ 65535 之间");
        return false;
      }
    }
    if (user.empty()) {
      formError = Tr(L"请填写用户名");
      return false;
    }
    if (!reg.empty() && StampMillis(reg) < 0) {
      formError = Tr(L"注册时间格式应为 YYYY-MM-DD HH:mm:ss");
      return false;
    }
    if (!exp.empty() && StampMillis(exp) < 0) {
      formError = Tr(L"到期时间格式应为 YYYY-MM-DD HH:mm:ss");
      return false;
    }
    if (!reg.empty() && !exp.empty() && StampMillis(exp) < StampMillis(reg)) {
      formError = Tr(L"到期时间早于注册时间");
      return false;
    }

    bool edit = formTarget >= 0 && formTarget < (int)hosts.size();
    Host h = edit ? hosts[static_cast<size_t>(formTarget)] : Host();
    h.name = name;
    h.host = host;
    h.port = port;
    h.user = user;
    h.auth = FormOpt(form, L"auth") == 1 ? std::wstring(L"密钥") : std::wstring(L"密码");
    h.password = FormValue(form, L"password");
    h.keyPath = TrimWs(FormValue(form, L"keyPath"));
    h.keyPass = FormValue(form, L"keyPass");
    h.registeredAt = reg;
    h.expireAt = exp;
    h.autoConnect = FormOpt(form, L"autoConnect") == 1;
    if (h.auth == L"密钥" && h.keyPath.empty() && edit) h.keyPath.clear();
    if (edit) {
      int hi = formTarget;
      if (hosts[(size_t)hi].host != h.host || hosts[(size_t)hi].port != h.port) {
        h.hostKey.clear();
        h.hostKeyType.clear();
      }
      hosts[(size_t)hi] = h;
      for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].hostId == hi && tabs[i].view == (int)View::Term) {
          tabs[i].title = h.name + Tr(L" · 终端");
        }
      }
    } else {
      if (h.id <= 0) h.id = nextHostId++;
      if (h.registeredAt.empty()) h.registeredAt = NowStamp();
      hosts.push_back(h);
      activeHost = (int)hosts.size() - 1;
    }
    dirty = true;
    modal = ModalKind::None;
    return true;
  }
  if (modal == ModalKind::TunForm) {
    std::wstring portText = TrimWs(FormValue(form, L"tunPort"));
    std::wstring dest = TrimWs(FormValue(form, L"tunDest"));
    if (portText.empty()) {
      formError = Tr(L"请填写本地端口");
      return false;
    }
    int port = _wtoi(portText.c_str());
    if (port < 1 || port > 65535) {
      formError = Tr(L"本地端口需在 1 ~ 65535 之间");
      return false;
    }
    if (dest.empty()) {
      formError = Tr(L"请填写目标地址");
      return false;
    }
    int hi = formHost >= 0 ? formHost : ActiveHostIndex();
    if (hi < 0 || hi >= (int)hosts.size()) {
      formError = Tr(L"请先创建一台主机");
      return false;
    }
    std::vector<Tunnel>& list = hosts[(size_t)hi].tunnels;
    Tunnel* t = nullptr;
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].id == formTunnel) t = &list[i];
    }
    if (t == nullptr) {
      Tunnel nt;
      nt.id = nextTunnelId++;
      nt.hostId = hosts[(size_t)hi].id;
      nt.on = true;
      list.push_back(nt);
      t = &list.back();
    }
    t->type = FormOpt(form, L"tunType") == 1 ? std::wstring(L"R") : std::wstring(L"L");
    t->port = portText;
    t->dest = dest;
    t->state = TunState::Down;
    t->retryIn = 0;
    t->fails = 0;
    t->nextAt = 0;
    dirty = true;
    modal = ModalKind::None;
    return true;
  }
  return false;
}

const wchar_t* ViewTitle(View v) {
  switch (v) {
    case View::Dash: return Tr(L"首页");
    case View::Term: return Tr(L"终端");
    case View::Sftp: return L"SFTP";
    case View::Tun: return Tr(L"端口转发");
    case View::Set: return Tr(L"设置");
  }
  return Tr(L"首页");
}

int SidebarVisibleHosts(const AppState& st, int* out, int cap) {
  if (!out || cap <= 0) return 0;
  std::wstring q = st.query;
  for (size_t i = 0; i < q.size(); ++i) {
    if (q[i] >= L'A' && q[i] <= L'Z') q[i] = static_cast<wchar_t>(q[i] - L'A' + L'a');
  }
  int n = 0;
  for (int i = 0; i < (int)st.hosts.size() && n < cap; ++i) {
    if (q.empty()) {
      out[n++] = i;
      continue;
    }
    std::wstring name = st.hosts[i].name;
    std::wstring addr = st.hosts[i].Addr();
    for (size_t k = 0; k < name.size(); ++k) {
      if (name[k] >= L'A' && name[k] <= L'Z') name[k] = static_cast<wchar_t>(name[k] - L'A' + L'a');
    }
    for (size_t k = 0; k < addr.size(); ++k) {
      if (addr[k] >= L'A' && addr[k] <= L'Z') addr[k] = static_cast<wchar_t>(addr[k] - L'A' + L'a');
    }
    if (name.find(q) != std::wstring::npos || addr.find(q) != std::wstring::npos) out[n++] = i;
  }
  auto keyOf = [&st](const Host& h) {
    return StampMillis(st.sortKey == 0 ? h.expireAt : h.registeredAt);
  };
  for (int i = 1; i < n; ++i) {
    int cur = out[i];
    long long b = keyOf(st.hosts[(size_t)cur]);
    int j = i - 1;
    while (j >= 0) {
      long long a = keyOf(st.hosts[(size_t)out[j]]);
      bool move = false;
      if (a < 0 && b >= 0) move = true;
      else if (a >= 0 && b >= 0) move = st.sortAsc ? (b < a) : (b > a);
      if (!move) break;
      out[j + 1] = out[j];
      --j;
    }
    out[j + 1] = cur;
  }
  return n;
}

int RecentHosts(const AppState& st, int* out, int cap) {
  if (!out || cap <= 0) return 0;
  int n = 0;
  for (int i = 0; i < (int)st.hosts.size(); ++i) {
    long long ms = StampMillis(st.hosts[(size_t)i].lastConnected);
    if (ms < 0) continue;
    int pos;
    if (n < cap) {
      pos = n++;
    } else {
      if (ms <= StampMillis(st.hosts[(size_t)out[cap - 1]].lastConnected)) continue;
      pos = cap - 1;
    }
    out[pos] = i;
    for (int j = pos; j > 0; --j) {
      long long a = StampMillis(st.hosts[(size_t)out[j - 1]].lastConnected);
      long long b = StampMillis(st.hosts[(size_t)out[j]].lastConnected);
      if (a >= b) break;
      std::swap(out[j - 1], out[j]);
    }
  }
  return n;
}

float SideScrollMax(const AppState& st, const Rect& sidebar) {
  int vis[kSideCardCap];
  int n = SidebarVisibleHosts(st, vis, kSideCardCap);
  float totalH = (st.S(126.0f) + st.S(8.0f)) * (float)n - st.S(8.0f);
  float viewH = SideListBottom(st, sidebar) - SideListTop(st, sidebar);
  return totalH > viewH ? totalH - viewH : 0.0f;
}

int SidebarHostAt(const AppState& st, const Rect& sidebar, float x, float y) {
  float foot = SidebarFootTop(st, sidebar);
  int vis[64];
  int visN = SidebarVisibleHosts(st, vis, 64);
  for (int k = 0; k < visN; ++k) {
    Rect row = SidebarHostRect(st, sidebar, k);
    if (row.b > foot - st.S(4)) break;
    if (row.Hit(x, y)) return vis[k];
  }
  return -1;
}

Rect SetNavRect(const AppState& st, const Rect& c, int index) {
  float top = c.t + st.S(14) + st.S(36) * (float)index;
  return Rect{ c.l + st.S(8), top, c.l + st.S(170) - st.S(8), top + st.S(34) };
}

void LayoutMetrics(const AppState& st, const Rect& client, Rect& sidebar, Rect& tabs, Rect& content,
                   Rect& status) {
  if (st.fullscreen) {
    sidebar = Rect{ client.l, client.t, client.l, client.b };
    tabs = Rect{ client.l, client.t, client.l, client.t };
    status = Rect{ client.l, client.b, client.l, client.b };
    content = client;
    return;
  }
  float titleH = st.S(L::TitleH);
  float tabsH = st.S(L::TabsH);
  float statusH = st.S(L::StatusH);
  float sideW = st.sideCollapsed ? st.S(kSideCollapsedW) : st.S(st.sidebarW);
  if (sideW > client.W() * kHalf) sideW = client.W() * kHalf;
  sidebar = Rect{ client.l, client.t + titleH, client.l + sideW, client.b };
  tabs = Rect{ sidebar.r, client.t + titleH, client.r, client.t + titleH + tabsH };
  status = Rect{ sidebar.r, client.b - statusH, client.r, client.b };
  content = Rect{ sidebar.r, tabs.b, client.r, status.t };
}

Rect SessSegRectOf(const AppState& st, const Rect& c, int slot) {
  float w = st.S(74.0f);
  float gap = st.S(4.0f);
  float left = c.l + st.S(14.0f);
  float top = c.t + st.S(8.0f);
  return Rect{ left + (w + gap) * (float)slot, top, left + (w + gap) * (float)slot + w,
               top + st.S(26.0f) };
}

int SessSegAt(const AppState& st, const Rect& c, float x, float y) {
  for (int k = 0; k < 3; ++k) {
    if (SessSegRectOf(st, c, k).Hit(x, y)) return k;
  }
  return -1;
}

Rect SessInnerRect(const AppState& st, const Rect& c) {
  return Rect{ c.l, c.t + st.S(42.0f), c.r, c.b };
}

void DrawSessionPage(Render& r, AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark, st.accentIdx);
  r.Fill(rc.D2D(), p.bg);
  Rect bar{ rc.l, rc.t, rc.r, rc.t + st.S(42.0f) };
  r.Fill(bar.D2D(), p.surface);
  r.Fill(D2D1::RectF(bar.l, bar.b - st.S(1.0f), bar.r, bar.b), p.line);
  const wchar_t* names[3] = { Tr(L"终端"), Tr(L"隧道"), Tr(L"文件") };
  for (int k = 0; k < 3; ++k) {
    Rect seg = SessSegRectOf(st, rc, k);
    bool on = k == st.sessTab;
    if (on) r.FillRound(seg.D2D(), st.S(L::RBtn), p.accentSoft);
    r.TextVertCenter(names[k], seg.D2D(), on ? p.accent : p.text2, FontId::Small, Align::Center);
  }
  int hid = st.CurrentHostId();
  const Host* h = (hid >= 0 && hid < (int)st.hosts.size()) ? &st.hosts[(size_t)hid] : nullptr;
  if (h != nullptr) {
    std::wstring right = h->name + L" · " + h->Addr();
    r.TextVertCenter(right.c_str(),
                     D2D1::RectF(rc.l + st.S(260.0f), bar.t, rc.r - st.S(16.0f), bar.b), p.text3,
                     FontId::MonoSm, Align::Right);
  }
  Rect inner = SessInnerRect(st, rc);
  if (st.sessTab == 1) {
    DrawTun(r, st, inner);
  } else if (st.sessTab == 2) {
    DrawSftp(r, st, inner);
  } else {
    DrawTermView(r, st, inner);
  }
}

void DrawAll(Render& r, AppState& st, const Rect& client) {
  const Palette& p = PaletteFor(st.dark, st.accentIdx);
  r.Fill(client.D2D(), p.bg);

  Rect sidebar, tabs, content, status;
  LayoutMetrics(st, client, sidebar, tabs, content, status);

  if (!st.fullscreen) {
    DrawTitleBar(r, st, client, p);
    DrawSidebar(r, st, sidebar, p);
    DrawTabs(r, st, tabs, p);
  }

  int sessHost = st.CurrentHostId();
  bool session = st.view == View::Term && sessHost >= 0;

  switch (st.view) {
    case View::Dash: DrawDash(r, st, content); break;
    case View::Term:
      if (session) DrawSessionPage(r, st, content);
      else DrawTermView(r, st, content);
      break;
    case View::Sftp: DrawSftp(r, st, content); break;
    case View::Tun: DrawTun(r, st, content); break;
    case View::Set: DrawSet(r, st, content); break;
  }

  if (!st.fullscreen) DrawStatus(r, st, status, p);
  if (!st.toast.empty()) {
    float pad = st.S(18.0f);
    float tw = r.Measure(st.toast.c_str(), FontId::Small) + pad * 2.0f;
    float th = st.S(34.0f);
    float mid = (client.l + client.r) * kHalf;
    Rect box{ mid - tw * kHalf, client.b - st.S(78.0f) - th, mid + tw * kHalf,
              client.b - st.S(78.0f) };
    r.FillRound(box.D2D(), st.S(L::RBtn),
                Alpha(st.toastKind == 1 ? p.danger : p.accent, 0.94f));
    r.TextVertCenter(st.toast.c_str(), box.D2D(), Rgb(0xFFFFFF), FontId::Small, Align::Center);
  }
  DrawModalImpl(r, st, client);
}
int HitTest(const AppState& st, const Rect& client, float x, float y) {
  Rect sidebar, tabs, content, status;
  LayoutMetrics(st, client, sidebar, tabs, content, status);

  if (st.modal != ModalKind::None) {
    Rect panel = ModalPanel(st, client);
    if (!panel.Hit(x, y)) return HitModalBackdrop;
    if (ModalCloseRect(st, panel).Hit(x, y)) return HitModalClose;
    if (ModalOkRect(st, panel).Hit(x, y)) return HitModalOk;
    if (st.modal == ModalKind::DatePicker) {
      if (ModalCancelRect(st, panel).Hit(x, y)) return HitModalCancel;
      if (DpArrowRect(st, panel, 0).Hit(x, y)) return HitDpPrev;
      if (DpArrowRect(st, panel, 1).Hit(x, y)) return HitDpNext;
      if (DpAuxRect(st, panel, 0).Hit(x, y)) return HitDpToday;
      if (DpAuxRect(st, panel, 1).Hit(x, y)) return HitDpClear;
      for (int k = 0; k < 3; ++k) {
        if (DpTimeBtnRect(st, panel, k).Hit(x, y)) return HitDpHour + k;
      }
      if (st.dpDrop == 0) {
        for (int k = 0; k < 42; ++k) {
          if (DpDayRect(st, panel, k).Hit(x, y)) return HitDpDayBase + k;
        }
      } else {
        int n = st.dpDrop == 1 ? 24 : 60;
        for (int k = 0; k < n; ++k) {
          if (DpCellRect(st, panel, k).Hit(x, y)) return HitDpCellBase + k;
        }
      }
      return HitNone;
    }
    if (ModalIsForm(st)) {
      if (ModalCancelRect(st, panel).Hit(x, y)) return HitModalCancel;
      if (st.modal == ModalKind::HostForm) {
        Rect ok = ModalOkRect(st, panel);
        Rect ks{ panel.l + st.S(22.0f), ok.t, panel.l + st.S(22.0f) + st.S(84.0f), ok.b };
        if (ks.Hit(x, y)) return HitModalKeyStore;
      }
      for (int i = 0; i < (int)st.form.size(); ++i) {
        Rect row = ModalFieldRect(st, panel, i);
        if (!row.Hit(x, y)) continue;
        const FormField& f = st.form[i];
        if (f.kind == FieldKind::Group) return HitNone;
        if (f.kind == FieldKind::Seg) {
          Rect seg{ row.l + st.S(84.0f), row.t + st.S(10.0f), row.r, row.b - st.S(10.0f) };
          if (seg.W() > st.S(190.0f)) seg.r = seg.l + st.S(190.0f);
          int n = (int)f.options.size();
          for (int k = 0; k < n; ++k) {
            Rect cell{ seg.l + seg.W() * (float)k / (float)n, seg.t,
                       seg.l + seg.W() * (float)(k + 1) / (float)n, seg.b };
            if (cell.Hit(x, y)) return HitFormSegBase + i * 4 + k;
          }
        }
        if (f.kind == FieldKind::File || f.kind == FieldKind::Date) {
          Rect br{ row.r - st.S(64.0f), row.t + st.S(9.0f), row.r, row.b - st.S(9.0f) };
          if (br.Hit(x, y)) return HitFormBrowseBase + i;
        }
        return HitFormFieldBase + i;
      }
    } else if (st.modal == ModalKind::Channel) {
      if (ModalCancelRect(st, panel).Hit(x, y)) return HitModalCancel;
    } else if (st.modal == ModalKind::Confirm) {
      if (ModalCancelRect(st, panel).Hit(x, y)) return HitModalCancel;
    } else if (st.modal == ModalKind::KeyStore) {
      for (int i = 0; i < (int)st.keyStore.size(); ++i) {
        if (ModalFieldRect(st, panel, i).Hit(x, y)) return HitKeyRowBase + i;
      }
    }
    return HitNone;
  }

  if (!st.fullscreen) {
    for (int i = 0; i < 3; ++i) {
      if (TitleButtonRect(st, client, i).Hit(x, y)) return HitTitleMin + i;
    }

    if (st.sideCollapsed) {
      if (SideExpandRect(st, sidebar).Hit(x, y)) return HitSideExpand;
      if (SideCollapsedSettingsRect(st, sidebar).Hit(x, y)) return HitSideSettings;
    } else {
      if (SideBrandFoldRect(st, sidebar).Hit(x, y)) return HitSideCollapse;
      if (SideSettingsRect(st, sidebar).Hit(x, y)) return HitSideSettings;
      if (SideCollapseRect(st, sidebar).Hit(x, y)) return HitSideCollapse;
      if (SideSearchRect(st, sidebar).Hit(x, y)) return HitSideSearch;
      if (SideSortKeyRect(st, sidebar).Hit(x, y)) return HitSortKey;
      if (SideSortDirRect(st, sidebar).Hit(x, y)) return HitSortDir;
      if (sidebar.Hit(x, y) && y >= SideListTop(st, sidebar) &&
          y < SideListBottom(st, sidebar)) {
        int vis[kSideCardCap];
        int visN = SidebarVisibleHosts(st, vis, kSideCardCap);
        for (int s = 0; s < visN; ++s) {
          if (!SideCardRect(st, sidebar, s).Hit(x, y)) continue;
          if (SideEditRectOf(st, sidebar, s).Hit(x, y)) {
            return HitSideCardBase + s * kSideCardSlots + 1;
          }
          for (int slot = 2; slot <= 5; ++slot) {
            if (SideBtnRectOf(st, sidebar, s, slot).Hit(x, y)) {
              return HitSideCardBase + s * kSideCardSlots + slot;
            }
          }
          return HitSideCardBase + s * kSideCardSlots;
        }
      }
    }

    for (int i = 0; i < (int)st.tabs.size(); ++i) {
      if (!TabRectOf(st, tabs, i).Hit(x, y)) continue;
      if (TabCloseRect(st, tabs, i).Hit(x, y)) return HitTabCloseBase + i;
      return HitTabBase + i;
    }
    for (int i = 0; i < 1; ++i) {
      if (TabsRightButtonRect(st, tabs, i).Hit(x, y)) return HitTabsRightBase + i;
    }
  }

  if (content.Hit(x, y)) {
    if (st.view == View::Dash) {
      if (DashNewHostRect(st, content).Hit(x, y)) return HitDashNewHost;
      int rec[kRecentCap];
      int recN = RecentHosts(st, rec, kRecentCap);
      for (int i = 0; i < recN; ++i) {
        if (!DashRecentCardRect(st, content, i).Hit(x, y)) continue;
        if (DashRecentBtnRect(st, content, i).Hit(x, y)) {
          return HitRecentBase + i * kSideCardSlots + 2;
        }
        return HitRecentBase + i * kSideCardSlots;
      }
    } else if (st.view == View::Set) {
      for (int i = 0; i < 6; ++i) {
        if (SetNavRect(st, content, i).Hit(x, y)) return HitSetNavBase + i;
      }
      int rowN = SetRowCount(st.setNav);
      for (int i = 0; i < rowN; ++i) {
        Rect row = SetRowRect(st, content, i);
        if (x > row.r - st.S(420.0f) && row.Hit(x, y)) return HitSetCtrlBase + i;
      }
    } else {
      bool session = st.view == View::Term && st.CurrentHostId() >= 0;
      int page = st.view == View::Tun ? 1 : (st.view == View::Sftp ? 2 : 0);
      Rect inner = content;
      if (session) {
        for (int k = 0; k < 3; ++k) {
          if (SessSegRectOf(st, content, k).Hit(x, y)) return HitSessSegBase + k;
        }
        page = st.sessTab;
        inner = SessInnerRect(st, content);
        if (!inner.Hit(x, y)) return HitNone;
      }
      if (page == 2) {
        if (FtpToolRect(st, inner, 0).Hit(x, y)) return HitFtpNew;
        if (FtpToolRect(st, inner, 1).Hit(x, y)) return HitFtpRefresh;
        if (FtpQueueRect(st, inner).Hit(x, y)) return HitFtpQueue;
        if (FtpActionRect(st, inner, 0).Hit(x, y)) return HitFtpDown;
        if (FtpActionRect(st, inner, 1).Hit(x, y)) return HitFtpUp;
        for (int side = 0; side < 2; ++side) {
          const std::vector<SftpEntry>& list = side == 0 ? st.sftpLocal : st.sftpRemote;
          for (int r = 0; r < (int)list.size() && r < 40; ++r) {
            if (SftpRowRectOf(st, inner, side, r).Hit(x, y)) return HitFtpRowBase + side * 40 + r;
          }
        }
      } else if (page == 1) {
        if (TunNewRect(st, inner).Hit(x, y)) return HitTunNew;
        int hi = session ? st.CurrentHostId() : st.ActiveHostIndex();
        const Host* h = (hi >= 0 && hi < (int)st.hosts.size()) ? &st.hosts[(size_t)hi] : nullptr;
        int tunN = h == nullptr ? 0 : (int)h->tunnels.size();
        for (int i = 0; i < tunN && i < 20; ++i) {
          if (TunSwitchRect(st, inner, i).Hit(x, y)) return HitTunSwitchBase + i;
          for (int k = 0; k < 4; ++k) {
            if (TunOpRect(st, inner, i, k).Hit(x, y)) {
              if (k == 0) return HitTunRefreshBase + i;
              if (k == 1) return HitTunEditBase + i;
              if (k == 2) return HitTunDelBase + i;
              return HitTunBrowseBase + i;
            }
          }
          if (TunRowRectOf(st, inner, i).Hit(x, y)) return HitTunRowBase + i;
        }
      }
    }
  }

  if (!st.fullscreen && status.Hit(x, y) && x < status.l + st.S(180)) return HitStatusConn;
  return HitNone;
}

void OnHit(AppState& st, int id, HWND hwnd) {
  if (st.modal == ModalKind::DatePicker && id != HitNone) {
    if (id == HitModalOk) {
      st.CommitDatePicker();
      return;
    }
    if (id == HitModalBackdrop || id == HitModalClose || id == HitModalCancel) {
      st.modal = st.dateReturn;
      st.dateField = -1;
      return;
    }
    if (id == HitDpPrev) {
      if (st.dpMonth == 1) {
        st.dpMonth = 12;
        --st.dpYear;
      } else {
        --st.dpMonth;
      }
      int dim = DaysInMonth(st.dpYear, st.dpMonth);
      if (st.dpDay > dim) st.dpDay = dim;
      st.dpDrop = 0;
      st.dirty = true;
      return;
    }
    if (id == HitDpNext) {
      if (st.dpMonth == 12) {
        st.dpMonth = 1;
        ++st.dpYear;
      } else {
        ++st.dpMonth;
      }
      int dim = DaysInMonth(st.dpYear, st.dpMonth);
      if (st.dpDay > dim) st.dpDay = dim;
      st.dpDrop = 0;
      st.dirty = true;
      return;
    }
    if (id == HitDpToday) {
      SYSTEMTIME now{};
      GetLocalTime(&now);
      st.dpYear = now.wYear;
      st.dpMonth = now.wMonth;
      st.dpDay = now.wDay;
      st.dpDrop = 0;
      st.dirty = true;
      return;
    }
    if (id == HitDpClear) {
      st.ClearDateField();
      return;
    }
    if (id >= HitDpHour && id <= HitDpSecond) {
      int k = id - HitDpHour + 1;
      st.dpDrop = st.dpDrop == k ? 0 : k;
      st.dirty = true;
      return;
    }
    if (id >= HitDpDayBase && id < HitDpCellBase) {
      int k = id - HitDpDayBase;
      int lead = FirstWeekdayOfMonth(st.dpYear, st.dpMonth);
      int dim = DaysInMonth(st.dpYear, st.dpMonth);
      int day = k - lead + 1;
      if (day >= 1 && day <= dim) st.dpDay = day;
      st.dirty = true;
      return;
    }
    if (id >= HitDpCellBase) {
      int v = id - HitDpCellBase;
      if (st.dpDrop == 1) st.dpHour = v;
      else if (st.dpDrop == 2) st.dpMinute = v;
      else if (st.dpDrop == 3) st.dpSecond = v;
      st.dpDrop = 0;
      st.dirty = true;
      return;
    }
    return;
  }
  if (st.modal != ModalKind::None && id != HitNone) {
    if (id == HitModalBackdrop || id == HitModalClose || id == HitModalCancel) {
      if (st.modal == ModalKind::KeyStore && st.keyReturn != ModalKind::None) {
        st.modal = st.keyReturn;
        st.keyReturn = ModalKind::None;
        st.modalTitle = st.modal == ModalKind::HostForm ? Tr(L"编辑主机") : Tr(L"新建主机");
        return;
      }
      st.modal = ModalKind::None;
      st.keyReturn = ModalKind::None;
      st.confirmHost = -1;
      st.confirmTunnel = -1;
      return;
    }
    if (id == HitModalOk) {
      if (st.modal == ModalKind::Prompt) {
        st.modal = ModalKind::None;
        st.pendingSftp = 7;
        return;
      }
      if (ModalIsForm(st)) {
        bool ok = st.CommitForm();
        if (!ok) {
          SK_LOGW("FORM_REJECT focus=%d fields=%d", st.formFocus, (int)st.form.size());
        } else {
          SK_LOGI("FORM_SAVED kind=%d", (int)st.modal);
        }
      } else if (st.modal == ModalKind::Confirm) {
        int hIndex = st.confirmHost;
        int tunId = st.confirmTunnel;
        int hostIndex = st.formHost;
        st.modal = ModalKind::None;
        st.confirmHost = -1;
        st.confirmTunnel = -1;
        if (hIndex >= 0) {
          st.RemoveHost(hIndex);
        } else if (tunId >= 0 && hostIndex >= 0 && hostIndex < (int)st.hosts.size()) {
          std::vector<Tunnel>& list = st.hosts[(size_t)hostIndex].tunnels;
          int sid = st.CurrentSessionId();
          for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].id != tunId) continue;
            if (list[i].state != TunState::Down && sid >= 0) {
              SshService::Instance().ForwardStopLocal(sid, _wtoi(list[i].port.c_str()));
            }
            list.erase(list.begin() + i);
            break;
          }
          st.dirty = true;
        }
      } else {
        st.modal = ModalKind::None;
      }
      return;
    }
    if (id == HitModalKeyStore) {
      int keep = st.formTarget;
      st.OpenKeyStore();
      st.formTarget = keep;
      return;
    }
    if (id >= HitKeyRowBase && id < HitSessSegBase) {
      int idx = id - HitKeyRowBase;
      if (idx >= 0 && idx < (int)st.keyStore.size()) {
        std::wstring dir = KeyDirectory();
        std::wstring pick = st.keyStore[(size_t)idx];
        std::wstring path = dir.empty() ? pick : dir + L"\\" + pick;
        for (size_t i = 0; i < st.form.size(); ++i) {
          if (st.form[i].key == L"keyPath") st.form[i].value = path;
          if (st.form[i].key == L"auth") st.form[i].opt = 1;
        }
        st.modal = ModalKind::HostForm;
        st.keyReturn = ModalKind::None;
        st.modalTitle = st.formTarget >= 0 ? Tr(L"编辑主机") : Tr(L"新建主机");
        st.modalNote = Tr(L"已从密钥库填入私钥路径");
      }
      return;
    }
    if (id >= HitFormSegBase && id < HitFormBrowseBase) {
      int rel = id - HitFormSegBase;
      int i = rel / 4;
      int k = rel % 4;
      if (i >= 0 && i < (int)st.form.size() && k < (int)st.form[i].options.size()) {
        st.form[i].opt = k;
        st.formFocus = i;
        if (st.form[i].key == L"auth") {
          for (size_t n = 0; n < st.form.size(); ++n) {
            if (st.form[n].key == L"keyPath") st.form[n].fresh = false;
          }
        }
        if (!FieldVisible(st, st.formFocus)) {
          for (int n = 0; n < (int)st.form.size(); ++n) {
            if (FieldVisible(st, n) && st.form[n].kind != FieldKind::Group) {
              st.formFocus = n;
              break;
            }
          }
        }
      }
      return;
    }
    if (id >= HitFormBrowseBase && id < HitModalKeyStore) {
      int i = id - HitFormBrowseBase;
      if (i < 0 || i >= (int)st.form.size()) return;
      st.formFocus = i;
      if (st.form[i].kind == FieldKind::Date) {
        st.OpenDatePicker(i);
        return;
      }
      std::wstring pick = PickKeyFile(hwnd);
      if (!pick.empty()) {
        st.form[(size_t)i].value = pick;
        for (size_t n = 0; n < st.form.size(); ++n) {
          if (st.form[n].key == L"auth") st.form[n].opt = 1;
        }
      }
      return;
    }
    if (id >= HitFormFieldBase && id < HitFormSegBase) {
      int i = id - HitFormFieldBase;
      st.formFocus = i;
      if (i >= 0 && i < (int)st.form.size() && st.form[i].kind == FieldKind::Seg) {
        int n = (int)st.form[i].options.size();
        if (n > 0) st.form[i].opt = (st.form[i].opt + 1) % n;
      } else if (i >= 0 && i < (int)st.form.size() && st.form[i].kind == FieldKind::Date) {
        st.OpenDatePicker(i);
      }
      return;
    }
    return;
  }

  if (id == HitNone) {
    st.searchFocus = false;
    return;
  }

  st.searchFocus = (id == HitSearch || id == HitDashSearch || id == HitSideSearch);

  if (id == HitSideSettings) {
    st.OpenPage(View::Set);
    return;
  }
  if (id == HitSideCollapse || id == HitSideExpand) {
    st.sideCollapsed = !st.sideCollapsed;
    st.sideScroll = 0.0f;
    st.hoverId = -1;
    st.dirty = true;
    return;
  }

  if (id >= HitSideCardBase && id < HitSortKey) {
    int base = id >= HitRecentBase ? HitRecentBase : HitSideCardBase;
    int rel = id - base;
    int pos = rel / kSideCardSlots;
    int slot = rel % kSideCardSlots;
    int idx = -1;
    if (base == HitRecentBase) {
      int rec[kRecentCap];
      int n = RecentHosts(st, rec, kRecentCap);
      if (pos < n) idx = rec[pos];
    } else {
      int vis[kSideCardCap];
      int n = SidebarVisibleHosts(st, vis, kSideCardCap);
      if (pos < n) idx = vis[pos];
    }
    if (idx >= 0 && idx < (int)st.hosts.size()) {
      const Host& h = st.hosts[(size_t)idx];
      if (slot >= 2 && slot <= 5 && h.state != HostState::Live &&
          !HostHasCredential(h)) {
        st.OpenHostForm(idx);
        st.formError = Tr(L"请先设置密码或私钥文件，再连接");
        return;
      }
      if (slot == 0) {
        st.activeHost = idx;
      } else if (slot == 1) {
        st.OpenHostForm(idx);
      } else if (slot == 2) {
        if (h.state == HostState::Live) {
          st.pendingDisconnect = idx;
        } else {
          st.pendingConnect = idx;
          st.pendingConnectTerm = false;
        }
      } else if (slot >= 3 && slot <= 5) {
        bool live = h.sessionId >= 0 && SshService::Instance().IsConnected(h.sessionId);
        if (!live) {
          st.toast = Tr(L"该主机未连接，请先点「连接」");
          st.toastAt = (long long)GetTickCount64();
          st.dirty = true;
          return;
        }
        st.OpenHostView(idx, slot - 3);
        if (slot == 5) st.pendingSftp = 1;
      }
    }
    return;
  }

  if (id == HitSortKey) {
    st.sortKey = st.sortKey == 0 ? 1 : 0;
    st.sideScroll = 0.0f;
    st.dirty = true;
    return;
  }

  if (id == HitSortDir) {
    st.sortAsc = !st.sortAsc;
    st.sideScroll = 0.0f;
    st.dirty = true;
    return;
  }

  if (id == HitTitleClose) {
    if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    else PostQuitMessage(0);
    return;
  }
  if (id == HitTitleMin) {
    if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
    return;
  }
  if (id == HitTitleMax) {
    if (hwnd) ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    return;
  }

  if (id >= HitHostBase && id < HitDashCardBase) {
    int index = id - HitHostBase;
    if (index >= 0 && index < (int)st.hosts.size()) {
      st.OpenHostSession(index);
      st.pendingConnect = index;
    }
    return;
  }

  if (id >= HitTabCloseBase && id < HitHostEditBase) {
    st.CloseTab(id - HitTabCloseBase);
    return;
  }

  if (id >= HitHostEditBase && id < HitSessSegBase) {
    int index = id - HitHostEditBase;
    if (index >= 0 && index < (int)st.hosts.size()) st.OpenHostForm(index);
    return;
  }

  if (id >= HitSessSegBase && id < HitDashToggleBase) {
    int slot = id - HitSessSegBase;
    st.sessTab = slot < 0 ? 0 : (slot > 2 ? 2 : slot);
    if (st.sessTab == 2) st.pendingSftp = 1;
    return;
  }

  if (id >= HitDashToggleBase && id < HitDashQuickBase) {
    int index = id - HitDashToggleBase;
    if (index >= 0 && index < (int)st.hosts.size()) {
      const Host& h = st.hosts[(size_t)index];
      if (h.state == HostState::Live) {
        st.pendingDisconnect = index;
      } else {
        st.pendingConnect = index;
        st.pendingConnectTerm = false;
      }
    }
    return;
  }

  if (id >= HitDashQuickBase && id < HitDashStatBase) {
    int rel = id - HitDashQuickBase;
    int index = rel / 4;
    int slot = rel % 4;
    if (index >= 0 && index < (int)st.hosts.size()) {
      st.OpenHostView(index, slot);
      if (slot == 0) {
        st.pendingConnect = index;
        st.pendingConnectTerm = true;
      }
      if (slot == 2) st.pendingSftp = 1;
    }
    return;
  }

  if (id >= HitTabBase && id < HitTabAdd) {
    int index = id - HitTabBase;
    if (index >= 0 && index < (int)st.tabs.size()) st.ActivateTab(index);
    return;
  }

  if (id == HitTabsRightBase) {
    int sid = st.CurrentSessionId();
    int hid = st.CurrentHostId();
    long long now = (long long)GetTickCount64();
    st.refreshAt = now;
    st.toastAt = now;
    st.toastKind = 0;
    if (sid < 0) {
      st.toast = Tr(L"当前页没有活动会话");
    } else if (!SshService::Instance().IsConnected(sid)) {
      if (hid >= 0) {
        st.pendingConnect = hid;
        st.pendingConnectTerm = false;
      }
      st.toast = Tr(L"会话已断开，正在重连…");
    } else {
      st.tunNextAt = 0;
      st.statsNextAt = 0;
      if (st.view == View::Sftp || (st.view == View::Term && st.sessTab == 2)) st.pendingSftp = 2;
      st.toast = Tr(L"已刷新当前页");
    }
    st.dirty = true;
    return;
  }

  if (id == HitSettings) {
    st.OpenPage(View::Set);
    return;
  }

  if (id == HitNewHost || id == HitDashNewHost) {
    st.OpenHostForm(-1);
    return;
  }

  if (id >= HitDashEditBase && id < HitFtpNew) {
    st.OpenHostForm(id - HitDashEditBase);
    return;
  }

  if (id >= HitDashCardBase && id < HitDashNewHost) {
    int index = id - HitDashCardBase;
    if (index >= 0 && index < (int)st.hosts.size()) {
      st.OpenHostSession(index);
      st.pendingConnect = index;
      st.pendingConnectTerm = true;
    }
    return;
  }

  if (id >= HitTunSwitchBase && id < HitTunRefreshBase) {
    int index = id - HitTunSwitchBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) {
        Tunnel& tn = list[(size_t)index];
        tn.on = !tn.on;
        tn.state = tn.on ? TunState::Retry : TunState::Down;
        tn.fails = 0;
        tn.retryIn = 0;
        tn.nextAt = 0;
        if (!tn.on) {
          int sid = st.CurrentSessionId();
          if (sid >= 0) {
            SshService::Instance().ForwardStopLocal(sid, _wtoi(tn.port.c_str()));
          }
        }
        st.dirty = true;
      }
    }
    return;
  }

  if (id == HitTunNew) {
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    st.OpenTunForm(hi, -1);
    return;
  }

  if (id >= HitTunRefreshBase && id < HitTunEditBase) {
    int index = id - HitTunRefreshBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) {
        Tunnel& tn = list[(size_t)index];
        int sid = st.CurrentSessionId();
        if (sid >= 0) SshService::Instance().ForwardStopLocal(sid, _wtoi(tn.port.c_str()));
        tn.state = TunState::Retry;
        tn.fails = 0;
        tn.retryIn = 0;
        tn.nextAt = 0;
        st.tunNextAt = 0;
        st.refreshAt = (long long)GetTickCount64();
        st.dirty = true;
      }
    }
    return;
  }

  if (id >= HitTunBrowseBase && id < HitTunBrowseBase + 20) {
    int index = id - HitTunBrowseBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) {
        st.pendingTunBrowse = index;
        st.refreshAt = (long long)GetTickCount64();
      }
    }
    return;
  }

  if (id >= HitTunEditBase && id < HitTunDelBase) {
    int index = id - HitTunEditBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) st.OpenTunForm(hi, list[(size_t)index].id);
    }
    return;
  }

  if (id >= HitTunDelBase && id < HitSetNavBase) {
    int index = id - HitTunDelBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) {
        st.formHost = hi;
        st.confirmTunnel = list[(size_t)index].id;
        st.confirmHost = -1;
        st.modal = ModalKind::Confirm;
        st.modalTitle = Tr(L"删除转发");
        st.modalNote = list[(size_t)index].port + L" → " + list[(size_t)index].dest;
        st.confirmNote = Tr(L"删除后立即停止该通道，操作不可撤销");
      }
    }
    return;
  }

  if (id >= HitTunRowBase) {
    int index = id - HitTunRowBase;
    int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
    if (hi >= 0 && hi < (int)st.hosts.size()) {
      std::vector<Tunnel>& list = st.hosts[(size_t)hi].tunnels;
      if (index >= 0 && index < (int)list.size()) st.OpenTunForm(hi, list[(size_t)index].id);
    }
    return;
  }

  if (id == HitFtpNew) {
    st.modal = ModalKind::Prompt;
    st.modalTitle = Tr(L"新建文件夹");
    st.modalNote = st.sftpPath;
    st.promptValue.clear();
    return;
  }

  if (id == HitFtpRefresh) {
    st.pendingSftp = 2;
    st.refreshAt = (long long)GetTickCount64();
    return;
  }

  if (id == HitFtpUp) {
    st.pendingSftp = 3;
    return;
  }

  if (id == HitFtpDown) {
    st.pendingSftp = 4;
    return;
  }

  if (id == HitFtpQueue) {
    st.modal = ModalKind::Queue;
    st.modalTitle = Tr(L"传输队列");
    st.modalNote = st.sftpQueue.empty() ? Tr(L"暂无待传输任务") : Tr(L"按加入顺序串行执行");
    return;
  }

  if (id >= HitFtpRowBase && id < HitFtpRowBase + 40) {
    st.sftpSide = 0;
    st.sftpSelLocal = id - HitFtpRowBase;
    return;
  }

  if (id >= HitFtpRowBase + 40 && id < HitTunNew) {
    st.sftpSide = 1;
    st.sftpSelRemote = id - HitFtpRowBase - 40;
    return;
  }

  if (id >= HitSetNavBase && id < HitSetCtrlBase) {
    st.setNav = id - HitSetNavBase;
    st.dirty = true;
    return;
  }

  if (id >= HitSetCtrlBase && id < HitStatusConn) {
    int row = id - HitSetCtrlBase;
    int kind = SetKindOf(st.setNav, row);
    int optN = SetOptionCount(st.setNav, row);
    if (kind == SetSwatch) {
      int* slot = SetIntSlot(st, st.setNav, row);
      if (slot) *slot = (*slot + 1) % 5;
    } else if (kind == SetSeg || kind == SetDrop) {
      int* slot = SetIntSlot(st, st.setNav, row);
      if (slot && optN > 0) *slot = (*slot + 1) % optN;
    } else if (kind == SetSwitch) {
      bool* flag = SetBoolSlot(st, st.setNav, row);
      if (flag) *flag = !*flag;
    }
    if (st.setNav == 2 && row == 1) {
      static const float kSizes[4] = { 12.0f, 13.0f, 14.0f, 16.0f };
      int si = st.termSizeIdx < 0 ? 1 : (st.termSizeIdx > 3 ? 3 : st.termSizeIdx);
      st.termFontSize = kSizes[si];
    }
    st.dirty = true;
    return;
  }

  if (id == HitStatusConn) {
    st.modal = ModalKind::StatusPanel;
    st.modalTitle = Tr(L"连接详情");
    st.modalNote = Tr(L"当前会话的协议参数与主机指纹");
    return;
  }
}

Rect TunOpRectOf(const AppState& st, const Rect& c, int index, int slot) {
  return TunOpRect(st, c, index, slot);
}

Rect SftpPanelRect(const AppState& st, const Rect& c, int side) {
  float top = c.t + st.S(42.0f);
  float bottom = c.b - st.S(48.0f);
  float mid = c.l + c.W() * kHalf;
  if (side == 0) return Rect{ c.l, top, mid - st.S(2.0f), bottom };
  return Rect{ mid + st.S(2.0f), top, c.r, bottom };
}

Rect SftpRowRectOf(const AppState& st, const Rect& c, int side, int row) {
  Rect panel = SftpPanelRect(st, c, side);
  float top = panel.t + st.S(32.0f) + st.S(36.0f) * (float)row;
  return Rect{ panel.l, top, panel.r, top + st.S(36.0f) };
}

Rect FtpActionRectOf(const AppState& st, const Rect& c, int slot) {
  return FtpActionRect(st, c, slot);
}

Rect DashStatCardRect(const AppState& st, const Rect& c, int index) {
  float left = c.l + st.S(28.0f);
  float right = c.r - st.S(28.0f);
  float gap = st.S(10.0f);
  float w = (right - left - gap * 3.0f) / 4.0f;
  float top = c.t + st.S(100.0f);
  float x = left + (w + gap) * (float)index;
  return Rect{ x, top, x + w, top + st.S(76.0f) };
}

Rect DashRecentCardRect(const AppState& st, const Rect& c, int index) {
  float left = c.l + st.S(28.0f);
  float right = c.r - st.S(28.0f);
  float gap = st.S(10.0f);
  float w = (right - left - gap * 2.0f) / 3.0f;
  float top = c.t + st.S(220.0f);
  float x = left + (w + gap) * (float)index;
  return Rect{ x, top, x + w, top + st.S(96.0f) };
}

Rect DashRecentBtnRect(const AppState& st, const Rect& c, int index) {
  Rect card = DashRecentCardRect(st, c, index);
  float w = st.S(64.0f);
  float h = st.S(24.0f);
  return Rect{ card.r - st.S(12.0f) - w, card.b - st.S(12.0f) - h,
               card.r - st.S(12.0f), card.b - st.S(12.0f) };
}

Rect DashEditRectOf(const AppState& st, const Rect& c, int index) {
  Rect card = DashCardRect(st, c, index);
  float s = st.S(18.0f);
  return Rect{ card.r - st.S(12.0f) - s, card.t + st.S(10.0f), card.r - st.S(12.0f),
               card.t + st.S(10.0f) + s };
}

Rect DashToggleRectOf(const AppState& st, const Rect& c, int index) {
  Rect card = DashCardRect(st, c, index);
  float left = card.l + st.S(14.0f);
  return Rect{ left, card.b - st.S(38.0f), left + st.S(76.0f), card.b - st.S(14.0f) };
}

Rect DashQuickRectOf(const AppState& st, const Rect& c, int index, int slot) {
  Rect card = DashCardRect(st, c, index);
  float w = st.S(48.0f);
  float gap = st.S(6.0f);
  float right = card.r - st.S(14.0f);
  float left = right - w * 3.0f - gap * 2.0f + (w + gap) * (float)slot;
  return Rect{ left, card.b - st.S(38.0f), left + w, card.b - st.S(14.0f) };
}

Rect DashStatRectOf(const AppState& st, const Rect& c, int index, int slot) {
  Rect card = DashCardRect(st, c, index);
  float gap = st.S(8.0f);
  float w = (card.W() - st.S(28.0f) - gap * 2.0f) / 3.0f;
  float left = card.l + st.S(14.0f) + (w + gap) * (float)slot;
  return Rect{ left, card.t + st.S(92.0f), left + w, card.t + st.S(126.0f) };
}

Rect TunRowRectOf(const AppState& st, const Rect& c, int index) { return TunRowRect(st, c, index); }

int TunIndexOfId(const Host& h, int tunnelId) {
  for (size_t i = 0; i < h.tunnels.size(); ++i) {
    if (h.tunnels[i].id == tunnelId) return (int)i;
  }
  return -1;
}

bool ModalIsForm(const AppState& st) {
  return st.modal == ModalKind::HostForm || st.modal == ModalKind::TunForm;
}

void DrawModal(Render& r, AppState& st, const Rect& client) {
  DrawModalImpl(r, st, client);
}

void ModalChar(AppState& st, wchar_t ch) {
  st.selectAll = false;
  if (st.modal == ModalKind::Prompt) {
    if (ch == L'\b') {
      if (!st.promptValue.empty()) st.promptValue.pop_back();
    } else if (ch >= 32 && ch != 127 && st.promptValue.size() < 120) {
      st.promptValue.push_back(ch);
    }
    st.dirty = true;
    return;
  }
  if (!ModalIsForm(st)) return;
  if (st.formFocus < 0 || st.formFocus >= (int)st.form.size()) return;
  FormField& f = st.form[static_cast<size_t>(st.formFocus)];
  if (f.kind == FieldKind::Seg || f.kind == FieldKind::Group || f.kind == FieldKind::Info) return;
  if (ch == L'\b') {
    if (!f.value.empty()) f.value.pop_back();
    f.fresh = false;
  } else if (ch >= 32 && ch != 127) {
    if (f.fresh) {
      f.value.clear();
      f.fresh = false;
    }
    if (f.value.size() < 240) f.value.push_back(ch);
  }
  st.dirty = true;
}

void ModalKey(AppState& st, int vk) {
  if (vk == VK_TAB || vk == VK_UP || vk == VK_DOWN || vk == VK_ESCAPE) st.selectAll = false;
  if (st.modal == ModalKind::None) return;
  if (st.modal == ModalKind::DatePicker) {
    if (vk == VK_ESCAPE) {
      st.modal = st.dateReturn;
      st.dateField = -1;
    } else if (vk == VK_RETURN) {
      st.CommitDatePicker();
    }
    return;
  }
  if (st.modal == ModalKind::Prompt) {
    if (vk == VK_RETURN) {
      st.modal = ModalKind::None;
      st.pendingSftp = 7;
    } else if (vk == VK_ESCAPE) {
      st.modal = ModalKind::None;
    }
    return;
  }
  if (ModalIsForm(st)) {
    int n = (int)st.form.size();
    if (n < 1) return;
    if (vk == VK_TAB || vk == VK_DOWN || vk == VK_UP) {
      int step = vk == VK_UP ? -1 : 1;
      int i = st.formFocus;
      for (int k = 0; k < n; ++k) {
        i = (i + step + n) % n;
        if (FieldVisible(st, i) && st.form[i].kind != FieldKind::Group) break;
      }
      st.formFocus = i;
    } else if (vk == VK_RETURN) {
      st.CommitForm();
    } else if (vk == VK_ESCAPE) {
      st.modal = ModalKind::None;
    }
    return;
  }
  if (vk == VK_ESCAPE || vk == VK_RETURN) st.modal = ModalKind::None;
}

}
