#pragma once
#include <string>
#include <vector>
#include <deque>
#include "ui/Render.h"
#include "ui/Chrome.h"

namespace sk {

struct Cell {
  wchar_t ch = L' ';
  wchar_t ch2 = 0;
  unsigned char fg = 7;
  unsigned char bg = 0;
  bool bold = false;
  bool cont = false;
};

struct Line {
  std::vector<Cell> cells;
};

class Terminal {
public:
  Terminal();
  void Reset(int cols, int rows);
  void Resize(int cols, int rows);
  void Feed(const std::string& utf8);
  void Draw(Render& r, const Rect& area, float fontSize, const Palette& p, int cursor = 0);
  void OnKey(wchar_t ch, bool ctrl, bool alt, bool shift);
  std::wstring TakeOut();
  int Cols() const { return cols_; }
  int Rows() const { return rows_; }
  void ScrollToBottom();
  void SetFocused(bool f) { focused_ = f; }
  bool Focused() const { return focused_; }
  void SetSelection(int r0, int c0, int r1, int c1);
  void ClearSelection();
  bool HasSelection() const { return selR0_ >= 0; }
  std::wstring SelectedText() const;

private:
  void NewLine();
  void PutChar(wchar_t ch, wchar_t ch2 = 0);
  void CarriageReturn();
  void Backspace();
  void ClearAll();
  void ParseSgr(const std::vector<int>& args);
  void HoldSeq(const std::string& src, size_t from);
  D2D1_COLOR_F ColorOf(unsigned char idx, bool bold, const Palette& p) const;

  int cols_ = 80;
  int rows_ = 24;
  int cx_ = 0;
  int cy_ = 0;
  unsigned char curFg_ = 7;
  unsigned char curBg_ = 0;
  bool curBold_ = false;
  std::deque<Line> screen_;
  std::wstring out_;
  bool dirty_ = true;
  bool focused_ = true;
  std::string seq_;
  int cursor_ = 0;
  int selR0_ = -1;
  int selC0_ = -1;
  int selR1_ = -1;
  int selC1_ = -1;
};

}
