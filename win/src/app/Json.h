#pragma once
#include <string>
#include <utility>
#include <vector>

namespace sk {

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

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

bool ParseJson(const std::string& src, JValue& out);
void DumpJson(const JValue& v, std::string& out, int level = 0);
std::string DumpJson(const JValue& v);

std::string JStr(const JValue& o, const char* key);
int JInt(const JValue& o, const char* key, int def = 0);
bool JBool(const JValue& o, const char* key, bool def = false);

}
