#include "ui/Terminal.h"
#include "app/Layout.h"
#include <d2d1helper.h>
#include <string>
#include <vector>
#include <deque>

namespace sk {

Terminal g_term;

namespace {

std::string ToUtf8(const std::wstring& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned cp = static_cast<unsigned>(s[i]);
    if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < s.size()) {
      unsigned lo = static_cast<unsigned>(s[i + 1]);
      if (lo >= 0xDC00u && lo <= 0xDFFFu) {
        cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
        ++i;
      }
    }
    if (cp < 0x80u) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
      out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
      out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
      out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
      out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
      out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
      out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
      out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
      out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
      out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
  }
  return out;
}

int DecodeUtf8(const std::string& s, size_t i, wchar_t out[2], int* units) {
  *units = 0;
  if (i >= s.size()) return 0;
  unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    out[0] = static_cast<wchar_t>(c);
    *units = 1;
    return 1;
  }
  int need = 0;
  unsigned cp = 0;
  if ((c & 0xE0) == 0xC0) {
    need = 1;
    cp = c & 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    need = 2;
    cp = c & 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    need = 3;
    cp = c & 0x07;
  } else {
    return 0;
  }
  if (i + static_cast<size_t>(need) >= s.size()) return 0;
  for (int k = 1; k <= need; ++k) {
    unsigned char d = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
    if ((d & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (d & 0x3F);
  }
  if (cp > 0xFFFFu) {
    unsigned v = cp - 0x10000u;
    out[0] = static_cast<wchar_t>(0xD800u + (v >> 10));
    out[1] = static_cast<wchar_t>(0xDC00u + (v & 0x3FFu));
    *units = 2;
  } else {
    out[0] = static_cast<wchar_t>(cp);
    *units = 1;
  }
  return need + 1;
}

std::wstring TrimW(const std::wstring& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == L' ' || s[b] == L'\t' || s[b] == L'\r' || s[b] == L'\n')) ++b;
  while (e > b && (s[e - 1] == L' ' || s[e - 1] == L'\t' || s[e - 1] == L'\r' || s[e - 1] == L'\n')) --e;
  return s.substr(b, e - b);
}

Line BlankLine(int cols) {
  Line ln;
  if (cols < 1) cols = 1;
  ln.cells.assign(static_cast<size_t>(cols), Cell());
  return ln;
}

void FeedWelcome(Terminal& t) {
  std::wstring w;
  w += L"\x1b[90mLast login: Thu Nov  5 09:31:02 2026 from 192.0.2.1\x1b[0m\r\n";
  w += L"\r\n";
  w += L"\x1b[32m";
  w += L"      /|\r\n";
  w += L"     / |\r\n";
  w += L"    /  |\r\n";
  w += L"   /   |______\r\n";
  w += L"   \\          \\\r\n";
  w += L"    \\__________\\\r\n";
  w += L"  ~~~~~~~~~~~~~~\x1b[0m\r\n";
  w += L"\r\n";
  w += L"\x1b[90m";
  for (int i = 0; i < 60; ++i) w.push_back(static_cast<wchar_t>(0x2500));
  w += L"\x1b[0m\r\n";
  w += L"\x1b[90m 主机\x1b[0m  192.0.2.10      \x1b[90m系统\x1b[0m  Linux 6.8.0 x86_64\r\n";
  w += L"\x1b[90m 内核\x1b[0m  libssh2 1.11.1  \x1b[90m加密\x1b[0m  aes256-ctr / chacha20\r\n";
  w += L"\x1b[90m 指纹\x1b[0m  \x1b[35mSHA256:9k2L\u2026Qm7E\x1b[0m \x1b[32m\u2713 已核验\x1b[0m\r\n";
  w += L"\x1b[90m";
  for (int i = 0; i < 60; ++i) w.push_back(static_cast<wchar_t>(0x2500));
  w += L"\x1b[0m\r\n";
  w += L"\r\n";
  w += L"\x1b[36m提示：Ctrl+Shift+F 搜索 · Ctrl+Shift+K 清屏 · F11 全屏终端\x1b[0m\r\n";
  t.Feed(ToUtf8(w));
}

}

Terminal::Terminal() {
  cols_ = 80;
  rows_ = 24;
  Reset(cols_, rows_);
  FeedWelcome(*this);
}

void Terminal::Reset(int cols, int rows) {
  cols_ = cols < 1 ? 1 : cols;
  rows_ = rows < 1 ? 1 : rows;
  cx_ = 0;
  cy_ = 0;
  curFg_ = 7;
  curBg_ = 0;
  curBold_ = false;
  screen_.clear();
  for (int i = 0; i < rows_; ++i) screen_.push_back(BlankLine(cols_));
  out_.clear();
  dirty_ = true;
}

void Terminal::Resize(int cols, int rows) {
  if (cols < 1) cols = 1;
  if (rows < 1) rows = 1;
  int oldRows = static_cast<int>(screen_.size());
  std::deque<Line> old;
  old.swap(screen_);
  cols_ = cols;
  rows_ = rows;
  for (int i = 0; i < rows_; ++i) screen_.push_back(BlankLine(cols_));
  int copyRows = oldRows < rows_ ? oldRows : rows_;
  int srcStart = oldRows - copyRows;
  int dstStart = rows_ - copyRows;
  for (int r = 0; r < copyRows; ++r) {
    const Line& s = old[static_cast<size_t>(srcStart + r)];
    Line& d = screen_[static_cast<size_t>(dstStart + r)];
    int n = static_cast<int>(s.cells.size());
    if (n > cols_) n = cols_;
    for (int c = 0; c < n; ++c) d.cells[static_cast<size_t>(c)] = s.cells[static_cast<size_t>(c)];
  }
  if (cx_ >= cols_) cx_ = cols_ - 1;
  if (cx_ < 0) cx_ = 0;
  if (cy_ >= rows_) cy_ = rows_ - 1;
  if (cy_ < 0) cy_ = 0;
  dirty_ = true;
}

void Terminal::NewLine() {
  ++cy_;
  if (cy_ >= rows_) {
    if (!screen_.empty()) screen_.pop_front();
    screen_.push_back(BlankLine(cols_));
    cy_ = rows_ - 1;
  }
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  if (cy_ < 0) cy_ = 0;
  dirty_ = true;
}

void Terminal::PutChar(wchar_t ch) {
  if (cols_ < 1 || rows_ < 1) return;
  if (cx_ >= cols_) {
    cx_ = 0;
    NewLine();
  }
  if (cx_ < 0) cx_ = 0;
  if (cy_ < 0) cy_ = 0;
  if (cy_ >= rows_) cy_ = rows_ - 1;
  while (static_cast<int>(screen_.size()) <= cy_) screen_.push_back(BlankLine(cols_));
  Line& ln = screen_[static_cast<size_t>(cy_)];
  if (static_cast<int>(ln.cells.size()) < cols_) ln.cells.resize(static_cast<size_t>(cols_), Cell());
  Cell& cell = ln.cells[static_cast<size_t>(cx_)];
  cell.ch = ch ? ch : L' ';
  cell.fg = curFg_;
  cell.bg = curBg_;
  cell.bold = curBold_;
  ++cx_;
  if (cx_ >= cols_) {
    cx_ = 0;
    NewLine();
  }
  dirty_ = true;
}

void Terminal::CarriageReturn() {
  cx_ = 0;
  dirty_ = true;
}

void Terminal::Backspace() {
  if (cx_ > 0) {
    --cx_;
  } else if (cy_ > 0) {
    --cy_;
    cx_ = cols_ - 1;
  } else {
    return;
  }
  if (cy_ >= 0 && cy_ < static_cast<int>(screen_.size())) {
    Line& ln = screen_[static_cast<size_t>(cy_)];
    if (cx_ >= 0 && cx_ < static_cast<int>(ln.cells.size())) {
      Cell& c = ln.cells[static_cast<size_t>(cx_)];
      c.ch = L' ';
      c.fg = curFg_;
      c.bg = curBg_;
    }
  }
  dirty_ = true;
}

void Terminal::ClearAll() {
  int width = cols_ < 1 ? 1 : cols_;
  for (size_t i = 0; i < screen_.size(); ++i) {
    screen_[i].cells.assign(static_cast<size_t>(width), Cell());
  }
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  cx_ = 0;
  cy_ = 0;
  dirty_ = true;
}

void Terminal::ParseSgr(const std::vector<int>& args) {
  if (args.empty()) {
    curFg_ = 7;
    curBg_ = 0;
    curBold_ = false;
    return;
  }
  for (size_t i = 0; i < args.size(); ++i) {
    int a = args[i];
    if (a == 0) {
      curFg_ = 7;
      curBg_ = 0;
      curBold_ = false;
    } else if (a == 1) {
      curBold_ = true;
    } else if (a == 22) {
      curBold_ = false;
    } else if (a >= 30 && a <= 37) {
      curFg_ = static_cast<unsigned char>(a - 30);
    } else if (a == 39) {
      curFg_ = 8;
    } else if (a >= 40 && a <= 47) {
      curBg_ = static_cast<unsigned char>(a - 40);
    } else if (a == 49) {
      curBg_ = 0;
    } else if (a >= 90 && a <= 97) {
      curFg_ = static_cast<unsigned char>(a);
    }
  }
}

void Terminal::Feed(const std::string& utf8) {
  size_t i = 0;
  size_t n = utf8.size();
  while (i < n) {
    unsigned char c = static_cast<unsigned char>(utf8[i]);
    if (c == '\r') {
      CarriageReturn();
      ++i;
      continue;
    }
    if (c == '\n') {
      NewLine();
      ++i;
      continue;
    }
    if (c == '\b') {
      Backspace();
      ++i;
      continue;
    }
    if (c == '\t') {
      int stop = ((cx_ / 4) + 1) * 4;
      if (stop > cols_) stop = cols_;
      while (cx_ < stop) PutChar(L' ');
      ++i;
      continue;
    }
    if (c == 0x1B) {
      if (i + 1 >= n) return;
      if (utf8[i + 1] != '[') {
        ++i;
        if (i < n) ++i;
        continue;
      }
      size_t j = i + 2;
      while (j < n) {
        unsigned char d = static_cast<unsigned char>(utf8[j]);
        if (d >= 0x40 && d <= 0x7E) break;
        ++j;
      }
      if (j >= n) return;
      char finalByte = utf8[j];
      std::string raw = utf8.substr(i + 2, j - (i + 2));
      std::vector<int> args;
      int value = 0;
      bool has = false;
      for (size_t k = 0; k < raw.size(); ++k) {
        char d = raw[k];
        if (d >= '0' && d <= '9') {
          value = value * 10 + (d - '0');
          if (value > 100000) value = 100000;
          has = true;
        } else if (d == ';') {
          args.push_back(has ? value : 0);
          value = 0;
          has = false;
        } else {
          has = false;
          value = 0;
        }
      }
      if (has) args.push_back(value);
      if (finalByte == 'm') {
        ParseSgr(args);
      } else if (finalByte == 'K') {
        int mode = args.empty() ? 0 : args[0];
        if (cy_ >= 0 && cy_ < static_cast<int>(screen_.size())) {
          Line& ln = screen_[static_cast<size_t>(cy_)];
          if (static_cast<int>(ln.cells.size()) < cols_) ln.cells.resize(static_cast<size_t>(cols_), Cell());
          int from = 0;
          int to = cols_;
          if (mode == 0) from = cx_ < 0 ? 0 : cx_;
          else if (mode == 1) to = cx_ + 1;
          if (from < 0) from = 0;
          if (to > cols_) to = cols_;
          for (int k = from; k < to; ++k) ln.cells[static_cast<size_t>(k)] = Cell();
        }
      } else if (finalByte == 'J') {
        ClearAll();
      } else if (finalByte == 'H' || finalByte == 'f') {
        int row = args.size() > 0 ? args[0] : 1;
        int col = args.size() > 1 ? args[1] : 1;
        if (row < 1) row = 1;
        if (col < 1) col = 1;
        if (row > rows_) row = rows_;
        if (col > cols_) col = cols_;
        cy_ = row - 1;
        cx_ = col - 1;
        dirty_ = true;
      }
      i = j + 1;
      continue;
    }
    wchar_t buf[2] = { 0, 0 };
    int units = 0;
    int used = DecodeUtf8(utf8, i, buf, &units);
    if (used <= 0) return;
    for (int k = 0; k < units; ++k) PutChar(buf[k]);
    i += static_cast<size_t>(used);
  }
}

D2D1_COLOR_F Terminal::ColorOf(unsigned char idx, bool bold, const Palette& p) const {
  D2D1_COLOR_F c = p.termText;
  switch (idx) {
    case 0: c = p.termBg; break;
    case 1: c = p.danger; break;
    case 2: c = p.accent; break;
    case 3: c = p.warn; break;
    case 4: c = p.blue; break;
    case 5: c = p.violet; break;
    case 6: c = p.info; break;
    case 7: c = p.termText; break;
    case 8: c = p.termText; break;
    case 90: c = p.termDim; break;
    case 91: c = p.danger; break;
    case 92: c = p.accent; break;
    case 93: c = p.warn; break;
    case 94: c = p.blue; break;
    case 95: c = p.violet; break;
    case 96: c = p.info; break;
    case 97: c = p.termText; break;
    default: c = p.termText; break;
  }
  if (bold) {
    D2D1_COLOR_F tint = p.dark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)
                               : D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    c = Mix(c, tint, 0.3f);
  }
  return c;
}

void Terminal::Draw(Render& r, const Rect& area, float fontSize, const Palette& p) {
  float scale = r.Dpi() > 0.0f ? r.Dpi() / 96.0f : 1.0f;
  float fs = fontSize > 0.0f ? fontSize * scale : L::TermFontSize * scale;
  float cw = r.CharW(FontId::Mono) * fs / 13.0f;
  if (!(cw > 0.0f)) cw = fs * 0.6f;
  if (!(cw > 0.0f)) cw = 7.0f;
  float lh = fs * L::TermLineGap;
  if (!(lh > 0.0f)) lh = fs * 1.4f;

  r.Fill(area.D2D(), p.termBg);
  r.PushClip(area.D2D());

  int rowCount = static_cast<int>(screen_.size());
  if (rowCount > rows_) rowCount = rows_;
  for (int row = 0; row < rowCount; ++row) {
    const Line& ln = screen_[static_cast<size_t>(row)];
    float y = area.t + lh * static_cast<float>(row);
    if (y >= area.b) break;
    int count = static_cast<int>(ln.cells.size());
    if (count > cols_) count = cols_;
    int c = 0;
    while (c < count) {
      const Cell& first = ln.cells[static_cast<size_t>(c)];
      int start = c;
      while (c < count) {
        const Cell& cur = ln.cells[static_cast<size_t>(c)];
        if (cur.fg != first.fg || cur.bg != first.bg || cur.bold != first.bold) break;
        ++c;
      }
      std::wstring seg;
      seg.reserve(static_cast<size_t>(c - start));
      bool hasText = false;
      for (int k = start; k < c; ++k) {
        wchar_t wc = ln.cells[static_cast<size_t>(k)].ch;
        if (wc == 0) wc = L' ';
        if (wc != L' ') hasText = true;
        seg.push_back(wc);
      }
      float x0 = area.l + cw * static_cast<float>(start);
      float x1 = area.l + cw * static_cast<float>(c);
      if (first.bg != 0) {
        r.Fill(D2D1::RectF(x0, y, x1, y + lh), ColorOf(first.bg, false, p));
      }
      if (hasText && !seg.empty()) {
        r.Text(seg.c_str(), D2D1::RectF(x0, y, area.r, y + lh),
               ColorOf(first.fg, first.bold, p), FontId::Mono);
      }
    }
  }

  if (cy_ >= 0 && cy_ < rows_ && cx_ >= 0 && cx_ <= cols_) {
    float x = area.l + cw * static_cast<float>(cx_);
    float y = area.t + lh * static_cast<float>(cy_);
    float cursorW = 7.0f * scale;
    float cursorH = lh * 0.85f;
    if (x + cursorW > area.r) x = area.r - cursorW;
    if (x < area.l) x = area.l;
    r.Fill(D2D1::RectF(x, y, x + cursorW, y + cursorH), p.accent);
  }

  r.PopClip();
  dirty_ = false;
}

void Terminal::OnKey(wchar_t ch, bool ctrl, bool alt, bool shift) {
  (void)shift;
  if (ch == L'\r' || ch == L'\n') {
    std::wstring cmd;
    if (cy_ >= 0 && cy_ < static_cast<int>(screen_.size())) {
      const Line& ln = screen_[static_cast<size_t>(cy_)];
      int n = static_cast<int>(ln.cells.size());
      if (n > cx_) n = cx_;
      for (int i = 0; i < n; ++i) cmd.push_back(ln.cells[static_cast<size_t>(i)].ch);
    }
    CarriageReturn();
    NewLine();
    cx_ = 0;
    out_ += cmd;
    out_ += L"\r\n";
    std::wstring cmdTrim = TrimW(cmd);
    std::wstring low = cmdTrim;
    for (size_t i = 0; i < low.size(); ++i) {
      if (low[i] >= L'A' && low[i] <= L'Z') low[i] = static_cast<wchar_t>(low[i] - L'A' + L'a');
    }
    if (low == L"clear") {
      ClearAll();
    } else if (low == L"ls") {
      Feed("\x1b[36mbin\x1b[0m  \x1b[36mdocs\x1b[0m  \x1b[36mprojects\x1b[0m  \x1b[36mnotes\x1b[0m  deploy.sh  README.md\r\n");
    } else if (low == L"pwd") {
      Feed("/home/skiff\r\n");
    } else if (low == L"whoami") {
      Feed("skiff\r\n");
    } else if (low == L"uptime") {
      Feed(" 09:41:07 up 42 days,  3:18,  1 user,  load average: 0.08, 0.11, 0.09\r\n");
    } else if (low == L"help") {
      Feed("\x1b[90mls  pwd  whoami  uptime  clear  help\x1b[0m\r\n");
    }
    dirty_ = true;
    return;
  }
  if (ch == L'\b' || ch == 127) {
    Backspace();
    out_.push_back(L'\b');
    dirty_ = true;
    return;
  }
  if (ch == L'\t') {
    int stop = ((cx_ / 4) + 1) * 4;
    if (stop > cols_) stop = cols_;
    while (cx_ < stop) {
      PutChar(L' ');
      out_.push_back(L' ');
    }
    dirty_ = true;
    return;
  }
  if (ch < 32) return;
  if (ctrl || alt) return;
  PutChar(ch);
  out_.push_back(ch);
  dirty_ = true;
}

std::wstring Terminal::TakeOut() {
  std::wstring s;
  s.swap(out_);
  return s;
}

void Terminal::ScrollToBottom() {
  cy_ = rows_ - 1;
  if (cy_ < 0) cy_ = 0;
  dirty_ = true;
}

}
