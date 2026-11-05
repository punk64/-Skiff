#pragma once

#include <windows.h>
#include <string>

namespace sk {

inline bool ParseStamp(const std::wstring& s, SYSTEMTIME& out) {
  if (s.size() < 10) return false;
  int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0;
  int n = swscanf(s.c_str(), L"%d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mi, &ss);
  if (n < 3) return false;
  if (y < 1970 || y > 2199) return false;
  if (mo < 1 || mo > 12) return false;
  if (d < 1 || d > 31) return false;
  if (n >= 4 && (hh < 0 || hh > 23)) return false;
  if (n >= 5 && (mi < 0 || mi > 59)) return false;
  if (n >= 6 && (ss < 0 || ss > 59)) return false;
  SYSTEMTIME st{};
  st.wYear = (WORD)y;
  st.wMonth = (WORD)mo;
  st.wDay = (WORD)d;
  st.wHour = (WORD)hh;
  st.wMinute = (WORD)mi;
  st.wSecond = (WORD)ss;
  FILETIME ft{};
  if (!SystemTimeToFileTime(&st, &ft)) return false;
  out = st;
  return true;
}

inline long long StampMillis(const std::wstring& s) {
  SYSTEMTIME st{};
  if (!ParseStamp(s, st)) return -1;
  FILETIME ft{};
  if (!SystemTimeToFileTime(&st, &ft)) return -1;
  ULARGE_INTEGER u{};
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
}

inline std::wstring FormatStamp(const SYSTEMTIME& st) {
  wchar_t buf[32];
  swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
  return std::wstring(buf);
}

inline std::wstring NowStamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  return FormatStamp(st);
}

inline std::wstring StampPlusDays(int days) {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  FILETIME ft{};
  if (!SystemTimeToFileTime(&st, &ft)) return std::wstring();
  ULARGE_INTEGER u{};
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  long long delta = (long long)days * 864000000000LL;
  if (delta < 0 && -delta > (long long)u.QuadPart) return std::wstring();
  u.QuadPart = (unsigned long long)((long long)u.QuadPart + delta);
  FILETIME f2{};
  f2.dwLowDateTime = u.LowPart;
  f2.dwHighDateTime = u.HighPart;
  SYSTEMTIME out{};
  if (!FileTimeToSystemTime(&f2, &out)) return std::wstring();
  return FormatStamp(out);
}

inline int DaysLeftOf(const std::wstring& expireAt) {
  long long ms = StampMillis(expireAt);
  if (ms < 0) return -1;
  FILETIME ft{};
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER u{};
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  long long now = (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
  long long diff = ms - now;
  if (diff >= 0) return (int)(diff / 86400000LL);
  return -(int)((-diff + 86399999LL) / 86400000LL);
}

inline std::wstring TrimWs(const std::wstring& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r' || s[a] == L'\n')) ++a;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r' || s[b - 1] == L'\n')) --b;
  return s.substr(a, b - a);
}

}
