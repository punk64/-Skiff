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

JValue HostJson(const Host& h) {
  JValue o;
  o.Set("name", JValue::Str(WideToUtf8(h.name)));
  o.Set("addr", JValue::Str(WideToUtf8(h.addr)));
  o.Set("auth", JValue::Str(WideToUtf8(h.auth)));
  o.Set("state", JValue::Str(StateText(h.state)));
  o.Set("daysLeft", JValue::Num((double)h.daysLeft));
  o.Set("autoConnect", JValue::Bool(h.autoConnect));
  o.Set("latency", JValue::Num((double)h.latency));
  return o;
}

Host HostFromJson(const JValue& j) {
  Host h;
  if (j.type != JType::Object) return h;
  const JValue* v = nullptr;
  if ((v = j.Find("name")) != nullptr && v->type == JType::String) {
    h.name = Utf8ToWide(v->text);
  }
  if ((v = j.Find("addr")) != nullptr && v->type == JType::String) {
    h.addr = Utf8ToWide(v->text);
  } else if ((v = j.Find("host")) != nullptr && v->type == JType::String) {
    h.addr = Utf8ToWide(v->text);
  }
  if ((v = j.Find("auth")) != nullptr && v->type == JType::String) {
    h.auth = Utf8ToWide(v->text);
  }
  if ((v = j.Find("state")) != nullptr) {
    if (v->type == JType::String) {
      h.state = StateFromText(v->text);
    } else if (v->type == JType::Number) {
      int n = (int)v->number;
      if (n >= 0 && n <= 2) h.state = (HostState)n;
    }
  }
  if ((v = j.Find("daysLeft")) != nullptr && v->type == JType::Number) {
    h.daysLeft = (int)v->number;
  }
  if ((v = j.Find("autoConnect")) != nullptr && v->type == JType::Bool) {
    h.autoConnect = v->boolean;
  }
  if ((v = j.Find("latency")) != nullptr && v->type == JType::Number) {
    h.latency = (int)v->number;
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

  bool dark = st.dark;
  double font = (double)st.termFontSize;
  double side = (double)st.sidebarW;
  std::string lang = g_lang;
  std::string accent = g_accent;
  bool hasHosts = false;
  std::vector<Host> hosts;

  const JValue* ui = root.Find("ui");
  if (ui != nullptr && ui->type == JType::Object) {
    const JValue* v = nullptr;
    if ((v = ui->Find("dark")) != nullptr && v->type == JType::Bool) dark = v->boolean;
    if ((v = ui->Find("lang")) != nullptr && v->type == JType::String) lang = v->text;
    if ((v = ui->Find("termFontSize")) != nullptr && v->type == JType::Number) font = v->number;
    if ((v = ui->Find("sidebarW")) != nullptr && v->type == JType::Number) side = v->number;
    if ((v = ui->Find("accent")) != nullptr && v->type == JType::String) accent = v->text;
  }

  const JValue* list = root.Find("hosts");
  if (list != nullptr && list->type == JType::Array) {
    hasHosts = true;
    hosts.reserve(list->items.size());
    for (size_t i = 0; i < list->items.size(); ++i) {
      hosts.push_back(HostFromJson(list->items[i]));
    }
  }

  if (hasHosts) st.hosts = std::move(hosts);
  st.dark = dark;
  st.termFontSize = (float)font;
  st.sidebarW = (float)side;
  g_lang = lang;
  g_accent = accent;
  return true;
}

bool SaveStore(const AppState& st) {
  JValue ui;
  ui.Set("dark", JValue::Bool(st.dark));
  ui.Set("lang", JValue::Str(g_lang));
  ui.Set("termFontSize", JValue::Num((double)st.termFontSize));
  ui.Set("sidebarW", JValue::Num((double)st.sidebarW));
  ui.Set("accent", JValue::Str(g_accent));

  JValue hosts = JValue::Arr();
  for (size_t i = 0; i < st.hosts.size(); ++i) {
    hosts.items.push_back(HostJson(st.hosts[i]));
  }

  JValue root;
  root.Set("version", JValue::Num(1));
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
