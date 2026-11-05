#include "KeyGen.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif

namespace sk {
namespace {

std::string Bytes(const unsigned char* p, size_t n) {
    if (p == nullptr || n == 0) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char*>(p), n);
}

void DerPutLen(std::string& out, size_t len) {
    if (len < 0x80) {
        out.push_back(static_cast<char>(len));
        return;
    }
    unsigned char tmp[sizeof(size_t)];
    size_t n = 0;
    size_t v = len;
    while (v != 0) {
        tmp[n++] = static_cast<unsigned char>(v & 0xFF);
        v >>= 8;
    }
    out.push_back(static_cast<char>(0x80 | n));
    while (n > 0) {
        --n;
        out.push_back(static_cast<char>(tmp[n]));
    }
}

std::string Tlv(unsigned char tag, const std::string& body) {
    std::string out;
    out.reserve(body.size() + 8);
    out.push_back(static_cast<char>(tag));
    DerPutLen(out, body.size());
    out += body;
    return out;
}

std::string DerIntRaw(const unsigned char* be, size_t n) {
    size_t i = 0;
    while (i + 1 < n && be[i] == 0x00) {
        ++i;
    }
    std::string body;
    if (n == 0) {
        body.push_back(static_cast<char>(0x00));
    } else {
        if ((be[i] & 0x80) != 0) {
            body.push_back(static_cast<char>(0x00));
        }
        body += Bytes(be + i, n - i);
    }
    return Tlv(0x02, body);
}

std::string DerIntVal(unsigned v) {
    unsigned char tmp[8];
    size_t n = 0;
    unsigned t = v;
    do {
        tmp[n++] = static_cast<unsigned char>(t & 0xFF);
        t >>= 8;
    } while (t != 0);
    std::string body;
    if ((tmp[n - 1] & 0x80) != 0) {
        body.push_back(static_cast<char>(0x00));
    }
    while (n > 0) {
        --n;
        body.push_back(static_cast<char>(tmp[n]));
    }
    return Tlv(0x02, body);
}

std::string DerOctet(const std::string& data) {
    return Tlv(0x04, data);
}

std::string DerBitString(const std::string& data) {
    std::string body;
    body.push_back(static_cast<char>(0x00));
    body += data;
    return Tlv(0x03, body);
}

std::string DerSequence(const std::string& body) {
    return Tlv(0x30, body);
}

std::string Base64(const std::string& in) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        unsigned v = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8) |
                     static_cast<unsigned char>(in[i + 2]);
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(kTable[(v >> 6) & 0x3F]);
        out.push_back(kTable[v & 0x3F]);
        i += 3;
    }
    size_t rest = in.size() - i;
    if (rest == 1) {
        unsigned v = static_cast<unsigned char>(in[i]) << 16;
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rest == 2) {
        unsigned v = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8);
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(kTable[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::string PemWrap(const char* label, const std::string& der) {
    std::string b64 = Base64(der);
    std::string out;
    out += "-----BEGIN ";
    out += label;
    out += "-----\n";
    for (size_t i = 0; i < b64.size(); i += 64) {
        size_t take = b64.size() - i;
        if (take > 64) {
            take = 64;
        }
        out.append(b64, i, take);
        out.push_back('\n');
    }
    out += "-----END ";
    out += label;
    out += "-----\n";
    return out;
}

void PutU32(std::string& out, unsigned v) {
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>(v & 0xFF));
}

std::string SshString(const std::string& data) {
    std::string out;
    PutU32(out, static_cast<unsigned>(data.size()));
    out += data;
    return out;
}

std::string SshMpint(const unsigned char* be, size_t n) {
    size_t i = 0;
    while (i < n && be[i] == 0x00) {
        ++i;
    }
    std::string body;
    if (i < n) {
        if ((be[i] & 0x80) != 0) {
            body.push_back(static_cast<char>(0x00));
        }
        body += Bytes(be + i, n - i);
    }
    std::string out;
    PutU32(out, static_cast<unsigned>(body.size()));
    out += body;
    return out;
}

const unsigned char kRsaAlgOid[] = {
    0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};

const unsigned char kEcPubOid[] = {
    0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};

const unsigned char kPrime256v1Oid[] = {
    0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

const unsigned char kNullDer[] = {0x05, 0x00};

struct AlgHandle {
    BCRYPT_ALG_HANDLE h;
    AlgHandle() : h(nullptr) {}
    ~AlgHandle() {
        if (h != nullptr) {
            BCryptCloseAlgorithmProvider(h, 0);
        }
    }
    AlgHandle(const AlgHandle&) = delete;
    AlgHandle& operator=(const AlgHandle&) = delete;
};

struct KeyHandle {
    BCRYPT_KEY_HANDLE h;
    KeyHandle() : h(nullptr) {}
    ~KeyHandle() {
        if (h != nullptr) {
            BCryptDestroyKey(h);
        }
    }
    KeyHandle(const KeyHandle&) = delete;
    KeyHandle& operator=(const KeyHandle&) = delete;
};

bool ExportBlob(BCRYPT_KEY_HANDLE key, LPCWSTR type, std::string& out, std::string& err) {
    ULONG cb = 0;
    NTSTATUS st = BCryptExportKey(key, nullptr, type, nullptr, 0, &cb, 0);
    if (!BCRYPT_SUCCESS(st) || cb == 0) {
        err = "导出密钥数据失败";
        return false;
    }
    out.resize(cb);
    st = BCryptExportKey(key, nullptr, type, reinterpret_cast<PUCHAR>(&out[0]), cb, &cb, 0);
    if (!BCRYPT_SUCCESS(st)) {
        err = "导出密钥数据失败";
        return false;
    }
    out.resize(cb);
    return true;
}

KeyPair GenRsa(const std::string& comment) {
    KeyPair kp;

    AlgHandle alg;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_RSA_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "打开 RSA 算法提供程序失败";
        return kp;
    }

    KeyHandle key;
    st = BCryptGenerateKeyPair(alg.h, &key.h, 2048, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "生成 RSA 密钥对失败";
        return kp;
    }
    st = BCryptFinalizeKeyPair(key.h, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "生成 RSA 密钥对失败";
        return kp;
    }

    std::string blob;
    if (!ExportBlob(key.h, BCRYPT_RSAPRIVATE_BLOB, blob, kp.error)) {
        return kp;
    }

    if (blob.size() < sizeof(BCRYPT_RSAKEY_BLOB)) {
        kp.error = "RSA 密钥数据异常";
        return kp;
    }
    BCRYPT_RSAKEY_BLOB hdr;
    std::memcpy(&hdr, blob.data(), sizeof(hdr));
    size_t cbExp = hdr.cbPublicExp;
    size_t cbMod = hdr.cbModulus;
    size_t cbP = hdr.cbPrime1;
    size_t cbQ = hdr.cbPrime2;
    size_t need = sizeof(BCRYPT_RSAKEY_BLOB) + cbExp + 2 * cbMod + 3 * cbP + 2 * cbQ;
    if (cbExp == 0 || cbMod == 0 || cbP == 0 || cbQ == 0 || blob.size() < need) {
        kp.error = "RSA 密钥数据异常";
        return kp;
    }

    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(blob.data()) + sizeof(BCRYPT_RSAKEY_BLOB);
    size_t off = 0;
    const unsigned char* pubExp = base + off;
    off += cbExp;
    const unsigned char* modulus = base + off;
    off += cbMod;
    const unsigned char* prime1 = base + off;
    off += cbP;
    const unsigned char* prime2 = base + off;
    off += cbQ;
    const unsigned char* exp1 = base + off;
    off += cbP;
    const unsigned char* exp2 = base + off;
    off += cbQ;
    const unsigned char* coef = base + off;
    off += cbP;
    const unsigned char* privExp = base + off;

    std::string pkcs1;
    pkcs1 += DerIntVal(0);
    pkcs1 += DerIntRaw(modulus, cbMod);
    pkcs1 += DerIntRaw(pubExp, cbExp);
    pkcs1 += DerIntRaw(privExp, cbMod);
    pkcs1 += DerIntRaw(prime1, cbP);
    pkcs1 += DerIntRaw(prime2, cbQ);
    pkcs1 += DerIntRaw(exp1, cbP);
    pkcs1 += DerIntRaw(exp2, cbQ);
    pkcs1 += DerIntRaw(coef, cbP);
    std::string pkcs1Der = DerSequence(pkcs1);

    std::string algId = DerSequence(Bytes(kRsaAlgOid, sizeof(kRsaAlgOid)) +
                                    Bytes(kNullDer, sizeof(kNullDer)));
    std::string pki;
    pki += DerIntVal(0);
    pki += algId;
    pki += DerOctet(pkcs1Der);
    kp.privatePem = PemWrap("PRIVATE KEY", DerSequence(pki));

    std::string pub;
    pub += SshString("ssh-rsa");
    pub += SshMpint(pubExp, cbExp);
    pub += SshMpint(modulus, cbMod);
    kp.publicLine = "ssh-rsa ";
    kp.publicLine += Base64(pub);
    if (!comment.empty()) {
        kp.publicLine += " ";
        kp.publicLine += comment;
    }

    kp.ok = true;
    return kp;
}

KeyPair GenEcdsa(const std::string& comment) {
    KeyPair kp;

    AlgHandle alg;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "打开 ECDSA 算法提供程序失败";
        return kp;
    }

    KeyHandle key;
    st = BCryptGenerateKeyPair(alg.h, &key.h, 256, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "生成 ECDSA 密钥对失败";
        return kp;
    }
    st = BCryptFinalizeKeyPair(key.h, 0);
    if (!BCRYPT_SUCCESS(st)) {
        kp.error = "生成 ECDSA 密钥对失败";
        return kp;
    }

    std::string blob;
    if (!ExportBlob(key.h, BCRYPT_ECCPRIVATE_BLOB, blob, kp.error)) {
        return kp;
    }

    if (blob.size() < sizeof(BCRYPT_ECCKEY_BLOB)) {
        kp.error = "ECDSA 密钥数据异常";
        return kp;
    }
    BCRYPT_ECCKEY_BLOB hdr;
    std::memcpy(&hdr, blob.data(), sizeof(hdr));
    size_t cbKey = hdr.cbKey;
    if (cbKey == 0 || blob.size() < sizeof(BCRYPT_ECCKEY_BLOB) + cbKey * 3) {
        kp.error = "ECDSA 密钥数据异常";
        return kp;
    }

    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(blob.data()) + sizeof(BCRYPT_ECCKEY_BLOB);
    const unsigned char* x = base;
    const unsigned char* y = base + cbKey;
    const unsigned char* d = base + cbKey * 2;

    std::string point;
    point.push_back(static_cast<char>(0x04));
    point += Bytes(x, cbKey);
    point += Bytes(y, cbKey);

    std::string sec1;
    sec1 += DerIntVal(1);
    sec1 += DerOctet(Bytes(d, cbKey));
    sec1 += Tlv(0xA0, Bytes(kPrime256v1Oid, sizeof(kPrime256v1Oid)));
    sec1 += Tlv(0xA1, DerBitString(point));
    std::string sec1Der = DerSequence(sec1);

    std::string algId = DerSequence(Bytes(kEcPubOid, sizeof(kEcPubOid)) +
                                    Bytes(kPrime256v1Oid, sizeof(kPrime256v1Oid)));
    std::string pki;
    pki += DerIntVal(0);
    pki += algId;
    pki += DerOctet(sec1Der);
    kp.privatePem = PemWrap("PRIVATE KEY", DerSequence(pki));

    std::string pub;
    pub += SshString("ecdsa-sha2-nistp256");
    pub += SshString("nistp256");
    pub += SshString(point);
    kp.publicLine = "ecdsa-sha2-nistp256 ";
    kp.publicLine += Base64(pub);
    if (!comment.empty()) {
        kp.publicLine += " ";
        kp.publicLine += comment;
    }

    kp.ok = true;
    return kp;
}

std::string Narrow(const std::wstring& w) {
    if (w.empty()) {
        return std::string();
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return std::string();
    }
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Widen(const std::string& s) {
    if (s.empty()) {
        return std::wstring();
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

bool WriteAll(const std::wstring& path, const std::string& data, std::string& err) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = "无法写入文件: ";
        err += Narrow(path);
        return false;
    }
    size_t off = 0;
    while (off < data.size()) {
        size_t remain = data.size() - off;
        DWORD chunk = remain > 0x10000000u ? 0x10000000u : static_cast<DWORD>(remain);
        DWORD written = 0;
        if (WriteFile(h, data.data() + off, chunk, &written, nullptr) == 0 || written == 0) {
            CloseHandle(h);
            err = "无法写入文件: ";
            err += Narrow(path);
            return false;
        }
        off += written;
    }
    CloseHandle(h);
    return true;
}

}

KeyPair Generate(KeyKind kind, const std::string& comment) {
    if (kind == KeyKind::Rsa2048) {
        return GenRsa(comment);
    }
    if (kind == KeyKind::EcdsaP256) {
        return GenEcdsa(comment);
    }
    if (kind == KeyKind::Ed25519) {
        KeyPair kp;
        kp.ok = false;
        kp.error = "Ed25519 暂不支持";
        return kp;
    }
    KeyPair kp;
    kp.ok = false;
    kp.error = "未知的密钥类型";
    return kp;
}

bool SaveTo(const KeyPair& kp, const std::wstring& dir, const std::string& name,
            std::string& err) {
    err.clear();
    if (!kp.ok) {
        err = "密钥尚未生成";
        return false;
    }
    if (dir.empty()) {
        err = "输出目录为空";
        return false;
    }
    if (name.empty()) {
        err = "文件名为空";
        return false;
    }

    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (CreateDirectoryW(dir.c_str(), nullptr) == 0) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                err = "无法创建目录: ";
                err += Narrow(dir);
                return false;
            }
        }
    } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        err = "目标不是目录: ";
        err += Narrow(dir);
        return false;
    }

    std::wstring base = Widen(name);
    if (base.empty()) {
        err = "文件名无效";
        return false;
    }

    std::wstring prefix = dir;
    if (prefix.back() != L'\\' && prefix.back() != L'/') {
        prefix.push_back(L'\\');
    }

    std::wstring pemPath = prefix + base + L".pem";
    std::wstring pubPath = prefix + base + L".pub";

    std::string pub = kp.publicLine;
    pub.push_back('\n');

    if (!WriteAll(pemPath, kp.privatePem, err)) {
        return false;
    }
    if (!WriteAll(pubPath, pub, err)) {
        return false;
    }
    return true;
}

}
