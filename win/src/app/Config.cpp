#include <windows.h>
#include <shlobj.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "app/App.h"
#include "app/Config.h"
#include "app/DateTime.h"

namespace sk {
namespace {

std::string g_lang = "zh";
std::string g_accent = "#3DDC97";

std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  if (n <= 0) return std::string();
  std::string out((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring out((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
  return out;
}

enum class JType { Null, Bool, Number, String, Array, Object };

struct JValue {
  JType type = JType::Null;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  std::vector<JValue> items;
  std::vector<std::pair<std::string, JValue>> members;

  static JValue Bool(bool v) {
    JValue j;
    j.type = JType::Bool;
    j.boolean = v;
    return j;
  }

  static JValue Num(double v) {
    JValue j;
    j.type = JType::Number;
    j.number = v;
    return j;
  }

  static JValue Str(std::string v) {
    JValue j;
    j.type = JType::String;
    j.text = std::move(v);
    return j;
  }

  static JValue Arr() {
    JValue j;
    j.type = JType::Array;
    return j;
  }

  JValue& Set(const std::string& key, JValue v) {
    type = JType::Object;
    members.emplace_back(key, std::move(v));
    return *this;
  }

  const JValue* Find(const std::string& key) const {
    if (type != JType::Object) return nullptr;
    for (const std::pair<std::string, JValue>& m : members) {
      if (m.first == key) return &m.second;
    }
    return nullptr;
  }
};

void AppendUtf8(std::string& out, unsigned cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

struct JsonParser {
  explicit JsonParser(const std::string& src) : src_(src) {}

  bool Parse(JValue& out) {
    Ws();
    if (!Value(out, 0)) return false;
    Ws();
    return pos_ == src_.size();
  }

  const std::string& src_;
  size_t pos_ = 0;

  static bool Digit(char c) { return c >= '0' && c <= '9'; }

  void Ws() {
    while (pos_ < src_.size()) {
      char c = src_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool Literal(const char* lit) {
    size_t n = std::strlen(lit);
    if (pos_ + n > src_.size()) return false;
    if (src_.compare(pos_, n, lit) != 0) return false;
    pos_ += n;
    return true;
  }

  bool Hex4(unsigned& out) {
    if (pos_ + 4 > src_.size()) return false;
    unsigned v = 0;
    for (int k = 0; k < 4; ++k) {
      char c = src_[pos_++];
      unsigned d = 0;
      if (c >= '0' && c <= '9') {
        d = (unsigned)(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        d = (unsigned)(c - 'a') + 10;
      } else if (c >= 'A' && c <= 'F') {
        d = (unsigned)(c - 'A') + 10;
      } else {
        return false;
      }
      v = (v << 4) | d;
    }
    out = v;
    return true;
  }

  bool String(std::string& out) {
    if (pos_ >= src_.size() || src_[pos_] != '"') return false;
    ++pos_;
    out.clear();
    while (pos_ < src_.size()) {
      unsigned char c = (unsigned char)src_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        if (c < 0x20) return false;
        out += (char)c;
        continue;
      }
      if (pos_ >= src_.size()) return false;
      char e = src_[pos_++];
      if (e == '"') {
        out += '"';
      } else if (e == '\\') {
        out += '\\';
      } else if (e == '/') {
        out += '/';
      } else if (e == 'b') {
        out += '\b';
      } else if (e == 'f') {
        out += '\f';
      } else if (e == 'n') {
        out += '\n';
      } else if (e == 'r') {
        out += '\r';
      } else if (e == 't') {
        out += '\t';
      } else if (e == 'u') {
        unsigned cp = 0;
        if (!Hex4(cp)) return false;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
          bool paired = false;
          if (pos_ + 1 < src_.size() && src_[pos_] == '\\' && src_[pos_ + 1] == 'u') {
            size_t save = pos_;
            pos_ += 2;
            unsigned lo = 0;
            if (!Hex4(lo)) return false;
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              paired = true;
            } else {
              pos_ = save;
            }
          }
          if (!paired) cp = 0xFFFD;
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          cp = 0xFFFD;
        }
        AppendUtf8(out, cp);
      } else {
        return false;
      }
    }
    return false;
  }

  bool Number(JValue& v) {
    size_t start = pos_;
    if (pos_ < src_.size() && src_[pos_] == '-') ++pos_;
    size_t intStart = pos_;
    while (pos_ < src_.size() && Digit(src_[pos_])) ++pos_;
    if (pos_ == intStart) {
      pos_ = start;
      return false;
    }
    if (pos_ < src_.size() && src_[pos_] == '.') {
      ++pos_;
      size_t fracStart = pos_;
      while (pos_ < src_.size() && Digit(src_[pos_])) ++pos_;
      if (pos_ == fracStart) {
        pos_ = start;
        return false;
      }
    }
    if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
      size_t expStart = pos_;
      while (pos_ < src_.size() && Digit(src_[pos_])) ++pos_;
      if (pos_ == expStart) {
        pos_ = start;
        return false;
      }
    }
    std::string token = src_.substr(start, pos_ - start);
    v.type = JType::Number;
    v.number = std::strtod(token.c_str(), nullptr);
    return true;
  }

  bool Value(JValue& v, int depth) {
    if (depth > 64) return false;
    Ws();
    if (pos_ >= src_.size()) return false;
    char c = src_[pos_];
    if (c == '{') return Object(v, depth + 1);
    if (c == '[') return Array(v, depth + 1);
    if (c == '"') {
      v.type = JType::String;
      return String(v.text);
    }
    if (c == 't') {
      v = JValue::Bool(true);
      return Literal("true");
    }
    if (c == 'f') {
      v = JValue::Bool(false);
      return Literal("false");
    }
    if (c == 'n') {
      v = JValue();
      return Literal("null");
    }
    return Number(v);
  }

  bool Object(JValue& v, int depth) {
    v.type = JType::Object;
    ++pos_;
    Ws();
    if (pos_ < src_.size() && src_[pos_] == '}') {
      ++pos_;
      return true;
    }
    while (true) {
      Ws();
      std::string key;
      if (!String(key)) return false;
      Ws();
      if (pos_ >= src_.size() || src_[pos_] != ':') return false;
      ++pos_;
      JValue child;
      if (!Value(child, depth)) return false;
      v.members.emplace_back(std::move(key), std::move(child));
      Ws();
      if (pos_ >= src_.size()) return false;
      if (src_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (src_[pos_] == '}') {
        ++pos_;
        return true;
      }
      return false;
    }
  }

  bool Array(JValue& v, int depth) {
    v.type = JType::Array;
    ++pos_;
    Ws();
    if (pos_ < src_.size() && src_[pos_] == ']') {
      ++pos_;
      return true;
    }
    while (true) {
      JValue child;
      if (!Value(child, depth)) return false;
      v.items.push_back(std::move(child));
      Ws();
      if (pos_ >= src_.size()) return false;
      if (src_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (src_[pos_] == ']') {
        ++pos_;
        return true;
      }
      return false;
    }
  }
};

void Indent(std::string& out, int level) {
  out.append((size_t)level * 2, ' ');
}

void DumpText(const std::string& s, std::string& out) {
  out += '"';
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c == '\b') {
      out += "\\b";
    } else if (c == '\f') {
      out += "\\f";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if (c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)c);
      out += buf;
    } else {
      out += (char)c;
    }
  }
  out += '"';
}

std::string NumberText(double v) {
  if (!(v == v)) return "0";
  if (v > -1.0e15 && v < 1.0e15 && std::floor(v) == v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", (long long)v);
    return std::string(buf);
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.10g", v);
  return std::string(buf);
}

void Dump(const JValue& v, std::string& out, int level) {
  if (v.type == JType::Null) {
    out += "null";
  } else if (v.type == JType::Bool) {
    out += v.boolean ? "true" : "false";
  } else if (v.type == JType::Number) {
    out += NumberText(v.number);
  } else if (v.type == JType::String) {
    DumpText(v.text, out);
  } else if (v.type == JType::Array) {
    if (v.items.empty()) {
      out += "[]";
      return;
    }
    out += "[\n";
    for (size_t i = 0; i < v.items.size(); ++i) {
      Indent(out, level + 1);
      Dump(v.items[i], out, level + 1);
      if (i + 1 < v.items.size()) out += ',';
      out += '\n';
    }
    Indent(out, level);
    out += ']';
  } else {
    if (v.members.empty()) {
      out += "{}";
      return;
    }
    out += "{\n";
    for (size_t i = 0; i < v.members.size(); ++i) {
      Indent(out, level + 1);
      DumpText(v.members[i].first, out);
      out += ": ";
      Dump(v.members[i].second, out, level + 1);
      if (i + 1 < v.members.size()) out += ',';
      out += '\n';
    }
    Indent(out, level);
    out += '}';
  }
}

std::string StateText(HostState s) {
  if (s == HostState::Live) return "live";
  if (s == HostState::Busy) return "busy";
  return "off";
}

HostState StateFromText(const std::string& s) {
  if (s == "live") return HostState::Live;
  if (s == "busy") return HostState::Busy;
  return HostState::Off;
}

const char* TunText(TunState s) {
  if (s == TunState::Up) return "up";
  if (s == TunState::Listen) return "listen";
  if (s == TunState::Retry) return "retry";
  return "down";
}

TunState TunFromText(const std::string& s) {
  if (s == "up") return TunState::Up;
  if (s == "listen") return TunState::Listen;
  if (s == "retry") return TunState::Retry;
  return TunState::Down;
}

JValue TunnelJson(const Tunnel& t) {
  JValue o;
  o.Set("id", JValue::Num((double)t.id));
  o.Set("hostId", JValue::Num((double)t.hostId));
  o.Set("type", JValue::Str(WideToUtf8(t.type)));
  o.Set("port", JValue::Str(WideToUtf8(t.port)));
  o.Set("dest", JValue::Str(WideToUtf8(t.dest)));
  o.Set("on", JValue::Bool(t.on));
  return o;
}

Tunnel TunnelFromJson(const JValue& j) {
  Tunnel t;
  if (j.type != JType::Object) return t;
  const JValue* v = nullptr;
  if ((v = j.Find("id")) != nullptr && v->type == JType::Number) t.id = (int)v->number;
  if ((v = j.Find("hostId")) != nullptr && v->type == JType::Number) t.hostId = (int)v->number;
  if ((v = j.Find("type")) != nullptr && v->type == JType::String) t.type = Utf8ToWide(v->text);
  if ((v = j.Find("port")) != nullptr && v->type == JType::String) t.port = Utf8ToWide(v->text);
  if ((v = j.Find("dest")) != nullptr && v->type == JType::String) t.dest = Utf8ToWide(v->text);
  if ((v = j.Find("on")) != nullptr && v->type == JType::Bool) t.on = v->boolean;
  if (t.port.empty()) {
    if ((v = j.Find("listen")) != nullptr && v->type == JType::String) {
      std::wstring listen = Utf8ToWide(v->text);
      size_t colon = listen.rfind(L':');
      t.port = colon == std::wstring::npos ? listen : listen.substr(colon + 1);
    }
  }
  if (t.dest.empty()) {
    if ((v = j.Find("target")) != nullptr && v->type == JType::String) {
      t.dest = Utf8ToWide(v->text);
    }
  }
  if ((v = j.Find("state")) != nullptr && v->type == JType::String) t.state = TunFromText(v->text);
  if ((v = j.Find("retryIn")) != nullptr && v->type == JType::Number) t.retryIn = (int)v->number;
  if (t.type.empty()) t.type = L"L";
  return t;
}

void SplitAddr(const std::wstring& addr, std::wstring& user, std::wstring& host, int& port) {
  std::wstring s = addr;
  size_t at = s.rfind(L'@');
  if (at != std::wstring::npos) {
    user = s.substr(0, at);
    s = s.substr(at + 1);
  }
  size_t colon = s.rfind(L':');
  if (colon != std::wstring::npos) {
    port = _wtoi(s.c_str() + colon + 1);
    s = s.substr(0, colon);
  }
  host = s;
  if (user.empty()) user = L"root";
  if (port < 1 || port > 65535) port = 22;
}

std::wstring AddDaysStamp(int days) { return StampPlusDays(days); }

JValue HostJson(const Host& h) {
  JValue o;
  o.Set("id", JValue::Num((double)h.id));
  o.Set("name", JValue::Str(WideToUtf8(h.name)));
  o.Set("user", JValue::Str(WideToUtf8(h.user)));
  o.Set("host", JValue::Str(WideToUtf8(h.host)));
  o.Set("port", JValue::Num((double)h.port));
  o.Set("auth", JValue::Str(WideToUtf8(h.auth)));
  o.Set("password", JValue::Str(WideToUtf8(h.password)));
  o.Set("keyPath", JValue::Str(WideToUtf8(h.keyPath)));
  o.Set("keyPass", JValue::Str(WideToUtf8(h.keyPass)));
  o.Set("hostKey", JValue::Str(WideToUtf8(h.hostKey)));
  o.Set("hostKeyType", JValue::Str(WideToUtf8(h.hostKeyType)));
  o.Set("registeredAt", JValue::Str(WideToUtf8(h.registeredAt)));
  o.Set("expireAt", JValue::Str(WideToUtf8(h.expireAt)));
  o.Set("lastConnected", JValue::Str(WideToUtf8(h.lastConnected)));
  o.Set("autoConnect", JValue::Bool(h.autoConnect));
  JValue list = JValue::Arr();
  for (size_t i = 0; i < h.tunnels.size(); ++i) {
    list.items.push_back(TunnelJson(h.tunnels[i]));
  }
  o.Set("tunnels", std::move(list));
  return o;
}

Host HostFromJson(const JValue& j) {
  Host h;
  if (j.type != JType::Object) return h;
  const JValue* v = nullptr;
  if ((v = j.Find("id")) != nullptr && v->type == JType::Number) h.id = (int)v->number;
  if ((v = j.Find("name")) != nullptr && v->type == JType::String) {
    h.name = Utf8ToWide(v->text);
  }
  if ((v = j.Find("user")) != nullptr && v->type == JType::String) {
    h.user = Utf8ToWide(v->text);
  }
  if ((v = j.Find("host")) != nullptr && v->type == JType::String) {
    h.host = Utf8ToWide(v->text);
  }
  if ((v = j.Find("port")) != nullptr && v->type == JType::Number) {
    h.port = (int)v->number;
  }
  if (h.user.empty() && h.host.empty()) {
    std::wstring legacy;
    if ((v = j.Find("addr")) != nullptr && v->type == JType::String) legacy = Utf8ToWide(v->text);
    if (!legacy.empty()) SplitAddr(legacy, h.user, h.host, h.port);
  }
  if (h.user.empty()) h.user = L"root";
  if (h.host.empty() && h.name.empty()) h.name = L"新主机";
  if (h.port < 1 || h.port > 65535) h.port = 22;
  if ((v = j.Find("auth")) != nullptr && v->type == JType::String) {
    h.auth = Utf8ToWide(v->text);
  }
  if (h.auth.empty()) h.auth = L"密码";
  if ((v = j.Find("password")) != nullptr && v->type == JType::String) {
    h.password = Utf8ToWide(v->text);
  }
  if ((v = j.Find("keyPath")) != nullptr && v->type == JType::String) {
    h.keyPath = Utf8ToWide(v->text);
  }
  if ((v = j.Find("keyPass")) != nullptr && v->type == JType::String) {
    h.keyPass = Utf8ToWide(v->text);
  }
  if ((v = j.Find("hostKey")) != nullptr && v->type == JType::String) {
    h.hostKey = Utf8ToWide(v->text);
  }
  if ((v = j.Find("hostKeyType")) != nullptr && v->type == JType::String) {
    h.hostKeyType = Utf8ToWide(v->text);
  }
  if ((v = j.Find("registeredAt")) != nullptr && v->type == JType::String) {
    h.registeredAt = Utf8ToWide(v->text);
  }
  if ((v = j.Find("expireAt")) != nullptr && v->type == JType::String) {
    h.expireAt = Utf8ToWide(v->text);
  }
  if (h.expireAt.empty()) {
    if ((v = j.Find("daysLeft")) != nullptr && v->type == JType::Number) {
      int days = (int)v->number;
      if (days >= 0) h.expireAt = AddDaysStamp(days);
    }
  }
  if ((v = j.Find("lastConnected")) != nullptr && v->type == JType::String) {
    h.lastConnected = Utf8ToWide(v->text);
  }
  if ((v = j.Find("autoConnect")) != nullptr && v->type == JType::Bool) {
    h.autoConnect = v->boolean;
  }
  if ((v = j.Find("state")) != nullptr) {
    if (v->type == JType::String) {
      h.state = StateFromText(v->text);
    } else if (v->type == JType::Number) {
      int n = (int)v->number;
      if (n >= 0 && n <= 2) h.state = (HostState)n;
    }
  }
  if ((v = j.Find("tunnels")) != nullptr && v->type == JType::Array) {
    for (size_t i = 0; i < v->items.size(); ++i) {
      h.tunnels.push_back(TunnelFromJson(v->items[i]));
    }
  }
  return h;
}

std::wstring ExeDir() {
  wchar_t buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
  if (n == 0 || n >= (DWORD)(sizeof(buf) / sizeof(buf[0]))) return std::wstring();
  std::wstring path(buf, n);
  size_t slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return std::wstring();
  return path.substr(0, slash);
}

bool ProbeWritable(const std::wstring& file) {
  HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  CloseHandle(h);
  return true;
}

std::wstring LocalAppDataSkiff() {
  wchar_t buf[MAX_PATH];
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf))) {
    return std::wstring();
  }
  return std::wstring(buf) + L"\\Skiff";
}

}

std::wstring ExeDirectory() { return ExeDir(); }

std::wstring FindKeyFor(const std::string& hostName) {
  std::wstring dir = KeyDirectory();
  if (dir.empty() || hostName.empty()) return std::wstring();
  std::wstring base = dir + L"\\" + Utf8ToWide(hostName);
  const wchar_t* exts[] = { L".pem", L".key", L"_rsa", L"" };
  for (int i = 0; i < 4; ++i) {
    std::wstring path = base + exts[i];
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) return path;
  }
  return std::wstring();
}

bool HostHasCredential(const Host& h) {
  if (!h.IsKeyAuth()) return !h.password.empty();
  if (!h.keyPath.empty()) return true;
  return !FindKeyFor(WideToUtf8(h.host)).empty();
}

std::wstring KeyDirectory() {
  std::wstring base = ExeDir();
  if (base.empty()) base = LocalAppDataSkiff();
  return base.empty() ? std::wstring() : base + L"\\keys";
}

std::vector<std::wstring> ListKeyFiles() {
  std::vector<std::wstring> out;
  std::wstring dir = KeyDirectory();
  if (dir.empty()) return out;
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
    out.push_back(fd.cFileName);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return out;
}

bool SystemPrefersLight() {
  HKEY key = nullptr;
  const wchar_t* sub = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
  if (RegOpenKeyExW(HKEY_CURRENT_USER, sub, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  LONG rc = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&value, &size);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS || type != REG_DWORD) return false;
  return value != 0;
}

StorePath ResolveStorePath() {
  StorePath sp;
  std::wstring dir = ExeDir();
  sp.file = dir + L"\\skiff_store.json";
  sp.portable = true;
  if (ProbeWritable(sp.file)) return sp;

  std::wstring base = LocalAppDataSkiff();
  if (base.empty()) {
    sp.portable = false;
    return sp;
  }
  CreateDirectoryW(base.c_str(), nullptr);
  sp.file = base + L"\\skiff_store.json";
  sp.portable = false;
  return sp;
}

bool LoadStore(AppState& st) {
  StorePath sp = ResolveStorePath();
  std::ifstream in(sp.file.c_str(), std::ios::binary);
  if (!in) return false;
  std::istreambuf_iterator<char> begin(in);
  std::istreambuf_iterator<char> end;
  std::string text(begin, end);
  if (text.empty()) return false;

  JValue root;
  JsonParser parser(text);
  if (!parser.Parse(root)) return false;
  if (root.type != JType::Object) return false;

  const JValue* ui = root.Find("ui");
  if (ui != nullptr && ui->type == JType::Object) {
    const JValue* v = nullptr;
    if ((v = ui->Find("termFontSize")) != nullptr && v->type == JType::Number) {
      st.termFontSize = (float)v->number;
    }
    if ((v = ui->Find("sideCollapsed")) != nullptr && v->type == JType::Bool) {
      st.sideCollapsed = v->boolean;
    }
    if ((v = ui->Find("lang")) != nullptr && v->type == JType::String) g_lang = v->text;
    if ((v = ui->Find("accent")) != nullptr && v->type == JType::String) g_accent = v->text;

    const JValue* theme = ui->Find("themeMode");
    if (theme != nullptr && theme->type == JType::Number) {
      st.themeMode = (int)theme->number;
    } else if ((v = ui->Find("dark")) != nullptr && v->type == JType::Bool) {
      st.dark = v->boolean;
      st.themeMode = v->boolean ? 1 : 2;
    }

    auto num = [ui](const char* key, int* dst) {
      const JValue* jv = ui->Find(key);
      if (jv != nullptr && jv->type == JType::Number) *dst = (int)jv->number;
    };
    auto flag = [ui](const char* key, bool* dst) {
      const JValue* jv = ui->Find(key);
      if (jv != nullptr && jv->type == JType::Bool) *dst = jv->boolean;
    };
    num("langMode", &st.langMode);
    num("accentIdx", &st.accentIdx);
    num("uiScaleIdx", &st.uiScaleIdx);
    num("termFontIdx", &st.termFontIdx);
    num("termSizeIdx", &st.termSizeIdx);
    num("cursorIdx", &st.cursorIdx);
    num("keepAliveIdx", &st.keepAliveIdx);
    num("timeoutIdx", &st.timeoutIdx);
    num("logLevelIdx", &st.logLevelIdx);
    num("logKeepIdx", &st.logKeepIdx);
    num("activeHost", &st.activeHost);
    num("setNav", &st.setNav);
    num("sortKey", &st.sortKey);
    num("lastView", &st.lastView);
    num("lastHost", &st.lastHost);
    flag("sortAsc", &st.sortAsc);
    flag("autoReconnect", &st.autoReconnect);
    flag("autoConnectAll", &st.autoConnectAll);
    flag("restoreLast", &st.restoreLast);
    flag("fpCheck", &st.fpCheck);
  }

  if (st.themeMode == 1) {
    st.dark = true;
  } else if (st.themeMode == 2) {
    st.dark = false;
  } else {
    st.dark = !SystemPrefersLight();
  }

  const JValue* list = root.Find("hosts");
  if (list != nullptr && list->type == JType::Array) {
    std::vector<Host> hosts;
    hosts.reserve(list->items.size());
    for (size_t i = 0; i < list->items.size(); ++i) {
      hosts.push_back(HostFromJson(list->items[i]));
    }
    st.hosts = std::move(hosts);
  }

  const JValue* tlist = root.Find("tunnels");
  if (tlist != nullptr && tlist->type == JType::Array && !st.hosts.empty()) {
    for (size_t i = 0; i < tlist->items.size(); ++i) {
      st.hosts[0].tunnels.push_back(TunnelFromJson(tlist->items[i]));
    }
  }

  int maxHost = 0;
  int maxTun = 0;
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    if (st.hosts[i].id <= 0) st.hosts[i].id = (int)i + 1;
    if (st.hosts[i].id > maxHost) maxHost = st.hosts[i].id;
    if (st.hosts[i].name.empty()) st.hosts[i].name = L"主机 " + std::to_wstring(i + 1);
    for (size_t k = 0; k < st.hosts[i].tunnels.size(); ++k) {
      Tunnel& t = st.hosts[i].tunnels[k];
      if (t.id <= 0) t.id = maxTun + 1;
      if (t.id > maxTun) maxTun = t.id;
      t.hostId = st.hosts[i].id;
      t.state = t.on ? TunState::Retry : TunState::Down;
      t.retryIn = 0;
      t.fails = 0;
      t.nextAt = 0;
    }
  }
  st.nextHostId = maxHost + 1;
  st.nextTunnelId = maxTun + 1;

  if (st.activeHost < 0 || st.activeHost >= (int)st.hosts.size()) st.activeHost = 0;
  if (st.setNav < 0 || st.setNav > 5) st.setNav = 0;
  if (st.sortKey < 0 || st.sortKey > 1) st.sortKey = 0;
  st.sideScroll = 0.0f;
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    st.hosts[i].state = HostState::Off;
    st.hosts[i].latency = -1;
    st.hosts[i].cpu = 0;
    st.hosts[i].ram = 0;
    st.hosts[i].net.clear();
    st.hosts[i].netp = 0;
  }
  st.RecalcDaysLeft();
  st.dirty = false;
  return true;
}

bool SaveStore(const AppState& st) {
  JValue ui;
  ui.Set("dark", JValue::Bool(st.dark));
  ui.Set("lang", JValue::Str(g_lang));
  ui.Set("termFontSize", JValue::Num((double)st.termFontSize));
  ui.Set("sideCollapsed", JValue::Bool(st.sideCollapsed));
  ui.Set("accent", JValue::Str(g_accent));
  ui.Set("themeMode", JValue::Num(st.themeMode));
  ui.Set("langMode", JValue::Num(st.langMode));
  ui.Set("accentIdx", JValue::Num(st.accentIdx));
  ui.Set("uiScaleIdx", JValue::Num(st.uiScaleIdx));
  ui.Set("termFontIdx", JValue::Num(st.termFontIdx));
  ui.Set("termSizeIdx", JValue::Num(st.termSizeIdx));
  ui.Set("cursorIdx", JValue::Num(st.cursorIdx));
  ui.Set("keepAliveIdx", JValue::Num(st.keepAliveIdx));
  ui.Set("timeoutIdx", JValue::Num(st.timeoutIdx));
  ui.Set("logLevelIdx", JValue::Num(st.logLevelIdx));
  ui.Set("logKeepIdx", JValue::Num(st.logKeepIdx));
  ui.Set("activeHost", JValue::Num(st.activeHost));
  ui.Set("setNav", JValue::Num(st.setNav));
  ui.Set("sortKey", JValue::Num(st.sortKey));
  ui.Set("lastView", JValue::Num(st.lastView));
  ui.Set("lastHost", JValue::Num(st.lastHost));
  ui.Set("sortAsc", JValue::Bool(st.sortAsc));
  ui.Set("autoReconnect", JValue::Bool(st.autoReconnect));
  ui.Set("autoConnectAll", JValue::Bool(st.autoConnectAll));
  ui.Set("restoreLast", JValue::Bool(st.restoreLast));
  ui.Set("fpCheck", JValue::Bool(st.fpCheck));

  JValue hosts = JValue::Arr();
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    hosts.items.push_back(HostJson(st.hosts[i]));
  }

  JValue root;
  root.Set("version", JValue::Num(3));
  root.Set("ui", std::move(ui));
  root.Set("hosts", std::move(hosts));

  std::string text;
  Dump(root, text, 0);
  text += '\n';

  StorePath sp = ResolveStorePath();
  std::wstring tmp = sp.file + L".tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wrote = 0;
  BOOL ok = WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr);
  CloseHandle(h);
  if (!ok || wrote != (DWORD)text.size()) {
    DeleteFileW(tmp.c_str());
    return false;
  }
  if (!MoveFileExW(tmp.c_str(), sp.file.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileW(tmp.c_str());
    return false;
  }
  return true;
}

std::string SerializeHosts(const AppState& st) {
  JValue arr = JValue::Arr();
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    arr.items.push_back(HostJson(st.hosts[i]));
  }
  std::string out;
  Dump(arr, out, 0);
  return out;
}

bool DeserializeHosts(const std::string& json, AppState& st) {
  JValue root;
  JsonParser parser(json);
  if (!parser.Parse(root)) return false;
  if (root.type != JType::Array) return false;
  std::vector<Host> hosts;
  hosts.reserve(root.items.size());
  for (size_t i = 0; i < root.items.size(); ++i) {
    hosts.push_back(HostFromJson(root.items[i]));
  }
  st.hosts = std::move(hosts);
  return true;
}

std::string StorePathUtf8() {
  return WideToUtf8(ResolveStorePath().file);
}

}
