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
  void ScrollLines(int lines);
  int ScrollBack() const { return scrollBack_; }
  void SetFocused(bool f) { focused_ = f; }
  bool Focused() const { return focused_; }
  void SetSelection(int r0, int c0, int r1, int c1);
  void ClearSelection();
  bool HasSelection() const { return selR0_ >= 0; }
  std::wstring SelectedText() const;
  void SelectWordAt(int row, int col);
  void SelectLineAt(int row);
  bool AppCursor() const { return appCursor_; }

private:
  void NewLine();
  void PutChar(wchar_t ch, wchar_t ch2 = 0);
  void CarriageReturn();
  void Backspace();
  void ClearAll();
  void EraseDisplay(int mode);
  void EraseLine(int mode);
  void InsertLines(int n);
  void DeleteLines(int n);
  void InsertChars(int n);
  void DeleteChars(int n);
  void ScrollScreenUp(int n);
  void ScrollScreenDown(int n);
  void ParseSgr(const std::vector<int>& args);
  void HoldSeq(const std::string& src, size_t from);
  D2D1_COLOR_F ColorOf(unsigned char idx, bool bold, const Palette& p) const;
  const Line& ViewLine(int idx) const;

  static constexpr int kHistMax = 2000;

  int cols_ = 80;
  int rows_ = 24;
  int cx_ = 0;
  int cy_ = 0;
  unsigned char curFg_ = 7;
  unsigned char curBg_ = 0;
  bool curBold_ = false;
  std::deque<Line> screen_;
  std::deque<Line> history_;
  int scrollBack_ = 0;
  std::wstring out_;
  bool dirty_ = true;
  bool focused_ = true;
  std::string seq_;
  bool wrapPending_ = false;
  bool appCursor_ = false;
  bool csiWarned_ = false;
  int savedCx_ = 0;
  int savedCy_ = 0;
  bool alt_ = false;
  std::deque<Line> mainScreen_;
  int mainCx_ = 0;
  int mainCy_ = 0;
  int cursor_ = 0;
  int selR0_ = -1;
  int selC0_ = -1;
  int selR1_ = -1;
  int selC1_ = -1;
};

}
