#include "ui/Chrome.h"
#include "ui/Render.h"
#include "ui/Terminal.h"
#include "app/App.h"
#include "app/Layout.h"
#include "app/Theme.h"

#include <string>

namespace sk {
namespace {

constexpr float kH = 0.5f;

float MidY(const Rect& rc) { return rc.t + rc.H() * kH; }

std::wstring Num(int v) { return std::to_wstring(v); }

D2D1_COLOR_F White() { return Rgb(0xFFFFFF); }

void CardFill(Render& r, const AppState& st, const Rect& rc, const Palette& p) {
  r.FillRound(rc.D2D(), st.S(L::RCard), p.surface);
}

void ChipText(Render& r, const AppState& st, const Rect& rc, const wchar_t* s,
              D2D1_COLOR_F fg, D2D1_COLOR_F bg) {
  r.FillRound(rc.D2D(), st.S(L::RChip), bg);
  r.TextVertCenter(s, rc.D2D(), fg, FontId::Micro, Align::Center);
}

void BtnOutline(Render& r, const AppState& st, const Rect& rc, const Palette& p,
                const wchar_t* label, D2D1_COLOR_F fg) {
  r.StrokeRound(rc.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
  r.TextVertCenter(label, rc.D2D(), fg, FontId::Small, Align::Center);
}

void BtnSolid(Render& r, const AppState& st, const Rect& rc, const Palette& p,
              const wchar_t* label) {
  r.FillRound(rc.D2D(), st.S(L::RCtrl), p.accent);
  r.TextVertCenter(label, rc.D2D(), p.accentFg, FontId::Small, Align::Center);
}

void SwitchPill(Render& r, const AppState& st, const Rect& rc, bool on, const Palette& p) {
  float rad = rc.H() * kH;
  r.FillRound(rc.D2D(), rad, on ? p.accent : p.surface2);
  float kr = rad - st.S(2.0f);
  float kx = on ? rc.r - st.S(2.0f) - kr : rc.l + st.S(2.0f) + kr;
  r.Circle(kx, MidY(rc), kr, White());
}

void IconFolder(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.1f);
  float top = rc.t + rc.H() * 0.22f;
  float bodyTop = rc.t + rc.H() * 0.36f;
  Rect body{ rc.l + rc.W() * 0.04f, bodyTop, rc.r - rc.W() * 0.04f, rc.b - rc.H() * 0.06f };
  r.StrokeRound(body.D2D(), st.S(2.0f), c, w);
  float x0 = rc.l + rc.W() * 0.16f;
  float x1 = rc.l + rc.W() * 0.42f;
  float x2 = rc.l + rc.W() * 0.56f;
  r.Line(x0, bodyTop, x0, top, c, w);
  r.Line(x0, top, x1, top, c, w);
  r.Line(x1, top, x2, bodyTop, c, w);
}

void IconFile(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.1f);
  Rect body{ rc.l + rc.W() * 0.18f, rc.t + rc.H() * 0.06f, rc.r - rc.W() * 0.18f, rc.b - rc.H() * 0.06f };
  r.StrokeRound(body.D2D(), st.S(1.5f), c, w);
  float fold = rc.W() * 0.26f;
  r.Line(body.r - fold, body.t, body.r, body.t + fold, c, w);
  r.Line(body.r - fold, body.t, body.r - fold, body.t + fold, c, w);
  r.Line(body.r - fold, body.t + fold, body.r, body.t + fold, c, w);
  float lx0 = body.l + rc.W() * 0.10f;
  float lx1 = body.r - rc.W() * 0.10f;
  r.Line(lx0, body.t + body.H() * 0.56f, lx1, body.t + body.H() * 0.56f, c, w);
  r.Line(lx0, body.t + body.H() * 0.78f, lx1, body.t + body.H() * 0.78f, c, w);
}

void IconLink(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  float h = rc.H() * 0.34f;
  Rect a{ rc.l, MidY(rc) - h * kH, rc.l + rc.W() * 0.62f, MidY(rc) + h * kH };
  Rect b{ rc.l + rc.W() * 0.38f, MidY(rc) - h * kH, rc.r, MidY(rc) + h * kH };
  r.StrokeRound(a.D2D(), h * kH, c, w);
  r.StrokeRound(b.D2D(), h * kH, c, w);
}

void IconGlobe(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.1f);
  float rad = rc.W() * 0.42f;
  float cx = rc.l + rc.W() * kH;
  float cy = rc.t + rc.H() * kH;
  r.StrokeRound(D2D1::RectF(cx - rad, cy - rad, cx + rad, cy + rad), rad, c, w);
  r.Line(cx - rad, cy, cx + rad, cy, c, w);
  Rect inner{ cx - rad * 0.45f, cy - rad, cx + rad * 0.45f, cy + rad };
  r.StrokeRound(inner.D2D(), rad * 0.45f, c, w);
}

void IconRefresh(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  float rad = rc.W() * 0.30f;
  float cx = rc.l + rc.W() * kH;
  float cy = MidY(rc);
  r.StrokeRound(D2D1::RectF(cx - rad, cy - rad, cx + rad, cy + rad), rad, c, w);
  r.Line(cx + rad * 0.15f, cy - rad, cx + rad * 1.05f, cy - rad * 0.55f, c, w);
  r.Line(cx + rad * 1.05f, cy - rad * 0.55f, cx + rad * 0.25f, cy - rad * 0.70f, c, w);
}

void IconPencil(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  float x0 = rc.l + rc.W() * 0.22f;
  float y0 = rc.b - rc.H() * 0.18f;
  float x1 = rc.r - rc.W() * 0.22f;
  float y1 = rc.t + rc.H() * 0.18f;
  float dx = rc.W() * 0.11f;
  float dy = rc.H() * 0.11f;
  r.Line(x0, y0, x1, y1, c, w);
  r.Line(x0 + dx, y0 - dy, x1 + dx, y1 - dy, c, w);
  r.Line(x0, y0, x0 + dx, y0 - dy, c, w);
  r.Line(x1, y1, x1 + dx, y1 - dy, c, w);
}

void IconTrash(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  float lid = rc.t + rc.H() * 0.30f;
  float left = rc.l + rc.W() * 0.24f;
  float right = rc.r - rc.W() * 0.24f;
  r.Line(rc.l + rc.W() * 0.12f, lid, rc.r - rc.W() * 0.12f, lid, c, w);
  r.Line(rc.l + rc.W() * 0.38f, rc.t + rc.H() * 0.16f, rc.r - rc.W() * 0.38f, rc.t + rc.H() * 0.16f, c, w);
  r.Line(rc.l + rc.W() * 0.38f, rc.t + rc.H() * 0.16f, rc.l + rc.W() * 0.38f, lid, c, w);
  r.Line(rc.r - rc.W() * 0.38f, rc.t + rc.H() * 0.16f, rc.r - rc.W() * 0.38f, lid, c, w);
  Rect body{ left, lid, right, rc.b - rc.H() * 0.10f };
  r.StrokeRound(body.D2D(), st.S(2.0f), c, w);
  float inL = left + rc.W() * 0.16f;
  float inR = right - rc.W() * 0.16f;
  r.Line(inL, lid + rc.H() * 0.14f, inL, body.b - rc.H() * 0.12f, c, w);
  r.Line(inR, lid + rc.H() * 0.14f, inR, body.b - rc.H() * 0.12f, c, w);
}

void IconUp(Render& r, const AppState& st, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.5f);
  float cx = rc.l + rc.W() * kH;
  float top = rc.t + rc.H() * 0.12f;
  r.Line(cx, rc.b - rc.H() * 0.10f, cx, top, c, w);
  r.Line(cx, top, rc.l + rc.W() * 0.18f, rc.t + rc.H() * 0.46f, c, w);
  r.Line(cx, top, rc.r - rc.W() * 0.18f, rc.t + rc.H() * 0.46f, c, w);
}

void NavIcon(Render& r, const AppState& st, int i, const Rect& rc, D2D1_COLOR_F c) {
  float w = st.S(1.2f);
  float cx = rc.l + rc.W() * kH;
  float cy = rc.t + rc.H() * kH;
  if (i == 0) {
    float rad = rc.W() * 0.22f;
    r.StrokeRound(D2D1::RectF(cx - rad, cy - rad, cx + rad, cy + rad), rad, c, w);
    float ir = rad * 0.38f;
    r.StrokeRound(D2D1::RectF(cx - ir, cy - ir, cx + ir, cy + ir), ir, c, w);
    float o0 = rad * 1.55f;
    float o1 = rc.W() * 0.47f;
    r.Line(cx, cy - o0, cx, cy - o1, c, w);
    r.Line(cx, cy + o0, cx, cy + o1, c, w);
    r.Line(cx - o0, cy, cx - o1, cy, c, w);
    r.Line(cx + o0, cy, cx + o1, cy, c, w);
  } else if (i == 1) {
    float rad = rc.W() * 0.23f;
    r.StrokeRound(D2D1::RectF(cx - rad, cy - rad, cx + rad, cy + rad), rad, c, w);
    float a = rc.W() * 0.37f;
    float b = rc.W() * 0.48f;
    r.Line(cx, cy - a, cx, cy - b, c, w);
    r.Line(cx, cy + a, cx, cy + b, c, w);
    r.Line(cx - a, cy, cx - b, cy, c, w);
    r.Line(cx + a, cy, cx + b, cy, c, w);
    float d = rc.W() * 0.32f;
    r.Line(cx + d * 0.7f, cy - d * 0.7f, cx + d, cy - d, c, w);
    r.Line(cx - d * 0.7f, cy + d * 0.7f, cx - d, cy + d, c, w);
  } else if (i == 2) {
    Rect box{ rc.l + rc.W() * 0.04f, rc.t + rc.H() * 0.12f, rc.r - rc.W() * 0.04f, rc.b - rc.H() * 0.12f };
    r.StrokeRound(box.D2D(), st.S(2.5f), c, w);
    float ax = box.l + rc.W() * 0.18f;
    float ay = box.t + box.H() * 0.30f;
    r.Line(ax, ay, ax + rc.W() * 0.16f, ay + rc.H() * 0.20f, c, w);
    r.Line(ax + rc.W() * 0.16f, ay + rc.H() * 0.20f, ax, ay + rc.H() * 0.40f, c, w);
    r.Line(box.l + rc.W() * 0.44f, box.b - rc.H() * 0.24f, box.r - rc.W() * 0.16f, box.b - rc.H() * 0.24f, c, w);
  } else if (i == 3) {
    float h = rc.H() * 0.34f;
    Rect a{ rc.l, cy - h * kH, rc.l + rc.W() * 0.62f, cy + h * kH };
    Rect b{ rc.l + rc.W() * 0.38f, cy - h * kH, rc.r, cy + h * kH };
    r.StrokeRound(a.D2D(), h * kH, c, w);
    r.StrokeRound(b.D2D(), h * kH, c, w);
  } else if (i == 4) {
    Rect box{ rc.l + rc.W() * 0.14f, rc.t + rc.H() * 0.06f, rc.r - rc.W() * 0.14f, rc.b - rc.H() * 0.06f };
    r.StrokeRound(box.D2D(), st.S(2.0f), c, w);
    r.Line(box.l + rc.W() * 0.14f, box.t + box.H() * 0.30f, box.r - rc.W() * 0.10f, box.t + box.H() * 0.30f, c, w);
    r.Line(box.l + rc.W() * 0.14f, box.t + box.H() * 0.54f, box.r - rc.W() * 0.10f, box.t + box.H() * 0.54f, c, w);
    r.Line(box.l + rc.W() * 0.14f, box.t + box.H() * 0.78f, box.l + rc.W() * 0.60f, box.t + box.H() * 0.78f, c, w);
  } else {
    float rad = rc.W() * 0.42f;
    r.StrokeRound(D2D1::RectF(cx - rad, cy - rad, cx + rad, cy + rad), rad, c, w);
    r.Circle(cx, cy - rad * 0.36f, st.S(1.2f), c);
    r.Line(cx, cy - rad * 0.04f, cx, cy + rad * 0.42f, c, w);
  }
}

struct FileRow {
  const wchar_t* name;
  const wchar_t* size;
  const wchar_t* time;
  bool dir;
};

void FilePanel(Render& r, const AppState& st, const Palette& p, const Rect& panel,
               const wchar_t* title, const wchar_t* path, bool remote,
               const FileRow* rows, int n, int highlight) {
  Rect head{ panel.l, panel.t, panel.r, panel.t + st.S(32.0f) };
  r.Fill(head.D2D(), p.surface);
  r.Fill(D2D1::RectF(head.l, head.b - st.S(1.0f), head.r, head.b), p.line);

  float icy = MidY(head);
  Rect ic{ head.l + st.S(12.0f), icy - st.S(6.0f), head.l + st.S(24.0f), icy + st.S(6.0f) };
  if (remote) {
    IconGlobe(r, st, ic, p.accent);
  } else {
    IconFolder(r, st, ic, p.accent);
  }

  float titleLeft = ic.r + st.S(9.0f);
  float pathLeft = head.r - st.S(12.0f) - r.Measure(path, FontId::MonoSm);
  r.TextVertCenter(title, D2D1::RectF(titleLeft, head.t, pathLeft - st.S(6.0f), head.b),
                   p.text2, FontId::Small);
  r.TextVertCenter(path, D2D1::RectF(pathLeft, head.t, head.r - st.S(12.0f), head.b),
                   p.text3, FontId::MonoSm);

  for (int i = 0; i < n; ++i) {
    Rect row{ panel.l, head.b + st.S(36.0f) * (float)i, panel.r,
              head.b + st.S(36.0f) * (float)(i + 1) };
    if (row.t >= panel.b) break;
    if (i == highlight) {
      Rect hl{ row.l + st.S(6.0f), row.t + st.S(2.0f), row.r - st.S(6.0f), row.b - st.S(2.0f) };
      r.FillRound(hl.D2D(), st.S(L::RCtrl), p.accentSoft);
    }
    float cy = MidY(row);
    float ib = st.S(22.0f);
    Rect ibx{ row.l + st.S(12.0f), cy - ib * kH, row.l + st.S(12.0f) + ib, cy + ib * kH };
    if (rows[i].dir) {
      r.FillRound(ibx.D2D(), st.S(6.0f), p.accentSoft);
      IconFolder(r, st, ibx.Inset(st.S(5.0f)), p.accent);
    } else {
      r.FillRound(ibx.D2D(), st.S(6.0f), p.surface2);
      IconFile(r, st, ibx.Inset(st.S(5.0f)), p.text3);
    }

    float timeRight = row.r - st.S(12.0f);
    float timeLeft = timeRight - st.S(92.0f);
    float sizeRight = timeLeft - st.S(8.0f);
    float sizeLeft = sizeRight - st.S(66.0f);
    float nameLeft = ibx.r + st.S(10.0f);
    float nameRight = sizeLeft - st.S(8.0f);
    if (nameRight > nameLeft) {
      r.PushClip(D2D1::RectF(nameLeft, row.t, nameRight, row.b));
      r.TextVertCenter(rows[i].name, D2D1::RectF(nameLeft, row.t, nameRight, row.b),
                       rows[i].dir ? p.text : p.text2, FontId::Body);
      r.PopClip();
    }
    r.TextVertCenter(rows[i].size, D2D1::RectF(sizeLeft, row.t, sizeRight, row.b),
                     p.text3, FontId::MonoSm, Align::Right);
    r.TextVertCenter(rows[i].time, D2D1::RectF(timeLeft, row.t, timeRight, row.b),
                     p.text3, FontId::MonoSm, Align::Right);
  }
}

float SegWidth(Render& r, const AppState& st, const wchar_t** items, int n) {
  float w = st.S(4.0f);
  for (int i = 0; i < n; ++i) w += r.Measure(items[i], FontId::Micro) + st.S(24.0f);
  return w;
}

void Segment(Render& r, const AppState& st, const Palette& p, const Rect& rc,
             const wchar_t** items, int n, int sel) {
  r.FillRound(rc.D2D(), st.S(9.0f), p.surface2);
  float inner = rc.W() - st.S(4.0f);
  float total = 0.0f;
  float ws[4];
  for (int i = 0; i < n; ++i) {
    ws[i] = r.Measure(items[i], FontId::Micro) + st.S(24.0f);
    total += ws[i];
  }
  if (total <= 0.0f) return;
  float scale = inner / total;
  if (scale > 1.0f) scale = 1.0f;
  float x = rc.l + st.S(2.0f);
  for (int i = 0; i < n; ++i) {
    float w = ws[i] * scale;
    Rect seg{ x, rc.t + st.S(2.0f), x + w, rc.b - st.S(2.0f) };
    if (i == sel) {
      r.FillRound(seg.D2D(), st.S(7.0f), p.surface);
      r.TextVertCenter(items[i], seg.D2D(), p.accent, FontId::Micro, Align::Center);
    } else {
      r.TextVertCenter(items[i], seg.D2D(), p.text3, FontId::Micro, Align::Center);
    }
    x += w;
  }
}

}

void DrawDash(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.bg);

  float left = rc.l + st.S(28.0f);
  float right = rc.r - st.S(28.0f);
  float top = rc.t + st.S(26.0f);

  r.TextVertCenter(L"下午好，欢迎回来", D2D1::RectF(left, top, right, top + st.S(30.0f)),
                   p.text, FontId::Display);
  r.TextVertCenter(L"5 台主机 · 2 个会话在线 · 3 条转发运行中",
                   D2D1::RectF(left, top + st.S(32.0f), right, top + st.S(52.0f)),
                   p.text3, FontId::Caption);

  Rect nb{ right - st.S(112.0f), rc.t + st.S(34.0f), right, rc.t + st.S(68.0f) };
  BtnSolid(r, st, nb, p, L"＋ 新建主机");

  float gap = st.S(10.0f);
  float statTop = rc.t + st.S(100.0f);
  float statH = st.S(76.0f);
  float statW = (right - left - gap * 3.0f) / 4.0f;
  const wchar_t* labels[4] = { L"在线会话", L"转发通道", L"今日传输", L"卡片到期" };
  const wchar_t* values[4] = { L"2", L"3/4", L"1.8 GB", L"2" };
  const wchar_t* subs[4] = { L"全部健康", L"1 条待重连", L"SFTP · 24 次", L"最近 2 天内" };

  for (int i = 0; i < 4; ++i) {
    float l = left + (statW + gap) * (float)i;
    Rect card{ l, statTop, l + statW, statTop + statH };
    CardFill(r, st, card, p);
    r.Circle(card.r - st.S(24.0f), card.t + st.S(26.0f), st.S(21.0f), p.accentSoft);
    r.TextVertCenter(labels[i],
                     D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(9.0f),
                                 card.r - st.S(14.0f), card.t + st.S(23.0f)),
                     p.text3, FontId::Micro);
    r.TextVertCenter(values[i],
                     D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(23.0f),
                                 card.r - st.S(14.0f), card.t + st.S(50.0f)),
                     p.text, FontId::Display);
    r.TextVertCenter(subs[i],
                     D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(53.0f),
                                 card.r - st.S(14.0f), card.t + st.S(67.0f)),
                     i == 3 ? p.warn : p.text3, FontId::Micro);
  }

  r.TextVertCenter(L"最近连接",
                   D2D1::RectF(left, rc.t + st.S(192.0f), right, rc.t + st.S(208.0f)),
                   p.text3, FontId::Micro);

  float avail = right - left;
  float minW = st.S(250.0f);
  int cols = (int)((avail + gap) / (minW + gap));
  if (cols < 1) cols = 1;
  if (cols > 4) cols = 4;
  float cardW = (avail - gap * (float)(cols - 1)) / (float)cols;
  float cardH = st.S(96.0f);
  float hostTop = rc.t + st.S(220.0f);

  for (int i = 0; i < (int)st.hosts.size(); ++i) {
    int col = i % cols;
    int row = i / cols;
    float l = left + (cardW + gap) * (float)col;
    float t = hostTop + (cardH + gap) * (float)row;
    Rect card{ l, t, l + cardW, t + cardH };
    if (card.t >= rc.b) break;
    CardFill(r, st, card, p);

    const Host& h = st.hosts[i];
    float dotR = st.S(4.0f);
    float dotX = card.l + st.S(16.0f);
    float dotY = card.t + st.S(24.0f);
    D2D1_COLOR_F dc = p.text3;
    if (h.state == HostState::Live) dc = p.accent;
    if (h.state == HostState::Busy) dc = p.warn;
    if (h.state != HostState::Off) r.Circle(dotX, dotY, dotR + st.S(3.0f), Alpha(dc, 0.14f));
    r.Circle(dotX, dotY, dotR, dc);

    float chipRight = card.r - st.S(14.0f);
    float nameRight = chipRight;
    if (h.state != HostState::Off) {
      const wchar_t* ct = h.state == HostState::Live ? L"已连接" : L"连接中";
      D2D1_COLOR_F cf = h.state == HostState::Live ? p.accent : p.warn;
      D2D1_COLOR_F cb = h.state == HostState::Live ? p.accentSoft : Alpha(p.warn, 0.13f);
      float cw = r.Measure(ct, FontId::Micro) + st.S(16.0f);
      float ch = st.S(18.0f);
      Rect chip{ chipRight - cw, dotY - ch * kH, chipRight, dotY + ch * kH };
      ChipText(r, st, chip, ct, cf, cb);
      nameRight = chip.l - st.S(8.0f);
    }

    float nameLeft = dotX + dotR + st.S(10.0f);
    if (nameRight > nameLeft) {
      r.PushClip(D2D1::RectF(nameLeft, card.t + st.S(8.0f), nameRight, card.t + st.S(38.0f)));
      r.TextVertCenter(h.name.c_str(),
                       D2D1::RectF(nameLeft, card.t + st.S(9.0f), nameRight, card.t + st.S(37.0f)),
                       p.text, FontId::Title);
      r.PopClip();
    }

    r.PushClip(D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(36.0f),
                           card.r - st.S(12.0f), card.t + st.S(56.0f)));
    r.TextVertCenter(h.addr.c_str(),
                     D2D1::RectF(card.l + st.S(16.0f), card.t + st.S(38.0f),
                                 card.r - st.S(14.0f), card.t + st.S(56.0f)),
                     p.text3, FontId::MonoSm);
    r.PopClip();

    float tagY = card.t + st.S(66.0f);
    float tagH = st.S(20.0f);
    float tagX = card.l + st.S(14.0f);
    float tagLimit = card.r - st.S(74.0f);
    std::wstring tags[2];
    int tagN = 0;
    tags[tagN++] = h.auth;
    if (h.autoConnect) tags[tagN++] = L"自动连接";
    for (int k = 0; k < tagN; ++k) {
      float tw = r.Measure(tags[k].c_str(), FontId::Micro) + st.S(16.0f);
      if (tagX + tw > tagLimit) break;
      Rect tag{ tagX, tagY, tagX + tw, tagY + tagH };
      ChipText(r, st, tag, tags[k].c_str(), p.text2, p.surface2);
      tagX += tw + st.S(6.0f);
    }

    float metaRight = card.r - st.S(14.0f);
    if (h.latency >= 0) {
      std::wstring lat = Num(h.latency) + L" ms";
      r.TextVertCenter(lat.c_str(),
                       D2D1::RectF(card.r - st.S(92.0f), tagY, metaRight, tagY + tagH),
                       p.accent, FontId::MonoSm, Align::Right);
    } else if (h.daysLeft >= 0) {
      std::wstring days = L"剩 " + Num(h.daysLeft) + L" 天";
      r.TextVertCenter(days.c_str(),
                       D2D1::RectF(card.r - st.S(102.0f), tagY, metaRight, tagY + tagH),
                       p.warn, FontId::Micro, Align::Right);
    } else {
      std::wstring days = L"已过期 " + Num(-h.daysLeft) + L" 天";
      r.TextVertCenter(days.c_str(),
                       D2D1::RectF(card.r - st.S(112.0f), tagY, metaRight, tagY + tagH),
                       p.danger, FontId::Micro, Align::Right);
    }
  }

  r.PopClip();
}

void DrawTermView(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.termBg);
  r.Circle(rc.r - st.S(40.0f), rc.t - st.S(30.0f), st.S(340.0f), Alpha(p.accentSoft, 0.55f));
  r.Circle(rc.r - st.S(90.0f), rc.t + st.S(10.0f), st.S(190.0f), Alpha(p.accentSoft, 0.38f));

  float promptH = st.S(38.0f);
  Rect term{ rc.l, rc.t, rc.r, rc.b - promptH };
  g_term.Draw(r, term, st.termFontSize, p);

  Rect bar{ rc.l, term.b, rc.r, rc.b };
  r.Fill(D2D1::RectF(bar.l, bar.t, bar.r, bar.t + st.S(1.0f)), p.line);

  const wchar_t* prefix = L"demo@prod:~$ ";
  float px = bar.l + st.S(18.0f);
  r.TextVertCenter(prefix, D2D1::RectF(px, bar.t, bar.r - st.S(18.0f), bar.b),
                   p.accent, FontId::Mono);
  float pw = r.Measure(prefix, FontId::Mono);
  float ch = r.LineH(FontId::Mono) * 0.82f;
  float cx = px + pw + st.S(2.0f);
  Rect cursor{ cx, MidY(bar) - ch * kH, cx + st.S(7.0f), MidY(bar) + ch * kH };
  r.Fill(cursor.D2D(), p.accent);
  r.PopClip();
}

void DrawSftp(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.bg);

  Rect tb{ rc.l, rc.t, rc.r, rc.t + st.S(42.0f) };
  r.Fill(tb.D2D(), p.surface);
  r.Fill(D2D1::RectF(tb.l, tb.b - st.S(1.0f), tb.r, tb.b), p.line);

  float tby = MidY(tb);
  Rect link{ tb.l + st.S(16.0f), tby - st.S(6.0f), tb.l + st.S(28.0f), tby + st.S(6.0f) };
  IconLink(r, st, link, p.text3);

  float bw = st.S(74.0f);
  float bh = st.S(28.0f);
  Rect b1{ rc.r - st.S(14.0f) - bw, tby - bh * kH, rc.r - st.S(14.0f), tby + bh * kH };
  Rect b0{ b1.l - st.S(8.0f) - bw, b1.t, b1.l - st.S(8.0f), b1.b };
  BtnOutline(r, st, b1, p, L"新建", p.text2);
  BtnOutline(r, st, b0, p, L"刷新", p.text2);

  float bx = link.r + st.S(10.0f);
  float bxLimit = b0.l - st.S(10.0f);
  r.PushClip(D2D1::RectF(bx, tb.t, bxLimit, tb.b));
  const wchar_t* crumb1 = L"跳板机";
  float w1 = r.Measure(crumb1, FontId::Small);
  r.TextVertCenter(crumb1, D2D1::RectF(bx, tb.t, bx + w1, tb.b), p.text3, FontId::Small);
  bx += w1 + st.S(6.0f);
  const wchar_t* crumb2 = L"/";
  float w2 = r.Measure(crumb2, FontId::Small);
  r.TextVertCenter(crumb2, D2D1::RectF(bx, tb.t, bx + w2, tb.b), p.text3, FontId::Small);
  bx += w2 + st.S(6.0f);
  const wchar_t* crumb3 = L"/srv/app";
  float w3 = r.Measure(crumb3, FontId::MonoSm);
  r.TextVertCenter(crumb3, D2D1::RectF(bx, tb.t, bx + w3, tb.b), p.text, FontId::MonoSm);
  bx += w3 + st.S(8.0f);
  r.TextVertCenter(L"· SFTP 已就绪", D2D1::RectF(bx, tb.t, bxLimit, tb.b), p.text3,
                   FontId::Caption);
  r.PopClip();

  Rect bar{ rc.l, rc.b - st.S(48.0f), rc.r, rc.b };
  float mid = rc.l + rc.W() * kH;
  Rect split{ mid - st.S(2.0f), tb.b, mid + st.S(2.0f), bar.t };
  r.Fill(split.D2D(), p.line);
  Rect lp{ rc.l, tb.b, mid - st.S(2.0f), bar.t };
  Rect rp{ mid + st.S(2.0f), tb.b, rc.r, bar.t };

  static const FileRow localRows[6] = {
    { L"..", L"", L"", true },
    { L"src", L"—", L"11-04 16:20", true },
    { L"config", L"—", L"11-03 09:12", true },
    { L"app-release.tar.gz", L"18.4 MB", L"11-05 09:02", false },
    { L"deploy.sh", L"2.1 KB", L"11-05 08:44", false },
    { L"CHANGELOG.md", L"8.6 KB", L"11-02 14:08", false }
  };
  static const FileRow remoteRows[7] = {
    { L"..", L"", L"", true },
    { L"html", L"—", L"11-01 10:33", true },
    { L"logs", L"—", L"11-05 09:30", true },
    { L"backup", L"—", L"10-28 03:00", true },
    { L"index.html", L"12.3 KB", L"11-04 18:02", false },
    { L"nginx.conf", L"3.8 KB", L"10-30 11:47", false },
    { L"VERSION", L"16 B", L"11-05 08:12", false }
  };

  FilePanel(r, st, p, lp, L"本地", L"D:\\work\\deploy", false, localRows, 6, -1);
  FilePanel(r, st, p, rp, L"远程", L"/srv/app", true, remoteRows, 7, 6);

  r.Fill(bar.D2D(), p.surface);
  r.Fill(D2D1::RectF(bar.l, bar.t, bar.r, bar.t + st.S(1.0f)), p.line);

  float by = MidY(bar);
  Rect up{ bar.l + st.S(18.0f), by - st.S(6.0f), bar.l + st.S(30.0f), by + st.S(6.0f) };
  IconUp(r, st, up, p.accent);

  const wchar_t* xfer = L"3 项传输中 · 2.4 MB/s";
  float tx = up.r + st.S(9.0f);
  r.TextVertCenter(xfer, D2D1::RectF(tx, bar.t, tx + st.S(210.0f), bar.b), p.text2,
                   FontId::Caption);
  tx += r.Measure(xfer, FontId::Caption) + st.S(18.0f);

  Rect q{ bar.r - st.S(14.0f) - st.S(58.0f), by - st.S(14.0f), bar.r - st.S(14.0f), by + st.S(14.0f) };
  BtnOutline(r, st, q, p, L"队列", p.text2);

  float pctRight = q.l - st.S(12.0f);
  float pctLeft = pctRight - st.S(30.0f);
  r.TextVertCenter(L"62%", D2D1::RectF(pctLeft, bar.t, pctRight, bar.b), p.text3,
                   FontId::MonoSm, Align::Right);

  float trackRight = pctLeft - st.S(10.0f);
  float trackLeft = tx;
  float trackW = trackRight - trackLeft;
  if (trackW > st.S(40.0f)) {
    float th = st.S(5.0f);
    Rect track{ trackLeft, by - th * kH, trackRight, by + th * kH };
    r.FillRound(track.D2D(), th * kH, p.surface2);
    float fw = trackW * 0.62f;
    r.FillRound(D2D1::RectF(track.l, track.t, track.l + fw, track.b), th * kH,
                Mix(p.accent, p.info, 0.55f));
    r.FillRound(D2D1::RectF(track.l, track.t, track.l + fw * 0.52f, track.b), th * kH,
                p.accent);
  }

  r.PopClip();
}

void DrawTun(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.bg);

  float left = rc.l + st.S(24.0f);
  float right = rc.r - st.S(24.0f);
  float top = rc.t + st.S(20.0f);

  Rect nb{ right - st.S(98.0f), top, right, top + st.S(32.0f) };
  BtnSolid(r, st, nb, p, L"＋ 新建转发");

  r.PushClip(D2D1::RectF(left, rc.t + st.S(14.0f), nb.l - st.S(12.0f), rc.t + st.S(70.0f)));
  r.TextVertCenter(L"端口转发", D2D1::RectF(left, rc.t + st.S(16.0f), nb.l, rc.t + st.S(46.0f)),
                   p.text, FontId::Display);
  r.TextVertCenter(L"10 秒巡检 · 不健康自动重启 · 退避 10/20/40/60s",
                   D2D1::RectF(left, rc.t + st.S(44.0f), nb.l, rc.t + st.S(62.0f)),
                   p.text3, FontId::Caption);
  r.PopClip();

  Rect table{ left, rc.t + st.S(88.0f), right, rc.t + st.S(88.0f) + st.S(36.0f) + st.S(52.0f) * 5.0f };
  CardFill(r, st, table, p);

  Rect head{ table.l, table.t, table.r, table.t + st.S(36.0f) };
  r.FillRound(head.D2D(), st.S(L::RCard), p.surface2);
  r.Fill(D2D1::RectF(head.l, head.t + st.S(12.0f), head.r, head.b), p.surface2);

  float inner = table.W() - st.S(28.0f);
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
  float xs[8];
  xs[0] = table.l + st.S(14.0f);
  for (int i = 0; i < 7; ++i) xs[i + 1] = xs[i] + cw[i];

  const wchar_t* headers[7] = { L"运行状态", L"类型", L"本地监听", L"目标地址",
                                L"所属主机", L"启用", L"操作" };
  for (int i = 0; i < 7; ++i) {
    r.PushClip(D2D1::RectF(xs[i], head.t, xs[i + 1], head.b));
    r.TextVertCenter(headers[i], D2D1::RectF(xs[i], head.t, xs[i + 1], head.b),
                     p.text3, FontId::Micro);
    r.PopClip();
  }

  const wchar_t* status[5] = { L"通道正常", L"通道正常", L"通道未通", L"重连 20s", L"已停止" };
  const int states[5] = { 0, 0, 1, 2, 3 };
  const wchar_t* types[5] = { L"L", L"L", L"L", L"R", L"L" };
  const wchar_t* listens[5] = { L"127.0.0.1:8080", L"127.0.0.1:5432", L"127.0.0.1:9200",
                                L"0.0.0.0:2222", L"127.0.0.1:6379" };
  const wchar_t* targets[5] = { L"192.0.2.10:80", L"192.0.2.24:5432", L"192.0.2.31:9200",
                                L"192.0.2.48:22", L"192.0.2.10:6379" };
  const wchar_t* owners[5] = { L"生产机", L"测试环境", L"构建节点", L"归档服务器", L"生产机" };
  const bool enabled[5] = { true, true, true, true, false };

  for (int i = 0; i < 5; ++i) {
    Rect row{ xs[0], head.b + st.S(52.0f) * (float)i, xs[7], head.b + st.S(52.0f) * (float)(i + 1) };
    r.Fill(D2D1::RectF(xs[0], row.t, xs[7], row.t + st.S(1.0f)), p.line);
    float cy = MidY(row);

    D2D1_COLOR_F sc = p.text3;
    if (states[i] == 0) sc = p.accent;
    if (states[i] == 1) sc = p.blue;
    if (states[i] == 2) sc = p.warn;
    float dotR = st.S(3.5f);
    float dx = xs[0] + dotR;
    if (states[i] == 0) r.Circle(dx, cy, dotR + st.S(3.0f), Alpha(sc, 0.16f));
    r.Circle(dx, cy, dotR, sc);
    float stx = dx + dotR + st.S(8.0f);
    r.PushClip(D2D1::RectF(stx, row.t, xs[1], row.b));
    r.TextVertCenter(status[i], D2D1::RectF(stx, row.t, xs[1], row.b), sc, FontId::Caption);
    r.PopClip();

    bool remote = types[i][0] == L'R';
    Rect chip{ xs[1], cy - st.S(9.0f), xs[1] + st.S(22.0f), cy + st.S(9.0f) };
    ChipText(r, st, chip, types[i], remote ? p.violet : p.text2,
             remote ? Alpha(p.violet, 0.13f) : p.surface2);

    r.PushClip(D2D1::RectF(xs[2], row.t, xs[3], row.b));
    r.TextVertCenter(listens[i], D2D1::RectF(xs[2], row.t, xs[3], row.b), p.text2,
                     FontId::MonoSm);
    r.PopClip();

    r.PushClip(D2D1::RectF(xs[3], row.t, xs[4], row.b));
    r.TextVertCenter(targets[i], D2D1::RectF(xs[3], row.t, xs[4], row.b), p.text2,
                     FontId::MonoSm);
    r.PopClip();

    r.PushClip(D2D1::RectF(xs[4], row.t, xs[5], row.b));
    r.TextVertCenter(owners[i], D2D1::RectF(xs[4], row.t, xs[5], row.b), p.text2,
                     FontId::Small);
    r.PopClip();

    float swRight = xs[6] - st.S(10.0f);
    Rect sw{ swRight - st.S(34.0f), cy - st.S(10.0f), swRight, cy + st.S(10.0f) };
    SwitchPill(r, st, sw, enabled[i], p);

    float ob = st.S(24.0f);
    float step = ob + st.S(2.0f);
    for (int k = 0; k < 3; ++k) {
      Rect cell{ xs[6] + step * (float)k, cy - ob * kH, xs[6] + step * (float)k + ob, cy + ob * kH };
      Rect ic = cell.Inset(st.S(5.0f));
      if (k == 0) {
        IconRefresh(r, st, ic, p.text3);
      } else if (k == 1) {
        IconPencil(r, st, ic, p.text3);
      } else {
        IconTrash(r, st, ic, p.text3);
      }
    }
  }

  r.PopClip();
}

void DrawSet(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.bg);

  float navW = st.S(170.0f);
  Rect nav{ rc.l, rc.t, rc.l + navW, rc.b };
  r.Fill(nav.D2D(), p.surface);
  r.Fill(D2D1::RectF(nav.r - st.S(1.0f), nav.t, nav.r, nav.b), p.line);

  const wchar_t* navNames[6] = { L"常规", L"外观", L"终端", L"连接", L"日志", L"关于" };
  for (int i = 0; i < 6; ++i) {
    Rect item{ rc.l + st.S(8.0f), rc.t + st.S(14.0f) + st.S(36.0f) * (float)i,
               rc.l + navW - st.S(8.0f), rc.t + st.S(14.0f) + st.S(36.0f) * (float)i + st.S(34.0f) };
    bool on = i == 0;
    if (on) r.FillRound(item.D2D(), st.S(8.0f), p.accentSoft);
    D2D1_COLOR_F fg = on ? p.accent : p.text2;
    Rect ic{ item.l + st.S(12.0f), MidY(item) - st.S(6.5f), item.l + st.S(25.0f), MidY(item) + st.S(6.5f) };
    NavIcon(r, st, i, ic, fg);
    r.TextVertCenter(navNames[i], D2D1::RectF(ic.r + st.S(10.0f), item.t, item.r - st.S(8.0f), item.b),
                     fg, FontId::Body);
  }

  float bx = rc.l + navW + st.S(28.0f);
  float bRight = rc.r - st.S(28.0f);
  float by = rc.t + st.S(24.0f);

  r.TextVertCenter(L"设置", D2D1::RectF(bx, by, bRight, by + st.S(30.0f)), p.text, FontId::Display);
  r.TextVertCenter(L"所有配置保存在程序同目录，绿色版不写入注册表",
                   D2D1::RectF(bx, by + st.S(32.0f), bRight, by + st.S(52.0f)), p.text3,
                   FontId::Caption);

  float cardW = st.S(660.0f);
  if (cardW > bRight - bx) cardW = bRight - bx;
  Rect card{ bx, by + st.S(74.0f), bx + cardW, by + st.S(74.0f) + st.S(58.0f) * 7.0f };
  CardFill(r, st, card, p);

  const wchar_t* rowLabel[7] = { L"主题", L"界面语言", L"强调色", L"终端字体",
                                 L"断线自动重连", L"启动自动连接", L"主机指纹校验" };
  const wchar_t* rowDesc[7] = { L"跟随系统外观或手动指定", L"重启后生效",
                                L"用于按钮、状态与高亮的强调色", L"终端网格使用等宽字体",
                                L"连接意外中断后自动恢复会话", L"打开程序时连接上次使用的主机",
                                L"首次连接时校验服务器指纹" };

  const wchar_t* themeItems[3] = { L"跟随系统", L"深色", L"浅色" };
  const wchar_t* langItems[2] = { L"简体中文", L"English" };

  float ctrlRight = card.r - st.S(18.0f);
  float ctrlLeft[7];
  ctrlLeft[0] = ctrlRight - SegWidth(r, st, themeItems, 3);
  ctrlLeft[1] = ctrlRight - SegWidth(r, st, langItems, 2);
  ctrlLeft[2] = ctrlRight - (st.S(17.0f) * 5.0f + st.S(8.0f) * 4.0f);
  ctrlLeft[3] = ctrlRight - st.S(170.0f);
  for (int i = 4; i < 7; ++i) ctrlLeft[i] = ctrlRight - st.S(34.0f);

  unsigned swatches[5] = { 0x3DDC97, 0x60A5FA, 0xA78BFA, 0xFBBF24, 0xF87171 };

  for (int i = 0; i < 7; ++i) {
    Rect row{ card.l, card.t + st.S(58.0f) * (float)i, card.r,
              card.t + st.S(58.0f) * (float)(i + 1) };
    if (i > 0) {
      r.Fill(D2D1::RectF(row.l + st.S(18.0f), row.t, row.r - st.S(18.0f), row.t + st.S(1.0f)),
             p.line);
    }
    float cy = MidY(row);
    float labelLeft = card.l + st.S(18.0f);
    float labelRight = ctrlLeft[i] - st.S(14.0f);
    if (labelRight > labelLeft + st.S(40.0f)) {
      r.PushClip(D2D1::RectF(labelLeft, row.t, labelRight, row.b));
      r.TextVertCenter(rowLabel[i], D2D1::RectF(labelLeft, cy - st.S(18.0f), labelRight, cy),
                       p.text, FontId::Body);
      r.TextVertCenter(rowDesc[i], D2D1::RectF(labelLeft, cy, labelRight, cy + st.S(18.0f)),
                       p.text3, FontId::Micro);
      r.PopClip();
    }

    if (i == 0) {
      Rect seg{ ctrlLeft[0], cy - st.S(13.0f), ctrlRight, cy + st.S(13.0f) };
      Segment(r, st, p, seg, themeItems, 3, 0);
    } else if (i == 1) {
      Rect seg{ ctrlLeft[1], cy - st.S(13.0f), ctrlRight, cy + st.S(13.0f) };
      Segment(r, st, p, seg, langItems, 2, 0);
    } else if (i == 2) {
      for (int k = 0; k < 5; ++k) {
        float l = ctrlLeft[2] + (st.S(17.0f) + st.S(8.0f)) * (float)k;
        Rect swRc{ l, cy - st.S(8.5f), l + st.S(17.0f), cy + st.S(8.5f) };
        r.FillRound(swRc.D2D(), st.S(5.0f), Rgb(swatches[k]));
        if (k == 0) {
          r.StrokeRound(swRc.Inset(st.S(1.0f)).D2D(), st.S(4.0f), p.text, st.S(2.0f));
        }
      }
    } else if (i == 3) {
      Rect dd{ ctrlLeft[3], cy - st.S(15.0f), ctrlRight, cy + st.S(15.0f) };
      r.StrokeRound(dd.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
      r.TextVertCenter(L"Cascadia Mono",
                       D2D1::RectF(dd.l + st.S(12.0f), dd.t, dd.r - st.S(28.0f), dd.b),
                       p.text2, FontId::Body);
      float ax = dd.r - st.S(20.0f);
      r.Line(ax, cy - st.S(2.0f), ax + st.S(5.0f), cy + st.S(3.0f), p.text3, st.S(1.2f));
      r.Line(ax + st.S(5.0f), cy + st.S(3.0f), ax + st.S(10.0f), cy - st.S(2.0f), p.text3,
             st.S(1.2f));
    } else {
      Rect tog{ ctrlLeft[i], cy - st.S(10.0f), ctrlRight, cy + st.S(10.0f) };
      SwitchPill(r, st, tog, true, p);
    }
  }

  r.PopClip();
}

}
