#include "ui/Chrome.h"
#include "ui/Render.h"
#include "ui/Terminal.h"
#include "ui/Sessions.h"
#include "app/App.h"
#include "app/Layout.h"
#include "app/Theme.h"
#include "app/Version.h"

#include <string>
#include <vector>

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

void FilePanel(Render& r, const AppState& st, const Palette& p, const Rect& panel,
               const wchar_t* title, const wchar_t* path, bool remote,
               const std::vector<SftpEntry>& rows, int highlight) {
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

  int n = (int)rows.size();
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
      r.TextVertCenter(rows[i].name.c_str(), D2D1::RectF(nameLeft, row.t, nameRight, row.b),
                       rows[i].dir ? p.text : p.text2, FontId::Body);
      r.PopClip();
    }
    r.TextVertCenter(rows[i].size.c_str(), D2D1::RectF(sizeLeft, row.t, sizeRight, row.b),
                     p.text3, FontId::MonoSm, Align::Right);
    r.TextVertCenter(rows[i].time.c_str(), D2D1::RectF(timeLeft, row.t, timeRight, row.b),
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

struct SetRowDef {
  const wchar_t* label;
  const wchar_t* desc;
  int kind;
};

const SetRowDef kSetRows[6][4] = {
  { { L"界面语言", L"重启后生效", SetSeg },
    { L"断线自动重连", L"连接意外中断后自动恢复会话", SetSwitch },
    { L"启动时自动连接", L"打开程序时连接上次使用的主机", SetSwitch },
    { L"启动时恢复上次界面", L"回到退出前所在的页面", SetSwitch } },
  { { L"主题", L"跟随系统外观或手动指定", SetSeg },
    { L"强调色", L"用于按钮、状态与高亮的强调色", SetSwatch },
    { L"界面缩放", L"按比例放大全部界面元素", SetDrop } },
  { { L"等宽字体", L"终端网格使用的字体", SetDrop },
    { L"终端字号", L"终端字符网格的字号", SetDrop },
    { L"光标样式", L"终端输入光标外观", SetSeg } },
  { { L"主机指纹校验", L"首次连接时校验服务器指纹", SetSwitch },
    { L"保活间隔", L"空闲时发送保活包的间隔", SetDrop },
    { L"连接超时", L"建立连接的最长等待时间", SetDrop } },
  { { L"日志级别", L"记录到日志文件的详细程度", SetDrop },
    { L"保留天数", L"超过期限的日志自动清理", SetDrop },
    { L"日志目录", L"程序同目录 logs 文件夹", SetPlain } },
  { { L"版本", L"轻舟 Skiff Windows 便携版", SetPlain },
    { L"内核", L"SSH 协议实现与加密后端", SetPlain },
    { L"许可", L"仅供个人使用", SetPlain } }
};

const int kSetRowN[6] = { 4, 3, 3, 3, 3, 3 };

const wchar_t** SetOptions(int nav, int row, int* n) {
  static const wchar_t* kTheme[3] = { L"跟随系统", L"深色", L"浅色" };
  static const wchar_t* kLang[2] = { L"简体中文", L"English" };
  static const wchar_t* kScale[4] = { L"90%", L"100%", L"110%", L"125%" };
  static const wchar_t* kFont[3] = { L"Cascadia Mono", L"Consolas", L"JetBrains Mono" };
  static const wchar_t* kSize[4] = { L"12 px", L"13 px", L"14 px", L"16 px" };
  static const wchar_t* kCursor[3] = { L"竖线", L"方块", L"下划线" };
  static const wchar_t* kKa[4] = { L"30 秒", L"60 秒", L"120 秒", L"关闭" };
  static const wchar_t* kTo[3] = { L"5 秒", L"10 秒", L"20 秒" };
  static const wchar_t* kLog[3] = { L"关闭", L"基本", L"详细" };
  static const wchar_t* kKeep[3] = { L"7 天", L"30 天", L"90 天" };
  *n = 0;
  if (nav == 0 && row == 0) { *n = 2; return kLang; }
  if (nav == 1 && row == 0) { *n = 3; return kTheme; }
  if (nav == 1 && row == 2) { *n = 4; return kScale; }
  if (nav == 2 && row == 0) { *n = 3; return kFont; }
  if (nav == 2 && row == 1) { *n = 4; return kSize; }
  if (nav == 2 && row == 2) { *n = 3; return kCursor; }
  if (nav == 3 && row == 1) { *n = 4; return kKa; }
  if (nav == 3 && row == 2) { *n = 3; return kTo; }
  if (nav == 4 && row == 0) { *n = 3; return kLog; }
  if (nav == 4 && row == 1) { *n = 3; return kKeep; }
  return nullptr;
}

const wchar_t* SetPlainText(int nav, int row) {
  if (nav == 4 && row == 2) return L"logs\\";
  if (nav == 5 && row == 0) return L"v" SKIFF_WIN_VERSION L" · win64";
  if (nav == 5 && row == 1) return L"libssh2 1.11.1 · WinCNG";
  if (nav == 5 && row == 2) return L"个人使用";
  return L"";
}

}

Rect SetRowRect(const AppState& st, const Rect& c, int index) {
  float navW = st.S(170.0f);
  float bx = c.l + navW + st.S(28.0f);
  float bRight = c.r - st.S(28.0f);
  float cardW = st.S(660.0f);
  if (cardW > bRight - bx) cardW = bRight - bx;
  float top = c.t + st.S(24.0f) + st.S(74.0f);
  return Rect{ bx, top + st.S(58.0f) * (float)index, bx + cardW,
               top + st.S(58.0f) * (float)(index + 1) };
}

int SetRowCount(int nav) {
  if (nav < 0 || nav > 5) return 0;
  return kSetRowN[nav];
}

int SetKindOf(int nav, int row) {
  if (nav < 0 || nav > 5 || row < 0 || row >= SetRowCount(nav)) return -1;
  return kSetRows[nav][row].kind;
}

int SetOptionCount(int nav, int row) {
  int n = 0;
  SetOptions(nav, row, &n);
  return n;
}

int* SetIntSlot(AppState& st, int nav, int row) {
  if (nav == 0 && row == 0) return &st.langMode;
  if (nav == 1 && row == 0) return &st.themeMode;
  if (nav == 1 && row == 1) return &st.accentIdx;
  if (nav == 1 && row == 2) return &st.uiScaleIdx;
  if (nav == 2 && row == 0) return &st.termFontIdx;
  if (nav == 2 && row == 1) return &st.termSizeIdx;
  if (nav == 2 && row == 2) return &st.cursorIdx;
  if (nav == 3 && row == 1) return &st.keepAliveIdx;
  if (nav == 3 && row == 2) return &st.timeoutIdx;
  if (nav == 4 && row == 0) return &st.logLevelIdx;
  if (nav == 4 && row == 1) return &st.logKeepIdx;
  return nullptr;
}

bool* SetBoolSlot(AppState& st, int nav, int row) {
  if (nav == 0 && row == 1) return &st.autoReconnect;
  if (nav == 0 && row == 2) return &st.autoConnectAll;
  if (nav == 0 && row == 3) return &st.restoreLast;
  if (nav == 3 && row == 0) return &st.fpCheck;
  return nullptr;
}

int SetIntValue(const AppState& st, int nav, int row) {
  if (nav == 0 && row == 0) return st.langMode;
  if (nav == 1 && row == 0) return st.themeMode;
  if (nav == 1 && row == 1) return st.accentIdx;
  if (nav == 1 && row == 2) return st.uiScaleIdx;
  if (nav == 2 && row == 0) return st.termFontIdx;
  if (nav == 2 && row == 1) return st.termSizeIdx;
  if (nav == 2 && row == 2) return st.cursorIdx;
  if (nav == 3 && row == 1) return st.keepAliveIdx;
  if (nav == 3 && row == 2) return st.timeoutIdx;
  if (nav == 4 && row == 0) return st.logLevelIdx;
  if (nav == 4 && row == 1) return st.logKeepIdx;
  return 0;
}

bool SetBoolValue(const AppState& st, int nav, int row) {
  if (nav == 0 && row == 1) return st.autoReconnect;
  if (nav == 0 && row == 2) return st.autoConnectAll;
  if (nav == 0 && row == 3) return st.restoreLast;
  if (nav == 3 && row == 0) return st.fpCheck;
  return false;
}

void DrawDash(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.bg);

  float left = rc.l + st.S(28.0f);
  float right = rc.r - st.S(28.0f);
  float top = rc.t + st.S(26.0f);

  int liveN = 0;
  int soonN = 0;
  int tunOn = 0;
  int tunAll = 0;
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    const Host& h = st.hosts[i];
    if (h.state == HostState::Live) ++liveN;
    int d = DaysLeftOf(h.expireAt);
    if (d >= 0 && d <= 7) ++soonN;
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      ++tunAll;
      if (h.tunnels[k].on && h.tunnels[k].state == TunState::Up) ++tunOn;
    }
  }

  r.TextVertCenter(L"下午好，欢迎回来", D2D1::RectF(left, top, right, top + st.S(30.0f)),
                   p.text, FontId::Display);
  std::wstring sub = Num((int)st.hosts.size()) + L" 台主机 · " + Num(liveN) +
                     L" 个会话在线 · " + Num(tunOn) + L" 条转发运行中";
  r.TextVertCenter(sub.c_str(), D2D1::RectF(left, top + st.S(32.0f), right, top + st.S(52.0f)),
                   p.text3, FontId::Caption);

  Rect nb{ right - st.S(112.0f), rc.t + st.S(34.0f), right, rc.t + st.S(68.0f) };
  BtnSolid(r, st, nb, p, L"＋ 新建主机");

  Rect sq = DashSearchRect(st, rc);
  r.FillRound(sq.D2D(), st.S(L::RCtrl), p.surface2);
  if (st.searchFocus) r.StrokeRound(sq.D2D(), st.S(L::RCtrl), p.accent, st.S(1.2f));
  float scy = sq.t + sq.H() * kH;
  float icx = sq.l + st.S(15.0f);
  r.Circle(icx, scy, st.S(4.0f), Alpha(p.text3, 0.55f));
  r.Line(icx + st.S(3.0f), scy + st.S(3.0f), icx + st.S(7.0f), scy + st.S(7.0f), p.text3,
         st.S(1.5f));
  std::wstring shint = st.query.empty() ? std::wstring(L"搜索主机…") : st.query;
  float hintLeft = icx + st.S(12.0f);
  r.PushClip(D2D1::RectF(hintLeft, sq.t, sq.r - st.S(8.0f), sq.b));
  r.TextVertCenter(shint.c_str(), D2D1::RectF(hintLeft, sq.t, sq.r - st.S(8.0f), sq.b),
                   st.query.empty() ? p.text3 : p.text, FontId::Small);
  r.PopClip();

  float gap = st.S(10.0f);
  float statTop = rc.t + st.S(100.0f);
  float statH = st.S(76.0f);
  float statW = (right - left - gap * 3.0f) / 4.0f;
  const wchar_t* labels[4] = { L"在线会话", L"转发通道", L"离线主机", L"卡片到期" };
  std::wstring values[4];
  values[0] = Num(liveN);
  values[1] = Num(tunOn) + L"/" + Num(tunAll);
  values[2] = Num((int)st.hosts.size() - liveN);
  values[3] = Num(soonN);
  const wchar_t* subs[4] = { liveN > 0 ? L"全部健康" : L"暂无在线",
                             tunAll > 0 && tunOn < tunAll ? L"部分待连通" : L"全部连通",
                             liveN == (int)st.hosts.size() ? L"全部在线" : L"待连接",
                             soonN > 0 ? L"7 天内到期" : L"暂无临期" };

  for (int i = 0; i < 4; ++i) {
    float l = left + (statW + gap) * (float)i;
    Rect card{ l, statTop, l + statW, statTop + statH };
    CardFill(r, st, card, p);
    r.Circle(card.r - st.S(18.0f), card.t + st.S(18.0f), st.S(13.0f), Alpha(p.accent, 0.09f));
    r.TextVertCenter(labels[i],
                     D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(9.0f),
                                 card.r - st.S(14.0f), card.t + st.S(23.0f)),
                     p.text3, FontId::Micro);
    r.TextVertCenter(values[i].c_str(),
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
  float minW = st.S(300.0f);
  int cols = (int)((avail + gap) / (minW + gap));
  if (cols < 1) cols = 1;
  if (cols > 4) cols = 4;
  float cardW = (avail - gap * (float)(cols - 1)) / (float)cols;
  float cardH = st.S(168.0f);
  float hostTop = rc.t + st.S(220.0f);

  int vis[64];
  int visN = SidebarVisibleHosts(st, vis, 64);

  for (int s = 0; s < visN; ++s) {
    int col = s % cols;
    int row = s / cols;
    float l = left + (cardW + gap) * (float)col;
    float t = hostTop + (cardH + gap) * (float)row;
    Rect card{ l, t, l + cardW, t + cardH };
    if (card.t >= rc.b) break;
    CardFill(r, st, card, p);

    const Host& h = st.hosts[(size_t)vis[s]];
    float dotR = st.S(4.0f);
    float dotX = card.l + st.S(16.0f);
    float dotY = card.t + st.S(24.0f);
    D2D1_COLOR_F dc = p.text3;
    if (h.state == HostState::Live) dc = p.accent;
    if (h.state == HostState::Busy) dc = p.warn;
    if (h.state != HostState::Off) r.Circle(dotX, dotY, dotR + st.S(3.0f), Alpha(dc, 0.14f));
    r.Circle(dotX, dotY, dotR, dc);

    Rect editRc = DashEditRectOf(st, rc, s);
    IconPencil(r, st, editRc.Inset(st.S(3.0f)), p.text3);

    float chipRight = editRc.l - st.S(6.0f);
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

    std::wstring addr = h.Addr();
    r.PushClip(D2D1::RectF(card.l + st.S(14.0f), card.t + st.S(36.0f),
                           card.r - st.S(12.0f), card.t + st.S(56.0f)));
    r.TextVertCenter(addr.c_str(),
                     D2D1::RectF(card.l + st.S(16.0f), card.t + st.S(38.0f),
                                 card.r - st.S(14.0f), card.t + st.S(56.0f)),
                     p.text3, FontId::MonoSm);
    r.PopClip();

    float tagY = card.t + st.S(62.0f);
    float tagH = st.S(20.0f);
    float tagX = card.l + st.S(14.0f);
    float metaRight = card.r - st.S(14.0f);
    std::wstring extra;
    D2D1_COLOR_F extraColor = p.text3;
    if (h.latency >= 0) {
      extra = Num(h.latency) + L" ms";
      extraColor = p.accent;
    } else if (h.HasExpire()) {
      int dl = h.DaysLeft();
      extra = dl < 0 ? std::wstring(L"已过期 ") + Num(-dl) + L" 天"
                     : std::wstring(L"剩 ") + Num(dl) + L" 天";
      if (dl < 0) extraColor = p.danger;
      else if (dl <= 7) extraColor = p.warn;
    }
    float tagLimit = metaRight;
    if (!extra.empty()) {
      float ew = r.Measure(extra.c_str(), FontId::Micro);
      r.TextVertCenter(extra.c_str(), D2D1::RectF(metaRight - ew - st.S(90.0f), tagY, metaRight,
                                                  tagY + tagH),
                       extraColor, FontId::Micro, Align::Right);
      tagLimit = metaRight - st.S(96.0f);
    }
    std::wstring tags[2];
    int tagN = 0;
    tags[tagN++] = h.IsKeyAuth() ? std::wstring(L"密钥") : std::wstring(L"密码");
    if (h.autoConnect) tags[tagN++] = L"自动连接";
    for (int k = 0; k < tagN; ++k) {
      float tw = r.Measure(tags[k].c_str(), FontId::Micro) + st.S(16.0f);
      if (tagX + tw > tagLimit) break;
      Rect tag{ tagX, tagY, tagX + tw, tagY + tagH };
      ChipText(r, st, tag, tags[k].c_str(), p.text2, p.surface2);
      tagX += tw + st.S(6.0f);
    }

    bool live = h.state == HostState::Live;
    const wchar_t* statNames[3] = { L"CPU", L"内存", L"带宽" };
    int statVals[3] = { live ? h.cpu : 0, live ? h.ram : 0, live ? h.netp : 0 };
    std::wstring statText[3];
    statText[0] = live ? Num(h.cpu) + L"%" : std::wstring(L"—");
    statText[1] = live ? Num(h.ram) + L"%" : std::wstring(L"—");
    statText[2] = live && !h.net.empty() ? h.net : std::wstring(L"—");
    for (int k = 0; k < 3; ++k) {
      Rect sr = DashStatRectOf(st, rc, s, k);
      r.TextVertCenter(statNames[k],
                       D2D1::RectF(sr.l, sr.t, sr.l + st.S(28.0f), sr.t + st.S(14.0f)), p.text3,
                       FontId::Micro);
      r.TextVertCenter(statText[k].c_str(),
                       D2D1::RectF(sr.l + st.S(28.0f), sr.t, sr.r, sr.t + st.S(14.0f)),
                       live ? p.text2 : p.text3, FontId::Micro, Align::Right);
      Rect track{ sr.l, sr.t + st.S(18.0f), sr.r, sr.t + st.S(22.0f) };
      r.FillRound(track.D2D(), st.S(2.0f), p.surface2);
      int pct = statVals[k];
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      if (pct > 0) {
        float fw = track.W() * (float)pct / 100.0f;
        D2D1_COLOR_F fc = pct >= 85 ? p.danger : (pct >= 60 ? p.warn : p.accent);
        r.FillRound(D2D1::RectF(track.l, track.t, track.l + fw, track.b), st.S(2.0f), fc);
      }
    }

    Rect tg = DashToggleRectOf(st, rc, s);
    if (live) {
      BtnOutline(r, st, tg, p, L"断开", p.danger);
    } else if (h.state == HostState::Busy) {
      BtnOutline(r, st, tg, p, L"连接中", p.warn);
    } else {
      BtnSolid(r, st, tg, p, L"连接");
    }
    const wchar_t* quickNames[3] = { L"终端", L"隧道", L"文件" };
    for (int k = 0; k < 3; ++k) {
      Rect q = DashQuickRectOf(st, rc, s, k);
      BtnOutline(r, st, q, p, quickNames[k], p.text2);
    }
  }

  if (visN == 0) {
    const wchar_t* tip = st.hosts.empty() ? L"还没有主机，点右上角新建" : L"没有匹配的主机";
    r.TextVertCenter(tip,
                     D2D1::RectF(left, hostTop + st.S(28.0f), right, hostTop + st.S(68.0f)),
                     p.text3, FontId::Body, Align::Center);
  }

  r.PopClip();
}

void DrawTermView(Render& r, const AppState& st, const Rect& rc) {
  const Palette& p = PaletteFor(st.dark);
  r.PushClip(rc.D2D());
  r.Fill(rc.D2D(), p.termBg);
  r.Circle(rc.r - st.S(40.0f), rc.t - st.S(30.0f), st.S(340.0f), Alpha(p.accentSoft, 0.55f));
  r.Circle(rc.r - st.S(90.0f), rc.t + st.S(10.0f), st.S(190.0f), Alpha(p.accentSoft, 0.38f));

  float padX = st.S(L::TermPadX);
  float padY = st.S(L::TermPadY);
  Rect term{ rc.l + padX, rc.t + padY, rc.r - padX, rc.b - padY };
  Terminal* t = SessionHub::Instance().Find(st.CurrentSessionId());
  if (t != nullptr && term.W() > st.S(200.0f) && term.H() > st.S(120.0f)) {
    t->Draw(r, term, st.termFontSize, p);
  } else {
    Rect mid{ rc.l, rc.t + rc.H() * 0.42f, rc.r, rc.t + rc.H() * 0.46f };
    r.TextVertCenter(L"未连接 · 从左侧主机列表打开一个会话",
                     mid.D2D(), p.termDim, FontId::Body, Align::Center);
  }
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
  int hid = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
  std::wstring crumbHost = (hid >= 0 && hid < (int)st.hosts.size())
                               ? st.hosts[(size_t)hid].name
                               : std::wstring(L"本地");
  const wchar_t* crumb1 = crumbHost.c_str();
  float w1 = r.Measure(crumb1, FontId::Small);
  r.TextVertCenter(crumb1, D2D1::RectF(bx, tb.t, bx + w1, tb.b), p.text3, FontId::Small);
  bx += w1 + st.S(6.0f);
  const wchar_t* crumb2 = L"/";
  float w2 = r.Measure(crumb2, FontId::Small);
  r.TextVertCenter(crumb2, D2D1::RectF(bx, tb.t, bx + w2, tb.b), p.text3, FontId::Small);
  bx += w2 + st.S(6.0f);
  float w3 = r.Measure(st.sftpPath.c_str(), FontId::MonoSm);
  r.TextVertCenter(st.sftpPath.c_str(), D2D1::RectF(bx, tb.t, bx + w3, tb.b), p.text,
                   FontId::MonoSm);
  bx += w3 + st.S(8.0f);
  std::wstring sstatus = std::wstring(L"· ") + st.sftpStatus;
  r.TextVertCenter(sstatus.c_str(), D2D1::RectF(bx, tb.t, bxLimit, tb.b), p.text3,
                   FontId::Caption);
  r.PopClip();

  Rect bar{ rc.l, rc.b - st.S(48.0f), rc.r, rc.b };
  Rect lp = SftpPanelRect(st, rc, 0);
  Rect rp = SftpPanelRect(st, rc, 1);
  Rect split{ lp.r, tb.b, rp.l, bar.t };
  r.Fill(split.D2D(), p.line);

  FilePanel(r, st, p, lp, L"本地", st.sftpLocalPath.empty() ? L"—" : st.sftpLocalPath.c_str(),
            false, st.sftpLocal, st.sftpSelLocal);
  FilePanel(r, st, p, rp, L"远程", st.sftpPath.c_str(), true, st.sftpRemote, st.sftpSelRemote);

  r.Fill(bar.D2D(), p.surface);
  r.Fill(D2D1::RectF(bar.l, bar.t, bar.r, bar.t + st.S(1.0f)), p.line);

  float by = MidY(bar);
  Rect up{ bar.l + st.S(18.0f), by - st.S(6.0f), bar.l + st.S(30.0f), by + st.S(6.0f) };
  IconUp(r, st, up, p.accent);

  std::wstring xfer = st.sftpStatus;
  if (xfer.empty()) {
    xfer = st.sftpQueue.empty() ? std::wstring(L"就绪")
                                : Num((int)st.sftpQueue.size()) + L" 项待传输";
  }
  float tx = up.r + st.S(9.0f);
  r.PushClip(D2D1::RectF(tx, bar.t, bar.r - st.S(160.0f), bar.b));
  r.TextVertCenter(xfer.c_str(), D2D1::RectF(tx, bar.t, bar.r - st.S(160.0f), bar.b), p.text2,
                   FontId::Caption);
  r.PopClip();

  Rect q{ bar.r - st.S(14.0f) - st.S(58.0f), by - st.S(14.0f), bar.r - st.S(14.0f), by + st.S(14.0f) };
  BtnOutline(r, st, q, p, L"队列", p.text2);

  for (int k = 0; k < 2; ++k) {
    Rect ab = FtpActionRectOf(st, rc, k);
    r.StrokeRound(ab.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
    float acx = ab.l + ab.W() * kH;
    float amy = MidY(ab);
    float ah = st.S(4.5f);
    float aw = st.S(3.2f);
    if (k == 1) {
      r.Line(acx, amy + ah, acx, amy - ah, p.text2, st.S(1.4f));
      r.Line(acx - aw, amy - ah * 0.15f, acx, amy - ah, p.text2, st.S(1.4f));
      r.Line(acx + aw, amy - ah * 0.15f, acx, amy - ah, p.text2, st.S(1.4f));
    } else {
      r.Line(acx, amy - ah, acx, amy + ah, p.text2, st.S(1.4f));
      r.Line(acx - aw, amy + ah * 0.15f, acx, amy + ah, p.text2, st.S(1.4f));
      r.Line(acx + aw, amy + ah * 0.15f, acx, amy + ah, p.text2, st.S(1.4f));
    }
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

  int hi = st.CurrentHostId() >= 0 ? st.CurrentHostId() : st.ActiveHostIndex();
  const Host* host = (hi >= 0 && hi < (int)st.hosts.size()) ? &st.hosts[(size_t)hi] : nullptr;

  r.PushClip(D2D1::RectF(left, rc.t + st.S(14.0f), nb.l - st.S(12.0f), rc.t + st.S(70.0f)));
  r.TextVertCenter(L"端口转发", D2D1::RectF(left, rc.t + st.S(16.0f), nb.l, rc.t + st.S(46.0f)),
                   p.text, FontId::Display);
  std::wstring sub = host == nullptr
                         ? std::wstring(L"未选择主机 · 先建立会话")
                         : host->name + L" · 10 秒巡检 · 不健康自动重启 · 退避 10/20/40/60s";
  r.TextVertCenter(sub.c_str(), D2D1::RectF(left, rc.t + st.S(44.0f), nb.l, rc.t + st.S(62.0f)),
                   p.text3, FontId::Caption);
  r.PopClip();

  int tunN = host == nullptr ? 0 : static_cast<int>(host->tunnels.size());
  Rect table{ left, rc.t + st.S(88.0f), right,
              rc.t + st.S(88.0f) + st.S(36.0f) + st.S(52.0f) * static_cast<float>(tunN) };
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

  const wchar_t* headers[7] = { L"运行状态", L"类型", L"端口", L"目标地址",
                                L"所属主机", L"启用", L"操作" };
  for (int i = 0; i < 7; ++i) {
    r.PushClip(D2D1::RectF(xs[i], head.t, xs[i + 1], head.b));
    r.TextVertCenter(headers[i], D2D1::RectF(xs[i], head.t, xs[i + 1], head.b),
                     p.text3, FontId::Micro);
    r.PopClip();
  }

  for (int i = 0; i < tunN; ++i) {
    const Tunnel& tn = host->tunnels[(size_t)i];
    Rect row{ xs[0], head.b + st.S(52.0f) * (float)i, xs[7], head.b + st.S(52.0f) * (float)(i + 1) };
    r.Fill(D2D1::RectF(xs[0], row.t, xs[7], row.t + st.S(1.0f)), p.line);
    float cy = MidY(row);

    D2D1_COLOR_F sc = p.text3;
    if (tn.state == TunState::Up) sc = p.accent;
    if (tn.state == TunState::Listen) sc = p.blue;
    if (tn.state == TunState::Retry) sc = p.warn;
    std::wstring stText;
    if (tn.state == TunState::Up) stText = L"通道正常";
    else if (tn.state == TunState::Listen) stText = L"通道未通";
    else if (tn.state == TunState::Retry) stText = std::wstring(L"重连 ") + Num(tn.retryIn) + L"s";
    else stText = L"已停止";

    float dotR = st.S(3.5f);
    float dx = xs[0] + dotR;
    if (tn.state == TunState::Up) r.Circle(dx, cy, dotR + st.S(3.0f), Alpha(sc, 0.16f));
    r.Circle(dx, cy, dotR, sc);
    float stx = dx + dotR + st.S(8.0f);
    r.PushClip(D2D1::RectF(stx, row.t, xs[1], row.b));
    r.TextVertCenter(stText.c_str(), D2D1::RectF(stx, row.t, xs[1], row.b), sc, FontId::Caption);
    r.PopClip();

    bool remote = tn.IsRemote();
    Rect chip{ xs[1], cy - st.S(9.0f), xs[1] + st.S(22.0f), cy + st.S(9.0f) };
    ChipText(r, st, chip, tn.type.c_str(), remote ? p.violet : p.text2,
             remote ? Alpha(p.violet, 0.13f) : p.surface2);

    r.PushClip(D2D1::RectF(xs[2], row.t, xs[3], row.b));
    r.TextVertCenter(tn.port.c_str(), D2D1::RectF(xs[2], row.t, xs[3], row.b), p.text2,
                     FontId::MonoSm);
    r.PopClip();

    r.PushClip(D2D1::RectF(xs[3], row.t, xs[4], row.b));
    r.TextVertCenter(tn.dest.c_str(), D2D1::RectF(xs[3], row.t, xs[4], row.b), p.text2,
                     FontId::MonoSm);
    r.PopClip();

    r.PushClip(D2D1::RectF(xs[4], row.t, xs[5], row.b));
    r.TextVertCenter(host->name.c_str(), D2D1::RectF(xs[4], row.t, xs[5], row.b), p.text2,
                     FontId::Small);
    r.PopClip();

    float swRight = xs[6] - st.S(10.0f);
    Rect sw{ swRight - st.S(34.0f), cy - st.S(10.0f), swRight, cy + st.S(10.0f) };
    SwitchPill(r, st, sw, tn.on, p);

    for (int k = 0; k < 3; ++k) {
      Rect cell = TunOpRectOf(st, rc, i, k);
      if (cell.r > row.r) cell.r = row.r;
      if (cell.l > cell.r) continue;
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

  if (tunN == 0) {
    r.TextVertCenter(host == nullptr ? L"未选择主机，先从左侧打开一个会话"
                                     : L"该主机暂无转发规则，点右上角新建",
                     D2D1::RectF(table.l, table.t + st.S(36.0f), table.r, table.t + st.S(96.0f)),
                     p.text3, FontId::Body, Align::Center);
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

  static const wchar_t* navNames[6] = { L"常规", L"外观", L"终端", L"连接", L"日志", L"关于" };
  int navIdx = st.setNav < 0 ? 0 : (st.setNav > 5 ? 5 : st.setNav);
  for (int i = 0; i < 6; ++i) {
    Rect item = SetNavRect(st, rc, i);
    bool on = i == navIdx;
    if (on) r.FillRound(item.D2D(), st.S(8.0f), p.accentSoft);
    D2D1_COLOR_F fg = on ? p.accent : p.text2;
    Rect ic{ item.l + st.S(12.0f), MidY(item) - st.S(6.5f), item.l + st.S(25.0f),
             MidY(item) + st.S(6.5f) };
    NavIcon(r, st, i, ic, fg);
    r.TextVertCenter(navNames[i], D2D1::RectF(ic.r + st.S(10.0f), item.t, item.r - st.S(8.0f), item.b),
                     fg, FontId::Body);
  }

  float bx = rc.l + navW + st.S(28.0f);
  float bRight = rc.r - st.S(28.0f);
  float by = rc.t + st.S(24.0f);

  r.TextVertCenter(navNames[navIdx], D2D1::RectF(bx, by, bRight, by + st.S(30.0f)), p.text,
                   FontId::Display);
  r.TextVertCenter(L"所有配置保存在程序同目录，绿色版不写入注册表",
                   D2D1::RectF(bx, by + st.S(32.0f), bRight, by + st.S(52.0f)), p.text3,
                   FontId::Caption);

  int rowN = SetRowCount(navIdx);
  if (rowN > 0) {
    Rect first = SetRowRect(st, rc, 0);
    Rect card{ first.l, first.t, first.r, first.t + st.S(58.0f) * (float)rowN };
    CardFill(r, st, card, p);
  }

  static const unsigned kSwatches[5] = { 0x3DDC97, 0x60A5FA, 0xA78BFA, 0xFBBF24, 0xF87171 };

  for (int i = 0; i < rowN; ++i) {
    Rect row = SetRowRect(st, rc, i);
    if (i > 0) {
      r.Fill(D2D1::RectF(row.l + st.S(18.0f), row.t, row.r - st.S(18.0f), row.t + st.S(1.0f)),
             p.line);
    }
    float cy = MidY(row);
    int kind = SetKindOf(navIdx, i);
    int optN = 0;
    const wchar_t** opts = SetOptions(navIdx, i, &optN);
    int val = SetIntValue(st, navIdx, i);
    bool flag = SetBoolValue(st, navIdx, i);

    float ctrlW = st.S(34.0f);
    if (kind == SetDrop) ctrlW = st.S(170.0f);
    else if (kind == SetSeg) ctrlW = SegWidth(r, st, opts, optN);
    else if (kind == SetSwatch) ctrlW = st.S(17.0f) * 5.0f + st.S(8.0f) * 4.0f;
    else if (kind == SetPlain) ctrlW = st.S(190.0f);
    float ctrlRight = row.r - st.S(18.0f);
    float ctrlLeft = ctrlRight - ctrlW;

    float labelLeft = row.l + st.S(18.0f);
    float labelRight = ctrlLeft - st.S(16.0f);
    if (labelRight > labelLeft + st.S(60.0f)) {
      r.PushClip(D2D1::RectF(labelLeft, row.t, labelRight, row.b));
      r.TextVertCenter(kSetRows[navIdx][i].label,
                       D2D1::RectF(labelLeft, cy - st.S(18.0f), labelRight, cy), p.text,
                       FontId::Body);
      r.TextVertCenter(kSetRows[navIdx][i].desc,
                       D2D1::RectF(labelLeft, cy, labelRight, cy + st.S(18.0f)), p.text3,
                       FontId::Micro);
      r.PopClip();
    }

    if (kind == SetSeg && opts && optN > 0) {
      Rect seg{ ctrlLeft, cy - st.S(13.0f), ctrlRight, cy + st.S(13.0f) };
      Segment(r, st, p, seg, opts, optN, val);
    } else if (kind == SetSwatch) {
      for (int k = 0; k < 5; ++k) {
        float l = ctrlLeft + (st.S(17.0f) + st.S(8.0f)) * (float)k;
        Rect swRc{ l, cy - st.S(8.5f), l + st.S(17.0f), cy + st.S(8.5f) };
        r.FillRound(swRc.D2D(), st.S(5.0f), Rgb(kSwatches[k]));
        if (k == val % 5) {
          r.StrokeRound(swRc.Inset(st.S(1.0f)).D2D(), st.S(4.0f), p.text, st.S(2.0f));
        }
      }
    } else if (kind == SetDrop && opts && optN > 0) {
      int idx = val < 0 ? 0 : (val >= optN ? optN - 1 : val);
      Rect dd{ ctrlLeft, cy - st.S(15.0f), ctrlRight, cy + st.S(15.0f) };
      r.StrokeRound(dd.D2D(), st.S(L::RBtn), p.lineStrong, st.S(1.0f));
      r.TextVertCenter(opts[idx], D2D1::RectF(dd.l + st.S(12.0f), dd.t, dd.r - st.S(26.0f), dd.b),
                       p.text2, FontId::Body);
      float ax = dd.r - st.S(20.0f);
      r.Line(ax, cy - st.S(2.0f), ax + st.S(5.0f), cy + st.S(3.0f), p.text3, st.S(1.2f));
      r.Line(ax + st.S(5.0f), cy + st.S(3.0f), ax + st.S(10.0f), cy - st.S(2.0f), p.text3,
             st.S(1.2f));
    } else if (kind == SetSwitch) {
      Rect tog{ ctrlLeft, cy - st.S(10.0f), ctrlRight, cy + st.S(10.0f) };
      SwitchPill(r, st, tog, flag, p);
    } else if (kind == SetPlain) {
      r.TextVertCenter(SetPlainText(navIdx, i),
                       D2D1::RectF(ctrlLeft, row.t, ctrlRight, row.b), p.text3, FontId::MonoSm,
                       Align::Right);
    }
  }

  r.PopClip();
}

}
