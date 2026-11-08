#include <windows.h>
#include <bcrypt.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app/App.h"
#include "app/Backup.h"
#include "app/Config.h"
#include "app/DateTime.h"
#include "app/Json.h"
#include "app/Version.h"

namespace sk {
namespace {

const char kMagic[] = "SKIFFBK/1";
const int kIter = 200000;
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string B64Enc(const std::string& in) {
  std::string out;
  size_t i = 0;
  while (i + 2 < in.size()) {
    unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) |
                 (unsigned char)in[i + 2];
    out += kB64[(v >> 18) & 63];
    out += kB64[(v >> 12) & 63];
    out += kB64[(v >> 6) & 63];
    out += kB64[v & 63];
    i += 3;
  }
  if (i + 1 == in.size()) {
    unsigned v = (unsigned char)in[i] << 16;
    out += kB64[(v >> 18) & 63];
    out += kB64[(v >> 12) & 63];
    out += "==";
  } else if (i + 2 == in.size()) {
    unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8);
    out += kB64[(v >> 18) & 63];
    out += kB64[(v >> 12) & 63];
    out += kB64[(v >> 6) & 63];
    out += '=';
  }
  return out;
}

int B64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool B64Dec(const std::string& in, std::string& out) {
  out.clear();
  unsigned buf = 0;
  int bits = 0;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
    int v = B64Val(c);
    if (v < 0) return false;
    buf = (buf << 6) | (unsigned)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += (char)((buf >> bits) & 0xFF);
    }
  }
  return true;
}

bool RndBytes(unsigned char* p, size_t n) {
  return BCryptGenRandom(nullptr, p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

bool Pbkdf2(const std::string& pwd, const unsigned char* salt, int saltLen, int iter,
            unsigned char* out, int outLen) {
  BCRYPT_ALG_HANDLE h = nullptr;
  if (BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;
  NTSTATUS rc = BCryptDeriveKeyPBKDF2(h, (PUCHAR)pwd.data(), (ULONG)pwd.size(), (PUCHAR)salt,
                                      (ULONG)saltLen, (ULONGLONG)iter, out, (ULONG)outLen, 0);
  BCryptCloseAlgorithmProvider(h, 0);
  return rc >= 0;
}

bool HmacSha256(const unsigned char* key, int keyLen, const std::string& data,
                unsigned char* out) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hh = nullptr;
  PUCHAR obj = nullptr;
  bool ok = false;
  DWORD objLen = 0;
  DWORD hashLen = 0;
  DWORD cb = 0;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;
  do {
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) < 0) {
      break;
    }
    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0) < 0) {
      break;
    }
    obj = (PUCHAR)malloc(objLen);
    if (obj == nullptr) break;
    if (BCryptCreateHash(alg, &hh, obj, objLen, (PUCHAR)key, (ULONG)keyLen, 0) < 0) break;
    if (BCryptHashData(hh, (PUCHAR)data.data(), (ULONG)data.size(), 0) < 0) break;
    if (BCryptFinishHash(hh, out, hashLen, 0) < 0) break;
    ok = true;
  } while (false);
  if (hh != nullptr) BCryptDestroyHash(hh);
  if (obj != nullptr) free(obj);
  BCryptCloseAlgorithmProvider(alg, 0);
  return ok;
}

bool AesCbc(bool enc, const unsigned char* key, const unsigned char* iv, const std::string& in,
            std::string& out) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_KEY_HANDLE kh = nullptr;
  PUCHAR obj = nullptr;
  bool ok = false;
  DWORD objLen = 0;
  DWORD cb = 0;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) return false;
  do {
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
                          sizeof(BCRYPT_CHAIN_MODE_CBC), 0) < 0) {
      break;
    }
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) < 0) {
      break;
    }
    obj = (PUCHAR)malloc(objLen);
    if (obj == nullptr) break;
    if (BCryptGenerateSymmetricKey(alg, &kh, obj, objLen, (PUCHAR)key, 32, 0) < 0) break;
    unsigned char iv1[16];
    unsigned char iv2[16];
    std::memcpy(iv1, iv, 16);
    std::memcpy(iv2, iv, 16);
    ULONG need = 0;
    if (enc) {
      if (BCryptEncrypt(kh, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv1, 16, nullptr, 0, &need,
                        BCRYPT_BLOCK_PADDING) < 0) {
        break;
      }
    } else {
      need = (ULONG)in.size() + 32;
    }
    out.assign(need, '\0');
    ULONG done = 0;
    NTSTATUS rc = enc ? BCryptEncrypt(kh, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv2, 16,
                                      (PUCHAR)&out[0], need, &done, BCRYPT_BLOCK_PADDING)
                      : BCryptDecrypt(kh, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv2, 16,
                                      (PUCHAR)&out[0], need, &done, BCRYPT_BLOCK_PADDING);
    if (rc < 0) {
      out.clear();
      break;
    }
    out.resize(done);
    ok = true;
  } while (false);
  if (kh != nullptr) BCryptDestroyKey(kh);
  if (obj != nullptr) free(obj);
  BCryptCloseAlgorithmProvider(alg, 0);
  return ok;
}

std::string Wrap64(const std::string& s) {
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    size_t n = s.size() - i;
    if (n > 64) n = 64;
    out += s.substr(i, n);
    out += '\n';
    i += n;
  }
  return out;
}

std::string TrimRight(const std::string& s) {
  size_t n = s.size();
  while (n > 0) {
    char c = s[n - 1];
    if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
      --n;
    } else {
      break;
    }
  }
  return s.substr(0, n);
}

std::wstring FileNameOf(const std::wstring& path) {
  size_t p = path.find_last_of(L"\\/");
  return p == std::wstring::npos ? path : path.substr(p + 1);
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& name) {
  if (dir.empty()) return name;
  if (dir.back() == L'\\' || dir.back() == L'/') return dir + name;
  return dir + L"\\" + name;
}

bool FileExists(const std::wstring& path) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string LowerAscii(std::string s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
  }
  return s;
}

std::string HostKeyOf(const Host& h) {
  return LowerAscii(WideToUtf8(h.user)) + "@" + LowerAscii(WideToUtf8(h.host)) + ":" +
         std::to_string(h.port);
}

std::string ThemeText(int mode) {
  if (mode == 1) return "dark";
  if (mode == 2) return "light";
  return "system";
}

bool WriteKeyFile(const std::wstring& dir, const std::string& name, const std::string& pem,
                  std::wstring& actual) {
  if (name.empty() || pem.empty()) return false;
  CreateDirectoryW(dir.c_str(), nullptr);
  std::wstring base = Utf8ToWide(name);
  for (int round = 0; round < 40; ++round) {
    std::wstring cand = base;
    if (round > 0) cand += L"-" + std::to_wstring(round + 1);
    std::wstring path = JoinPath(dir, cand);
    if (!FileExists(path)) {
      if (!WriteTextFile(path, pem)) return false;
      actual = cand;
      return true;
    }
    std::string exist;
    if (ReadTextFile(path, exist) && exist == pem) {
      actual = cand;
      return true;
    }
  }
  return false;
}

}

std::wstring ResolveKeyFile(const Host& h) {
  if (h.keyPath.empty()) return FindKeyFor(WideToUtf8(h.host));
  if (FileExists(h.keyPath)) return h.keyPath;
  std::wstring cand = JoinPath(KeyDirectory(), h.keyPath);
  if (FileExists(cand)) return cand;
  return FindKeyFor(WideToUtf8(h.host));
}

bool ReadTextFile(const std::wstring& path, std::string& out) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > (LONGLONG)64 * 1024 * 1024) {
    CloseHandle(h);
    return false;
  }
  out.assign((size_t)size.QuadPart, '\0');
  DWORD total = 0;
  while (total < (DWORD)out.size()) {
    DWORD got = 0;
    if (!ReadFile(h, &out[total], (DWORD)out.size() - total, &got, nullptr) || got == 0) break;
    total += got;
  }
  CloseHandle(h);
  out.resize(total);
  return true;
}

bool WriteTextFile(const std::wstring& path, const std::string& text) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wrote = 0;
  BOOL ok = WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr);
  CloseHandle(h);
  return ok && wrote == (DWORD)text.size();
}

std::wstring BackupDefaultName() {
  SYSTEMTIME t{};
  GetLocalTime(&t);
  wchar_t buf[64];
  swprintf_s(buf, L"Skiff-backup-%04d%02d%02d-%02d%02d.skbk", t.wYear, t.wMonth, t.wDay, t.wHour,
             t.wMinute);
  return std::wstring(buf);
}

std::string BuildBackupPayload(const AppState& st) {
  JValue keys = JValue::Arr();
  JValue hosts = JValue::Arr();
  std::vector<std::wstring> keyNames;
  std::vector<std::string> keyPems;

  for (size_t i = 0; i < st.hosts.size(); ++i) {
    const Host& h = st.hosts[i];
    bool keyAuth = h.IsKeyAuth();
    JValue o;
    o.Set("name", JValue::Str(WideToUtf8(h.name)));
    o.Set("user", JValue::Str(WideToUtf8(h.user)));
    o.Set("host", JValue::Str(WideToUtf8(h.host)));
    o.Set("port", JValue::Num((double)h.port));
    o.Set("auth", JValue::Str(keyAuth ? "key" : "password"));
    o.Set("secret", JValue::Str(WideToUtf8(keyAuth ? h.keyPass : h.password)));
    o.Set("hostKey", JValue::Str(WideToUtf8(h.hostKey)));
    o.Set("hostKeyType", JValue::Str(WideToUtf8(h.hostKeyType)));
    o.Set("registeredAt", JValue::Str(WideToUtf8(h.registeredAt)));
    o.Set("expireAt", JValue::Str(WideToUtf8(h.expireAt)));
    o.Set("lastConnected", JValue::Str(WideToUtf8(h.lastConnected)));
    o.Set("autoConnect", JValue::Bool(h.autoConnect));

    std::wstring keyFile;
    if (keyAuth) {
      std::wstring path = ResolveKeyFile(h);
      if (!path.empty()) {
        std::string pem;
        if (ReadTextFile(path, pem) && !pem.empty()) {
          keyFile = FileNameOf(path);
          bool found = false;
          for (size_t k = 0; k < keyNames.size(); ++k) {
            if (keyNames[k] == keyFile) {
              found = true;
              break;
            }
          }
          if (!found) {
            keyNames.push_back(keyFile);
            keyPems.push_back(pem);
          }
        }
      }
    }
    o.Set("keyFile", JValue::Str(WideToUtf8(keyFile)));

    JValue tun = JValue::Arr();
    for (size_t k = 0; k < h.tunnels.size(); ++k) {
      const Tunnel& t = h.tunnels[k];
      JValue tv;
      tv.Set("type", JValue::Str(WideToUtf8(t.type)));
      tv.Set("port", JValue::Str(WideToUtf8(t.port)));
      tv.Set("dest", JValue::Str(WideToUtf8(t.dest)));
      tv.Set("on", JValue::Bool(t.on));
      tun.items.push_back(tv);
    }
    o.Set("tunnels", std::move(tun));
    hosts.items.push_back(o);
  }

  for (size_t i = 0; i < keyNames.size(); ++i) {
    JValue k;
    k.Set("name", JValue::Str(WideToUtf8(keyNames[i])));
    k.Set("pem", JValue::Str(keyPems[i]));
    keys.items.push_back(k);
  }

  JValue ui;
  ui.Set("theme", JValue::Str(ThemeText(st.themeMode)));
  ui.Set("lang", JValue::Str(st.langMode == 1 ? "en" : "zh"));
  ui.Set("termFontSize", JValue::Num((double)st.termFontSize));
  ui.Set("autoReconnect", JValue::Bool(st.autoReconnect));
  ui.Set("autoConnectAll", JValue::Bool(st.autoConnectAll));
  ui.Set("fpCheck", JValue::Bool(st.fpCheck));
  ui.Set("keepAlive", JValue::Bool(st.keepAliveIdx > 0));

  JValue src;
  src.Set("platform", JValue::Str("win"));
  src.Set("version", JValue::Str(WideToUtf8(SKIFF_WIN_VERSION)));

  JValue root;
  root.Set("format", JValue::Str("skiff-backup"));
  root.Set("schema", JValue::Num(1));
  root.Set("createdAt", JValue::Str(WideToUtf8(NowStamp())));
  root.Set("source", std::move(src));
  root.Set("keys", std::move(keys));
  root.Set("hosts", std::move(hosts));
  root.Set("ui", std::move(ui));
  return DumpJson(root);
}

bool MergeBackupPayload(const std::string& json, AppState& st, MergeStat& stat, std::wstring& err) {
  JValue root;
  if (!ParseJson(json, root) || root.type != JType::Object) {
    err = L"备份内容不是有效的 JSON";
    return false;
  }
  if (JStr(root, "format") != "skiff-backup") {
    err = L"不是轻舟备份文件";
    return false;
  }

  std::vector<std::string> srcNames;
  std::vector<std::wstring> dstNames;
  const JValue* keys = root.Find("keys");
  if (keys != nullptr && keys->type == JType::Array) {
    std::wstring dir = KeyDirectory();
    for (size_t i = 0; i < keys->items.size(); ++i) {
      const JValue& k = keys->items[i];
      std::string name = JStr(k, "name");
      std::string pem = JStr(k, "pem");
      if (name.empty() || pem.empty()) continue;
      std::wstring actual;
      if (!WriteKeyFile(dir, name, pem, actual)) continue;
      srcNames.push_back(name);
      dstNames.push_back(actual);
      ++stat.keys;
    }
  }

  std::vector<std::string> exist;
  for (size_t i = 0; i < st.hosts.size(); ++i) exist.push_back(HostKeyOf(st.hosts[i]));

  const JValue* hosts = root.Find("hosts");
  if (hosts != nullptr && hosts->type == JType::Array) {
    for (size_t i = 0; i < hosts->items.size(); ++i) {
      const JValue& o = hosts->items[i];
      if (o.type != JType::Object) continue;
      Host h;
      h.name = Utf8ToWide(JStr(o, "name"));
      h.user = Utf8ToWide(JStr(o, "user"));
      h.host = Utf8ToWide(JStr(o, "host"));
      h.port = JInt(o, "port", 22);
      if (h.user.empty()) h.user = L"root";
      if (h.port < 1 || h.port > 65535) h.port = 22;
      if (h.name.empty()) h.name = h.host.empty() ? L"导入主机" : h.host;
      std::string key = HostKeyOf(h);
      bool dup = false;
      for (size_t k = 0; k < exist.size(); ++k) {
        if (exist[k] == key) {
          dup = true;
          break;
        }
      }
      if (dup) {
        ++stat.skipped;
        continue;
      }

      bool keyAuth = JStr(o, "auth") == "key";
      h.auth = keyAuth ? L"密钥" : L"密码";
      std::wstring secret = Utf8ToWide(JStr(o, "secret"));
      if (keyAuth) {
        h.keyPass = secret;
      } else {
        h.password = secret;
      }
      std::string keyFile = JStr(o, "keyFile");
      if (keyAuth && !keyFile.empty()) {
        std::wstring mapped = Utf8ToWide(keyFile);
        for (size_t k = 0; k < srcNames.size(); ++k) {
          if (srcNames[k] == keyFile) {
            mapped = dstNames[k];
            break;
          }
        }
        h.keyPath = JoinPath(KeyDirectory(), mapped);
      }
      h.hostKey = Utf8ToWide(JStr(o, "hostKey"));
      h.hostKeyType = Utf8ToWide(JStr(o, "hostKeyType"));
      h.registeredAt = Utf8ToWide(JStr(o, "registeredAt"));
      h.expireAt = Utf8ToWide(JStr(o, "expireAt"));
      h.lastConnected = Utf8ToWide(JStr(o, "lastConnected"));
      h.autoConnect = JBool(o, "autoConnect", false);
      h.state = HostState::Off;
      h.latency = -1;
      h.id = st.nextHostId++;

      const JValue* tun = o.Find("tunnels");
      if (tun != nullptr && tun->type == JType::Array) {
        for (size_t k = 0; k < tun->items.size(); ++k) {
          const JValue& tv = tun->items[k];
          Tunnel t;
          t.type = Utf8ToWide(JStr(tv, "type"));
          if (t.type.empty()) t.type = L"L";
          t.port = Utf8ToWide(JStr(tv, "port"));
          t.dest = Utf8ToWide(JStr(tv, "dest"));
          t.on = JBool(tv, "on", true);
          t.state = TunState::Down;
          t.id = st.nextTunnelId++;
          t.hostId = h.id;
          h.tunnels.push_back(t);
        }
      }

      exist.push_back(key);
      st.hosts.push_back(h);
      ++stat.added;
    }
  }

  if (stat.added == 0 && stat.keys == 0 && stat.skipped == 0) {
    err = L"备份里没有可导入的数据";
    return false;
  }
  return true;
}

std::string SealBackup(const std::string& payload, const std::wstring& password, bool encrypt) {
  JValue head;
  head.Set("plat", JValue::Str("win"));
  head.Set("ver", JValue::Str(WideToUtf8(SKIFF_WIN_VERSION)));
  if (!encrypt) {
    head.Set("enc", JValue::Str("none"));
    std::string text = kMagic;
    text += '\n';
    text += DumpJson(head);
    text += "\n\n";
    text += payload;
    text += '\n';
    return text;
  }

  unsigned char salt[16];
  unsigned char iv[16];
  unsigned char km[64];
  unsigned char mac[32];
  if (!RndBytes(salt, 16) || !RndBytes(iv, 16)) return std::string();
  std::string pwd = WideToUtf8(password);
  if (!Pbkdf2(pwd, salt, 16, kIter, km, 64)) return std::string();
  std::string cipher;
  if (!AesCbc(true, km, iv, payload, cipher)) return std::string();

  head.Set("enc", JValue::Str("aes-256-cbc+hmac-sha256"));
  head.Set("iter", JValue::Num(kIter));
  head.Set("salt", JValue::Str(B64Enc(std::string((const char*)salt, 16))));
  head.Set("iv", JValue::Str(B64Enc(std::string((const char*)iv, 16))));
  std::string headText = DumpJson(head);

  std::string macData = headText;
  macData += "\n\n";
  macData += cipher;
  if (!HmacSha256(km + 32, 32, macData, mac)) return std::string();

  std::string text = kMagic;
  text += '\n';
  text += headText;
  text += "\n\n";
  text += Wrap64(B64Enc(cipher));
  text += "MAC ";
  text += B64Enc(std::string((const char*)mac, 32));
  text += '\n';
  return text;
}

bool OpenBackup(const std::string& text, const std::wstring& password, std::string& payload) {
  std::string doc;
  doc.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\r') doc += text[i];
  }
  size_t nl = doc.find('\n');
  if (nl == std::string::npos) return false;
  if (doc.compare(0, nl, kMagic) != 0) return false;
  std::string rest = doc.substr(nl + 1);

  std::string macB64;
  size_t macPos = rest.rfind("\nMAC ");
  if (macPos != std::string::npos) {
    macB64 = TrimRight(rest.substr(macPos + 5));
    rest = rest.substr(0, macPos);
  }
  size_t sep = rest.find("\n\n");
  if (sep == std::string::npos) return false;
  std::string headText = rest.substr(0, sep);
  std::string body = TrimRight(rest.substr(sep + 2));

  JValue head;
  if (!ParseJson(headText, head) || head.type != JType::Object) return false;
  std::string enc = JStr(head, "enc");
  if (enc.empty() || enc == "none") {
    payload = body;
    return true;
  }
  if (password.empty() || macB64.empty()) return false;

  std::string saltRaw;
  std::string ivRaw;
  if (!B64Dec(JStr(head, "salt"), saltRaw) || saltRaw.size() != 16) return false;
  if (!B64Dec(JStr(head, "iv"), ivRaw) || ivRaw.size() != 16) return false;
  int iter = JInt(head, "iter", kIter);
  if (iter < 1000 || iter > 5000000) return false;

  unsigned char km[64];
  std::string pwd = WideToUtf8(password);
  if (!Pbkdf2(pwd, (const unsigned char*)saltRaw.data(), 16, iter, km, 64)) return false;

  std::string cipher;
  if (!B64Dec(body, cipher) || cipher.empty()) return false;

  std::string macData = headText;
  macData += "\n\n";
  macData += cipher;
  unsigned char mac[32];
  if (!HmacSha256(km + 32, 32, macData, mac)) return false;
  std::string want;
  if (!B64Dec(macB64, want) || want.size() != 32) return false;
  unsigned char diff = 0;
  for (int i = 0; i < 32; ++i) diff |= (unsigned char)(want[(size_t)i] ^ mac[i]);
  if (diff != 0) return false;

  return AesCbc(false, km, (const unsigned char*)ivRaw.data(), cipher, payload);
}

}
