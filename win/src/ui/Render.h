#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "app/Theme.h"

namespace sk {

enum class FontId { Display = 0, Title, Body, Small, Caption, Micro, Mono, MonoSm, Count };
enum class Align { Left = 0, Center, Right };

class Render {
public:
  bool Init(HWND hwnd);
  void Shutdown();
  bool Begin();
  void End();
  void Resize(UINT pxW, UINT pxH, float dpi);
  void SetUiScale(float s);
  void SetMonoFamily(int idx);

  ID2D1RenderTarget* Target() const { return rt_; }
  float Dpi() const { return dpi_; }

  void Fill(const D2D1_RECT_F& rc, D2D1_COLOR_F c);
  void FillRound(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F c);
  void StrokeRound(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F c, float w = 1.0f);
  void Circle(float cx, float cy, float radius, D2D1_COLOR_F c);
  void Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float w = 1.0f);
  void Text(const wchar_t* s, const D2D1_RECT_F& rc, D2D1_COLOR_F c, FontId f, Align a = Align::Left);
  void TextVertCenter(const wchar_t* s, const D2D1_RECT_F& rc, D2D1_COLOR_F c, FontId f, Align a = Align::Left);

  float Measure(const wchar_t* s, FontId f);
  float LineH(FontId f);
  float CharW(FontId f);

  void PushClip(const D2D1_RECT_F& rc);
  void PopClip();

private:
  HWND hwnd_ = nullptr;
  ID2D1Factory* factory_ = nullptr;
  ID2D1HwndRenderTarget* rt_ = nullptr;
  IDWriteFactory* dwrite_ = nullptr;
  IDWriteTextFormat* fonts_[static_cast<int>(FontId::Count)]{};
  ID2D1SolidColorBrush* brush_ = nullptr;
  float dpi_ = 96.0f;
  float uiScale_ = 1.0f;
  int monoFamily_ = 0;
  float fontScale_ = 0.0f;
  int clip_ = 0;
};

}
