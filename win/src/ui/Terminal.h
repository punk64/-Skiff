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
  void Draw(Render& r, const Rect& area, float fontSize, const Palette& p);
  void OnKey(wchar_t ch, bool ctrl, bool alt, bool shift);
  std::wstring TakeOut();
  int Cols() const { return cols_; }
  int Rows() const { return rows_; }
  void ScrollToBottom();
  bool HasSelection() const { return false; }

private:
  void NewLine();
  void PutChar(wchar_t ch, wchar_t ch2 = 0);
  void CarriageReturn();
  void Backspace();
  void ClearAll();
  void ParseSgr(const std::vector<int>& args);
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
};

}
