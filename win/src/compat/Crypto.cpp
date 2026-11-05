#include <windows.h>
#include <bcrypt.h>
#include <cstdlib>

extern "C" unsigned char* skSHA256(const unsigned char* data, size_t len, unsigned char* out) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objLen = 0;
    DWORD hashLen = 0;
    DWORD cb = 0;
    PUCHAR obj = nullptr;

    if (out == nullptr) return nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return out;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen),
                          sizeof(objLen), &cb, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }
    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen),
                          sizeof(hashLen), &cb, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }
    obj = static_cast<PUCHAR>(malloc(objLen));
    if (obj == nullptr) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }
    if (BCryptCreateHash(alg, &hash, obj, objLen, nullptr, 0, 0) < 0) {
        free(obj);
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }
    BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(len), 0);
    BCryptFinishHash(hash, out, hashLen, 0);
    BCryptDestroyHash(hash);
    free(obj);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}
