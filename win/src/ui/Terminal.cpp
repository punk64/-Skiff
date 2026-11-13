#include "ui/Terminal.h"
#include "app/Layout.h"
#include "compat/Log.h"
#include <d2d1helper.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>

namespace sk {

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

bool WideCp(unsigned cp) {
  if (cp >= 0x1100u && cp <= 0x115Fu) return true;
  if (cp >= 0x2E80u && cp <= 0x303Eu) return true;
  if (cp >= 0x3041u && cp <= 0x33FFu) return true;
  if (cp >= 0x3400u && cp <= 0x4DBFu) return true;
  if (cp >= 0x4E00u && cp <= 0x9FFFu) return true;
  if (cp >= 0xA000u && cp <= 0xA4CFu) return true;
  if (cp >= 0xA960u && cp <= 0xA97Fu) return true;
  if (cp >= 0xAC00u && cp <= 0xD7A3u) return true;
  if (cp >= 0xF900u && cp <= 0xFAFFu) return true;
  if (cp >= 0xFE10u && cp <= 0xFE19u) return true;
  if (cp >= 0xFE30u && cp <= 0xFE6Fu) return true;
  if (cp >= 0xFF00u && cp <= 0xFF60u) return true;
  if (cp >= 0xFFE0u && cp <= 0xFFE6u) return true;
  return false;
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
  w += L"\x1b[36m提示：Ctrl+K 搜索主机 · Ctrl+, 设置 · F11 全屏终端\x1b[0m\r\n";
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
  history_.clear();
  scrollBack_ = 0;
  alt_ = false;
  mainScreen_.clear();
  wrapPending_ = false;
  savedCx_ = 0;
  savedCy_ = 0;
  csiWarned_ = false;
  for (int i = 0; i < rows_; ++i) screen_.push_back(BlankLine(cols_));
  out_.clear();
  dirty_ = true;
}

void Terminal::Resize(int cols, int rows) {
  if (cols < 1) cols = 1;
  if (rows < 1) rows = 1;
  int oldRows = static_cast<int>(screen_.size());
  if (cols == cols_ && rows == rows_ && oldRows == rows_) return;
  std::deque<Line> old;
  old.swap(screen_);
  alt_ = false;
  mainScreen_.clear();
  history_.clear();
  scrollBack_ = 0;
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
  cy_ += dstStart - srcStart;
  if (cx_ >= cols_) cx_ = cols_ - 1;
  if (cx_ < 0) cx_ = 0;
  if (cy_ >= rows_) cy_ = rows_ - 1;
  if (cy_ < 0) cy_ = 0;
  if (selR0_ >= 0) {
    selR0_ += dstStart - srcStart;
    selR1_ += dstStart - srcStart;
    if (selR0_ < 0) selR0_ = 0;
    if (selR1_ < 0) selR1_ = 0;
  }
  dirty_ = true;
}

void Terminal::NewLine() {
  ++cy_;
  if (cy_ >= rows_) {
    if (!screen_.empty()) {
      history_.push_back(std::move(screen_.front()));
      while (static_cast<int>(history_.size()) > kHistMax) history_.pop_front();
      if (scrollBack_ > 0 && scrollBack_ < kHistMax) ++scrollBack_;
      screen_.pop_front();
    }
    screen_.push_back(BlankLine(cols_));
    cy_ = rows_ - 1;
  }
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  if (cy_ < 0) cy_ = 0;
  dirty_ = true;
}

void Terminal::PutChar(wchar_t ch, wchar_t ch2) {
  if (cols_ < 1 || rows_ < 1) return;
  if (ch == 0) ch = L' ';
  int w = (ch2 != 0 || WideCp(static_cast<unsigned>(ch))) ? 2 : 1;
  if (w > cols_) w = 1;
  if (wrapPending_ || cx_ + w > cols_) {
    wrapPending_ = false;
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
  cell.ch = ch;
  cell.ch2 = ch2;
  cell.fg = curFg_;
  cell.bg = curBg_;
  cell.bold = curBold_;
  cell.cont = false;
  if (w == 2) {
    Cell& tail = ln.cells[static_cast<size_t>(cx_ + 1)];
    tail.ch = L' ';
    tail.ch2 = 0;
    tail.fg = curFg_;
    tail.bg = curBg_;
    tail.bold = curBold_;
    tail.cont = true;
  }
  cx_ += w;
  if (cx_ >= cols_) {
    cx_ = cols_;
    wrapPending_ = true;
  }
  dirty_ = true;
}

void Terminal::CarriageReturn() {
  cx_ = 0;
  wrapPending_ = false;
  dirty_ = true;
}

void Terminal::Backspace() {
  if (wrapPending_) {
    wrapPending_ = false;
    cx_ = cols_ > 0 ? cols_ - 1 : 0;
    dirty_ = true;
    return;
  }
  if (cx_ > 0) {
    --cx_;
    dirty_ = true;
  }
}

void Terminal::ClearAll() {
  int width = cols_ < 1 ? 1 : cols_;
  for (size_t i = 0; i < screen_.size(); ++i) {
    screen_[i].cells.assign(static_cast<size_t>(width), Cell());
  }
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  cx_ = 0;
  cy_ = 0;
  wrapPending_ = false;
  dirty_ = true;
}

void Terminal::EraseDisplay(int mode) {
  int width = cols_ < 1 ? 1 : cols_;
  if (mode == 3) {
    history_.clear();
    scrollBack_ = 0;
    dirty_ = true;
    return;
  }
  if (screen_.empty()) return;
  if (mode == 0 || mode == 1) {
    if (mode == 1) {
      for (int r = 0; r < cy_ && r < static_cast<int>(screen_.size()); ++r) {
        screen_[static_cast<size_t>(r)].cells.assign(static_cast<size_t>(width), Cell());
      }
    }
    if (cy_ >= 0 && cy_ < static_cast<int>(screen_.size())) {
      Line& ln = screen_[static_cast<size_t>(cy_)];
      if (static_cast<int>(ln.cells.size()) < width) ln.cells.resize(static_cast<size_t>(width), Cell());
      int from = 0;
      int to = width;
      if (mode == 0) {
        from = cx_ < 0 ? 0 : cx_;
        if (from > width) from = width;
      } else {
        to = cx_ + 1;
        if (to < 0) to = 0;
        if (to > width) to = width;
      }
      for (int k = from; k < to; ++k) ln.cells[static_cast<size_t>(k)] = Cell();
    }
    if (mode == 0) {
      for (int r = cy_ + 1; r < static_cast<int>(screen_.size()); ++r) {
        screen_[static_cast<size_t>(r)].cells.assign(static_cast<size_t>(width), Cell());
      }
    }
  } else {
    for (size_t r = 0; r < screen_.size(); ++r) {
      screen_[r].cells.assign(static_cast<size_t>(width), Cell());
    }
    cx_ = 0;
    cy_ = 0;
    wrapPending_ = false;
  }
  dirty_ = true;
}

void Terminal::EraseLine(int mode) {
  if (cy_ < 0 || cy_ >= static_cast<int>(screen_.size())) return;
  if (mode != 0 && mode != 1 && mode != 2) return;
  int width = cols_ < 1 ? 1 : cols_;
  Line& ln = screen_[static_cast<size_t>(cy_)];
  if (static_cast<int>(ln.cells.size()) < width) ln.cells.resize(static_cast<size_t>(width), Cell());
  int from = 0;
  int to = width;
  if (mode == 0) {
    from = cx_ < 0 ? 0 : cx_;
    if (from > width) from = width;
  } else if (mode == 1) {
    to = cx_ + 1;
    if (to < 0) to = 0;
    if (to > width) to = width;
  }
  for (int k = from; k < to; ++k) ln.cells[static_cast<size_t>(k)] = Cell();
  dirty_ = true;
}

void Terminal::InsertLines(int n) {
  if (n < 1) n = 1;
  if (rows_ < 1) return;
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  if (cy_ < 0 || cy_ >= static_cast<int>(screen_.size())) return;
  for (int k = 0; k < n; ++k) {
    screen_.insert(screen_.begin() + cy_, BlankLine(cols_));
    screen_.pop_back();
  }
  dirty_ = true;
}

void Terminal::DeleteLines(int n) {
  if (n < 1) n = 1;
  if (rows_ < 1) return;
  while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
  for (int k = 0; k < n; ++k) {
    if (cy_ < 0 || cy_ >= static_cast<int>(screen_.size())) break;
    screen_.erase(screen_.begin() + cy_);
    screen_.push_back(BlankLine(cols_));
  }
  dirty_ = true;
}

void Terminal::InsertChars(int n) {
  if (n < 1) n = 1;
  if (cy_ < 0 || cy_ >= static_cast<int>(screen_.size())) return;
  int width = cols_ < 1 ? 1 : cols_;
  Line& ln = screen_[static_cast<size_t>(cy_)];
  if (static_cast<int>(ln.cells.size()) < width) ln.cells.resize(static_cast<size_t>(width), Cell());
  if (cx_ < 0) cx_ = 0;
  if (cx_ >= width) return;
  if (n > width - cx_) n = width - cx_;
  ln.cells.insert(ln.cells.begin() + cx_, static_cast<size_t>(n), Cell());
  ln.cells.resize(static_cast<size_t>(width));
  dirty_ = true;
}

void Terminal::DeleteChars(int n) {
  if (n < 1) n = 1;
  if (cy_ < 0 || cy_ >= static_cast<int>(screen_.size())) return;
  int width = cols_ < 1 ? 1 : cols_;
  Line& ln = screen_[static_cast<size_t>(cy_)];
  if (static_cast<int>(ln.cells.size()) < width) ln.cells.resize(static_cast<size_t>(width), Cell());
  if (cx_ < 0) cx_ = 0;
  if (cx_ >= width) return;
  if (n > width - cx_) n = width - cx_;
  ln.cells.erase(ln.cells.begin() + cx_, ln.cells.begin() + cx_ + n);
  ln.cells.resize(static_cast<size_t>(width), Cell());
  dirty_ = true;
}

void Terminal::ScrollScreenUp(int n) {
  if (n < 1) n = 1;
  if (screen_.empty()) return;
  for (int k = 0; k < n; ++k) {
    screen_.pop_front();
    screen_.push_back(BlankLine(cols_));
  }
  dirty_ = true;
}

void Terminal::ScrollScreenDown(int n) {
  if (n < 1) n = 1;
  if (screen_.empty()) return;
  for (int k = 0; k < n; ++k) {
    screen_.pop_back();
    screen_.push_front(BlankLine(cols_));
  }
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

void Terminal::HoldSeq(const std::string& src, size_t from) {
  seq_ = src.substr(from);
  if (seq_.size() > 4096) seq_.clear();
}

void Terminal::Feed(const std::string& utf8) {
  std::string src;
  if (seq_.empty()) {
    src = utf8;
  } else {
    src.swap(seq_);
    src += utf8;
  }
  size_t i = 0;
  size_t n = src.size();
  while (i < n) {
    unsigned char c = static_cast<unsigned char>(src[i]);
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
    if (c == 0x07) {
      ++i;
      continue;
    }
    if (c == 0x1B) {
      if (i + 1 >= n) {
        HoldSeq(src, i);
        return;
      }
      char kind = src[i + 1];
      if (kind == ']' || kind == 'P' || kind == 'X' || kind == '^' || kind == '_') {
        size_t j = i + 2;
        size_t stop = 0;
        bool closed = false;
        while (j < n) {
          unsigned char d = static_cast<unsigned char>(src[j]);
          if (d == 0x07) {
            stop = j + 1;
            closed = true;
            break;
          }
          if (d == 0x1B && j + 1 < n && src[j + 1] == '\\') {
            stop = j + 2;
            closed = true;
            break;
          }
          ++j;
        }
        if (!closed) {
        HoldSeq(src, i);
        return;
        }
        i = stop;
        continue;
      }
      if (kind == '(' || kind == ')' || kind == '*' || kind == '+' || kind == '-' ||
          kind == '.' || kind == '/') {
        if (i + 2 >= n) {
        HoldSeq(src, i);
        return;
        }
        i += 3;
        continue;
      }
      if (kind == '7' || kind == '8' || kind == 'D' || kind == 'E' || kind == 'M') {
        if (kind == '7') {
          savedCx_ = cx_;
          savedCy_ = cy_;
        } else if (kind == '8') {
          cx_ = savedCx_;
          cy_ = savedCy_;
          if (cx_ < 0) cx_ = 0;
          if (cx_ > cols_ - 1) cx_ = cols_ - 1;
          if (cy_ < 0) cy_ = 0;
          if (cy_ > rows_ - 1) cy_ = rows_ - 1;
          wrapPending_ = false;
          dirty_ = true;
        } else if (kind == 'D') {
          NewLine();
        } else if (kind == 'E') {
          CarriageReturn();
          NewLine();
        } else {
          if (cy_ > 0) --cy_;
          else ScrollScreenDown(1);
          wrapPending_ = false;
          dirty_ = true;
        }
        i += 2;
        continue;
      }
      if (kind != '[') {
        i += 2;
        continue;
      }
      size_t j = i + 2;
      while (j < n) {
        unsigned char d = static_cast<unsigned char>(src[j]);
        if (d >= 0x40 && d <= 0x7E) break;
        ++j;
      }
      if (j >= n) {
        HoldSeq(src, i);
        return;
      }
      char finalByte = src[j];
      std::string raw = src.substr(i + 2, j - (i + 2));
      bool priv = !raw.empty() && (raw[0] == '?' || raw[0] == '>' || raw[0] == '<' || raw[0] == '=');
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
      int a0 = args.size() > 0 ? args[0] : 1;
      if (a0 < 1) a0 = 1;
      if (finalByte == 'm') {
        ParseSgr(args);
      } else if (finalByte == 'K') {
        EraseLine(args.empty() ? 0 : args[0]);
      } else if (finalByte == 'J') {
        EraseDisplay(args.empty() ? 0 : args[0]);
      } else if (finalByte == 'H' || finalByte == 'f') {
        int row = args.size() > 0 ? args[0] : 1;
        int col = args.size() > 1 ? args[1] : 1;
        if (row < 1) row = 1;
        if (col < 1) col = 1;
        if (row > rows_) row = rows_;
        if (col > cols_) col = cols_;
        cy_ = row - 1;
        cx_ = col - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'A') {
        cy_ -= a0;
        if (cy_ < 0) cy_ = 0;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'B') {
        cy_ += a0;
        if (cy_ > rows_ - 1) cy_ = rows_ - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'C') {
        cx_ += a0;
        if (cx_ > cols_ - 1) cx_ = cols_ - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'D') {
        cx_ -= a0;
        if (cx_ < 0) cx_ = 0;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'E') {
        cy_ += a0;
        if (cy_ > rows_ - 1) cy_ = rows_ - 1;
        cx_ = 0;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'F') {
        cy_ -= a0;
        if (cy_ < 0) cy_ = 0;
        cx_ = 0;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'G') {
        cx_ = a0 - 1;
        if (cx_ < 0) cx_ = 0;
        if (cx_ > cols_ - 1) cx_ = cols_ - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'd') {
        cy_ = a0 - 1;
        if (cy_ < 0) cy_ = 0;
        if (cy_ > rows_ - 1) cy_ = rows_ - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (finalByte == 'L') {
        InsertLines(a0);
      } else if (finalByte == 'M') {
        DeleteLines(a0);
      } else if (finalByte == '@') {
        InsertChars(a0);
      } else if (finalByte == 'P') {
        DeleteChars(a0);
      } else if (finalByte == 'S') {
        ScrollScreenUp(a0);
      } else if (finalByte == 'T') {
        ScrollScreenDown(a0);
      } else if (finalByte == 's') {
        savedCx_ = cx_;
        savedCy_ = cy_;
      } else if (finalByte == 'u') {
        cx_ = savedCx_;
        cy_ = savedCy_;
        if (cx_ < 0) cx_ = 0;
        if (cx_ > cols_ - 1) cx_ = cols_ - 1;
        if (cy_ < 0) cy_ = 0;
        if (cy_ > rows_ - 1) cy_ = rows_ - 1;
        wrapPending_ = false;
        dirty_ = true;
      } else if (priv && (finalByte == 'h' || finalByte == 'l') && a0 == 1) {
        appCursor_ = (finalByte == 'h');
      } else if (priv && (finalByte == 'h' || finalByte == 'l') && a0 == 1049) {
        if (finalByte == 'h' && !alt_) {
          mainScreen_ = screen_;
          mainCx_ = cx_;
          mainCy_ = cy_;
          alt_ = true;
          ClearAll();
        } else if (finalByte == 'l' && alt_) {
          screen_ = std::move(mainScreen_);
          mainScreen_.clear();
          cx_ = mainCx_;
          cy_ = mainCy_;
          if (cx_ < 0) cx_ = 0;
          if (cx_ > cols_ - 1) cx_ = cols_ - 1;
          if (cy_ < 0) cy_ = 0;
          if (cy_ > rows_ - 1) cy_ = rows_ - 1;
          alt_ = false;
          wrapPending_ = false;
          while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine(cols_));
          dirty_ = true;
        }
      } else if (!csiWarned_) {
        csiWarned_ = true;
        SK_LOGW("term: unhandled CSI args='%s' final=%c", raw.c_str(), finalByte);
      }
      i = j + 1;
      continue;
    }
    wchar_t pair[2] = { 0, 0 };
    int units = 0;
    int used = DecodeUtf8(src, i, pair, &units);
    if (used <= 0) return;
    PutChar(pair[0], units == 2 ? pair[1] : 0);
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

static bool IsWordChar(wchar_t c) {
  if (c >= L'a' && c <= L'z') return true;
  if (c >= L'A' && c <= L'Z') return true;
  if (c >= L'0' && c <= L'9') return true;
  return c == L'_' || c == L'.' || c == L'/' || c == L'-' || c == L'~' || c == L':' || c == L'@';
}

void Terminal::SelectWordAt(int row, int col) {
  if (row < 0 || row >= rows_) return;
  const Line& ln = ViewLine(static_cast<int>(history_.size()) - scrollBack_ + row);
  int n = static_cast<int>(ln.cells.size());
  if (n > cols_) n = cols_;
  if (n <= 0) return;
  if (col < 0) col = 0;
  if (col >= n) col = n - 1;
  int a = col;
  int b = col;
  while (a > 0 && IsWordChar(ln.cells[static_cast<size_t>(a - 1)].ch)) --a;
  while (b + 1 < n && IsWordChar(ln.cells[static_cast<size_t>(b + 1)].ch)) ++b;
  SetSelection(row, a, row, b);
}

void Terminal::SelectLineAt(int row) {
  if (row < 0 || row >= rows_) return;
  SetSelection(row, 0, row, cols_ > 0 ? cols_ - 1 : 0);
}

void Terminal::SetSelection(int r0, int c0, int r1, int c1) {
  selR0_ = r0;
  selC0_ = c0;
  selR1_ = r1;
  selC1_ = c1;
  dirty_ = true;
}

void Terminal::ClearSelection() {
  selR0_ = -1;
  selC0_ = -1;
  selR1_ = -1;
  selC1_ = -1;
  dirty_ = true;
}

std::wstring Terminal::SelectedText() const {
  if (selR0_ < 0) return std::wstring();
  int sr = selR0_, sc = selC0_, er = selR1_, ec = selC1_;
  if (sr > er || (sr == er && sc > ec)) {
    int t = sr; sr = er; er = t;
    t = sc; sc = ec; ec = t;
  }
  int maxRow = er;
  if (maxRow >= rows_) maxRow = rows_ - 1;
  int base = static_cast<int>(history_.size()) - scrollBack_;
  std::wstring out;
  for (int row = sr; row <= maxRow; ++row) {
    if (row < 0) continue;
    const Line& ln = ViewLine(base + row);
    int cs = row == sr ? sc : 0;
    int ce = row == er ? ec + 1 : cols_;
    if (cs < 0) cs = 0;
    if (ce > (int)ln.cells.size()) ce = (int)ln.cells.size();
    std::wstring line;
    for (int c = cs; c < ce; ++c) {
      const Cell& cell = ln.cells[static_cast<size_t>(c)];
      if (cell.cont) continue;
      line.push_back(cell.ch ? cell.ch : L' ');
      if (cell.ch2) line.push_back(cell.ch2);
    }
    while (!line.empty() && line.back() == L' ') line.pop_back();
    out += line;
    if (row < maxRow) out += L"\r\n";
  }
  return out;
}

void Terminal::Draw(Render& r, const Rect& area, float fontSize, const Palette& p, int cursor) {
  cursor_ = cursor;
  float scale = r.Dpi() > 0.0f ? r.Dpi() / 96.0f : 1.0f;
  float fs = fontSize > 0.0f ? fontSize * scale : L::TermFontSize * scale;
  float cw = r.CharW(FontId::Mono) * fs / 13.0f;
  if (!(cw > 0.0f)) cw = fs * 0.6f;
  if (!(cw > 0.0f)) cw = 7.0f;
  float lh = fs * L::TermLineGap;
  if (!(lh > 0.0f)) lh = fs * 1.4f;

  r.Fill(area.D2D(), p.termBg);
  r.PushClip(area.D2D());

  int total = static_cast<int>(history_.size() + screen_.size());
  int origin = static_cast<int>(history_.size()) - scrollBack_;
  static int logR0 = -2;
  static int logR1 = -2;
  if (selR0_ != logR0 || selR1_ != logR1) {
    logR0 = selR0_;
    logR1 = selR1_;
    SK_LOGI("term draw sel %d,%d-%d,%d sb=%d rows=%d total=%d", selR0_, selC0_, selR1_, selC1_,
            scrollBack_, rows_, total);
  }
  if (origin < 0) origin = 0;
  int rowCount = total - origin;
  if (rowCount > rows_) rowCount = rows_;
  for (int row = 0; row < rowCount; ++row) {
    const Line& ln = ViewLine(origin + row);
    float y = area.t + lh * static_cast<float>(row);
    if (y >= area.b) break;
    if (selR0_ >= 0) {
      int sr = selR0_, sc = selC0_, er = selR1_, ec = selC1_;
      if (sr > er || (sr == er && sc > ec)) {
        int t = sr; sr = er; er = t;
        t = sc; sc = ec; ec = t;
      }
      if (row >= sr && row <= er) {
        int cs = row == sr ? sc : 0;
        int ce = row == er ? ec + 1 : cols_;
        float x0 = area.l + cw * static_cast<float>(cs);
        float x1 = area.l + cw * static_cast<float>(ce);
        if (x1 > area.r) x1 = area.r;
        if (x1 > x0) {
          float mr = p.termBg.r + (p.accent.r - p.termBg.r) * 0.45f;
          float mg = p.termBg.g + (p.accent.g - p.termBg.g) * 0.45f;
          float mb = p.termBg.b + (p.accent.b - p.termBg.b) * 0.45f;
          r.Fill(D2D1::RectF(x0, y, x1, y + lh), D2D1::ColorF(mr, mg, mb, 1.0f));
          float bar = cw * 0.18f;
          if (bar < 2.0f) bar = 2.0f;
          r.Fill(D2D1::RectF(x0, y, x0 + bar, y + lh), p.accent);
        }
      }
    }
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
      float x0 = area.l + cw * static_cast<float>(start);
      float x1 = area.l + cw * static_cast<float>(c);
      if (first.bg != 0) r.Fill(D2D1::RectF(x0, y, x1, y + lh), ColorOf(first.bg, false, p));
      D2D1_COLOR_F fg = ColorOf(first.fg, first.bold, p);
      std::wstring seg;
      int segStart = -1;
      int k = start;
      while (k < c) {
        const Cell& cell = ln.cells[static_cast<size_t>(k)];
        if (cell.cont) {
          ++k;
          continue;
        }
        bool wide = (k + 1 < c) && ln.cells[static_cast<size_t>(k + 1)].cont;
        if (wide) {
          if (segStart >= 0 && !seg.empty()) {
            float sx = area.l + cw * static_cast<float>(segStart);
            float ex = area.l + cw * static_cast<float>(k);
            r.Text(seg.c_str(), D2D1::RectF(sx, y, ex, y + lh), fg, FontId::Mono);
            seg.clear();
            segStart = -1;
          }
          std::wstring wseg;
          wseg.push_back(cell.ch ? cell.ch : L' ');
          if (cell.ch2) wseg.push_back(cell.ch2);
          float sx = area.l + cw * static_cast<float>(k);
          r.Text(wseg.c_str(), D2D1::RectF(sx, y, sx + cw * 3.0f, y + lh), fg, FontId::Mono);
          k += 2;
          continue;
        }
        if (segStart < 0) segStart = k;
        seg.push_back(cell.ch ? cell.ch : L' ');
        if (cell.ch2) seg.push_back(cell.ch2);
        ++k;
      }
      if (segStart >= 0 && !seg.empty()) {
        float sx = area.l + cw * static_cast<float>(segStart);
        float ex = area.l + cw * static_cast<float>(segStart + static_cast<int>(seg.size()));
        if (ex > area.r) ex = area.r;
        r.Text(seg.c_str(), D2D1::RectF(sx, y, ex, y + lh), fg, FontId::Mono);
      }
    }
  }

  bool blinkOn = !focused_ || ((GetTickCount64() / 530ULL) % 2ULL) == 0ULL;
  if (blinkOn && scrollBack_ == 0 && selR0_ < 0 && cy_ >= 0 && cy_ < rows_ && cx_ >= 0 && cx_ <= cols_) {
    int wide = 1;
    if (cy_ < static_cast<int>(screen_.size())) {
      const Line& ln = screen_[static_cast<size_t>(cy_)];
      if (cx_ + 1 < static_cast<int>(ln.cells.size()) &&
          ln.cells[static_cast<size_t>(cx_ + 1)].cont) {
        wide = 2;
      }
    }
    float x = area.l + cw * static_cast<float>(cx_);
    float y = area.t + lh * static_cast<float>(cy_);
    float cursorW = cw * static_cast<float>(wide);
    float cursorH = lh * 0.86f;
    if (x + cursorW > area.r) x = area.r - cursorW;
    if (x < area.l) x = area.l;
    if (cursor_ == 1) {
      r.Fill(D2D1::RectF(x, y + lh * 0.07f, x + cursorW, y + lh * 0.07f + cursorH), p.accent);
    } else if (cursor_ == 2) {
      float uh = lh * 0.12f;
      if (uh < 2.0f) uh = 2.0f;
      r.Fill(D2D1::RectF(x, y + lh - uh, x + cursorW, y + lh), p.accent);
    } else {
      float lw = cw * 0.14f;
      if (lw < 2.0f) lw = 2.0f;
      r.Fill(D2D1::RectF(x, y + lh * 0.07f, x + lw, y + lh * 0.07f + cursorH), p.accent);
    }
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
  scrollBack_ = 0;
  dirty_ = true;
}

void Terminal::ScrollLines(int lines) {
  if (lines == 0 || rows_ < 1) return;
  int limit = static_cast<int>(history_.size());
  int next = scrollBack_ + lines;
  if (next < 0) next = 0;
  if (next > limit) next = limit;
  if (next == scrollBack_) return;
  scrollBack_ = next;
  if (scrollBack_ > 0) ClearSelection();
  dirty_ = true;
}

const Line& Terminal::ViewLine(int idx) const {
  int h = static_cast<int>(history_.size());
  if (idx < 0) idx = 0;
  if (idx < h) return history_[static_cast<size_t>(idx)];
  int s = idx - h;
  if (s < 0) s = 0;
  if (s >= static_cast<int>(screen_.size())) s = static_cast<int>(screen_.size()) - 1;
  if (s < 0) s = 0;
  return screen_[static_cast<size_t>(s)];
}

}
