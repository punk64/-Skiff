#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "app/Json.h"

namespace sk {

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

namespace {

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

}

bool ParseJson(const std::string& src, JValue& out) {
  JsonParser parser(src);
  return parser.Parse(out);
}

void DumpJson(const JValue& v, std::string& out, int level) { Dump(v, out, level); }

std::string DumpJson(const JValue& v) {
  std::string out;
  Dump(v, out, 0);
  return out;
}

std::string JStr(const JValue& o, const char* key) {
  const JValue* v = o.Find(key);
  return (v != nullptr && v->type == JType::String) ? v->text : std::string();
}

int JInt(const JValue& o, const char* key, int def) {
  const JValue* v = o.Find(key);
  return (v != nullptr && v->type == JType::Number) ? (int)v->number : def;
}

bool JBool(const JValue& o, const char* key, bool def) {
  const JValue* v = o.Find(key);
  return (v != nullptr && v->type == JType::Bool) ? v->boolean : def;
}

}
