#include "ui/Render.h"
#include "app/Layout.h"
#include "compat/Log.h"
#include <d2d1helper.h>

namespace sk {
namespace {

UINT32 TextLen(const wchar_t* s) {
  UINT32 n = 0;
  while (s && s[n]) ++n;
  return n;
}

float FontSizeOf(FontId f) {
  switch (f) {
    case FontId::Display: return L::FsDisplay;
    case FontId::Title: return L::FsTitle;
    case FontId::Body: return L::FsBody;
    case FontId::Small: return L::FsSmall;
    case FontId::Caption: return L::FsCaption;
    case FontId::Micro: return L::FsMicro;
    case FontId::Mono: return 13.0f;
    case FontId::MonoSm: return 11.5f;
    default: return L::FsBody;
  }
}

bool IsMonoFont(FontId f) {
  return f == FontId::Mono || f == FontId::MonoSm;
}

void ReleaseFonts(IDWriteTextFormat** fonts) {
  for (int i = 0; i < static_cast<int>(FontId::Count); ++i) {
    if (fonts[i]) {
      fonts[i]->Release();
      fonts[i] = nullptr;
    }
  }
}

void BuildFonts(IDWriteFactory* dw, IDWriteTextFormat** fonts, float dpi) {
  ReleaseFonts(fonts);
  if (!dw) return;
  float scale = dpi / 96.0f;
  for (int i = 0; i < static_cast<int>(FontId::Count); ++i) {
    FontId id = static_cast<FontId>(i);
    const wchar_t* family = IsMonoFont(id) ? L"Cascadia Mono" : L"Microsoft YaHei UI";
    IDWriteTextFormat* fmt = nullptr;
    HRESULT hr = dw->CreateTextFormat(
      family,
      nullptr,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      FontSizeOf(id) * scale,
      L"zh-cn",
      &fmt);
    if (SUCCEEDED(hr) && fmt) {
      fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
      fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
      fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      fonts[i] = fmt;
    }
  }
}

bool CreateTarget(ID2D1Factory* factory, HWND hwnd, UINT w, UINT h, ID2D1HwndRenderTarget** out) {
  if (!factory || !hwnd || !out) return false;
  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
  D2D1_HWND_RENDER_TARGET_PROPERTIES hprops =
    D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h), D2D1_PRESENT_OPTIONS_IMMEDIATELY);
  return SUCCEEDED(factory->CreateHwndRenderTarget(props, hprops, out));
}

void ClientSize(HWND hwnd, UINT* w, UINT* h) {
  RECT rc{};
  if (hwnd) GetClientRect(hwnd, &rc);
  *w = (UINT)(rc.right - rc.left);
  *h = (UINT)(rc.bottom - rc.top);
  if (*w == 0) *w = 1;
  if (*h == 0) *h = 1;
}

void ReleaseBrush(ID2D1SolidColorBrush** brush) {
  if (*brush) {
    (*brush)->Release();
    *brush = nullptr;
  }
}

}

bool Render::Init(HWND hwnd) {
  if (!hwnd) return false;
  hwnd_ = hwnd;
  if (!factory_) {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_))) {
      SK_LOGE("d2d factory failed");
      return false;
    }
  }
  if (!dwrite_) {
    HRESULT hr = DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED,
      __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown**>(&dwrite_));
    if (FAILED(hr)) {
      SK_LOGE("dwrite factory failed hr=%08x", (unsigned)hr);
      return false;
    }
  }
  UINT w = 1;
  UINT h = 1;
  ClientSize(hwnd, &w, &h);
  if (!rt_) {
    if (!CreateTarget(factory_, hwnd, w, h, &rt_)) {
      SK_LOGE("create hwnd target failed %ux%u", w, h);
      return false;
    }
  }
  rt_->SetDpi(96.0f, 96.0f);
  if (!brush_) {
    if (FAILED(rt_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_))) {
      SK_LOGE("create brush failed");
      return false;
    }
  }
  BuildFonts(dwrite_, fonts_, dpi_);
  clip_ = 0;
  return true;
}

void Render::Shutdown() {
  ReleaseFonts(fonts_);
  ReleaseBrush(&brush_);
  if (rt_) {
    rt_->Release();
    rt_ = nullptr;
  }
  if (dwrite_) {
    dwrite_->Release();
    dwrite_ = nullptr;
  }
  if (factory_) {
    factory_->Release();
    factory_ = nullptr;
  }
  clip_ = 0;
  hwnd_ = nullptr;
  dpi_ = 96.0f;
}

bool Render::Begin() {
  if (!rt_) return false;
  rt_->BeginDraw();
  rt_->SetTransform(D2D1::Matrix3x2F::Identity());
  return true;
}

void Render::End() {
  if (!rt_) return;
  HRESULT hr = rt_->EndDraw();
  static int fails = 0;
  if (FAILED(hr)) {
    if (fails < 5) {
      ++fails;
      SK_LOGE("enddraw hr=%08x", (unsigned)hr);
    }
    if (hr != D2DERR_RECREATE_TARGET) {
      if (hwnd_ && fails < 50) InvalidateRect(hwnd_, nullptr, FALSE);
      return;
    }
  }
  if (hr != D2DERR_RECREATE_TARGET) return;
  ReleaseBrush(&brush_);
  rt_->Release();
  rt_ = nullptr;
  clip_ = 0;
  if (!factory_ || !hwnd_) return;
  UINT w = 1;
  UINT h = 1;
  ClientSize(hwnd_, &w, &h);
  if (!CreateTarget(factory_, hwnd_, w, h, &rt_)) return;
  rt_->SetDpi(96.0f, 96.0f);
  rt_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
}

void Render::Resize(UINT pxW, UINT pxH, float dpi) {
  if (dpi > 0.0f) dpi_ = dpi;
  if (pxW == 0) pxW = 1;
  if (pxH == 0) pxH = 1;
  clip_ = 0;
  if (!rt_) {
    if (factory_ && hwnd_ && CreateTarget(factory_, hwnd_, pxW, pxH, &rt_)) {
      rt_->SetDpi(96.0f, 96.0f);
      if (!brush_) {
        rt_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
      }
    }
  } else {
    rt_->Resize(D2D1::SizeU(pxW, pxH));
    rt_->SetDpi(96.0f, 96.0f);
  }
  BuildFonts(dwrite_, fonts_, dpi_);
}

void Render::Fill(const D2D1_RECT_F& rc, D2D1_COLOR_F c) {
  if (!rt_ || !brush_) return;
  brush_->SetColor(c);
  rt_->FillRectangle(rc, brush_);
}

void Render::FillRound(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F c) {
  if (!rt_ || !brush_) return;
  brush_->SetColor(c);
  D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rc, radius, radius);
  rt_->FillRoundedRectangle(rr, brush_);
}

void Render::StrokeRound(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F c, float w) {
  if (!rt_ || !brush_) return;
  brush_->SetColor(c);
  D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rc, radius, radius);
  rt_->DrawRoundedRectangle(rr, brush_, w);
}

void Render::Circle(float cx, float cy, float radius, D2D1_COLOR_F c) {
  if (!rt_ || !brush_) return;
  brush_->SetColor(c);
  D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);
  rt_->FillEllipse(e, brush_);
}

void Render::Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float w) {
  if (!rt_ || !brush_) return;
  brush_->SetColor(c);
  rt_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush_, w);
}

void Render::Text(const wchar_t* s, const D2D1_RECT_F& rc, D2D1_COLOR_F c, FontId f, Align a) {
  if (!rt_ || !brush_ || !s || !s[0]) return;
  IDWriteTextFormat* fmt = fonts_[static_cast<int>(f)];
  if (!fmt) return;
  brush_->SetColor(c);
  D2D1_RECT_F r = rc;
  if (a != Align::Left) {
    float w = Measure(s, f);
    float x = a == Align::Center
      ? rc.left + ((rc.right - rc.left) - w) * 0.5f
      : rc.right - w;
    r = D2D1::RectF(x, rc.top, rc.right, rc.bottom);
  }
  rt_->DrawText(s, TextLen(s), fmt, r, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE);
}

void Render::TextVertCenter(const wchar_t* s, const D2D1_RECT_F& rc, D2D1_COLOR_F c, FontId f, Align a) {
  if (!rt_ || !brush_ || !s || !s[0]) return;
  float lh = LineH(f);
  if (lh <= 0.0f) lh = rc.bottom - rc.top;
  float top = rc.top + ((rc.bottom - rc.top) - lh) * 0.5f;
  Text(s, D2D1::RectF(rc.left, top, rc.right, rc.bottom), c, f, a);
}

float Render::Measure(const wchar_t* s, FontId f) {
  if (!dwrite_ || !s || !s[0]) return 0.0f;
  IDWriteTextFormat* fmt = fonts_[static_cast<int>(f)];
  if (!fmt) return 0.0f;
  IDWriteTextLayout* layout = nullptr;
  HRESULT hr = dwrite_->CreateTextLayout(s, TextLen(s), fmt, 1.0e6f, 1.0e6f, &layout);
  if (FAILED(hr) || !layout) return 0.0f;
  DWRITE_TEXT_METRICS m{};
  float w = 0.0f;
  if (SUCCEEDED(layout->GetMetrics(&m))) w = m.width;
  layout->Release();
  return w;
}

float Render::LineH(FontId f) {
  if (!dwrite_) return 0.0f;
  IDWriteTextFormat* fmt = fonts_[static_cast<int>(f)];
  if (!fmt) return 0.0f;
  IDWriteTextLayout* layout = nullptr;
  if (FAILED(dwrite_->CreateTextLayout(L"Hg", 2, fmt, 1.0e6f, 1.0e6f, &layout)) || !layout) return 0.0f;
  DWRITE_TEXT_METRICS m{};
  float h = 0.0f;
  if (SUCCEEDED(layout->GetMetrics(&m))) h = m.height;
  layout->Release();
  return h;
}

float Render::CharW(FontId f) {
  return Measure(L"M", f);
}

void Render::PushClip(const D2D1_RECT_F& rc) {
  if (!rt_) return;
  rt_->PushAxisAlignedClip(rc, D2D1_ANTIALIAS_MODE_ALIASED);
  ++clip_;
}

void Render::PopClip() {
  if (!rt_ || clip_ <= 0) return;
  rt_->PopAxisAlignedClip();
  --clip_;
}

}
