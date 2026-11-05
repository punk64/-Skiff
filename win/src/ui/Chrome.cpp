#include "ui/Chrome.h"

#include <commdlg.h>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/SshService.h"
#include "ui/Sessions.h"
#include "app/Config.h"
#include "compat/Log.h"

namespace sk {
namespace {

constexpr float kHalf = 0.5f;

float MidY(const Rect& rc) { return rc.t + rc.H() * kHalf; }

std::wstring Num(int v) { return std::to_wstring(v); }

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
  ofn.lpstrTitle = L"选择私钥文件";
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

Palette BuildPalette(bool dark) {
  Palette p{};
  p.dark = dark;
  if (dark) {
    p.bg = Rgb(0x08090C);
    p.surface = Rgb(0x14171D);
    p.surface2 = Rgb(0x1C2028);
    p.line = Rgb(0x262C37);
    p.lineStrong = Rgb(0x2E3644);
    p.text = Rgb(0xE8EDF5);
    p.text2 = Rgb(0x98A4B6);
    p.text3 = Rgb(0x78849B);
    p.accent = Rgb(0x3DDC97);
    p.accentFg = Rgb(0x04140D);
    p.accentSoft = Alpha(Rgb(0x3DDC97), 0.13f);
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
    p.accent = Rgb(0x0E9F6E);
    p.accentFg = Rgb(0xFFFFFF);
    p.accentSoft = Alpha(Rgb(0x0E9F6E), 0.10f);
    p.danger = Rgb(0xDC2626);
    p.warn = Rgb(0xB45309);
    p.info = Rgb(0x2563EB);
    p.violet = Rgb(0x7C3AED);
    p.blue = Rgb(0xC2711D);
    p.termBg = Rgb(0x0B0D11);
    p.termText = Rgb(0xD7DEE9);
    p.termDim = Rgb(0x6D7889);
  }
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

float TabsInnerRight(const AppState& st, const Rect& tabs) {
  float x = tabs.l + st.S(6);
  for (size_t i = 0; i < st.tabs.size(); ++i) x += TabWidthOf(st, st.tabs[i]) + st.S(2);
  return x;
}

Rect TabAddRect(const AppState& st, const Rect& tabs) {
  float size = st.S(30);
  float x = TabsInnerRight(st, tabs);
  float limit = tabs.r - st.S(6) - st.S(28) * 2 - st.S(4);
  if (x + size > limit) x = limit - size;
  float top = tabs.t + (tabs.H() - size) * kHalf;
  return Rect{ x, top, x + size, top + size };
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
    const wchar_t* brand = L"轻舟 Skiff";
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
  Rect lg{ s.l + st.S(14), s.t + st.S(16), s.l + st.S(14) + logo, s.t + st.S(16) + logo };
  r.FillRound(lg.D2D(), st.S(9), p.accentSoft);
  DrawSail(r, st, lg.Inset(st.S(9)), p.accent);

  float tx = lg.r + st.S(9);
  float textRight = s.r - st.S(14);
  r.TextVertCenter(L"轻舟", D2D1::RectF(tx, lg.t, textRight, lg.t + lg.H() * 0.60f), p.text,
                   FontId::Body);
  r.TextVertCenter(L"SKIFF · PORTABLE",
                   D2D1::RectF(tx, lg.t + lg.H() * 0.52f, textRight, lg.b), p.text3,
                   FontId::Micro);

  Rect sq = SidebarSearchRect(st, s);
  r.FillRound(sq.D2D(), st.S(L::RCtrl), p.surface2);
  if (st.searchFocus) r.StrokeRound(sq.D2D(), st.S(L::RCtrl), p.accent, st.S(1.2f));
  float iconSize = st.S(12);
  Rect ic{ sq.l + st.S(10), MidY(sq) - iconSize * kHalf, sq.l + st.S(10) + iconSize,
           MidY(sq) + iconSize * kHalf };
  DrawSearchIcon(r, st, ic, p.text3);

  const wchar_t* kbText = L"Ctrl K";
  float kw = r.Measure(kbText, FontId::Micro) + st.S(8);
  float kh = st.S(16);
  Rect kb{ sq.r - st.S(10) - kw, MidY(sq) - kh * kHalf, sq.r - st.S(10), MidY(sq) + kh * kHalf };
  r.StrokeRound(kb.D2D(), st.S(4), p.line, st.S(1));
  r.TextVertCenter(kbText, kb.D2D(), p.text3, FontId::Micro, Align::Center);

  bool hasQuery = !st.query.empty();
  std::wstring hint = hasQuery ? st.query : std::wstring(L"搜索主机…");
  float hintLeft = ic.r + st.S(8);
  if (kb.l - st.S(6) > hintLeft) {
    r.PushClip(D2D1::RectF(hintLeft, sq.t, kb.l - st.S(6), sq.b));
    r.TextVertCenter(hint.c_str(), D2D1::RectF(hintLeft, sq.t, kb.l, sq.b),
                     hasQuery ? p.text : p.text3, FontId::Small);
    r.PopClip();
  }

  int vis[64];
  int visN = SidebarVisibleHosts(st, vis, 64);
  Rect sec = SidebarSectionRect(st, s);
  std::wstring secText = std::wstring(L"主机 · ") + Num(visN);
  r.TextVertCenter(secText.c_str(), sec.D2D(), p.text3, FontId::Micro);

  float foot = SidebarFootTop(st, s);
  for (int k = 0; k < visN; ++k) {
    Rect row = SidebarHostRect(st, s, k);
    if (row.b > foot - st.S(4)) break;
    int i = vis[k];
    const Host& h = st.hosts[i];
    if (i == st.activeHost) {
      r.FillRound(row.D2D(), st.S(L::RCtrl), p.accentSoft);
      Rect edge{ row.l, row.t + st.S(11), row.l + st.S(3), row.b - st.S(11) };
      r.FillRound(edge.D2D(), st.S(1.5f), p.accent);
    }

    float dot = st.S(L::HostDot);
    float cyDot = MidY(row);
    float cxDot = row.l + st.S(9) + dot * kHalf;
    D2D1_COLOR_F dc = p.text3;
    if (h.state == HostState::Live) dc = p.accent;
    if (h.state == HostState::Busy) dc = p.blue;
    if (h.state != HostState::Off) r.Circle(cxDot, cyDot, dot * kHalf + st.S(3), Alpha(dc, 0.13f));
    r.Circle(cxDot, cyDot, dot * kHalf, dc);

    std::wstring chip;
    D2D1_COLOR_F chipFg = p.text3;
    D2D1_COLOR_F chipBg = Alpha(p.text3, 0.12f);
    bool hasChip = true;
    int dl = h.DaysLeft();
    if (h.state == HostState::Live) {
      chip = L"在线";
      chipFg = p.accent;
      chipBg = p.accentSoft;
    } else if (h.state == HostState::Busy) {
      chip = L"连接中";
      chipFg = p.warn;
      chipBg = Alpha(p.warn, 0.12f);
    } else if (dl == INT_MIN) {
      hasChip = false;
    } else if (dl < 0) {
      chip = L"已过期";
      chipFg = p.danger;
      chipBg = Alpha(p.danger, 0.12f);
    } else {
      chip = std::wstring(L"剩 ") + Num(dl) + L" 天";
      chipFg = dl <= 7 ? p.warn : p.text3;
      chipBg = dl <= 7 ? Alpha(p.warn, 0.12f) : Alpha(p.text3, 0.12f);
    }

    bool editHover = i == st.hoverHost;
    Rect chipRc = (editHover || !hasChip) ? HostEditRect(st, s, k) : ChipRectOf(r, st, chip, row);
    if (editHover) {
      DrawChip(r, st, L"编辑", chipRc, p.accent, p.accentSoft);
    } else if (hasChip) {
      DrawChip(r, st, chip, chipRc, chipFg, chipBg);
    }

    float nx = cxDot + dot * kHalf + st.S(10);
    float clipRight = chipRc.l - st.S(6);
    if (clipRight > nx) {
      r.PushClip(D2D1::RectF(nx, row.t, clipRight, row.b));
      r.TextVertCenter(h.name.c_str(), D2D1::RectF(nx, row.t + st.S(10), clipRight, row.t + st.S(28)),
                       p.text, FontId::Body);
      std::wstring addr = h.Addr();
      r.TextVertCenter(addr.c_str(), D2D1::RectF(nx, row.t + st.S(28), clipRight, row.b - st.S(9)),
                       p.text3, FontId::MonoSm);
      r.PopClip();
    }
  }

  r.Fill(Rect{ s.l, foot, s.r, foot + st.S(1) }.D2D(), p.line);
  for (int i = 0; i < 2; ++i) {
    Rect item = SidebarFootItemRect(st, s, i);
    const wchar_t* label = i == 0 ? L"设置" : L"快捷键";
    r.TextVertCenter(label, D2D1::RectF(item.l + st.S(10), item.t, item.r - st.S(10), item.b),
                     p.text2, FontId::Body);
    if (i == 0) {
      const wchar_t* hintText = L"Ctrl ,";
      r.TextVertCenter(hintText, D2D1::RectF(item.l, item.t, item.r - st.S(10), item.b),
                       p.text3, FontId::Micro, Align::Right);
    }
  }
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

  Rect add = TabAddRect(st, tabs);
  float plus = st.S(6);
  float acx = add.l + add.W() * kHalf;
  float acy = MidY(add);
  r.Line(acx - plus, acy, acx + plus, acy, p.text3, st.S(1.4f));
  r.Line(acx, acy - plus, acx, acy + plus, p.text3, st.S(1.4f));

  Rect refresh = TabsRightButtonRect(st, tabs, 0);
  DrawRefreshIcon(r, st, refresh.Inset(st.S(7)), p.text3);
  Rect more = TabsRightButtonRect(st, tabs, 1);
  float mcx = more.l + more.W() * kHalf;
  float mcy = MidY(more);
  float dotR = st.S(1.3f);
  float step = st.S(5);
  r.Circle(mcx - step, mcy, dotR, p.text3);
  r.Circle(mcx, mcy, dotR, p.text3);
  r.Circle(mcx + step, mcy, dotR, p.text3);

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
    conn = L"已连接";
  } else if (ss == SshState::Connecting) {
    dotColor = p.warn;
    textColor = p.warn;
    conn = L"连接中";
  } else if (ss == SshState::Failed) {
    dotColor = p.danger;
    textColor = p.danger;
    conn = L"连接失败";
  } else {
    conn = L"未连接";
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
  left[1] = ss == SshState::Connected ? std::wstring(L"aes256-ctr") : std::wstring(L"—");
  left[2] = ss == SshState::Connected ? std::wstring(L"chacha20-poly1305") : std::wstring(L"—");
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
  right[0] = L"v1.0.0";
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
  float fixedW = st.S(108.0f) + st.S(72.0f) + st.S(84.0f) + st.S(62.0f) + st.S(76.0f);
  float unit = (inner - fixedW) / 2.6f;
  if (unit < st.S(36.0f)) unit = st.S(36.0f);
  float cw[7] = { st.S(108.0f), st.S(72.0f), unit * 1.1f, unit * 1.5f,
                  st.S(84.0f), st.S(62.0f), st.S(76.0f) };
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

int ModalRowCount(const AppState& st) {
  if (IsFormModal(st)) return (int)st.form.size();
  if (st.modal == ModalKind::StatusPanel) return 7;
  if (st.modal == ModalKind::Help) return 7;
  if (st.modal == ModalKind::Queue) return st.sftpQueue.empty() ? 1 : (int)st.sftpQueue.size();
  if (st.modal == ModalKind::KeyStore) return st.keyStore.empty() ? 1 : (int)st.keyStore.size();
  if (st.modal == ModalKind::Prompt) return 1;
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
  for (int i = 0; i < n; ++i) h += ModalRowHAt(st, i);
  return h;
}

Rect ModalPanel(const AppState& st, const Rect& client) {
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
  float top = panel.t + st.S(64.0f) - st.formScroll;
  for (int k = 0; k < i; ++k) top += ModalRowHAt(st, k);
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

void DrawModalImpl(Render& r, AppState& st, const Rect& client) {
  if (st.modal == ModalKind::None) return;
  const Palette& p = PaletteFor(st.dark);
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
      float boxRight = browse ? valRight - st.S(64.0f) : valRight;
      Rect box{ valLeft, row.t + st.S(9.0f), boxRight, row.b - st.S(9.0f) };
      r.FillRound(box.D2D(), st.S(L::RCtrl), p.surface2);
      if (focus) r.StrokeRound(box.Inset(st.S(0.5f)).D2D(), st.S(L::RCtrl), p.accent, st.S(1.5f));
      std::wstring shown = f.value;
      if (f.kind == FieldKind::Pass && !shown.empty()) shown.assign(shown.size(), L'\x2022');
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
      if (browse) {
        Rect br{ boxRight + st.S(8.0f), box.t, valRight, box.b };
        r.StrokeRound(br.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
        r.TextVertCenter(L"浏览…", br.D2D(), p.text2, FontId::Small, Align::Center);
      }
    }
    r.PopClip();
    if (!st.formError.empty()) {
      r.TextVertCenter(st.formError.c_str(),
                       D2D1::RectF(panel.l + st.S(22.0f), panel.b - st.S(54.0f),
                                   panel.r - st.S(120.0f), panel.b - st.S(28.0f)),
                       p.danger, FontId::Micro);
    }
  } else if (st.modal == ModalKind::Confirm) {
    Rect body = ModalBodyRect(st, panel);
    r.TextVertCenter(st.confirmNote.c_str(), body.D2D(), p.text2, FontId::Body, Align::Center);
  } else if (st.modal == ModalKind::KeyStore) {
    Rect body = ModalBodyRect(st, panel);
    r.PushClip(D2D1::RectF(body.l, body.t, body.r, body.b));
    if (st.keyStore.empty()) {
      r.TextVertCenter(L"keys 目录为空，可放置私钥文件后重开", body.D2D(), p.text3, FontId::Body,
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
    std::wstring shown = st.promptValue.empty() ? std::wstring(L"输入名称") : st.promptValue;
    r.TextVertCenter(shown.c_str(),
                     D2D1::RectF(box.l + st.S(10.0f), box.t, box.r - st.S(8.0f), box.b),
                     st.promptValue.empty() ? p.text3 : p.text, FontId::Body);
  } else if (st.modal == ModalKind::StatusPanel) {
    std::wstring keys[7] = { L"主机", L"地址", L"认证", L"加密", L"密钥交换", L"延迟", L"主机指纹" };
    std::wstring vals[7];
    const Host* h = nullptr;
    if (st.activeHost >= 0 && st.activeHost < (int)st.hosts.size()) {
      h = &st.hosts[static_cast<size_t>(st.activeHost)];
    }
    vals[0] = h ? h->name : L"—";
    vals[1] = h ? h->Addr() : std::wstring(L"—");
    vals[2] = h ? h->auth : L"—";
    vals[3] = L"aes256-ctr";
    vals[4] = L"curve25519-sha256";
    vals[5] = st.statusLatency >= 0 ? std::to_wstring(st.statusLatency) + L" ms" : L"—";
    vals[6] = st.statusKey.empty() ? std::wstring(L"未记录") : st.statusKey;
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
  } else if (st.modal == ModalKind::Help) {
    const wchar_t* keys[7] = { L"Ctrl + K", L"Ctrl + 1", L"Ctrl + 2", L"Ctrl + 3",
                               L"Ctrl + 4", L"Ctrl + 0", L"F11" };
    const wchar_t* vals[7] = { L"搜索主机", L"终端视图", L"SFTP 视图", L"端口转发",
                               L"设置", L"主机台", L"全屏 / 退出全屏" };
    for (int i = 0; i < 7; ++i) {
      Rect row = ModalFieldRect(st, panel, i);
      r.PushClip(D2D1::RectF(row.l, row.t, row.l + st.S(112.0f), row.b));
      r.TextVertCenter(keys[i], D2D1::RectF(row.l, row.t, row.l + st.S(112.0f), row.b), p.accent,
                       FontId::MonoSm);
      r.PopClip();
      r.TextVertCenter(vals[i], D2D1::RectF(row.l + st.S(112.0f), row.t, row.r, row.b), p.text2,
                       FontId::Body);
    }
  } else if (st.modal == ModalKind::Queue) {
    if (st.sftpQueue.empty()) {
      Rect row = ModalFieldRect(st, panel, 0);
      r.TextVertCenter(L"传输队列为空", row.D2D(), p.text3, FontId::Body, Align::Center);
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
    r.TextVertCenter(L"保存", ok.D2D(), p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancel.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(L"取消", cancel.D2D(), p.text2, FontId::Body, Align::Center);
    if (st.modal == ModalKind::HostForm) {
      Rect ks{ panel.l + st.S(22.0f), ok.t, panel.l + st.S(22.0f) + st.S(84.0f), ok.b };
      r.StrokeRound(ks.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
      r.TextVertCenter(L"密钥库", ks.D2D(), p.text2, FontId::Body, Align::Center);
    }
  } else if (st.modal == ModalKind::Confirm) {
    Rect ok = ModalOkRect(st, panel);
    Rect cancel = ModalCancelRect(st, panel);
    bool danger = st.confirmHost >= 0 || st.confirmTunnel >= 0;
    r.FillRound(ok.D2D(), st.S(L::RBtn), danger ? p.danger : p.accent);
    r.TextVertCenter(danger ? L"删除" : L"确定", ok.D2D(),
                     danger ? Rgb(0xFFFFFF) : p.accentFg, FontId::Body, Align::Center);
    r.StrokeRound(cancel.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    r.TextVertCenter(L"取消", cancel.D2D(), p.text2, FontId::Body, Align::Center);
  } else {
    Rect ok = ModalOkRect(st, panel);
    r.FillRound(ok.D2D(), st.S(L::RBtn), p.surface2);
    r.TextVertCenter(L"关闭", ok.D2D(), p.text2, FontId::Body, Align::Center);
  }
}

}

const Palette& DarkPalette() {
  static const Palette p = BuildPalette(true);
  return p;
}

const Palette& LightPalette() {
  static const Palette p = BuildPalette(false);
  return p;
}

const Palette& PaletteFor(bool dark) { return dark ? DarkPalette() : LightPalette(); }

void AppState::Seed() {
  hosts.clear();
  tabs.clear();
  nextHostId = 1;
  nextTunnelId = 1;

  auto addHost = [this](const wchar_t* name, const wchar_t* user, const wchar_t* host, int port,
                        const wchar_t* auth, int expireDays, bool autoConn) {
    Host h;
    h.id = nextHostId++;
    h.name = name;
    h.user = user;
    h.host = host;
    h.port = port;
    h.auth = auth;
    h.registeredAt = StampPlusDays(-120);
    h.expireAt = expireDays == 0 ? std::wstring() : StampPlusDays(expireDays);
    h.autoConnect = autoConn;
    hosts.push_back(h);
  };

  addHost(L"生产机", L"demo", L"192.0.2.10", 22, L"密钥", 300, false);
  addHost(L"测试环境", L"demo", L"192.0.2.24", 2222, L"密码", 60, true);
  addHost(L"跳板机", L"demo", L"example.com", 22, L"密钥", 5, false);
  addHost(L"构建节点", L"demo", L"192.0.2.31", 22, L"密钥", 120, true);
  addHost(L"归档服务器", L"demo", L"192.0.2.48", 2200, L"密码", 0, false);

  Tab home;
  home.view = (int)View::Dash;
  home.title = L"主机台";
  home.icon = (int)View::Dash;
  tabs.push_back(home);
  activeTab = 0;

  statusHost = L"生产机";

  auto addTun = [this](int hi, const wchar_t* type, const wchar_t* port, const wchar_t* dest,
                       bool on) {
    if (hi < 0 || hi >= (int)hosts.size()) return;
    Tunnel t;
    t.id = nextTunnelId++;
    t.hostId = hosts[(size_t)hi].id;
    t.type = type;
    t.port = port;
    t.dest = dest;
    t.on = on;
    t.state = TunState::Down;
    hosts[(size_t)hi].tunnels.push_back(t);
  };
  addTun(0, L"L", L"8080", L"192.0.2.10:80", true);
  addTun(0, L"L", L"6379", L"192.0.2.10:6379", false);
  addTun(1, L"L", L"5432", L"192.0.2.24:5432", true);
  addTun(2, L"L", L"9200", L"192.0.2.31:9200", true);
  addTun(3, L"R", L"2222", L"127.0.0.1:22", true);

  sftpPath = L"/";
  sftpStatus = L"未连接";
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
    case View::Tun: return L"端口转发";
    case View::Set: return L"设置";
    default: return L"主机台";
  }
}

int AppState::OpenHostSession(int hostId) {
  if (hostId < 0 || hostId >= (int)hosts.size()) return -1;
  int found = FindTabByHost(hostId);
  if (found >= 0) {
    ActivateTab(found);
    return tabs[(size_t)found].sessionId;
  }
  Tab t;
  t.view = (int)View::Term;
  t.hostId = hostId;
  t.sessionId = nextSessionId++;
  t.title = hosts[(size_t)hostId].name + L" · 终端";
  t.icon = (int)View::Term;
  tabs.push_back(t);
  activeHost = hostId;
  activeTab = (int)tabs.size() - 1;
  view = View::Term;
  SessionHub::Instance().Ensure(t.sessionId);
  return t.sessionId;
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
}

void AppState::ActivateTab(int index) {
  if (index < 0 || index >= (int)tabs.size()) return;
  activeTab = index;
  view = (View)tabs[(size_t)index].view;
  if (tabs[(size_t)index].hostId >= 0) activeHost = tabs[(size_t)index].hostId;
}

void AppState::CloseTab(int index) {
  if (index < 0 || index >= (int)tabs.size()) return;
  int sid = tabs[(size_t)index].sessionId;
  if (sid >= 0) {
    SshService::Instance().Disconnect(sid);
    SessionHub::Instance().Close(sid);
    int hid = tabs[(size_t)index].hostId;
    if (hid >= 0 && hid < (int)hosts.size()) {
      hosts[(size_t)hid].state = HostState::Off;
      hosts[(size_t)hid].latency = -1;
    }
  }
  tabs.erase(tabs.begin() + index);
  if (tabs.empty()) {
    Tab home;
    home.view = (int)View::Dash;
    home.title = L"主机台";
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

void AppState::RemoveHost(int hostId) {
  if (hostId < 0 || hostId >= (int)hosts.size()) return;
  for (int i = (int)tabs.size() - 1; i >= 0; --i) {
    if (tabs[(size_t)i].hostId == hostId) CloseTab(i);
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
  modalTitle = L"密钥库";
  modalNote = L"取自程序目录 keys\\，选中即填入私钥路径";
}

void AppState::ResetFingerprint(int hostIndex) {
  if (hostIndex < 0 || hostIndex >= (int)hosts.size()) return;
  hosts[(size_t)hostIndex].hostKey.clear();
  hosts[(size_t)hostIndex].hostKeyType.clear();
  dirty = true;
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

  form.push_back({ L"g1", L"基本信息", L"", L"", FieldKind::Group });
  form.push_back({ L"name", L"名称", h.name, L"例如 生产机", FieldKind::Text });
  form.push_back({ L"host", L"主机", h.host, L"域名或 IP", FieldKind::Text });
  form.push_back({ L"port", L"端口", edit ? std::to_wstring(h.port) : std::wstring(L"22"),
                   L"默认 22", FieldKind::Text });
  form.push_back({ L"user", L"用户名", edit ? h.user : std::wstring(L"root"), L"登录用户",
                   FieldKind::Text });

  form.push_back({ L"g2", L"认证", L"", L"", FieldKind::Group });
  form.push_back({ L"auth", L"认证方式", L"", L"", FieldKind::Seg,
                   { L"密码", L"密钥" }, authOpt });
  form.push_back({ L"password", L"密码", h.password, L"随配置保存在本机", FieldKind::Pass });
  form.push_back({ L"keyPath", L"私钥文件", h.keyPath, L"留空则自动查找 keys 目录",
                   FieldKind::File });
  form.push_back({ L"keyPass", L"私钥口令", h.keyPass, L"私钥无口令可留空", FieldKind::Pass });

  form.push_back({ L"g3", L"时间与自动化", L"", L"", FieldKind::Group });
  form.push_back({ L"registeredAt", L"注册时间", h.registeredAt, L"YYYY-MM-DD HH:mm:ss",
                   FieldKind::Text });
  form.push_back({ L"expireAt", L"到期时间", h.expireAt, L"留空表示不限", FieldKind::Text });
  form.push_back({ L"autoConnect", L"启动连接", L"", L"", FieldKind::Seg,
                   { L"关闭", L"开启" }, h.autoConnect ? 1 : 0 });
  if (edit && !h.hostKey.empty()) {
    form.push_back({ L"hostKey", L"主机指纹", h.hostKey, L"重置后下次连接重新记录",
                     FieldKind::Info });
  }

  formFocus = 1;
  modal = ModalKind::HostForm;
  modalTitle = edit ? L"编辑主机" : L"新建主机";
  modalNote = L"配置写入程序同目录 skiff_store.json，不写注册表";
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

  form.push_back({ L"g1", L"转发规则", L"", L"", FieldKind::Group });
  form.push_back({ L"tunType", L"类型", L"", L"", FieldKind::Seg,
                   { L"本地 L", L"远程 R" }, tv.IsRemote() ? 1 : 0 });
  form.push_back({ L"tunPort", L"本地端口", tv.port, L"例如 8080", FieldKind::Text });
  form.push_back({ L"tunDest", L"目标地址", tv.dest, L"例如 192.0.2.10:80", FieldKind::Text });
  form.push_back({ L"tunHost", L"所属主机", hi >= 0 && hi < (int)hosts.size()
                                               ? hosts[(size_t)hi].name : std::wstring(L"未选择"),
                   L"转发经由的主机", FieldKind::Info });

  formFocus = 1;
  modal = ModalKind::TunForm;
  modalTitle = edit ? L"编辑转发" : L"新建转发";
  modalNote = L"保存后立即进入巡检队列";
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
      formError = L"请填写名称";
      return false;
    }
    if (host.empty()) {
      formError = L"请填写主机地址";
      return false;
    }
    for (size_t i = 0; i < host.size(); ++i) {
      if (host[i] == L' ' || host[i] == L'@' || host[i] == L':') {
        formError = L"主机地址不能包含空格、@ 或冒号，端口请填在端口栏";
        return false;
      }
    }
    int port = 22;
    if (!portText.empty()) {
      port = _wtoi(portText.c_str());
      if (port < 1 || port > 65535) {
        formError = L"端口需在 1 ~ 65535 之间";
        return false;
      }
    }
    if (user.empty()) {
      formError = L"请填写用户名";
      return false;
    }
    if (!reg.empty() && StampMillis(reg) < 0) {
      formError = L"注册时间格式应为 YYYY-MM-DD HH:mm:ss";
      return false;
    }
    if (!exp.empty() && StampMillis(exp) < 0) {
      formError = L"到期时间格式应为 YYYY-MM-DD HH:mm:ss";
      return false;
    }
    if (!reg.empty() && !exp.empty() && StampMillis(exp) < StampMillis(reg)) {
      formError = L"到期时间早于注册时间";
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
          tabs[i].title = h.name + L" · 终端";
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
      formError = L"请填写本地端口";
      return false;
    }
    int port = _wtoi(portText.c_str());
    if (port < 1 || port > 65535) {
      formError = L"本地端口需在 1 ~ 65535 之间";
      return false;
    }
    if (dest.empty()) {
      formError = L"请填写目标地址";
      return false;
    }
    int hi = formHost >= 0 ? formHost : ActiveHostIndex();
    if (hi < 0 || hi >= (int)hosts.size()) {
      formError = L"请先创建一台主机";
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
    case View::Dash: return L"主机台";
    case View::Term: return L"终端";
    case View::Sftp: return L"SFTP";
    case View::Tun: return L"端口转发";
    case View::Set: return L"设置";
  }
  return L"主机台";
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
  return n;
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
  float sideW = st.S(st.sidebarW);
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
  const Palette& p = PaletteFor(st.dark);
  r.Fill(rc.D2D(), p.bg);
  Rect bar{ rc.l, rc.t, rc.r, rc.t + st.S(42.0f) };
  r.Fill(bar.D2D(), p.surface);
  r.Fill(D2D1::RectF(bar.l, bar.b - st.S(1.0f), bar.r, bar.b), p.line);
  const wchar_t* names[3] = { L"终端", L"隧道", L"文件" };
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
  const Palette& p = st.dark ? DarkPalette() : LightPalette();
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
        if (f.kind == FieldKind::File) {
          Rect br{ row.r - st.S(64.0f), row.t + st.S(9.0f), row.r, row.b - st.S(9.0f) };
          if (br.Hit(x, y)) return HitFormBrowseBase + i;
        }
        return HitFormFieldBase + i;
      }
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

    if (SidebarSearchRect(st, sidebar).Hit(x, y)) return HitSearch;

    float foot = SidebarFootTop(st, sidebar);
    int vis[64];
    int visN = SidebarVisibleHosts(st, vis, 64);
    for (int k = 0; k < visN; ++k) {
      Rect row = SidebarHostRect(st, sidebar, k);
      if (row.b > foot - st.S(4)) break;
      if (!row.Hit(x, y)) continue;
      if (st.hoverHost == vis[k] && HostEditRect(st, sidebar, k).Hit(x, y)) {
        return HitHostEditBase + vis[k];
      }
      return HitHostBase + vis[k];
    }
    if (SidebarFootItemRect(st, sidebar, 0).Hit(x, y)) return HitSettings;
    if (SidebarFootItemRect(st, sidebar, 1).Hit(x, y)) return HitHelp;

    for (int i = 0; i < (int)st.tabs.size(); ++i) {
      if (!TabRectOf(st, tabs, i).Hit(x, y)) continue;
      if (TabCloseRect(st, tabs, i).Hit(x, y)) return HitTabCloseBase + i;
      return HitTabBase + i;
    }
    if (TabAddRect(st, tabs).Hit(x, y)) return HitTabAdd;
    for (int i = 0; i < 2; ++i) {
      if (TabsRightButtonRect(st, tabs, i).Hit(x, y)) return HitTabsRightBase + i;
    }
  }

  if (content.Hit(x, y)) {
    if (st.view == View::Dash) {
      if (DashNewHostRect(st, content).Hit(x, y)) return HitDashNewHost;
      for (int i = 0; i < (int)st.hosts.size(); ++i) {
        if (!DashCardRect(st, content, i).Hit(x, y)) continue;
        if (DashEditRectOf(st, content, i).Hit(x, y)) return HitDashEditBase + i;
        if (DashToggleRectOf(st, content, i).Hit(x, y)) return HitDashToggleBase + i;
        for (int k = 0; k < 3; ++k) {
          if (DashQuickRectOf(st, content, i, k).Hit(x, y)) return HitDashQuickBase + i * 4 + k;
        }
        return HitDashCardBase + i;
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
          for (int k = 0; k < 3; ++k) {
            if (TunOpRect(st, inner, i, k).Hit(x, y)) {
              if (k == 0) return HitTunRefreshBase + i;
              if (k == 1) return HitTunEditBase + i;
              return HitTunDelBase + i;
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
  if (st.modal != ModalKind::None && id != HitNone) {
    if (id == HitModalBackdrop || id == HitModalClose || id == HitModalCancel) {
      if (st.modal == ModalKind::KeyStore && st.keyReturn != ModalKind::None) {
        st.modal = st.keyReturn;
        st.keyReturn = ModalKind::None;
        st.modalTitle = st.modal == ModalKind::HostForm ? L"编辑主机" : L"新建主机";
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
        st.modalTitle = st.formTarget >= 0 ? L"编辑主机" : L"新建主机";
        st.modalNote = L"已从密钥库填入私钥路径";
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
      }
      return;
    }
    if (id >= HitFormBrowseBase && id < HitModalKeyStore) {
      int i = id - HitFormBrowseBase;
      if (i < 0 || i >= (int)st.form.size()) return;
      st.formFocus = i;
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
      }
      return;
    }
    return;
  }

  if (id == HitNone) {
    st.searchFocus = false;
    return;
  }

  st.searchFocus = (id == HitSearch);

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
      if (slot == 0) st.pendingConnect = index;
      if (slot == 2) st.pendingSftp = 1;
    }
    return;
  }

  if (id >= HitTabBase && id < HitTabAdd) {
    int index = id - HitTabBase;
    if (index >= 0 && index < (int)st.tabs.size()) st.ActivateTab(index);
    return;
  }

  if (id == HitTabAdd) {
    Tab t;
    t.view = (int)View::Term;
    t.sessionId = st.nextSessionId++;
    t.title = L"新会话";
    t.icon = (int)View::Term;
    st.tabs.push_back(t);
    st.activeTab = (int)st.tabs.size() - 1;
    st.view = View::Term;
    SessionHub::Instance().Ensure(t.sessionId);
    return;
  }

  if (id == HitTabsRightBase) {
    int sid = st.CurrentSessionId();
    int hid = st.CurrentHostId();
    if (sid >= 0 && hid >= 0) {
      if (SshService::Instance().IsConnected(sid)) SshService::Instance().Disconnect(sid);
      SessionHub::Instance().Ensure(sid).Reset(80, 24);
      st.pendingConnect = hid;
    }
    return;
  }

  if (id == HitTabsRightBase + 1) {
    st.modal = ModalKind::Help;
    st.modalTitle = L"快捷键";
    st.modalNote = L"窗口内全局生效";
    return;
  }

  if (id == HitSettings) {
    st.OpenPage(View::Set);
    return;
  }

  if (id == HitHelp) {
    st.OpenPage(View::Set);
    st.setNav = 5;
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
        st.dirty = true;
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
        st.modalTitle = L"删除转发";
        st.modalNote = list[(size_t)index].port + L" → " + list[(size_t)index].dest;
        st.confirmNote = L"删除后立即停止该通道，操作不可撤销";
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
    st.modalTitle = L"新建文件夹";
    st.modalNote = st.sftpPath;
    st.promptKind = 0;
    st.promptValue.clear();
    return;
  }

  if (id == HitFtpRefresh) {
    st.pendingSftp = 2;
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
    st.modalTitle = L"传输队列";
    st.modalNote = st.sftpQueue.empty() ? L"暂无待传输任务" : L"按加入顺序串行执行";
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
    st.dirty = true;
    return;
  }

  if (id == HitStatusConn) {
    st.modal = ModalKind::StatusPanel;
    st.modalTitle = L"连接详情";
    st.modalNote = L"当前会话的协议参数与主机指纹";
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
  if (st.modal == ModalKind::None) return;
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
    if (vk == VK_TAB || vk == VK_DOWN) {
      st.formFocus = (st.formFocus + 1) % n;
    } else if (vk == VK_UP) {
      st.formFocus = (st.formFocus + n - 1) % n;
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
