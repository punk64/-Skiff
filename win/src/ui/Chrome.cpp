#include "ui/Chrome.h"

#include <string>
#include <vector>

namespace sk {
namespace {

constexpr float kHalf = 0.5f;

float MidY(const Rect& rc) { return rc.t + rc.H() * kHalf; }

std::wstring Num(int v) { return std::to_wstring(v); }

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
  int n = (int)((avail + st.S(10)) / (st.S(250) + st.S(10)));
  if (n < 1) n = 1;
  if (n > 4) n = 4;
  return n;
}

Rect DashCardRect(const AppState& st, const Rect& c, int index) {
  int cols = DashCols(st, c);
  float gap = st.S(10);
  float avail = c.W() - st.S(56);
  float w = (avail - gap * (float)(cols - 1)) / (float)cols;
  float top = DashCardsTop(st, c) + (float)(index / cols) * (st.S(96) + gap);
  float left = c.l + st.S(28) + (float)(index % cols) * (w + gap);
  return Rect{ left, top, left + w, top + st.S(96) };
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

Rect TunSwitchRect(const AppState& st, const Rect& c, int index) {
  Rect row = TunRowRect(st, c, index);
  float w = st.S(34);
  float h = st.S(20);
  float right = row.r - st.S(14) - st.S(76) - st.S(10);
  return Rect{ right - w, MidY(row) - h * kHalf, right, MidY(row) + h * kHalf };
}

Rect SetNavRect(const AppState& st, const Rect& c, int index) {
  float top = c.t + st.S(14) + st.S(36) * (float)index;
  return Rect{ c.l + st.S(8), top, c.l + st.S(170) - st.S(8), top + st.S(34) };
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

  Rect sec = SidebarSectionRect(st, s);
  std::wstring secText = std::wstring(L"主机 · ") + Num((int)st.hosts.size());
  r.TextVertCenter(secText.c_str(), sec.D2D(), p.text3, FontId::Micro);

  float foot = SidebarFootTop(st, s);
  for (int i = 0; i < (int)st.hosts.size(); ++i) {
    Rect row = SidebarHostRect(st, s, i);
    if (row.b > foot - st.S(4)) break;
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
    if (h.state == HostState::Live) {
      chip = L"在线";
      chipFg = p.accent;
      chipBg = p.accentSoft;
    } else if (h.state == HostState::Busy && h.daysLeft < 0) {
      chip = L"连接中";
      chipFg = p.warn;
      chipBg = Alpha(p.warn, 0.12f);
    } else if (h.daysLeft >= 0) {
      chip = std::wstring(L"剩 ") + Num(h.daysLeft) + L" 天";
      chipFg = p.warn;
      chipBg = Alpha(p.warn, 0.12f);
    } else {
      chip = L"已过期";
      chipFg = p.danger;
      chipBg = Alpha(p.danger, 0.12f);
    }

    Rect chipRc = ChipRectOf(r, st, chip, row);
    DrawChip(r, st, chip, chipRc, chipFg, chipBg);

    float nx = cxDot + dot * kHalf + st.S(10);
    float clipRight = chipRc.l - st.S(6);
    if (clipRight > nx) {
      r.PushClip(D2D1::RectF(nx, row.t, clipRight, row.b));
      r.TextVertCenter(h.name.c_str(), D2D1::RectF(nx, row.t + st.S(10), clipRight, row.t + st.S(28)),
                       p.text, FontId::Body);
      r.TextVertCenter(h.addr.c_str(), D2D1::RectF(nx, row.t + st.S(28), clipRight, row.b - st.S(9)),
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
    float closeLeft = tab.r - st.S(10) - st.S(17);
    if (closeLeft - st.S(6) > titleLeft) {
      r.PushClip(D2D1::RectF(titleLeft, tab.t, closeLeft - st.S(4), tab.b));
      r.TextVertCenter(st.tabs[i].title.c_str(),
                       D2D1::RectF(titleLeft, tab.t, closeLeft, tab.b), fg, FontId::Small);
      r.PopClip();
    }

    Rect xr{ closeLeft, MidY(tab) - st.S(17) * kHalf, closeLeft + st.S(17),
             MidY(tab) + st.S(17) * kHalf };
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

  float cy = MidY(status);
  float x = status.l + st.S(12);
  float dot = st.S(6);
  r.Circle(x + dot * kHalf, cy, dot * kHalf, p.accent);
  x += dot + st.S(6);

  std::wstring conn = std::wstring(L"已连接 · ") + st.statusHost;
  float connW = r.Measure(conn.c_str(), FontId::MonoSm);
  r.TextVertCenter(conn.c_str(), D2D1::RectF(x, status.t, x + connW + st.S(2), status.b), p.accent,
                   FontId::MonoSm);
  x += connW;

  std::wstring left[3];
  left[0] = Num(st.statusLatency) + L" ms";
  left[1] = L"aes256-ctr";
  left[2] = L"chacha20-poly1305";
  for (int i = 0; i < 3; ++i) {
    x += st.S(14);
    r.Fill(Rect{ x, status.t + st.S(6), x + st.S(1), status.b - st.S(6) }.D2D(), p.line);
    x += st.S(14);
    float w = r.Measure(left[i].c_str(), FontId::MonoSm);
    r.TextVertCenter(left[i].c_str(), D2D1::RectF(x, status.t, x + w + st.S(2), status.b), p.text3,
                     FontId::MonoSm);
    x += w;
  }

  const wchar_t* right[4] = { L"v1.0.0", L"UTF-8", L"80×24", L"SHA256:9k2L…Qm7E" };
  float rx = status.r - st.S(12);
  for (int i = 0; i < 4; ++i) {
    float w = r.Measure(right[i], FontId::MonoSm);
    r.TextVertCenter(right[i], D2D1::RectF(status.l, status.t, rx, status.b), p.text3,
                     FontId::MonoSm, Align::Right);
    rx -= w;
    if (i < 3) {
      rx -= st.S(14);
      r.Fill(Rect{ rx, status.t + st.S(6), rx + st.S(1), status.b - st.S(6) }.D2D(), p.line);
      rx -= st.S(14);
    }
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

  Host h1;
  h1.name = L"生产机";
  h1.addr = L"demo@192.0.2.10:22";
  h1.auth = L"密钥";
  h1.state = HostState::Live;
  h1.daysLeft = 90;
  h1.latency = 12;
  hosts.push_back(h1);

  Host h2;
  h2.name = L"测试环境";
  h2.addr = L"demo@192.0.2.24:2222";
  h2.auth = L"密码";
  h2.state = HostState::Live;
  h2.daysLeft = 30;
  h2.autoConnect = true;
  h2.latency = 28;
  hosts.push_back(h2);

  Host h3;
  h3.name = L"跳板机";
  h3.addr = L"demo@example.com:22";
  h3.auth = L"密钥";
  h3.state = HostState::Busy;
  h3.daysLeft = 5;
  hosts.push_back(h3);

  Host h4;
  h4.name = L"构建节点";
  h4.addr = L"demo@192.0.2.31:22";
  h4.auth = L"密钥";
  h4.state = HostState::Off;
  h4.daysLeft = 45;
  h4.autoConnect = true;
  hosts.push_back(h4);

  Host h5;
  h5.name = L"归档服务器";
  h5.addr = L"demo@192.0.2.48:2200";
  h5.auth = L"密码";
  h5.state = HostState::Off;
  h5.daysLeft = -1;
  hosts.push_back(h5);

  Tab t1;
  t1.view = (int)View::Dash;
  t1.title = L"主机台";
  t1.icon = (int)View::Dash;
  tabs.push_back(t1);

  Tab t2;
  t2.view = (int)View::Term;
  t2.title = L"生产机 · 终端";
  t2.icon = (int)View::Term;
  t2.live = true;
  tabs.push_back(t2);

  Tab t3;
  t3.view = (int)View::Sftp;
  t3.title = L"跳板机 · SFTP";
  t3.icon = (int)View::Sftp;
  tabs.push_back(t3);

  Tab t4;
  t4.view = (int)View::Tun;
  t4.title = L"端口转发";
  t4.icon = (int)View::Tun;
  tabs.push_back(t4);

  statusHost = L"生产机";
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

void LayoutMetrics(const AppState& st, const Rect& client, Rect& sidebar, Rect& tabs, Rect& content,
                   Rect& status) {
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

void DrawAll(Render& r, AppState& st, const Rect& client) {
  const Palette& p = st.dark ? DarkPalette() : LightPalette();
  r.Fill(client.D2D(), p.bg);

  Rect sidebar, tabs, content, status;
  LayoutMetrics(st, client, sidebar, tabs, content, status);

  DrawTitleBar(r, st, client, p);
  DrawSidebar(r, st, sidebar, p);
  DrawTabs(r, st, tabs, p);

  switch (st.view) {
    case View::Dash: DrawDash(r, st, content); break;
    case View::Term: DrawTermView(r, st, content); break;
    case View::Sftp: DrawSftp(r, st, content); break;
    case View::Tun: DrawTun(r, st, content); break;
    case View::Set: DrawSet(r, st, content); break;
  }

  DrawStatus(r, st, status, p);
}

int HitTest(const AppState& st, const Rect& client, float x, float y) {
  Rect sidebar, tabs, content, status;
  LayoutMetrics(st, client, sidebar, tabs, content, status);

  for (int i = 0; i < 3; ++i) {
    if (TitleButtonRect(st, client, i).Hit(x, y)) return HitTitleMin + i;
  }

  if (SidebarSearchRect(st, sidebar).Hit(x, y)) return HitSearch;

  float foot = SidebarFootTop(st, sidebar);
  for (int i = 0; i < (int)st.hosts.size(); ++i) {
    Rect row = SidebarHostRect(st, sidebar, i);
    if (row.b > foot - st.S(4)) break;
    if (row.Hit(x, y)) return HitHostBase + i;
  }
  if (SidebarFootItemRect(st, sidebar, 0).Hit(x, y)) return HitSettings;
  if (SidebarFootItemRect(st, sidebar, 1).Hit(x, y)) return HitHelp;

  for (int i = 0; i < (int)st.tabs.size(); ++i) {
    if (TabRectOf(st, tabs, i).Hit(x, y)) return HitTabBase + i;
  }
  if (TabAddRect(st, tabs).Hit(x, y)) return HitTabAdd;

  if (content.Hit(x, y)) {
    if (st.view == View::Dash) {
      if (DashNewHostRect(st, content).Hit(x, y)) return HitDashNewHost;
      for (int i = 0; i < (int)st.hosts.size(); ++i) {
        if (DashCardRect(st, content, i).Hit(x, y)) return HitDashCardBase + i;
      }
    } else if (st.view == View::Sftp) {
      if (FtpToolRect(st, content, 0).Hit(x, y)) return HitFtpRefresh;
      if (FtpToolRect(st, content, 1).Hit(x, y)) return HitFtpDown;
      if (FtpToolRect(st, content, 2).Hit(x, y)) return HitFtpUp;
      if (FtpQueueRect(st, content).Hit(x, y)) return HitFtpQueue;
    } else if (st.view == View::Tun) {
      if (TunNewRect(st, content).Hit(x, y)) return HitTunNew;
      for (int i = 0; i < (int)st.hosts.size(); ++i) {
        if (TunSwitchRect(st, content, i).Hit(x, y)) return HitTunSwitchBase + i;
      }
    } else if (st.view == View::Set) {
      for (int i = 0; i < 6; ++i) {
        if (SetNavRect(st, content, i).Hit(x, y)) return HitSetNavBase + i;
      }
    }
  }

  if (status.Hit(x, y) && x < status.l + st.S(180)) return HitStatusConn;
  return HitNone;
}

void OnHit(AppState& st, int id) {
  if (id == HitNone) return;

  if (id == HitTitleClose) {
    PostQuitMessage(0);
    return;
  }
  if (id == HitTitleMin) {
    ShowWindow(GetActiveWindow(), SW_MINIMIZE);
    return;
  }
  if (id == HitTitleMax) {
    HWND hwnd = GetActiveWindow();
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    return;
  }

  if (id >= HitHostBase && id < HitHostBase + (int)st.hosts.size()) {
    st.activeHost = id - HitHostBase;
    st.view = View::Term;
    return;
  }

  if (id >= HitTabBase && id < HitTabBase + (int)st.tabs.size()) {
    int index = id - HitTabBase;
    st.activeTab = index;
    st.view = (View)st.tabs[index].view;
    return;
  }

  if (id == HitSettings) {
    st.view = View::Set;
    return;
  }

  if (id >= HitDashCardBase && id < HitDashNewHost) {
    int index = id - HitDashCardBase;
    if (index >= 0 && index < (int)st.hosts.size()) {
      st.activeHost = index;
      st.view = View::Term;
    }
    return;
  }

  if (id >= HitTunSwitchBase && id < HitSetNavBase) {
    int index = id - HitTunSwitchBase;
    if (index >= 0 && index < (int)st.hosts.size()) {
      st.hosts[index].autoConnect = !st.hosts[index].autoConnect;
    }
    return;
  }
}

}
