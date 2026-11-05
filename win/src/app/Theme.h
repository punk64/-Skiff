#pragma once
#include <d2d1.h>
#include <d2d1helper.h>

namespace sk {

struct Palette {
  bool dark;
  D2D1_COLOR_F bg;
  D2D1_COLOR_F surface;
  D2D1_COLOR_F surface2;
  D2D1_COLOR_F line;
  D2D1_COLOR_F lineStrong;
  D2D1_COLOR_F text;
  D2D1_COLOR_F text2;
  D2D1_COLOR_F text3;
  D2D1_COLOR_F accent;
  D2D1_COLOR_F accentFg;
  D2D1_COLOR_F accentSoft;
  D2D1_COLOR_F danger;
  D2D1_COLOR_F warn;
  D2D1_COLOR_F info;
  D2D1_COLOR_F violet;
  D2D1_COLOR_F blue;
  D2D1_COLOR_F termBg;
  D2D1_COLOR_F termText;
  D2D1_COLOR_F termDim;
};

inline D2D1_COLOR_F Rgb(unsigned hex, float alpha) {
  return D2D1::ColorF(
    ((hex >> 16) & 0xFF) / 255.0f,
    ((hex >> 8) & 0xFF) / 255.0f,
    (hex & 0xFF) / 255.0f,
    alpha);
}

inline D2D1_COLOR_F Rgb(unsigned hex) {
  return Rgb(hex, 1.0f);
}

inline D2D1_COLOR_F Alpha(D2D1_COLOR_F c, float a) {
  c.a = a;
  return c;
}

inline D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
  return D2D1::ColorF(
    a.r + (b.r - a.r) * t,
    a.g + (b.g - a.g) * t,
    a.b + (b.b - a.b) * t,
    a.a + (b.a - a.a) * t);
}

const Palette& DarkPalette();
const Palette& LightPalette();
const Palette& PaletteFor(bool dark);

}
