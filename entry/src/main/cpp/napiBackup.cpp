#include <napi/native_api.h>
#include <hilog/log.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <cstring>
#include <string>

#include "napiBackup.h"

namespace {

const int kIter = 200000;
const char kMagic[] = "SKIFFBK/1";
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string B64Enc(const std::string& in) {
    std::string out;
    size_t i = 0;
    while (i + 2 < in.size()) {
        unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
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

bool Pbkdf2(const std::string& pwd, const unsigned char* salt, int iter, unsigned char* out, int outLen) {
    return PKCS5_PBKDF2_HMAC(pwd.data(), (int)pwd.size(), salt, 16, iter, EVP_sha256(), outLen, out) == 1;
}

bool AesCbc(bool enc, const unsigned char* key, const unsigned char* iv, const std::string& in,
            std::string& out) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) return false;
    bool ok = false;
    int outLen = 0;
    int total = 0;
    out.assign(in.size() + 32, '\0');
    do {
        if (EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv, enc ? 1 : 0) != 1) break;
        if (EVP_CipherUpdate(ctx, (unsigned char*)&out[0], &outLen, (const unsigned char*)in.data(),
                             (int)in.size()) != 1) {
            break;
        }
        total = outLen;
        if (EVP_CipherFinal_ex(ctx, (unsigned char*)&out[total], &outLen) != 1) break;
        total += outLen;
        out.resize((size_t)total);
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool HmacSha256(const unsigned char* key, int keyLen, const std::string& data, unsigned char* out) {
    unsigned int len = 0;
    if (HMAC(EVP_sha256(), key, keyLen, (const unsigned char*)data.data(), data.size(), out, &len) == nullptr) {
        return false;
    }
    return len == 32;
}

std::string HeadJson(const std::string& enc, int iter, const std::string& salt, const std::string& iv) {
    std::string s = "{\"plat\":\"mobile\",\"enc\":\"";
    s += enc;
    s += "\"";
    if (!salt.empty()) {
        s += ",\"iter\":";
        s += std::to_string(iter);
        s += ",\"salt\":\"";
        s += salt;
        s += "\",\"iv\":\"";
        s += iv;
        s += "\"";
    }
    s += "}";
    return s;
}

std::string Field(const std::string& json, const char* key) {
    std::string pat = "\"";
    pat += key;
    pat += "\":\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return std::string();
    p += pat.size();
    size_t e = json.find('"', p);
    if (e == std::string::npos) return std::string();
    return json.substr(p, e - p);
}

int NumField(const std::string& json, const char* key, int def) {
    std::string pat = "\"";
    pat += key;
    pat += "\":";
    size_t p = json.find(pat);
    if (p == std::string::npos) return def;
    p += pat.size();
    int v = 0;
    bool any = false;
    while (p < json.size() && json[p] >= '0' && json[p] <= '9') {
        v = v * 10 + (json[p] - '0');
        ++p;
        any = true;
    }
    return any ? v : def;
}

std::string Seal(const std::string& payload, const std::string& pwd, bool encrypt) {
    if (!encrypt) {
        std::string text = kMagic;
        text += '\n';
        text += HeadJson("none", 0, std::string(), std::string());
        text += "\n\n";
        text += payload;
        text += '\n';
        return text;
    }
    unsigned char salt[16];
    unsigned char iv[16];
    unsigned char km[64];
    unsigned char mac[32];
    if (RAND_bytes(salt, 16) != 1 || RAND_bytes(iv, 16) != 1) return std::string();
    if (!Pbkdf2(pwd, salt, kIter, km, 64)) return std::string();
    std::string cipher;
    if (!AesCbc(true, km, iv, payload, cipher)) return std::string();
    std::string head = HeadJson("aes-256-cbc+hmac-sha256", kIter, B64Enc(std::string((const char*)salt, 16)),
                                B64Enc(std::string((const char*)iv, 16)));
    std::string macData = head;
    macData += "\n\n";
    macData += cipher;
    if (!HmacSha256(km + 32, 32, macData, mac)) return std::string();
    std::string text = kMagic;
    text += '\n';
    text += head;
    text += "\n\n";
    text += Wrap64(B64Enc(cipher));
    text += "MAC ";
    text += B64Enc(std::string((const char*)mac, 32));
    text += '\n';
    return text;
}

bool Open(const std::string& text, const std::string& pwd, std::string& payload) {
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
    std::string head = rest.substr(0, sep);
    std::string body = TrimRight(rest.substr(sep + 2));

    std::string enc = Field(head, "enc");
    if (enc.empty() || enc == "none") {
        payload = body;
        return true;
    }
    if (pwd.empty() || macB64.empty()) return false;

    std::string saltRaw;
    std::string ivRaw;
    if (!B64Dec(Field(head, "salt"), saltRaw) || saltRaw.size() != 16) return false;
    if (!B64Dec(Field(head, "iv"), ivRaw) || ivRaw.size() != 16) return false;
    int iter = NumField(head, "iter", kIter);
    if (iter < 1000 || iter > 5000000) return false;

    unsigned char km[64];
    if (!Pbkdf2(pwd, (const unsigned char*)saltRaw.data(), iter, km, 64)) return false;

    std::string cipher;
    if (!B64Dec(body, cipher) || cipher.empty()) return false;

    std::string macData = head;
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

bool GetString(napi_env env, napi_value v, std::string& out) {
    size_t len = 0;
    if (napi_get_value_string_utf8(env, v, nullptr, 0, &len) != napi_ok) return false;
    out.assign(len, '\0');
    size_t copied = 0;
    if (napi_get_value_string_utf8(env, v, &out[0], len + 1, &copied) != napi_ok) return false;
    out.resize(copied);
    return true;
}

napi_value MakeString(napi_env env, const std::string& s) {
    napi_value out = nullptr;
    napi_create_string_utf8(env, s.c_str(), s.size(), &out);
    return out;
}

napi_value NAPI_SealBackup(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3] = { nullptr, nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 2) return MakeString(env, std::string());
    std::string payload;
    std::string pwd;
    if (!GetString(env, args[0], payload)) return MakeString(env, std::string());
    GetString(env, args[1], pwd);
    bool encrypt = true;
    if (argc >= 3) napi_get_value_bool(env, args[2], &encrypt);
    return MakeString(env, Seal(payload, pwd, encrypt));
}

napi_value NAPI_OpenBackup(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = { nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1) return MakeString(env, std::string());
    std::string text;
    std::string pwd;
    if (!GetString(env, args[0], text)) return MakeString(env, std::string());
    if (argc >= 2) GetString(env, args[1], pwd);
    std::string payload;
    if (!Open(text, pwd, payload)) {
        OH_LOG_WARN(LOG_APP, "openBackup failed");
        return MakeString(env, std::string());
    }
    return MakeString(env, payload);
}

}

void RegisterBackupFunctions(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "sealBackup", nullptr, NAPI_SealBackup, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "openBackup", nullptr, NAPI_OpenBackup, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_status status = napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    if (status != napi_ok) {
        OH_LOG_ERROR(LOG_APP, "Failed to register backup N-API functions");
    }
}
